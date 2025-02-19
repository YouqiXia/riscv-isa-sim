/**
 * @file mem_event_ctrl.cc
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-01-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "mem_event_ctrl.h"

#include <cassert>

#include "ext_log.h"
#include "mmu.h"
#include "processor.h"

void mem_event_ctrl_t::init_mem_unit(const memunit_init_arg_t &arg) {
  proc.int_grnt = false;
  proc.stb = std::make_unique<core_stb_t>(arg.stb_num, arg.stb_data_byte);
  assert(proc.stb);
  proc.rob = std::make_unique<core_rob_t>(arg.rob_num);
  assert(proc.rob);
  proc.set_deep_ctrl(true);
}

void mem_event_ctrl_t::handle_mem_event(const mem_event_t &mem_event) {
  proc.mem_event_arg = mem_event;
  proc.set_debug(true);

  if (mem_event.state.is_commit) {
    handle_commit(mem_event);
  } else if (mem_event.state.is_stmem) {
    handle_stmem(mem_event);
  } else if (mem_event.state.is_prepare) {
    handle_prepare(mem_event);
  } else {
    MCC_LOG("[MCC_API] %s, NO event! time: %lu, core%u, EXIT...\n", __func__,
            mem_event.timestamp, mem_event.core_id);
    exit(0);
  }
}

void mem_event_ctrl_t::handle_prepare(const mem_event_t &mem_event) {
  if (mem_event.insn.is_st) {
    prepare_st(mem_event);
  } else if (mem_event.insn.is_ld) {
    prepare_ld(mem_event);
  } else if (mem_event.insn.is_amo) {
    prepare_amo(mem_event);
  } else if (mem_event.insn.is_lr) {
    prepare_lr(mem_event);
  } else if (mem_event.insn.is_sc) {
    prepare_sc(mem_event);
  } else if (mem_event.insn.is_csr) {
    prepare_csr(mem_event);
  }
}

void mem_event_ctrl_t::handle_commit(const mem_event_t &mem_event) {
  proc.step(1);
}

void mem_event_ctrl_t::handle_stmem(const mem_event_t &mem_event) {
  if (mem_event.insn.is_amo) {
    stmem_amo(mem_event);
  } else if (mem_event.insn.is_sc) {
    stmem_sc(mem_event);
  } else if (mem_event.insn.is_st) {
    stmem_st(mem_event);
  }
}

void mem_event_ctrl_t::on_commit_store(const std::vector<uint64_t> &paddrs,
                                    char *buf, size_t len) {
  // handle st, amo, sc
  assert(len <= sizeof(uint64_t));
  const auto &mem_event = proc.mem_event_arg;
  auto &rob = proc.rob;
  rob_entry_t *rob_entry = &rob->rob_array[mem_event.rob_idx];
  assert(rob_entry->valid);

  uint64_t data = *(uint64_t *)buf;
  auto paddr = paddrs.at(0);

  auto check_st_info = [&] {
    return (rob_entry->paddr != paddr) or (rob_entry->len != len) or
           (rob_entry->st_data != data);
  };

  if (mem_event.insn.is_st) {
    if (not check_st_info()) {
      assert(0);
    }
  } else if (mem_event.insn.is_amo or mem_event.insn.is_sc) {
    if (not mem_event.insn.is_io) {
      if (rob_entry->has_stmem) {
        if (not check_st_info()) {
          assert(0);
        }
      } else {
        rob_entry->paddr = paddr;
        rob_entry->st_data = data;
        rob_entry->len = len;
      }
    } else {
      rob_entry->has_stmem = true;
    }
  }

  rob_entry->has_commit = true;
  if (rob_entry->has_stmem or mem_event.insn.is_st/*because of data saved in stb*/) {
    free_rob_entry(rob_entry);
  }
}

void mem_event_ctrl_t::on_commit_load(const std::vector<uint64_t> &paddrs,
                                    char *buf, size_t len) {
  // handle ld, lr, amo
  assert(len <= sizeof(uint64_t));
  const auto &mem_event = proc.mem_event_arg;
  auto &rob = proc.rob;
  auto paddr = paddrs.at(0);
  rob_entry_t *rob_entry = &rob->rob_array[mem_event.rob_idx];
  if (not rob_entry->valid or (rob_entry->paddr != paddr) or
      (rob_entry->len != len)) {
    MCC_LOG("[MCC commit_%s] inconsistency! time: %lu, core%d_rob%u, pc: "
            "0x%lx, %s, paddr: 0x%lx-0x%lx, len: %u-%lu\n",
            get_mcc_insn_str(rob_entry->insn), mem_event.timestamp,
            mem_event.core_id, mem_event.rob_idx, proc.state.pc,
            rob_entry->valid ? "valid" : "invalid", rob_entry->paddr, paddr,
            rob_entry->len, len);
    assert(0);
  }

  *(uint64_t *)buf = rob_entry->ld_data;
  rob_entry->has_commit = true;
  if (not mem_event.insn.is_amo and not mem_event.insn.is_csr) { // todo. move csr logic
    free_rob_entry(rob_entry);
  }
}

