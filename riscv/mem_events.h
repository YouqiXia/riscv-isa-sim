/**
 * @file mem_events.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-02-05
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef _MEM_EVENTS_H_
#define _MEM_EVENTS_H_

#include "eventbase.h"
#include <cstdint>
#include <cstdio>
#include <string>

struct memunit_initcore : public eventbase_t {
  uint64_t core_id;

  std::string serialization() const override {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "mcc_initcore %lu\n", core_id);
    return cmd;
  }
};

struct memunit_init_arg_t : public eventbase_t {
  uint32_t rob_num;
  uint32_t stb_num;
  uint32_t stb_data_byte;

  std::string serialization() const override {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "mcc_init %lu %lu %lu\n", rob_num, stb_num,
             stb_data_byte);
    return cmd;
  }
};

struct insn_flag_t {
  uint64_t is_st : 1,
           is_ld : 1,
           is_amo : 1,
           is_lr : 1,
           is_sc : 1,
           is_csr : 1,
           is_io : 1,
           : 0;
  
  insn_flag_t()
    : is_st(0),
      is_ld(0),
      is_amo(0),
      is_lr(0),
      is_sc(0),
      is_csr(0),
      is_io(0) {}
};

struct state_flag_t {
  uint64_t is_prepare : 1,
           is_commit : 1,
           is_stmem : 1,
           : 0;

  state_flag_t()
    : is_prepare(0),
      is_commit(0),
      is_stmem(0) {}
};

struct mem_event_t : public eventbase_t {
  enum insn_type_t {
    NONE = 0,
    STORE,
    LOAD,
    AMO,
    LR,
    SC,
    CSR_READ,
  };

  enum event_state_t {
    INVALID = 0,
    PREPARE,
    COMMIT,
    STMEM,
  };

  mem_event_t(insn_type_t itype, event_state_t estate, bool is_io) {
    set_insn_flag(0);
    insn.is_io = is_io;
    switch (itype) {
    case insn_type_t::SC:
      insn.is_sc = 1;
    case insn_type_t::STORE:
      insn.is_st = 1;
      break;
    case insn_type_t::LR:
      insn.is_lr = 1;
    case insn_type_t::LOAD:
      insn.is_ld = 1;
      break;
    case insn_type_t::AMO:
      insn.is_st = 1;
      insn.is_ld = 1;
      insn.is_amo = 1;
      break;
    case insn_type_t::CSR_READ:
      insn.is_ld = 1;
      insn.is_csr = 1;
    case insn_type_t::NONE:
    default:
      set_insn_flag(0);
      break;
    }

    set_state(0);
    switch (estate) {
    case event_state_t::PREPARE:
      state.is_prepare = 1;
      break;
    case event_state_t::COMMIT:
      state.is_commit = 1;
      break;
    case event_state_t::STMEM:
      state.is_stmem = 1;
      break;
    }
  }

  void set_state(uint64_t val) { *(uint64_t *)&state = val; }
  void set_insn_flag(uint64_t val) { *(uint64_t *)&insn = val; }
  bool is_mem_insn() { return (*(uint64_t *)&insn != uint64_t(0)); }
  uint64_t get_state() { return *(uint64_t *)&state; }
  uint64_t get_insn_flag() { return *(uint64_t *)&insn; }

  std::string serialization() const override {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mcc_event %lu %lu %lx %lx %lx %lx %lu %lu %lx\n",
             core_id, timestamp, *(uint64_t *)&state, *(uint64_t *)&insn, paddr,
             data, len, rob_idx, union_arg.stb_idx);
    return cmd;
  }

  uint32_t core_id;
  uint64_t timestamp; // debug only
  state_flag_t state;
  insn_flag_t insn;
  uint64_t paddr; // lsu: phy addr; csr: csr addr
  uint64_t data;  // lsu: data; csr: csr data
  uint32_t len;
  uint32_t rob_idx;
  union {
    uint64_t stb_idx; // prepare-st: stb_idx
    uint64_t stb_idx_bitmap; // stmem-st: stb_idx_bitmap
  } union_arg;
};

#endif // _MEM_EVENTS_H_