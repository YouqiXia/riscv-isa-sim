/**
 * @file mem_units.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-01-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef EXT_MEM_UNITS_H
#define EXT_MEM_UNITS_H

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "ext_log.h"
#include "mem_events.h"

#define BITS_PER_BYTE 8

inline void mcc_log_data(const uint8_t *data, int len) {
  for (int i = len - 1; 0 <= i; i--) {
    MCC_LOG("%02x", data[i]);
    if (i) {
      if (0 == (i & 3))
        MCC_LOG("_");
    } else {
      MCC_LOG("\n");
    }
  }
}

inline const char *get_mcc_non_io_insn_str(const insn_flag_t &insn) {
  if (insn.is_ld)
    return "ld";
  if (insn.is_st)
    return "st";
  if (insn.is_amo)
    return "amo";
  if (insn.is_lr)
    return "lr";
  if (insn.is_sc)
    return "sc";
  if (insn.is_csr)
    return "csr";
  return "unknown";
}

inline char *get_mcc_insn_str(const insn_flag_t &insn) {
  static char buf[32];
  snprintf(buf, sizeof(buf), "%s%s", insn.is_io ? "io" : "",
           get_mcc_non_io_insn_str(insn));
  return buf;
}

struct rob_entry_t {
  rob_entry_t() : valid(false), has_commit(false), has_stmem(false) {}
  bool valid;
  insn_flag_t insn;
  uint64_t paddr;
  uint64_t ld_data;
  uint64_t st_data;
  uint32_t len;
  bool has_commit;
  bool has_stmem;
};

struct core_rob_t {
  core_rob_t(uint32_t rob_num) : rob_array(rob_num) {}
  std::vector<rob_entry_t> rob_array;
};

struct stb_entry_t {
  static constexpr size_t MAX_DATA_BYTE = 64;

  uint64_t paddr;
  uint64_t wmask;
  std::vector<uint8_t> data;

  stb_entry_t(uint32_t data_byte) : wmask(0), data(data_byte, 0) {
    static_assert(sizeof(wmask) * BITS_PER_BYTE == MAX_DATA_BYTE);
    if (data_byte > MAX_DATA_BYTE) {
      throw std::runtime_error("data_byte > MAX_DATA_BYTE");
    }
    if (data_byte % 2 != 0) {
      throw std::runtime_error("data_byte % 2 != 0");
    }
  }

  size_t size() const { return data.size(); }
};

struct core_stb_t {
  static constexpr size_t MAX_STB_NUM = 64;

  uint64_t stb_busy_bitmap;
  const uint64_t stb_data_byte;
  std::vector<stb_entry_t> stb_array;

  core_stb_t(uint32_t stb_num, uint32_t stb_data_byte)
      : stb_busy_bitmap(uint64_t(0)), stb_data_byte(stb_data_byte),
        stb_array(stb_num, stb_entry_t(stb_data_byte)) {
    static_assert(sizeof(stb_busy_bitmap) * BITS_PER_BYTE == MAX_STB_NUM);
    if (stb_num > MAX_STB_NUM) {
      throw std::runtime_error("stb_num > MAX_STB_NUM");
    }
  }
  size_t get_stb_num() const { return stb_array.size(); }
  size_t total_size() const { return stb_array.size() * stb_data_byte; }

  bool load(uint64_t paddr, uint64_t len, uint8_t *data);
  void store(uint64_t paddr, uint64_t len, const uint8_t *data, size_t stb_idx);
};

#endif // EXT_MEM_UNITS_H