bool mem_event_ctrl_t::on_commit_check_lr(uint64_t paddr, size_t len) {
  // handle sc
  auto mmu = proc.mmu;
  mmu->mem_event_log.reset();
  mmu->mem_event_log.paddrs.push_back(paddr);

  uint64_t data = 0;
  on_commit_load(mmu->mem_event_log.paddrs, (char *)&data, len);

  const auto &mem_event = proc.mem_event_arg;
  auto &rob = proc.rob;
  rob_entry_t *rob_entry = &rob->rob_array[mem_event.rob_idx];

  free_rob_entry(rob_entry);

  return data != 0;
}

void mem_event_ctrl_t::prepare_st(const mem_event_t &mem_event) {
  auto &stb = proc.stb;
  rob_entry_t *rob_entry = alloc_rob_entry(mem_event);

  if (mem_event.insn.is_io) {
    // only write into rob in mmio
    MCC_LOG("[MCC prepare_iost] time: %lu, core%d_rob%u, paddr: 0x%lx-0x%lx, "
            "len: %lu, st_data: 0x%lx\n",
            mem_event.timestamp, mem_event.core_id, mem_event.rob_idx,
            rob_entry->paddr, rob_entry->paddr >> 6, rob_entry->len,
            rob_entry->st_data);
    return;
  }

  stb->store(rob_entry->paddr, rob_entry->len, (uint8_t *)&rob_entry->st_data,
             mem_event.union_arg.stb_idx);
}

void mem_event_ctrl_t::prepare_ld(const mem_event_t &mem_event) {
  auto &rob = proc.rob;
  auto &stb = proc.stb;

  rob_entry_t *rob_entry = alloc_rob_entry(mem_event);
  if (mem_event.insn.is_io) {
    return;
  }

  uint64_t pre_data = 0;
  load_from_mem((uint8_t *)&pre_data, rob_entry->paddr, rob_entry->len);
  bool stb_hit = stb->load(rob_entry->paddr, rob_entry->len, (uint8_t *)&pre_data);
  if (rob_entry->ld_data != pre_data) {
    assert(0);
  }
}

void mem_event_ctrl_t::prepare_amo(const mem_event_t &mem_event) {
  auto &rob = proc.rob;
  rob_entry_t *rob_entry = alloc_rob_entry(mem_event);

  if (mem_event.insn.is_io) {
    return;
  }

  uint64_t pre_data = 0;
  load_from_mem((uint8_t *)&pre_data, rob_entry->paddr, rob_entry->len);
  if (rob_entry->ld_data != pre_data) { // compare
    assert(0);
  }
}

void mem_event_ctrl_t::prepare_lr(const mem_event_t &mem_event) {
  assert(not mem_event.insn.is_io);
  prepare_amo(mem_event);
}

void mem_event_ctrl_t::prepare_sc(const mem_event_t &mem_event) {
  auto &rob = proc.rob;
  rob_entry_t *rob_entry = alloc_rob_entry(mem_event);
}

void mem_event_ctrl_t::prepare_csr(const mem_event_t &mem_event) {
  prepare_sc(mem_event);
}

void mem_event_ctrl_t::stmem_st(const mem_event_t &mem_event) {
  if (mem_event.insn.is_io) {
    return;
  }
  auto &stb = proc.stb;

  uint64_t stb_idx_bitmap = mem_event.union_arg.stb_idx_bitmap;
  for (uint32_t i = 0; i < stb->get_stb_num(); i++) {
    if (stb_idx_bitmap & (uint64_t(1) << i)) {
      stb_entry_t *stb_entry = &stb->stb_array[i];
      if (stb_entry->wmask == uint64_t(0)) {
        MCC_LOG("[MCC stmem_st] time: %lu, core%d_stb%u is not valid\n",
                mem_event.timestamp, mem_event.core_id, i);
        assert(0);
      }
      auto paddr = stb_entry->paddr & ~(proc.stb->stb_data_byte - 1);
      store_into_mem(stb_entry->data.data(), paddr, stb_entry->size(), stb_entry->wmask);

      stb_entry->wmask = 0;
      proc.stb->stb_busy_bitmap &= ~(uint64_t(1) << i); // dealloc stb idx
    }
  }
}

