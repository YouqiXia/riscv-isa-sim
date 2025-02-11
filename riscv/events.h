/**
 * @file events.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-02-05
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef EXT_EVENTS_H_
#define EXT_EVENTS_H_

#include "mem_events.h"

#include <cstdint>
#include <cstdio>
#include <string>

struct tint_arg_t : public eventbase_t {
  uint32_t core_id;
  uint64_t timestamp; // debug only
  bool is_timer;
  bool set_ip;
  bool clr_ip;

  std::string serialization() const override {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "tint %lu %lu %lu %lu %lu\n", core_id, timestamp,
             is_timer, set_ip, clr_ip);
    return cmd;
  }
};

struct gbl_t : public eventbase_t {
  std::string serialization() const override {
    char cmd[8];
    snprintf(cmd, sizeof(cmd), "mcc_gbl\n");
    return cmd;
  }
};

#endif // EXT_EVENTS_H_