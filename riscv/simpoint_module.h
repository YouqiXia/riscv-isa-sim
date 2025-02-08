#ifndef RISCV_SIMPOINT_H
#define RISCV_SIMPOINT_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>

#include "csrs.h"
#include "devices.h"
#include "platform.h"
#include "simif.h"
#include "syscall.h"

#define SIMPOINT_BOOTROM_BASE (0x10000)
#define SIMPOINT_BOOTROM_SIZE (0x10000)

/*
 * Support 2G, 4G and 8G MEM_SIZE, defined at software/spike/riscv/platform.h
 * 2G MEM start at 0x80000000,
 * 4G MEM start at 0x200000,
 * 8G MEM start at 0x4000000000
 */
#define SIMPOINT_MAINRAM_BASE (s_platform_cfg.simpoint_mainram_base)

#define SIMPOINT_BOOT_CODE_INSNS (551)

#define SIMPOINT_BB_FILE_NAME (bb_trace.bb_file_name)

#define CSR_SIMPOINT (0x8c2)

typedef struct simpoint_module_config {
  const char *simpoint_file_name;
  uint64_t maxinsns;
  uint64_t intervals;
  const char *snapshot_load_name;
  const char *snapshot_save_name;
  bool simpoint_roi;      // enable simpoint region of interest
  addr_t simpoint_start;  // enable simpoint trace from this start pc
  bool snapshot_compress; // whether the mianram of the snapshot is compressed

  [[nodiscard]] bool is_simpoint_enabled() const {
    return ((maxinsns > 0) || (intervals > 0) || (simpoint_roi) ||
            (snapshot_load_name != nullptr) ||
            (snapshot_save_name != nullptr) || (simpoint_file_name != nullptr));
  }
} simpoint_module_config_t;

typedef struct simpoint {
  simpoint(uint64_t i, uint64_t j) : start(i), id(j) {}
  bool operator<(const simpoint &j) const { return (start < j.start); }

  uint64_t start;
  uint64_t id;
} simpoint_t;

struct simpoint_trace_t {
  uint64_t ninst = 0;
  uint64_t simpoint_next = 0;

  bool keeprun_after_done = false;
};

struct bb_trace_t {
  uint64_t last_pc = 0;
  uint64_t ninst = 0;
  uint64_t next_bbv_dump;
  std::unordered_map<uint64_t, uint64_t> bbv;
  std::unordered_map<uint64_t, uint64_t> pc2id;
  int next_id = 1;

  bool is_init = false;

  std::string bb_file_name = "spike_simpoint.bb";
  bool is_bb_file_open = false;
};

class simpoint_module_t {
public:
  explicit simpoint_module_t(const simpoint_module_config_t &config,
                             simif_t *sim, bus_t *bus, clint_t *clint,
                             syscall_t *syscall, std::string dtb);
  ~simpoint_module_t();

  void simpoint_step(size_t step);
  void simpoint_exit();

  void set_simpoint_roi(bool simpoint_roi) {
    config.simpoint_roi = simpoint_roi;
  }
  bool get_simpoint_roi() const { return config.simpoint_roi; }

  void set_maxinsns(uint64_t value) { config.maxinsns = value; }

  void set_benchmark_exit_code(uint64_t value) { benchmark_exit_code = value; }
  void set_terminate_simulation(bool value) { terminate_simulation = value; }

  uint64_t get_mcycle();
  uint64_t get_minstret();

  void set_module_id(int id) { module_id = id; }
  bool open_bb_fout();

  void rollback(reg_t to_pc, uint64_t step_from_curr_pc);

private:
  uint64_t insn_counter; // simpoint internal
  simpoint_module_config_t config;
  simif_t *sim;
  bus_t *bus;
  clint_t *clint;
  syscall_t *syscall;
  processor_t *proc;
  std::string dtb;
  std::unique_ptr<rom_device_t> bootrom;

  std::vector<simpoint> simpoints;
  std::ofstream simpoints_bb_fout;

  uint64_t benchmark_exit_code;
  bool terminate_simulation;

  simpoint_trace_t sp_trace;
  bb_trace_t bb_trace;
  reg_t step_last_pc = UINT64_MAX;

  int module_id = 0;
  bool has_rollback = false;

  [[nodiscard]] bool is_maxinsns_reached() const;

  void cpu_serialize(const char *save_name);
  void ram_serialize(const char *save_name);
  void fds_serialize(const char *save_name);
  void cpu_deserialize(const char *load_name);
  void ram_deserialize(const char *load_name);
  void fds_deserialize(const char *load_name);

  void read_simpoints();
  void save_regs_file(const char *save_name);
  void create_boot_rom(const char *save_name);
  void create_bb_trace(reg_t pc);
  void create_sp_checkpoints();
};

class simpoint_csr_t : public csr_t {
public:
  simpoint_csr_t(processor_t *proc, reg_t addr);
  [[nodiscard]] reg_t read() const noexcept override;

protected:
  bool unlogged_write(reg_t val) noexcept override;

private:
  simpoint_module_t *simpoint_module;
};

#endif // RISCV_SIMPOINT_H
