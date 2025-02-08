#include "simpoint_module.h"
#include "dts.h"
#include "libfdt.h"
#include "processor.h"

#include <fcntl.h>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

#include "spikeAdpterHooks.hpp"
#include "spikehooker.h"

#define VSEW8 (0x0)
#define VLMUL8 (0x3)

simpoint_module_t::simpoint_module_t(const simpoint_module_config_t &config,
                                     simif_t *sim, bus_t *bus, clint_t *clint,
                                     syscall_t *syscall, std::string dtb)
    : insn_counter(0), config(config), sim(sim), bus(bus), clint(clint),
      syscall(syscall), proc(sim->get_harts().at(0)), dtb(std::move(dtb)),
      benchmark_exit_code(0), terminate_simulation(false) {
  const std::map<size_t, processor_t *> &harts = sim->get_harts();
  if (harts.size() != 1) {
    std::cerr << "error: simpoint only supports running on single core."
              << std::endl;
    exit(-1);
  }

  if (config.snapshot_load_name) {
    cpu_deserialize(config.snapshot_load_name);
    ram_deserialize(config.snapshot_load_name);
    fds_deserialize(config.snapshot_load_name);
  }

  if (config.simpoint_file_name) {
    read_simpoints();
  }
}

void simpoint_module_t::read_simpoints() {
  std::ifstream simpoint_fin(config.simpoint_file_name);
  if (!simpoint_fin.is_open()) {
    std::cerr << "error: could not open simpoint file "
              << config.simpoint_file_name << std::endl;
    exit(-1);
  }

  std::string filename = config.simpoint_file_name;
  auto pos = filename.rfind(".keeprun");
  if (pos != std::string::npos and pos + 8 == filename.size()) {
    sp_trace.keeprun_after_done = true;
  }

  uint64_t distance = 0;
  uint64_t num = 0;
  while (simpoint_fin >> distance >> num) {
    uint64_t start = distance * config.intervals;
    if (0 == start) {
      start = SIMPOINT_BOOT_CODE_INSNS;
    }

    simpoints.emplace_back(start, num);
  }

  if (simpoints.empty()) {
    std::cerr << "error: simpoint file " << config.simpoint_file_name
              << " appears empty of invalid" << std::endl;
    exit(-1);
  }

  std::sort(simpoints.begin(), simpoints.end());
  for (auto sp : simpoints) {
    std::cerr << std::endl;
    std::cerr << "simpoint " << sp.id << " start at " << sp.start << std::endl;
  }
}

simpoint_module_t::~simpoint_module_t() = default;

bool simpoint_module_t::is_maxinsns_reached() const {
  if (0 == config.maxinsns) {
    return false;
  }

  return insn_counter >= config.maxinsns;
}

void simpoint_module_t::simpoint_step(size_t step) {
  reg_t &last_pc = step_last_pc;
  reg_t pc = proc->get_state()->pc;

  if (pc < SIMPOINT_BOOTROM_BASE || pc == last_pc) {
    // ignore instructions in spike bootrom and duplicated.
    return;
  }

  if (!config.simpoint_roi && pc >= config.simpoint_start) {
    config.simpoint_roi = true;
  }

  if (config.simpoint_roi) {
    if (!simpoints.empty()) {
      create_sp_checkpoints();
    } else if (open_bb_fout() && simpoints_bb_fout.is_open()) {
      // create bb trace
      create_bb_trace(pc);
    }
  }

  if (is_maxinsns_reached()) {
    simpoint_exit();
    std::cerr << std::endl
              << "*** core0 exit due to reaching maxinsns. ***" << std::endl;
    if (exitHook(0)) {
      exit(0);
    }
  }

  if (terminate_simulation) {
    std::cerr
        << std::endl
        << "*** core0 exit due to simpoint terminate, benchmark exit code "
        << benchmark_exit_code << ". ***" << std::endl;
    if (exitHook(0)) {
      exit(0);
    }
  }

  bool keep_going = true;
  if (proc and proc->get_log_commits_enabled()) {
    keep_going = continueHook();
  }
  if (keep_going) {
    insn_counter += step;
    last_pc = pc;
  }
}

void simpoint_module_t::create_sp_checkpoints() {
  uint64_t &ninst = sp_trace.ninst;
  uint64_t &simpoint_next = sp_trace.simpoint_next;
  ninst++;
  if (simpoint_next >= simpoints.size()) {
    return;
  }

  std::string module_id_str;
  if (module_id != 0) {
    module_id_str += "_";
    module_id_str += std::to_string(module_id);
  }

  auto &sp = simpoints[simpoint_next];
  if (ninst > sp.start) {
    std::string save_name = "sp" + std::to_string(sp.id) + module_id_str;
    cpu_serialize(save_name.c_str());
    ram_serialize(save_name.c_str());
    fds_serialize(save_name.c_str());

    simpoint_next++;
    if (simpoint_next == simpoints.size() and sp_trace.keeprun_after_done == false) {
      std::cerr << std::endl
                << "*** core0 exit because all checkpoints create finished. ***"
                << std::endl;
      if (exitHook(0)) {
        exit(0);
      }
    }
  }
}

