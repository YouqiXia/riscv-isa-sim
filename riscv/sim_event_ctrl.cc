/**
 * @file sim_event_ctrl.cc
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-01-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "sim_event_ctrl.h"

#include <cassert>

#include "mem_event_ctrl.h"
#include "proc_event_ctrl.h"
#include "sim.h"

sim_event_ctrl_t::sim_event_ctrl_t(sim_t &sim) : sim(sim) {}

void sim_event_ctrl_t::set_tint(const tint_t &int_arg) {
  processor_t *p = sim.get_core(int_arg.core_idx);
  p->get_state()->mip->backdoor_write_with_mask(MIP_MTIP, MIP_MTIP);
}

void sim_event_ctrl_t::set_tint(const mtint_t &int_arg) {
  size_t core_id = int_arg.core_id;
  assert(sim.get_harts().count(core_id));
  processor_t *proc = sim.get_harts().at(core_id);
  proc_event_ctrl_t(*proc).set_mtint(int_arg);
}

void sim_event_ctrl_t::init_core_memunit(size_t core_id) {
  assert(sim.get_harts().count(core_id));
  processor_t *proc = sim.get_harts().at(core_id);
  mem_event_ctrl_t(*proc).init_mem_unit(sim.memunit_init_arg);
}

void sim_event_ctrl_t::mem_event_step(const mem_event_t &mem_event) {
  size_t core_id = mem_event.core_id;
  assert(sim.get_harts().count(core_id));
  processor_t *proc = sim.get_harts().at(core_id);

  mem_event_ctrl_t(*proc).handle_mem_event(mem_event);
}

void sim_event_ctrl_t::do_tick() {
  bool start = false;
  size_t step_count = 0;
  bool can_tick = true;
  for (auto &[id, hart] : sim.get_harts()) {
    if (not start) {
      start = true;
      step_count = hart->get_step_count();
    }
    if (step_count != hart->get_step_count()) {
      can_tick = false;
      break;
    }
  }

  if (can_tick) {
    sim.devices_tick(sim.INTERLEAVE);
    for (auto &[id, hart] : sim.get_harts()) {
      hart->reset_step_count();
    }
  }
}

void sim_event_ctrl_t::do_host_acts() {}