void mem_event_ctrl_t::stmem_amo(const mem_event_t &mem_event) {
  rob_entry_t *rob_entry = &proc.rob->rob_array[mem_event.rob_idx];
  assert(rob_entry->valid);
  if (mem_event.insn.is_io) {
    rob_entry->has_stmem = true;
    if (rob_entry->has_commit) {
      free_rob_entry(rob_entry);
    }
    return;
  }

  if (rob_entry->has_commit) {
    if (rob_entry->paddr != mem_event.paddr or
        rob_entry->st_data != mem_event.data or
        rob_entry->len != mem_event.len) {
      assert(0);
    }
  } else {
    rob_entry->paddr = mem_event.paddr;
    rob_entry->st_data = mem_event.data;
    rob_entry->len = mem_event.len;
  }

  MCC_LOG("[MCC stmem_amo] time: %lu, core%d_rob%u, paddr: 0x%lx-0x%lx, len: "
          "%u, data: 0x%lx\n",
          mem_event.timestamp, proc.get_id(), mem_event.rob_idx,
          rob_entry->paddr, rob_entry->paddr >> 6, rob_entry->len,
          rob_entry->st_data);

  store_into_mem((uint8_t *)&rob_entry->st_data, rob_entry->paddr, rob_entry->len);
  rob_entry->has_stmem = true;
  if (rob_entry->has_commit) {
    free_rob_entry(rob_entry);
  }
}

void mem_event_ctrl_t::stmem_sc(const mem_event_t &mem_event) {
  rob_entry_t *rob_entry = &proc.rob->rob_array[mem_event.rob_idx];
  assert(not rob_entry->has_commit and not mem_event.insn.is_io);
  rob_entry->paddr = mem_event.paddr;
  rob_entry->st_data = mem_event.data;
  rob_entry->len = mem_event.len;

  MCC_LOG("[MCC stmem_sc] time: %lu, core%d_rob%u, commit: %d, paddr: "
          "0x%lx-0x%lx, len: %u, data: 0x%lx\n",
          mem_event.timestamp, proc.get_id(), mem_event.rob_idx, 0,
          rob_entry->paddr, rob_entry->paddr >> 6, rob_entry->len,
          rob_entry->st_data);

  store_into_mem((uint8_t *)&rob_entry->st_data, rob_entry->paddr, rob_entry->len);
}

rob_entry_t *mem_event_ctrl_t::alloc_rob_entry(const mem_event_t &mem_event) {
  assert(mem_event.state.is_prepare);
  auto &rob = proc.rob;

  rob_entry_t *rob_entry = &rob->rob_array[mem_event.rob_idx];
  assert(rob_entry->valid == false);
  rob_entry->valid = true;
  rob_entry->insn = mem_event.insn;
  rob_entry->paddr = mem_event.paddr;
  rob_entry->len = mem_event.len;
  rob_entry->has_commit = false;
  rob_entry->has_stmem = false;

  if (mem_event.insn.is_st) { // st
    rob_entry->st_data = mem_event.data;
  } else { // ld, amo, lr, sc, csr
    rob_entry->ld_data = mem_event.data;
  }
  return rob_entry;
}

void mem_event_ctrl_t::free_rob_entry(rob_entry_t *rob_entry) {
  rob_entry->valid = false;
}

size_t mem_event_ctrl_t::intra_page(uint64_t addr, size_t len) const {
  return std::min(len, PGSIZE - addr % PGSIZE);
}

void mem_event_ctrl_t::load_intra_page(uint8_t *buf, uint64_t paddr, size_t len,
                                       uint64_t mask) {
  auto sim = proc.sim;
  auto mmu = proc.mmu;

  uint8_t *host_addr = (uint8_t *)sim->addr_to_mem(paddr);
  if (host_addr) {
    for (uint32_t i = 0; i < len; i++) {
      if (mask & (uint64_t(1) << i)) {
        buf[i] = host_addr[i];
      }
    }
  } else {
    mmu->mmio_load(paddr, len, buf);
  }
}

void mem_event_ctrl_t::store_intra_page(const uint8_t *buf, uint64_t paddr,
                                        size_t len, uint64_t mask) {
  auto sim = proc.sim;
  auto mmu = proc.mmu;

  uint8_t *host_addr = (uint8_t *)sim->addr_to_mem(paddr);
  if (host_addr) {
    for (uint32_t i = 0; i < len; i++) {
      if (mask & (uint64_t(1) << i)) {
        host_addr[i] = buf[i];
      }
    }
  } else {
    mmu->mmio_store(paddr, len, buf);
  }
}

void mem_event_ctrl_t::load_from_mem(uint8_t *buf, uint64_t paddr, size_t len,
                                     uint64_t mask) {
  auto len_page0 = intra_page(paddr, len);
  load_intra_page(buf, paddr, len_page0, mask);
  if (len_page0 != len) {
    load_intra_page(buf + len_page0, paddr + len_page0, len - len_page0,
                    mask << len_page0);
  }
}

void mem_event_ctrl_t::store_into_mem(const uint8_t *buf, uint64_t paddr,
                                      size_t len, uint64_t mask) {
  auto len_page0 = intra_page(paddr, len);
  store_intra_page(buf, paddr, len_page0, mask);
  if (len_page0 != len) {
    store_intra_page(buf + len_page0, paddr + len_page0, len - len_page0,
                     mask << len_page0);
  }
}