void simpoint_module_t::create_bb_trace(reg_t pc) {
  if (has_rollback) {
    std::cerr << "Warnning, bb trace do not support rollback!!!" << std::endl;
    return;
  }
  if (not bb_trace.is_init) {
    bb_trace.next_bbv_dump = config.intervals + insn_counter;
    bb_trace.is_init = true;
  }
  uint64_t &last_pc = bb_trace.last_pc;
  uint64_t &ninst = bb_trace.ninst;
  uint64_t &next_bbv_dump = bb_trace.next_bbv_dump;
  std::unordered_map<uint64_t, uint64_t> &bbv = bb_trace.bbv;
  std::unordered_map<uint64_t, uint64_t> &pc2id = bb_trace.pc2id;
  int &next_id = bb_trace.next_id;

  ninst++;

  if (insn_counter >= next_bbv_dump) {
    if (config.maxinsns - insn_counter >= config.intervals) {
      next_bbv_dump += config.intervals;
    } else {
      next_bbv_dump = config.maxinsns;
    }

    if (!bbv.empty()) {
      simpoints_bb_fout << "T";
      for (const auto ent : bbv) {
        auto it = pc2id.find(ent.first);
        uint64_t id = 0;
        if (it == pc2id.end()) {
          id = next_id;
          next_id++;
          pc2id[ent.first] = next_id;
        } else {
          id = it->second;
        }

        simpoints_bb_fout << ":" << id << ":" << ent.second << " ";
      }
      simpoints_bb_fout << std::endl;
      bbv.clear();
    }
  }

  if ((last_pc + 2) != pc && (last_pc + 4) != pc) {
    bbv[last_pc] += ninst;
    //    simpoints_bb_fout << "xxxBB 0x" << std::hex << pc << " " << std::dec
    //    << ninst << std::endl;
    ninst = 0;
  }
  last_pc = pc;
}

static inline void compress_file(const std::string &source_file,
                                 const std::string &dest_file) {
  // compress the file using system's zip tool
  std::string command = "zip -r " + dest_file + " " + source_file;

  std::cerr << "command: " + command << std::endl;

  int ret = std::system(command.c_str());
  if (ret != 0) {
    std::cerr << "error: compress " << source_file << " failed." << std::endl;
    exit(ret);
  }
}

static inline void decompress_file(const std::string &source_file) {
  // decompress the file using system's unzip tool
  std::string command = "unzip " + source_file;

  std::cerr << "command: " + command << std::endl;

  int ret = std::system(command.c_str());
  if (ret != 0) {
    std::cerr << "error: decompress " << source_file << " failed." << std::endl;
    exit(ret);
  }
}

static inline void remove_file(const std::string &file) {
  int ret = std::remove(file.c_str());
  if (ret != 0) {
    std::cerr << "error: remove " << file << " failed." << std::endl;
    exit(ret);
  }
}

void simpoint_module_t::cpu_deserialize(const char *load_name) {
  proc->set_debug_boot();

  // load boot code to bootrom
  std::string bootrom_load_name(load_name);
  bootrom_load_name.append(".bootram");

  std::ifstream boot_fin(bootrom_load_name, std::ios::binary);
  if (!boot_fin.good()) {
    std::cerr << "can't find bootram: " << bootrom_load_name << std::endl;
    exit(-1);
  }

  std::stringstream boot_stream;
  boot_stream << boot_fin.rdbuf();
  std::string boot_image = boot_stream.str();
  std::vector<char> rom;
  rom.insert(rom.begin(), boot_image.begin(), boot_image.end());

  bootrom = std::make_unique<rom_device_t>(rom);
  bus->add_device(SIMPOINT_BOOTROM_BASE, bootrom.get());
}

void simpoint_module_t::ram_deserialize(const char *load_name) {
  // load main ram to memory
  std::string mainram_load_name(load_name);
  mainram_load_name.append(".mainram");

  if (config.snapshot_compress) {
    std::string mainram_compress_name = mainram_load_name + std::string(".zip");
    decompress_file(mainram_compress_name);
  }

  std::ifstream main_fin(mainram_load_name, std::ios::binary);
  if (!main_fin.good()) {
    std::cerr << "can't find mainram: " << mainram_load_name << std::endl;
    exit(-1);
  }

  main_fin.seekg(0, std::ios::end);
  std::streampos fileSize = main_fin.tellg();
  main_fin.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(fileSize);

  main_fin.read(reinterpret_cast<char *>(buffer.data()), fileSize);

  sim->mmio_store(SIMPOINT_MAINRAM_BASE, fileSize, buffer.data());

  if (config.snapshot_compress) {
    remove_file(mainram_load_name);
  }
}

void simpoint_module_t::fds_deserialize(const char *load_name) {
  std::string fds_load_name(load_name);
  fds_load_name.append(".fds");

  std::ifstream fds_fin(fds_load_name);
  if (!fds_fin.good()) {
    std::cerr << "can't find fds: " << fds_load_name
              << ", unable to restore fd mapping in syscall." << std::endl;
    return;
  }

  std::vector<int> &fds = syscall->get_fds().get_fds_vector();
  size_t i = 0;
  off_t offset = 0;
  std::string path;
  int flags = 0;
  mode_t mode = 0;

  while (fds_fin >> i >> path >> flags >> mode >> offset) {
    if (i == fds.size()) {
      fds.resize(i + 1);
    }

    int fd = open(path.c_str(), flags, mode);
    if (-1 == fd) {
      std::cerr << "error: open file " << path << " failed" << std::endl;
      exit(-1);
    }

    off_t ret = lseek(fd, offset, SEEK_SET);
    if (-1 == ret) {
      std::cerr << "error: set file offset " << path << " failed" << std::endl;
      exit(-1);
    }

    fds[i] = fd;
  }
}

