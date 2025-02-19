/**
 * @file mem_event_ctrl.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-01-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef EXT_MEM_EVENT_CTRL_H
#define EXT_MEM_EVENT_CTRL_H

#include "mem_events.h"
#include "mem_units.h"

class processor_t;

class mem_event_ctrl_t {
public:
  mem_event_ctrl_t(processor_t &proc) : proc(proc) {}

  void init_mem_unit(const memunit_init_arg_t &arg);

  void handle_mem_event(const mem_event_t &mem_event);
  void handle_prepare(const mem_event_t &mem_event);
  void handle_commit(const mem_event_t &mem_event);
  void handle_stmem(const mem_event_t &mem_event);

  void on_commit_store(const std::vector<uint64_t> &paddrs, char *buf, size_t len);
  void on_commit_load(const std::vector<uint64_t> &paddrs, char *buf, size_t len);
  bool on_commit_check_lr(uint64_t paddr, size_t len);

  void prepare_st(const mem_event_t &mem_event);
  void prepare_ld(const mem_event_t &mem_event);
  void prepare_amo(const mem_event_t &mem_event);
  void prepare_lr(const mem_event_t &mem_event);
  void prepare_sc(const mem_event_t &mem_event);
  void prepare_csr(const mem_event_t &mem_event);

  void stmem_st(const mem_event_t &mem_event);
  void stmem_amo(const mem_event_t &mem_event);
  void stmem_sc(const mem_event_t &mem_event);

private:
  rob_entry_t *alloc_rob_entry(const mem_event_t &mem_event);
  void free_rob_entry(rob_entry_t *rob_entry);

  size_t intra_page(uint64_t addr, size_t len) const;

  void load_intra_page(uint8_t *buf, uint64_t paddr, size_t len,
                       uint64_t mask = uint64_t(-1));

  void store_intra_page(const uint8_t *buf, uint64_t paddr, size_t len,
                        uint64_t mask = uint64_t(-1));

  void load_from_mem(uint8_t *buf, uint64_t paddr, size_t len,
                     uint64_t mask = uint64_t(-1));
  void store_into_mem(const uint8_t *buf, uint64_t paddr, size_t len,
                      uint64_t mask = uint64_t(-1));

private:
  processor_t &proc;
};

#endif // EXT_MEM_EVENT_CTRL_H