/**
 * @file mem_units.cc
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-01-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "mem_units.h"

#include <cassert>

bool core_stb_t::load(uint64_t paddr, uint64_t len, uint8_t *data) {
  bool stb_hit = false; // include partial hit

  if (stb_busy_bitmap == uint64_t(0))
    return false;

  auto paddr_align = paddr & ~(stb_data_byte - 1);
  auto paddr_base = paddr & (stb_data_byte - 1);
  auto paddr_end = paddr_base + len;
  for (uint32_t i = 0; i < get_stb_num(); i++) {
    auto *stb_entry = &stb_array[i];
    auto stb_paddr_align = stb_entry->paddr & ~(stb_data_byte - 1);
    if (stb_entry->wmask && (paddr_align == stb_paddr_align)) {
      for (auto j = paddr_base; j < paddr_end; j++) {
        if (stb_entry->wmask & (uint64_t(1) << j)) {
          data[j - paddr_base] = stb_entry->data[j];
          stb_hit = true;
        }
      }
    }
  }

  return stb_hit;
}

void core_stb_t::store(uint64_t paddr, uint64_t len, const uint8_t *data,
                       size_t stb_idx) {
  assert(stb_busy_bitmap & (uint64_t(1) << stb_idx) == 0);
  stb_entry_t *stb_entry = &stb_array[stb_idx];
  uint32_t shift = paddr & (stb_data_byte - 1);
  for (uint64_t i = 0; i < len; i++) {
    stb_entry->data[shift + i] = data[i];
  }
  stb_entry->paddr = paddr;
  stb_entry->wmask |= ((uint64_t(1) << len) - 1) << shift;
  stb_busy_bitmap |= (uint64_t(1) << stb_idx);

  MCC_LOG(
      "[core_stb_t::store] stb%lu_0x%lx, paddr: 0x%lx-0x%lx, len: %lu, wmask: "
      "0x%lx, data: 0x",
      stb_idx, stb_busy_bitmap, paddr, paddr >> 6, len, stb_entry->wmask);
  mcc_log_data(data, len);
}