void simpoint_module_t::save_regs_file(const char *save_name) {
  std::string regs_save_name(save_name);
  regs_save_name.append(".re_regs");

  std::ofstream regs_save_fout(regs_save_name);
  if (!regs_save_fout.is_open()) {
    std::cerr << "error: create regs file " << regs_save_name << " failed"
              << std::endl;
    exit(-1);
  }

  state_t *state = proc->get_state();

  regs_save_fout << "# spike serialization file" << std::endl;

  regs_save_fout << "pc:0x" << std::hex << state->pc << std::endl;

  for (int i = 0; i < 32; i++) {
    regs_save_fout << "reg_x" << std::dec << i << ":" << std::hex
                   << state->XPR[i] << std::endl;
  }

  if (proc->get_flen()) {
    for (int i = 0; i < 32; i++) {
      regs_save_fout << "reg_f" << std::dec << i << ":" << std::hex
                     << state->FPR[i].v[0] << std::endl;
    }

    regs_save_fout << "fflags:" << state->fflags << std::endl;
    regs_save_fout << "frm:" << state->frm << std::endl;
  }

  if (proc->extension_enabled('V')) {
    for (int r = 0; r < 32; r++) {
      const int vlen = (int)(proc->VU.get_vlen()) >> 3;
      const int elen = (int)(proc->VU.get_elen()) >> 3;
      const int num_elem = vlen / elen;

      regs_save_fout << "reg_v" << std::dec << std::setw(2) << std::setfill('0')
                     << r << ":" << std::endl;
      ;
      for (int e = num_elem - 1; e >= 0; --e) {
        uint64_t val;
        switch (elen) {
        case 8:
          val = proc->VU.elt<uint64_t>(r, e);
          regs_save_fout << std::dec << "[" << e << "]: 0x" << std::hex
                         << std::setfill('0') << std::setw(16) << val << "  ";
          break;
        case 4:
          val = proc->VU.elt<uint32_t>(r, e);
          regs_save_fout << std::dec << "[" << e << "]: 0x" << std::hex
                         << std::setfill('0') << std::setw(8) << (uint32_t)val
                         << "  ";
          break;
        case 2:
          val = proc->VU.elt<uint16_t>(r, e);
          regs_save_fout << std::dec << "[" << e << "]: 0x" << std::hex
                         << std::setfill('0') << std::setw(8) << (uint16_t)val
                         << "  ";
          break;
        case 1:
          val = proc->VU.elt<uint8_t>(r, e);
          regs_save_fout << std::dec << "[" << e << "]: 0x" << std::hex
                         << std::setfill('0') << std::setw(8)
                         << (int)(uint8_t)val << "  ";
          break;
        default:
          assert(0);
        }
        regs_save_fout << std::endl;
      }
    }

    regs_save_fout << "vstart:" << proc->VU.vstart->read() << std::endl;
    regs_save_fout << "vxsat:" << proc->VU.vxsat->read() << std::endl;
    regs_save_fout << "vxrm:" << proc->VU.vxrm->read() << std::endl;
    regs_save_fout << "vcsr:" << proc->get_state()->csrmap[CSR_VCSR]->read()
                   << std::endl;
    regs_save_fout << "vl:" << proc->VU.vl->read() << std::endl;
    regs_save_fout << "vtype:" << proc->VU.vtype->read() << std::endl;
    regs_save_fout << "vlenb:" << proc->get_state()->csrmap[CSR_VLENB]->read()
                   << std::endl;
  }

  const char *priv_str = "USHM";
  regs_save_fout << "priv:" << priv_str[state->prv] << std::endl;
  regs_save_fout << "insn_counter:" << std::dec << insn_counter << std::endl;

  regs_save_fout << "mstatus:" << std::hex << state->mstatus->read()
                 << std::endl;
  regs_save_fout << "mtvec:" << std::hex << state->csrmap[CSR_MTVEC]->read()
                 << std::endl;
  regs_save_fout << "mscratch:" << std::hex
                 << state->csrmap[CSR_MSCRATCH]->read() << std::endl;
  regs_save_fout << "mepc:" << std::hex << state->csrmap[CSR_MEPC]->read()
                 << std::endl;
  regs_save_fout << "mcause:" << std::hex << state->csrmap[CSR_MCAUSE]->read()
                 << std::endl;
  regs_save_fout << "mtval:" << std::hex << state->csrmap[CSR_MTVAL]->read()
                 << std::endl;

  regs_save_fout << "misa:" << std::hex << state->misa->read() << std::endl;
  regs_save_fout << "mie:" << std::hex << state->csrmap[CSR_MIE]->read()
                 << std::endl;
  regs_save_fout << "mip:" << std::hex << state->csrmap[CSR_MIP]->read()
                 << std::endl;
  regs_save_fout << "medeleg:" << std::hex << state->csrmap[CSR_MEDELEG]->read()
                 << std::endl;
  regs_save_fout << "mideleg:" << std::hex << state->csrmap[CSR_MIDELEG]->read()
                 << std::endl;
  regs_save_fout << "mcounteren:" << std::hex
                 << state->csrmap[CSR_MCOUNTEREN]->read() << std::endl;
  regs_save_fout << "mcountinhibit:" << std::hex
                 << state->csrmap[CSR_MCOUNTINHIBIT]->read() << std::endl;
  regs_save_fout << "tselect:" << std::hex << state->csrmap[CSR_TSELECT]->read()
                 << std::endl;

  regs_save_fout << "stvec:" << std::hex << state->csrmap[CSR_STVEC]->read()
                 << std::endl;
  regs_save_fout << "sscratch:" << std::hex
                 << state->csrmap[CSR_SSCRATCH]->read() << std::endl;
  regs_save_fout << "sepc:" << std::hex << state->csrmap[CSR_SEPC]->read()
                 << std::endl;
  regs_save_fout << "scause:" << std::hex << state->csrmap[CSR_SCAUSE]->read()
                 << std::endl;
  regs_save_fout << "stval:" << std::hex << state->csrmap[CSR_STVAL]->read()
                 << std::endl;
  regs_save_fout << "satp:" << std::hex << state->csrmap[CSR_SATP]->read()
                 << std::endl;
  regs_save_fout << "scounteren:" << std::hex
                 << state->csrmap[CSR_SCOUNTEREN]->read() << std::endl;

  for (int i = 0; i < 4; i += 2) {
    regs_save_fout << "pmpcfg" << i << ":" << std::hex
                   << state->csrmap[CSR_PMPCFG0 + i]->read() << std::endl;
  }

  for (int i = 0; i < 16; i++) {
    regs_save_fout << "pmpaddr" << std::dec << i << ":" << std::hex
                   << state->csrmap[CSR_PMPADDR0 + i]->read() << std::endl;
  }

  std::cerr << "NOTE: creating a new regs file: " << regs_save_name
            << std::endl;

  regs_save_fout.close();
}

