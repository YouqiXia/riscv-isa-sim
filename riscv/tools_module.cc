/**
 * @file tools_module.cc
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "tools_module.h"

#include <iostream>

#include "sim.h"
#include "devices_extension.h"

void roi_match_t::check(char c) {
  if (roi_beg and roi_end) {
    roi_beg = false;
    roi_end = false;
  }
  if (not roi_beg) {
    roi_beg = roi_beg_checker.check(c);
  } else if (not roi_end) {
    roi_end = roi_end_checker.check(c);
  }
}

tools_module_t::tools_module_t(sim_t *_sim) : sim(_sim) {
  for (auto &dev : sim->devices) {
    auto dev_ptr = dev.get();
    if (auto ptr = dynamic_cast<uart_z1_t*>(dev_ptr)) {
      ptr->set_roi_match(&roi_match);
    }
    if (auto ptr = dynamic_cast<ns16550_t*>(dev_ptr)) {
      ptr->set_roi_match(&roi_match);
    }
  }
}