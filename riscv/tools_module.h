/**
 * @file tools_module.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef TOOLS_MODULE_H
#define TOOLS_MODULE_H

#include "ext_utils.h"

/**
 * @brief The object to determine if in roi now.
 */
class roi_match_t {
public:
  bool in_roi() const {
    return roi_beg and not roi_end;
  }
  void check(char c);

private:
  bool roi_beg = false;
  bool roi_end = false;
  stream_kmp_t roi_beg_checker{"CURR_MINSTRET"};
  stream_kmp_t roi_end_checker{"CURR_MCYCLE"};
};

class sim_t;
class tools_module_t {
public:
  tools_module_t(sim_t *_sim);
  const roi_match_t &get_roi_match() const { return roi_match; }

private:
  sim_t *sim = nullptr;
  roi_match_t roi_match;
};

#endif