static uint32_t create_csrrw(int rs, uint32_t csrn) {
  return 0x1073 | ((csrn & 0xFFF) << 20) | ((rs & 0x1F) << 15);
}

static uint32_t create_csrrs(int rd, uint32_t csrn) {
  return 0x2073 | ((csrn & 0xFFF) << 20) | ((rd & 0x1F) << 7);
}

static uint32_t create_auipc(int rd, uint32_t addr) {
  if (addr & 0x800)
    addr += 0x800;

  return 0x17 | ((rd & 0x1F) << 7) | ((addr >> 12) << 12);
}

/*
 * Might use it one day, but GCC doesn't like unused static functions
 * static uint32_t create_lui(int rd, uint32_t addr) {
 *     return 0x37 | ((rd & 0x1F) << 7) | ((addr >> 12) << 12);
 * }
 */

static uint32_t create_addi(int rd, uint32_t addr) {
  uint32_t pos = addr & 0xFFF;

  return 0x13 | ((rd & 0x1F) << 7) | ((rd & 0x1F) << 15) |
         ((pos & 0xFFF) << 20);
}

static uint32_t create_seti(int rd, uint32_t data) {
  return 0x13 | ((rd & 0x1F) << 7) | ((data & 0xFFF) << 20);
}

static uint32_t create_ld(int rd, int rs1) {
  return 3 | ((rd & 0x1F) << 7) | (3 << 12) | ((rs1 & 0x1F) << 15);
}

static uint32_t create_sd(int rs1, int rs2) {
  return 0x23 | ((rs2 & 0x1F) << 20) | (3 << 12) | ((rs1 & 0x1F) << 15);
}

static uint32_t create_fld(int rd, int rs1) {
  return 7 | ((rd & 0x1F) << 7) | (0x3 << 12) | ((rs1 & 0x1F) << 15);
}

static uint32_t create_vtypei(int vlmul, int vsew, int vta, int vma) {
  return (vlmul & 0x7) | (vsew & 0x7) << 3 | (vta & 0x1) << 4 |
         (vma & 0x1) << 5;
}

static uint32_t create_vsetvli(int rd, int rs1, uint32_t vtypei) {
  return 87 | ((rd & 0x1F) << 7) | (7 << 12) | ((rs1 & 0x1F) << 15) |
         ((vtypei & 0x7FF) << 20);
}

static uint32_t create_vle8(int vd, int rs1) {
  return 7 | ((vd & 0x1F) << 7) | ((rs1 & 0x1F) << 15) | (1 << 25);
}

static void create_csr12_recovery(uint32_t *rom, uint32_t *code_pos,
                                  uint32_t csrn, uint16_t val) {
  rom[(*code_pos)++] = create_seti(1, val & 0xFFF);
  rom[(*code_pos)++] = create_csrrw(1, csrn);
}

static void create_csr64_recovery(uint32_t *rom, uint32_t *code_pos,
                                  uint32_t *data_pos, uint32_t csrn,
                                  uint64_t val) {
  uint32_t data_off = sizeof(uint32_t) * (*data_pos - *code_pos);

  rom[(*code_pos)++] = create_auipc(1, data_off);
  rom[(*code_pos)++] = create_addi(1, data_off);
  rom[(*code_pos)++] = create_ld(1, 1);
  rom[(*code_pos)++] = create_csrrw(1, csrn);

  rom[(*data_pos)++] = val & 0xFFFFFFFF;
  rom[(*data_pos)++] = val >> 32;
}

