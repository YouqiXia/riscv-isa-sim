#ifndef _RISCV_CTRL_ARG_H
#define _RISCV_CTRL_ARG_H

struct ctrl_arg {
  ctrl_arg() {
    enable_watch_addr = false;
    watch_addr = uint64_t(0);
    dump_rd = false;
    step_cnt = 5000;
    log_proc_id_mask = uint64_t(0);
    log_proc_end_mask = uint64_t(0);
    log_pc_start = uint64_t(0);
    log_pc_end = uint64_t(0);
  }
  bool enable_watch_addr;
  uint64_t watch_addr;
  bool dump_rd;
  uint64_t step_cnt;

  uint64_t log_proc_id_mask;
  uint64_t log_proc_end_mask;
  uint64_t log_pc_start;
  uint64_t log_pc_end;
};

extern ctrl_arg spike_ctrl_arg;
#endif
