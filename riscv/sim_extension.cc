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

void sim_t::create_dummy_proc() {
  for (auto proc : procs) {
    push_dummy_proc(isa_string.c_str(), proc->get_id(), is_halted);
  }
  dummy_proc_enabled = true;
}

void sim_t::push_dummy_proc(const char *isa_str, size_t hartid, bool halted) {
  dummy_procs.push_back(std::make_unique<processor_t>(
      isa_str, cfg->priv, cfg, this, hartid, halted, log_file.get(), sout_));
}

processor_t *sim_t::get_dummy_proc(size_t cid) {
  return dummy_procs[hartid_to_idx(cid)].get();
}

void sim_t::dummy_proc_exec(size_t n, size_t cid) {
  dummy_procs[hartid_to_idx(cid)]->dummy_step(n);
}

void sim_t::copy_to_dummy(size_t cid) {
  auto real_proc = procs[hartid_to_idx(cid)];
  auto dummy_proc = dummy_procs[hartid_to_idx(cid)].get();

  assert(real_proc and dummy_proc);

  // Clone real processor to dummy
  dummy_proc->get_state()->XPR = real_proc->get_state()->XPR;
  dummy_proc->get_state()->FPR = real_proc->get_state()->FPR;
  dummy_proc->get_state()->prv = real_proc->get_state()->prv;
  dummy_proc->get_state()->prev_prv = real_proc->get_state()->prev_prv;
  dummy_proc->get_state()->prv_changed = real_proc->get_state()->prv_changed;
  dummy_proc->get_state()->v_changed = real_proc->get_state()->v_changed;
  dummy_proc->get_state()->v = real_proc->get_state()->v;
  dummy_proc->get_state()->prev_v = real_proc->get_state()->prev_v;
  dummy_proc->get_state()->debug_mode = real_proc->get_state()->debug_mode;
  dummy_proc->get_state()->serialized = real_proc->get_state()->serialized;
  dummy_proc->get_state()->log_reg_write =
      real_proc->get_state()->log_reg_write;
  dummy_proc->get_state()->log_mem_read = real_proc->get_state()->log_mem_read;
  dummy_proc->get_state()->log_mem_write =
      real_proc->get_state()->log_mem_write;
  dummy_proc->get_state()->last_inst_priv =
      real_proc->get_state()->last_inst_priv;
  dummy_proc->get_state()->last_inst_xlen =
      real_proc->get_state()->last_inst_xlen;
  dummy_proc->get_state()->last_inst_flen =
      real_proc->get_state()->last_inst_flen;

  if (real_proc->extension_enabled('V')) {
    dummy_proc->VU.setvl_count = real_proc->VU.setvl_count;
    dummy_proc->VU.vlmax = real_proc->VU.vlmax;
    dummy_proc->VU.vlenb = real_proc->VU.vlenb;

    dummy_proc->VU.vstart->write_raw(real_proc->VU.vstart->read());
    dummy_proc->VU.vxrm->write_raw(real_proc->VU.vxrm->read());
    dummy_proc->VU.vl->write_raw(real_proc->VU.vl->read());
    dummy_proc->VU.vtype->write_raw(real_proc->VU.vtype->read());

    dummy_proc->VU.vma = real_proc->VU.vma;
    dummy_proc->VU.vta = real_proc->VU.vta;
    dummy_proc->VU.vsew = real_proc->VU.vsew;
    dummy_proc->VU.vflmul = real_proc->VU.vflmul;
    dummy_proc->VU.ELEN = real_proc->VU.ELEN;
    dummy_proc->VU.VLEN = real_proc->VU.VLEN;
    dummy_proc->VU.vill = real_proc->VU.vill;
    dummy_proc->VU.vstart_alu = real_proc->VU.vstart_alu;

    for (int r = 0; r < 32; r++) {
      const int vlen = (int)(dummy_proc->VU.get_vlen()) >> 3;
      const int elen = (int)(dummy_proc->VU.get_elen()) >> 3;
      const int num_elem = vlen / elen;

      for (int e = num_elem - 1; e >= 0; --e) {
        switch (elen) {
        case 8:
          dummy_proc->VU.elt<uint64_t>(r, e) =
              real_proc->VU.elt<uint64_t>(r, e);
          break;
        case 4:
          dummy_proc->VU.elt<uint32_t>(r, e) =
              real_proc->VU.elt<uint32_t>(r, e);
          break;
        case 2:
          dummy_proc->VU.elt<uint16_t>(r, e) =
              real_proc->VU.elt<uint16_t>(r, e);
          break;
        case 1:
          dummy_proc->VU.elt<uint8_t>(r, e) = real_proc->VU.elt<uint8_t>(r, e);
          break;
        default:
          assert(0);
        }
      }
    }
  }

  dummy_proc->get_state()->log_reg_write.clear();
  dummy_proc->get_state()->log_mem_read.clear();
  dummy_proc->get_state()->log_mem_write.clear();
}