static void create_reg_recovery(uint32_t *rom, uint32_t *code_pos,
                                uint32_t *data_pos, int rn, uint64_t val) {
  uint32_t data_off = sizeof(uint32_t) * (*data_pos - *code_pos);

  rom[(*code_pos)++] = create_auipc(rn, data_off);
  rom[(*code_pos)++] = create_addi(rn, data_off);
  rom[(*code_pos)++] = create_ld(rn, rn);

  rom[(*data_pos)++] = val & 0xFFFFFFFF;
  rom[(*data_pos)++] = val >> 32;
}

static void create_vreg_recovery(uint32_t *rom, uint32_t *code_pos,
                                 uint32_t *data_pos, int vn,
                                 uint32_t *vreg_data, uint32_t size) {
  uint32_t data_off = sizeof(uint32_t) * (*data_pos - *code_pos);

  rom[(*code_pos)++] = create_auipc(1, data_off);
  rom[(*code_pos)++] = create_addi(1, data_off);
  rom[(*code_pos)++] = create_vle8(vn, 1);

  for (uint32_t i = 0; i < size; i++) {
    rom[(*data_pos)++] = *vreg_data;
    vreg_data++;
  }
}

static void create_io64_recovery(uint32_t *rom, uint32_t *code_pos,
                                 uint32_t *data_pos, uint64_t addr,
                                 uint64_t val) {
  uint32_t data_off = sizeof(uint32_t) * (*data_pos - *code_pos);

  rom[(*code_pos)++] = create_auipc(1, data_off);
  rom[(*code_pos)++] = create_addi(1, data_off);
  rom[(*code_pos)++] = create_ld(1, 1);

  rom[(*data_pos)++] = addr & 0xFFFFFFFF;
  rom[(*data_pos)++] = addr >> 32;

  uint32_t data_off2 = sizeof(uint32_t) * (*data_pos - *code_pos);
  rom[(*code_pos)++] = create_auipc(2, data_off2);
  rom[(*code_pos)++] = create_addi(2, data_off2);
  rom[(*code_pos)++] = create_ld(2, 2);

  rom[(*code_pos)++] = create_sd(1, 2);

  rom[(*data_pos)++] = val & 0xFFFFFFFF;
  rom[(*data_pos)++] = val >> 32;
}

static void create_hang_nonzero_hart(uint32_t *rom, uint32_t *code_pos,
                                     uint32_t *data_pos) {
  /* Note, this matches the bootloader prologue from copy_kernel() */

  rom[(*code_pos)++] = 0xf1402573; // start:  csrr   a0, mhartid
  rom[(*code_pos)++] = 0x00050663; //         beqz   a0, 1f
  rom[(*code_pos)++] = 0x10500073; // 0:      wfi
  rom[(*code_pos)++] = 0xffdff06f; //         j      0b
  // 1:
}

