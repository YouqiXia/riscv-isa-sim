#include "sim.h"

#include "spikeAdpterHooks.hpp"

void sim_t::step_proc(size_t n, size_t cid) {
  multi_proc_data.proc_errs[cid] = NO_ERR;
  current_step = multi_proc_data.proc_current_steps[cid];
  if (current_step >= INTERLEAVE) {
    multi_proc_data.proc_errs[cid] = WAIT_TICK;
    return;
  }

  for (size_t i = 0, steps = 0; i < n; i += steps)
  {
    steps = std::min(n - i, INTERLEAVE - current_step);
    procs[cid]->step(steps);

    current_step += steps;

    multi_proc_data.proc_current_steps[cid] = current_step;
    multi_proc_data.steps_sum += steps;

    if (multi_proc_data.steps_sum == procs.size() * INTERLEAVE) {
      for (auto &step_num : multi_proc_data.proc_current_steps) {
        step_num = 0;
      }
      multi_proc_data.steps_sum = 0;
      reg_t rtc_ticks = INTERLEAVE / INSNS_PER_RTC_TICK;
      if (not procs[cid]->get_log_commits_enabled() or continueHook()) {
        for (auto &dev : devices) dev->tick(rtc_ticks);
      }

      current_step = 0;
      continue;
    }
    break;
  }
}