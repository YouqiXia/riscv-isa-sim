// See LICENSE for license details.

#ifndef _RISCV_SIM_H
#define _RISCV_SIM_H

#include "cfg.h"
#include "debug_module.h"
#include "devices.h"
#include "log_file.h"
#include "processor.h"
#include "simif.h"
//// RiVAI: simpoint add --YC
#include "../patch/simpoint_module.h"
//// RiVAI: simpoint add end --YC

#include <fesvr/htif.h>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <sys/types.h>

// code extension beg
#include "tools_module.h"
// code extension end

// code extension beg
enum proc_err_t : int8_t {
  NO_ERR = 0,
  WAIT_TICK = 1,
};
// code extension end

class mmu_t;
class remote_bitbang_t;
class socketif_t;

// Type for holding a pair of device factory and device specialization arguments.
using device_factory_sargs_t = std::pair<const device_factory_t*, std::vector<std::string>>;

// this class encapsulates the processors and memory in a RISC-V machine.
class sim_t : public htif_t, public simif_t
{
  friend class SimExtension;
  friend class tools_module_t;
public:
  sim_t(const cfg_t *cfg, bool halted,
        std::vector<std::pair<reg_t, abstract_mem_t*>> mems,
        const std::vector<device_factory_sargs_t>& plugin_device_factories,
        const std::vector<std::string>& args,
        const debug_module_config_t &dm_config, const char *log_path,
        bool dtb_enabled, const char *dtb_file,
        bool socket_enabled,
        FILE *cmd_file); // needed for command line option --cmd
  ~sim_t();

  // run the simulation to completion
  int run();
  void set_debug(bool value);
  void set_histogram(bool value);
  void add_device(reg_t addr, std::shared_ptr<abstract_device_t> dev);

  // Configure logging
  //
  // If enable_log is true, an instruction trace will be generated. If
  // enable_commitlog is true, so will the commit results
  void configure_log(bool enable_log, bool enable_commitlog);
  void configure_log(bool enable_log, bool enable_commitlog, bool enable_commitlog_stant);

  void set_procs_debug(bool value);
  void set_remote_bitbang(remote_bitbang_t* remote_bitbang) {
    this->remote_bitbang = remote_bitbang;
  }
  const char* get_dts() { return dts.c_str(); }
  processor_t* get_core(size_t i) { return procs.at(i); }
  abstract_interrupt_controller_t* get_intctrl() const { assert(plic.get()); return plic.get(); }
  virtual const cfg_t &get_cfg() const override { return *cfg; }

  virtual const std::map<size_t, processor_t*>& get_harts() const override { return harts; }

  // Callback for processors to let the simulation know they were reset.
  virtual void proc_reset(unsigned id) override;

  size_t INTERLEAVE = 1; // rivai: Inorder to pass model rob replay, INTERLEAVE=1 is needed, but then it can not run 602.gcc_s.elf. todo: find out the reason.
  static const size_t INSNS_PER_RTC_TICK = 100; // 10 MHz clock for 1 BIPS core
  static const size_t CPU_HZ = 1000000000; // 1GHz CPU
  //// RiVAI: simpoint add --YC
  // FIXME: Putting function definition in sim.cc file will cause linking error
  void set_simpoint_module(const simpoint_module_config_t &sm_config) {
    uint64_t tohost_addr = 0;
    uint64_t fromhost_addr = 0;
    if (sm_config.snapshot_load_name) {
      // If the emulator loads a checkpoint, try to extract the symbols needed
      // for htif from elf, if provided
      try {
        std::vector<std::string> targets = get_targets();
        std::string elf_name = targets.at(1);
        addr_t entry = 0;
        std::map<std::string, uint64_t> symbols =
            load_payload(elf_name, &entry, 0);
        if (not sm_config.disable_host and symbols.count("tohost") && symbols.count("fromhost")) {
          tohost_addr = symbols["tohost"];
          fromhost_addr = symbols["fromhost"];
          set_tohost_addr(tohost_addr);
          set_fromhost_addr(fromhost_addr);
        } else {
          std::cerr << "warning: tohost and fromhost symbols not in ELF; can't "
                       "communicate with target\n"
                    << std::endl;
        }
        // Delete the target in targets, such as pk, because it has been
        // replaced by "None" to avoid repeated loading when restoring
        // checkpoints
        targets.erase(targets.begin() + 1);
        set_targets(targets);
      } catch (const std::out_of_range &e) {
        std::cerr << "warning: ELF is not provided, the tohost and formhost "
                     "symbols cannot be obtained, and the htif communication "
                     "with the target cannot be used"
                  << std::endl;
      }
    }
    simpoint_module = std::make_unique<simpoint_module_t>(
        sm_config, this, &bus, clint.get(), get_syscall(), dtb);
    processor_t *proc = get_core(0);
    proc->set_simpoint_module(simpoint_module.get());
  };
  simpoint_module_t *get_simpoint_module() { return simpoint_module.get(); }
  //// RiVAI: simpoint add end --YC
void interactive_reg(const std::string& cmd, const std::vector<std::string>& args);
virtual void idle() override;
private:
  const cfg_t * const cfg;
  std::vector<std::pair<reg_t, abstract_mem_t*>> mems;
  std::vector<processor_t*> procs;
  std::map<size_t, processor_t*> harts;
  std::pair<reg_t, reg_t> initrd_range;
  std::string dts;
  std::string dtb;
  bool dtb_enabled;
  std::vector<std::shared_ptr<abstract_device_t>> devices;
  std::shared_ptr<clint_t> clint;
  std::shared_ptr<plic_t> plic;
  bus_t bus;
  log_file_t log_file;