void simpoint_module_t::create_boot_rom(const char *save_name) {
  std::string boot_rom_name(save_name);
  boot_rom_name.append(".bootram");

  std::ofstream boot_rom_fout(boot_rom_name);
  if (!boot_rom_fout.is_open()) {
    std::cerr << "Create bootram file " << boot_rom_name << " failed"
              << std::endl;
    exit(-1);
  }

  state_t *state = proc->get_state();

  if (state->prv != PRV_M ||
      state->pc > (SIMPOINT_BOOTROM_BASE + SIMPOINT_BOOTROM_SIZE)) {
    std::cerr << "NOTE: creating a new boot rom:  " << boot_rom_name
              << std::endl;
  } else if (SIMPOINT_BOOTROM_BASE < state->pc) {
    std::cerr << "ERROR: could not checkpoint when running inside the ROM."
              << std::endl;
    exit(-4);
  } else {
    std::cerr << "ERROR: unexpected PC address 0x" << std::hex << state->pc
              << std::endl;
    exit(-4);
  }

  uint32_t rom[SIMPOINT_BOOTROM_SIZE / 4] = {0};

  // ROM organization

  uint32_t code_pos = 0;
  uint32_t data_pos = 0xB00;
  uint32_t data_pos_start = data_pos;

  if (sim->get_harts().size() == 1) {
    create_hang_nonzero_hart(rom, &code_pos, &data_pos);
  }

  create_csr64_recovery(rom, &code_pos, &data_pos, 0x7b1,
                        state->pc); // Write to DPC (CSR, 0x7b1)

  // Write current privilege level to prv in dcsr (0 user, 1 supervisor, 2 user)
  // dcsr is at 0x7b0 prv is bits 0 & 1
  // dcsr.stopcount = 1
  // dcsr.stoptime  = 1
  // dcsr = 0x600 | (PrivLevel & 0x3)
  if (PRV_HS == state->prv) {
    std::cerr << "UNSUPPORTED Priv mode (no hyper)" << std::endl;
    exit(-4);
  }

  create_csr12_recovery(rom, &code_pos, 0x7b0, 0x600 | state->prv);

  // NOTE: mstatus & misa should be one of the first because risvemu breaks down
  // this register for performance reasons. E.g: restoring the fflags also
  // changes parts of the mstats
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x300,
                        state->mstatus->read()); // mstatus
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x301,
                        state->misa->read()); // misa

  // All the remaining CSRs
  if (state->sstatus->enabled(
          SSTATUS_FS)) { // If the FPU is down, you can not recover flags
    create_csr12_recovery(rom, &code_pos, 0x001, state->fflags->read());
    // Only if fflags, otherwise it would raise an illegal instruction
    create_csr12_recovery(rom, &code_pos, 0x002, state->frm->read());
    create_csr12_recovery(rom, &code_pos, 0x003,
                          state->fflags->read() | (state->frm->read() << 5));

    for (int i = 0; i < 32; i++) {
      uint32_t data_off = sizeof(uint32_t) * (data_pos - code_pos);
      rom[code_pos++] = create_auipc(1, data_off);
      rom[code_pos++] = create_addi(1, data_off);
      rom[code_pos++] = create_fld(i, 1);

      rom[data_pos++] = static_cast<uint32_t>(state->FPR[i].v[0] & 0xFFFFFFFF);
      rom[data_pos++] = static_cast<uint32_t>(state->FPR[i].v[0] >> 32);
    }
  }

  if (proc->extension_enabled('V') && state->sstatus->enabled(SSTATUS_VS)) {
    rom[code_pos++] = create_seti(1, -1); // li ra, -1
    rom[code_pos++] = create_vsetvli(1, 1, create_vtypei(VLMUL8, VSEW8, 0, 0));
    ; // vsetvli ra, ra, e8, m8

    uint32_t size = proc->VU.VLEN * 8 / 32;
    uint32_t *vreg_data = nullptr;

    vreg_data = &proc->VU.elt<uint32_t>(0, 0, false);
    create_vreg_recovery(rom, &code_pos, &data_pos, 0, vreg_data,
                         size); // vle8.v v0...v7
    vreg_data = &proc->VU.elt<uint32_t>(8, 0, false);
    create_vreg_recovery(rom, &code_pos, &data_pos, 8, vreg_data,
                         size); // vle8.v v8...v15
    vreg_data = &proc->VU.elt<uint32_t>(16, 0, false);
    create_vreg_recovery(rom, &code_pos, &data_pos, 16, vreg_data,
                         size); // vle8.v v16...v24
    vreg_data = &proc->VU.elt<uint32_t>(24, 0, false);
    create_vreg_recovery(rom, &code_pos, &data_pos, 24, vreg_data,
                         size); // vle8.v v25...v31

    create_csr64_recovery(rom, &code_pos, &data_pos, CSR_VSTART,
                          proc->VU.vstart->read());
    create_csr64_recovery(rom, &code_pos, &data_pos, CSR_VXSAT,
                          proc->VU.vxsat->read());
    create_csr64_recovery(rom, &code_pos, &data_pos, CSR_VXRM,
                          proc->VU.vxrm->read());
    create_csr64_recovery(rom, &code_pos, &data_pos, CSR_VCSR,
                          proc->get_state()->csrmap[CSR_VCSR]->read());

    volatile uint32_t vl = proc->VU.vl->read();
    volatile uint32_t vtype = proc->VU.vtype->read();
    rom[code_pos++] = create_seti(1, vl);          // li ra, vl
    rom[code_pos++] = create_vsetvli(1, 1, vtype); // vsetvli ra, ra, vtypei
  }

  // Recover CPU CSRs

  // Cycle and instruction are alias across modes. Just write to m-mode counter
  // Already done before CLINT. create_csr64_recovery(rom, &code_pos, &data_pos,
  // 0xb00, s->insn_counter); // mcycle create_csr64_recovery(rom, &code_pos,
  // &data_pos, 0xb02, s->insn_counter); // instret

  for (int i = 0; i < 29; i++) {
    create_csr12_recovery(rom, &code_pos, 0xb03 + i,
                          0); // reset mhpmcounter3..32
    create_csr64_recovery(rom, &code_pos, &data_pos, 0x323 + i,
                          state->csrmap[CSR_MHPMEVENT3 + i]->read());
  }

  create_csr64_recovery(rom, &code_pos, &data_pos, 0x7a0,
                        state->tselect->read());
  // FIXME: create_csr64_recovery(rom, &code_pos, &data_pos, 0x7a1, s->tdata1);
  // // tdata1
  // FIXME: create_csr64_recovery(rom, &code_pos, &data_pos, 0x7a2, s->tdata2);
  // // tdata2

  create_csr64_recovery(rom, &code_pos, &data_pos, 0x302,
                        state->medeleg->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x303,
                        state->mideleg->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x304,
                        state->mie->read()); // mie & sie
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x305, state->mtvec->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x105, state->stvec->read());
  create_csr12_recovery(rom, &code_pos, 0x320,
                        state->csrmap[CSR_MCOUNTINHIBIT]->read());
  create_csr12_recovery(rom, &code_pos, 0x306, state->mcounteren->read());
  create_csr12_recovery(rom, &code_pos, 0x106, state->scounteren->read());

