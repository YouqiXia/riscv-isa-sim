/**
 * @file proc_event_ctrl.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-02-05
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef EXT_PROC_EVENT_CTRL_H
#define EXT_PROC_EVENT_CTRL_H

#include "events.h"

class processor_t;

class proc_event_ctrl_t {
public:
  proc_event_ctrl_t(processor_t &proc);
  void set_mtint(const mtint_t &int_arg);
  bool on_commit_csr(int which, uint64_t &csr_data);

private:
  processor_t &proc;
};

#endif // EXT_PROC_EVENT_CTRL_H