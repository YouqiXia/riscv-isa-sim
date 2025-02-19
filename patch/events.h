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

struct tint_t : public eventbase_t {
  uint32_t core_idx{0};

  std::string serialization() const override {
    char cmd[32] = {0};
    snprintf(cmd, sizeof(cmd), "tint %u\n", core_idx);
    return cmd;
  }
};

struct mtint_t : public eventbase_t {
  uint32_t core_id{0};
  bool is_timer{true};
  bool clr_ip{false};
  bool int_grnt{false};
  uint64_t timestamp{0}; // debug only

  std::string serialization() const override {
    char cmd[256] = {0};
    snprintf(cmd, sizeof(cmd), "mtint %u %u %u %u %lu\n", core_id, is_timer, clr_ip, int_grnt, timestamp);
    return cmd;
  }
};

struct gbl_t : public eventbase_t {
  std::string serialization() const override {
    char cmd[16] = {0};
    snprintf(cmd, sizeof(cmd), "mcc_gbl\n");
    return cmd;
  }
};

struct mtime_access_t : public eventbase_t {
  uint64_t mtime = 0;
  bool is_set = false;

  std::string serialization() const override {
    char cmd[64] = {0};
    if (is_set) {
      snprintf(cmd, sizeof(cmd), "mtime %016lx\n", mtime);
    } else {
      snprintf(cmd, sizeof(cmd), "mtime\n");
    }
    return cmd;
  }
};

struct csr_access_t : public eventbase_t {
  enum type_t : uint32_t {
    MCYCLE,
  };

  uint32_t core_idx = 0;
  uint64_t val = 0;
  type_t type;
  bool is_set = false;

  std::string serialization() const override {
    char cmd[64] = {0};
    if (is_set) {
      snprintf(cmd, sizeof(cmd), "csr %u %u %016lx\n", core_idx, type, val);
    } else {
      snprintf(cmd, sizeof(cmd), "csr %u %u\n", core_idx, type);
    }
    return cmd;
  }
};

#endif // EXT_EVENTS_H_