#define CSR_PMPCFG(n) (0x3A0 + (n))  // n = 0 or 2
#define CSR_PMPADDR(n) (0x3B0 + (n)) // n = 0..15
  for (uint32_t i = 0; i < proc->n_pmp; ++i) {
    create_csr64_recovery(rom, &code_pos, &data_pos, CSR_PMPADDR(i),
                          state->pmpaddr[i]->read());
  }

  for (uint32_t i = 0; i < proc->n_pmp / 4; i += 2) {
    create_csr64_recovery(rom, &code_pos, &data_pos, CSR_PMPCFG(i),
                          state->csrmap[CSR_PMPCFG0 + i]->read());
  }

  create_csr64_recovery(rom, &code_pos, &data_pos, 0x340,
                        state->csrmap[CSR_MSCRATCH]->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x341, state->mepc->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x342,
                        state->mcause->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x343, state->mtval->read());

  create_csr64_recovery(rom, &code_pos, &data_pos, 0x140,
                        state->csrmap[CSR_SSCRATCH]->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x141, state->sepc->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x142,
                        state->scause->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x143, state->stval->read());

  create_csr64_recovery(rom, &code_pos, &data_pos, 0x344,
                        state->mip->read()); // mip & sip

  for (int i = 3; i < 32; i++) { // Not 1 and 2 which are used by create_...
    create_reg_recovery(rom, &code_pos, &data_pos, i, state->XPR[i]);
  }

  // Assuming 16 ratio between CPU and CLINT and that CPU is reset to zero
  void *fdt = (void *)dtb.c_str();

  reg_t clint_base;
  if (fdt_parse_clint(fdt, &clint_base, "riscv,clint0") != 0) {
    std::cerr << "Could not find the address of clint, failed to create bootrom"
              << std::endl;
    exit(-4);
  }

  create_io64_recovery(rom, &code_pos, &data_pos, clint_base + 0x4000,
                       clint->get_mtimecmp(0));
  create_csr64_recovery(rom, &code_pos, &data_pos, 0xb02,
                        state->minstret->read());
  create_csr64_recovery(rom, &code_pos, &data_pos, 0xb00,
                        state->mcycle->read());

  create_io64_recovery(rom, &code_pos, &data_pos, clint_base + 0xbff8,
                       clint->get_mtime());

  for (int i = 1; i < 3; i++) { // recover 1 and 2 now
    create_reg_recovery(rom, &code_pos, &data_pos, i, state->XPR[i]);
  }

  rom[code_pos++] = create_csrrw(1, 0x7b2);
  create_csr64_recovery(rom, &code_pos, &data_pos, 0x180, state->satp->read());
  // last Thing because it changes addresses. Use dscratch register to remember
  // reg 1
  rom[code_pos++] = create_csrrs(1, 0x7b2);

  // dret 0x7b200073
  rom[code_pos++] = 0x7b200073;

  if ((SIMPOINT_BOOTROM_SIZE / 4) <= data_pos || data_pos_start <= code_pos) {
    std::cerr << "ERROR: ROM is too small. ROM_SIZE should increase."
              << std::endl;
    std::cerr << "Current code_pos=" << code_pos << " data_pos=" << data_pos
              << std::endl;
    exit(-6);
  }

  boot_rom_fout.write(reinterpret_cast<char *>(rom), sizeof(rom));
  boot_rom_fout.close();
}

void simpoint_module_t::cpu_serialize(const char *save_name) {
  save_regs_file(save_name);
  create_boot_rom(save_name);
}

void simpoint_module_t::ram_serialize(const char *save_name) {
  // save main ram to file
  std::string mainram_save_name(save_name);
  mainram_save_name.append(".mainram");

  auto mainram_pair = bus->find_device(SIMPOINT_MAINRAM_BASE);
  auto *mainram = dynamic_cast<mem_t *>(mainram_pair.second);

  std::ofstream mainram_fout(mainram_save_name, std::ios::binary);
  if (!mainram_fout.is_open()) {
    std::cerr << "Create mainram file " << mainram_save_name << " failed"
              << std::endl;
    exit(-1);
  }

  mainram->dump(mainram_fout);
  mainram_fout.close();

  if (config.snapshot_compress) {
    std::string mainram_compress_save_name =
        mainram_save_name + std::string(".zip");
    compress_file(mainram_save_name, mainram_compress_save_name);

    std::cerr << "NOTE: creating a new main ram:  "
              << mainram_compress_save_name << std::endl;

    remove_file(mainram_save_name);
  } else {
    std::cerr << "NOTE: creating a new main ram:  " << mainram_save_name
              << std::endl;
  }
}

