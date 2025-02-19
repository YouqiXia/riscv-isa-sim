/**
 * @file sim_event_ctrl.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-01-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef EXT_SIM_EVENT_CTRL_H
#define EXT_SIM_EVENT_CTRL_H

#include "events.h"

class sim_t;

class sim_event_ctrl_t {
public:
  sim_event_ctrl_t(sim_t &sim);
  void set_tint(const tint_t &int_arg);
  void set_tint(const mtint_t &int_arg);
  void init_core_memunit(size_t core_id);
  void mem_event_step(const mem_event_t &mem_event);
  void do_tick();
  void do_host_acts();

private:
  sim_t &sim;
};

#endif // EXT_SIM_EVENT_CTRL_H