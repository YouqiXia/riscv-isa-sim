#include "sim.h"

#include <iostream>

#include "spikeAdpterHooks.hpp"

#include "easy_args.h"
#include "remote_bitbang.h"

void sim_t::set_current_proc(size_t proc) {
  if (g_easy_args.specify_proc) {
    current_proc = proc;
  }
}

size_t sim_t::hartid_to_idx(size_t hartid) const {
  // return hartid_to_idx_map.at(hartid);
  return hartid;
}

void sim_t::init_multicore_data() {
  if (multi_proc_data.proc_current_steps.empty()) {
    multi_proc_data.proc_current_steps.resize(procs.size(), 0);
    multi_proc_data.proc_errs.resize(procs.size(), NO_ERR);
  }
}

bool sim_t::is_multicore_mode() const {
  return not multi_proc_data.proc_current_steps.empty();
}

size_t sim_t::idle_ext(size_t n, size_t cid) {
  if (done())
    return 0;

  init_multicore_data();

  current_step = multi_proc_data.proc_current_steps[hartid_to_idx(cid)];

  n = std::min(n, INTERLEAVE - current_step);

  if (debug || ctrlc_pressed)
    interactive();
  else
    step_ext(n, cid);

  if (not_in_step()) {
    if (remote_bitbang)
      remote_bitbang->tick();
  }
  
  return n;
}

bool sim_t::not_in_step() const {
  return is_multicore_mode() ? multi_proc_data.steps_sum == 0 : current_step == 0;
}

size_t sim_t::step_ext(size_t n, size_t cid) {
  size_t total_steps_done = 0;
  while (n > 0) {
    auto steps_done = step_proc(n, cid);
    if (steps_done == 0) {
      break;
    }
    n -= steps_done;
    total_steps_done += steps_done;
  }

  if (multi_proc_data.steps_sum == procs.size() * INTERLEAVE) {
    devices_tick(INTERLEAVE);
    prepare_next_ticks();
  }
  return total_steps_done;
}

size_t sim_t::step_proc(size_t n, size_t cid) {
  size_t total_steps = 0;
  auto core_idx = hartid_to_idx(cid);

  multi_proc_data.proc_errs[core_idx] = NO_ERR;
  current_step = multi_proc_data.proc_current_steps[core_idx];
  if (current_step >= INTERLEAVE) {
    multi_proc_data.proc_errs[core_idx] = WAIT_TICK;
    return 0;
  }

  for (size_t i = 0, steps = 0; i < n; i += steps) {
    steps = std::min(n - i, INTERLEAVE - current_step);
    procs[core_idx]->step(steps);

    current_step += steps;

    multi_proc_data.proc_current_steps[core_idx] = current_step;
    multi_proc_data.steps_sum += steps;
    total_steps += steps;
    break;
  }
  return total_steps;
}

void sim_t::prepare_next_ticks() {
  for (auto &step_num : multi_proc_data.proc_current_steps) {
    step_num = 0;
  }
  multi_proc_data.steps_sum = 0;
  current_step = 0;
}

void sim_t::devices_tick(size_t num) {
  reg_t rtc_ticks = (num + REMAINDER) / INSNS_PER_RTC_TICK;
  REMAINDER = (num + REMAINDER) % INSNS_PER_RTC_TICK;
  if (not get_log_commits_enabled() or continueHook())
    devices_rtc_tick(rtc_ticks);
}

void sim_t::devices_rtc_tick(size_t rtc_ticks) {
  for (auto &dev : devices)
    dev->tick(rtc_ticks);
}