  FILE *cmd_file; // pointer to debug command input file

  socketif_t *socketif;
  std::ostream sout_; // used for socket and terminal interface

  // merge from p600v2 --ZQ
  void exit_handler();
  // merge from p600v2 end --ZQ
  processor_t* get_core(const std::string& i);
  void step(size_t n); // step through simulation
  size_t current_step;
  size_t current_proc;
  bool debug;
  bool histogram_enabled; // provide a histogram of PCs
  bool log;
  remote_bitbang_t* remote_bitbang;
  std::optional<std::function<void()>> next_interactive_action;

  //// RiVAI: simpoint add --YC
  std::unique_ptr<simpoint_module_t> simpoint_module;
  //// RiVAI: simpoint add end --YC
  // memory-mapped I/O routines
  virtual char* addr_to_mem(reg_t paddr) override;
  virtual bool mmio_load(reg_t paddr, size_t len, uint8_t* bytes) override;
  virtual bool mmio_store(reg_t paddr, size_t len, const uint8_t* bytes) override;
  void set_rom();

  virtual const char* get_symbol(uint64_t paddr) override;

  // presents a prompt for introspection into the simulation
  void interactive();

  // functions that help implement interactive()
  void interactive_help(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_quit(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_run(const std::string& cmd, const std::vector<std::string>& args, bool noisy);
  void interactive_run_noisy(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_run_silent(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_vreg(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_freg(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_fregh(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_fregs(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_fregd(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_pc(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_insn(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_priv(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_mem(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_str(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_dumpmems(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_mtime(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_mtimecmp(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_until(const std::string& cmd, const std::vector<std::string>& args, bool noisy);
  void interactive_until_silent(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_until_noisy(const std::string& cmd, const std::vector<std::string>& args);
  reg_t get_reg(const std::vector<std::string>& args);
  freg_t get_freg(const std::vector<std::string>& args, int size);
  reg_t get_mem(const std::vector<std::string>& args);
  reg_t get_pc(const std::vector<std::string>& args);
  reg_t get_insn(const std::vector<std::string>& args);

  friend class processor_t;
  friend class mmu_t;

  // htif
  virtual void reset() override;
  virtual void read_chunk(addr_t taddr, size_t len, void* dst) override;
  virtual void write_chunk(addr_t taddr, size_t len, const void* src) override;
  virtual size_t chunk_align() override { return 8; }
  virtual size_t chunk_max_size() override { return 8; }
  virtual endianness_t get_target_endianness() const override;

public:
  // Initialize this after procs, because in debug_module_t::reset() we
  // enumerate processors, which segfaults if procs hasn't been initialized
  // yet.
  debug_module_t debug_module;

  // code extension beg
public:
  void interactive_extension(const std::string& cmd, const std::vector<std::string>& args);

  void set_current_proc(size_t proc);
  struct {
    std::vector<size_t> proc_current_steps;
    std::vector<proc_err_t> proc_errs;
    size_t steps_sum = 0;
  } multi_proc_data;

  void enable_specify_proc(bool val);
  proc_err_t proc_err(size_t id) const;
  size_t nprocs() const { return procs.size(); }
  char* sd_addr2Mem(reg_t paddr) { return addr_to_mem(paddr); }

  const tools_module_t *get_tools_module() const { return tools_module.get(); }
  state_t *get_state(size_t proc = 0) const { return procs[proc]->get_state(); }

  void set_log_commits(bool val) {
    for (processor_t *proc : procs) {
      proc->set_log_commits(val);
    }
  }

  size_t idle_ext(size_t n, size_t cid);
  bool not_in_step() const;

  void set_interleave(size_t val) { INTERLEAVE = val; }

  void step_proc(size_t n, size_t cid);

  bool is_multicore_mode() const {
    return not multi_proc_data.proc_current_steps.empty();
  }

private:
  std::unique_ptr<tools_module_t> tools_module;
  // code extension end
};

extern volatile bool ctrlc_pressed;

#endif
