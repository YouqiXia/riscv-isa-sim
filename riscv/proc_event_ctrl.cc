/**
 * @file proc_event_ctrl.cc
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-02-05
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "proc_event_ctrl.h"

#include "processor.h"

const char *get_mcc_csr_str(reg_t paddr, bool &is_mcc_csr) {
  is_mcc_csr = true;

  switch (paddr) {
  case CSR_TIME:
    return "time";
  case CSR_CYCLE:
    return "cycle";
  case CSR_MCYCLE:
    return "mcycle";
  case CSR_INSTRET:
    return "instret";
  case CSR_MINSTRET:
    return "minstret";
  }

  is_mcc_csr = false;
  return "unkown";
}

proc_event_ctrl_t::proc_event_ctrl_t(processor_t &proc) : proc(proc) {}

void proc_event_ctrl_t::set_mtint(const mtint_t &int_arg) {
  reg_t mask, val;

  proc.int_grnt = int_arg.int_grnt;

  if (!proc.int_grnt) {
    mask = int_arg.is_timer ? MIP_MTIP : MIP_MSIP;
    val = int_arg.clr_ip ? reg_t(0) : mask;
    proc.get_state()->mip->backdoor_write_with_mask(mask, val);
  }
}

bool proc_event_ctrl_t::on_commit_csr(int which, uint64_t &csr_data) {
  if (not proc.get_deep_ctrl()) {
    return false;
  }
  uint64_t paddr = which;
  bool is_mcc_csr;
  const char *commit_csr_str = get_mcc_csr_str(paddr, is_mcc_csr);
  if (not is_mcc_csr) {
    return false;
  }

  const auto &event_arg = proc.mem_event_arg;
  rob_entry_t *rob_entry = &proc.rob->rob_array[event_arg.rob_idx];
  const char *rob_csr_str = get_mcc_csr_str(rob_entry->paddr, is_mcc_csr);

  if (!rob_entry->insn.is_csr || !rob_entry->valid ||
      (rob_entry->paddr != paddr)) {
    fprintf(stdout, "%s %d\n", __func__, __LINE__);
    MCC_LOG(
        "[MCC commit_csr] inconsistency! insn: %s, time: %lu, core%d_rob%u, "
        "pc: 0x%lx, %s, paddr: 0x%lx-0x%lx_%s-%s, ld_data: 0x%lx\n",
        get_mcc_insn_str(rob_entry->insn), event_arg.timestamp,
        event_arg.core_id, event_arg.rob_idx, proc.state.pc,
        rob_entry->valid ? "valid" : "invalid", rob_entry->paddr, paddr,
        rob_csr_str, commit_csr_str, rob_entry->ld_data);
    assert(0);
  }

  MCC_LOG("[MCC commit_csr] time: %lu, core%d_rob%u, pc: 0x%lx, paddr: "
          "0x%lx_%s, ld_data: 0x%lx\n",
          event_arg.timestamp, event_arg.core_id, event_arg.rob_idx,
          proc.state.pc, rob_entry->paddr, rob_csr_str, rob_entry->ld_data);

  rob_entry->valid = false;
  csr_data = rob_entry->ld_data;
  return true;
}