void simpoint_module_t::fds_serialize(const char *save_name) {
  std::string fds_save_name(save_name);
  fds_save_name.append(".fds");

  std::ofstream fds_fout(fds_save_name);

  if (!fds_fout.is_open()) {
    std::cerr << "Create fds file " << fds_save_name << " failed" << std::endl;
    exit(-1);
  }

  std::vector<int> &fds = syscall->get_fds().get_fds_vector();

  // Skip stdin, stdout, stderr
  for (size_t i = 3; i < fds.size(); i++) {
    int fd = fds[i];
    if (fd != -1) {
      std::string fd_path = "/proc/self/fd/" + std::to_string(fd);
      char file_path[PATH_MAX + 1] = {0};

      ssize_t ret = readlink(fd_path.c_str(), file_path, PATH_MAX);
      if (-1 == ret) {
        std::cerr << "error: failed to read " << fd_path << std::endl;
        exit(-1);
      }

      size_t offset = lseek(fd, 0, SEEK_CUR);
      if ((size_t)-1 == offset) {
        std::cerr << "error: failed to get offset " << fd_path << std::endl;
        exit(-1);
      }

      int flags = fcntl(fd, F_GETFD);
      if (-1 == flags) {
        std::cerr << "error: failed to get flags " << fd_path << std::endl;
        exit(-1);
      }

      struct stat file_stat = {0};
      if (fstat(fd, &file_stat) != 0) {
        std::cerr << "error: failed to get stat " << fd_path << std::endl;
        exit(-1);
      }
      mode_t st_mode = file_stat.st_mode;
      mode_t mode = ((st_mode & S_IRWXU) >> 6) * 100 +
                    ((st_mode & S_IRWXG) >> 3) * 10 + (st_mode & S_IRWXO);

      //      std::cerr << i << " " << file_path << " " << fd  << " " << flags
      //      << " " << mode << " " << offset << std::endl;
      fds_fout << i << " " << file_path << " " << flags << " " << mode << " "
               << offset << std::endl;
    }
  }

  std::cerr << "NOTE: creating a new fds file:  " << fds_save_name << std::endl;
  fds_fout.close();
}

void simpoint_module_t::simpoint_exit() {
  if (config.snapshot_save_name) {
    cpu_serialize(config.snapshot_save_name);
    ram_serialize(config.snapshot_save_name);
    fds_serialize(config.snapshot_save_name);
  }

  if (simpoints_bb_fout.is_open()) {
    std::cerr << std::endl
              << "write basic block trace to " << SIMPOINT_BB_FILE_NAME
              << std::endl;
    simpoints_bb_fout.close();
  }

  if (benchmark_exit_code > 0) {
    std::cerr << std::endl
              << "Benchmark exited with code: " << benchmark_exit_code
              << std::endl;
  }
}

uint64_t simpoint_module_t::get_mcycle() {
  return proc->get_state()->minstret->read();
}

uint64_t simpoint_module_t::get_minstret() {
  return proc->get_state()->mcycle->read();
}

bool simpoint_module_t::open_bb_fout() {
  if (not bb_trace.is_bb_file_open && simpoints.empty() && this->config.intervals > 0) {
    if (module_id != 0) {
      SIMPOINT_BB_FILE_NAME += "_";
      SIMPOINT_BB_FILE_NAME += std::to_string(module_id);
    }
    simpoints_bb_fout.open(SIMPOINT_BB_FILE_NAME);
    if (!simpoints_bb_fout.is_open()) {
      std::cerr << "error: could not open spike_simpoint.bb for dumping trace"
                << std::endl;
      exit(-1);
    }
    bb_trace.is_bb_file_open = true;
  }
  return true;
}

void simpoint_module_t::rollback(reg_t to_pc, uint64_t step_from_curr_pc) {
  // For now, rollback only support sp checkpoints.
  insn_counter -= step_from_curr_pc;
  step_last_pc = to_pc;

  // for sp checkpoints
  if (step_from_curr_pc >= sp_trace.ninst) {
    sp_trace.ninst = 0;
    sp_trace.simpoint_next = 0;
  } else {
    sp_trace.ninst -= step_from_curr_pc;
    for (size_t i = 0; i < simpoints.size(); ++i) {
      if (sp_trace.ninst <= simpoints[i].start) {
        sp_trace.simpoint_next = i;
        break;
      }
    }
  }
  has_rollback = true;
}

simpoint_csr_t::simpoint_csr_t(processor_t *const proc, const reg_t addr)
    : csr_t(proc, addr) {
  simpoint_module = proc->get_simpoint_module();
}

reg_t simpoint_csr_t::read() const noexcept { return 0; }

bool simpoint_csr_t::unlogged_write(const reg_t val) noexcept {
  uint64_t minstret = simpoint_module->get_minstret();
  uint64_t mcycle = simpoint_module->get_mcycle();

  if ((val & 3) == 3) {
    uint64_t new_maxinsns = val >> 2;
    std::cerr << "simpoint adjust maxinsns to " << new_maxinsns << std::endl;
    simpoint_module->set_maxinsns(new_maxinsns);
  } else if ((val & 3) == 2) {
    uint64_t exit_code = val >> 2;
    std::cerr << "simpoint terminate, minstret: " << minstret
              << ", mcycle: " << mcycle << std::endl;
    simpoint_module->set_benchmark_exit_code(exit_code);
    // FIXME: re-enable roi
    if (not HOOK_BOOL(isMissPredict)) {
      simpoint_module->set_terminate_simulation(true);
    }
  } else if ((val & 1) && simpoint_module->get_simpoint_roi()) {
    std::cerr << "simpoint ROI already started" << std::endl;
  } else if ((val & 1) == 0 && simpoint_module->get_simpoint_roi()) {
    std::cerr << "simpoint ROI finished, minstret: " << minstret
              << ", mcycle: " << mcycle << std::endl;
    simpoint_module->set_simpoint_roi(false);
  } else if ((val & 1) == 0 && !simpoint_module->get_simpoint_roi()) {
    std::cerr << "simpoint ROI already finished" << std::endl;
  } else {
    std::cerr << "simpoint ROI started, minstret: " << minstret
              << ",  mcycle: " << mcycle << std::endl;
    simpoint_module->set_simpoint_roi(true);
  }

  return true;
}
