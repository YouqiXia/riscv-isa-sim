/**
 * @file spikehooker.h
 * @author rick (binglin.qiu@rivai.ai)
 * @brief The base class which define serveal interface function to hook spike action, the actual behaviors should be defined in sub-class. 
 * @version 0.1
 * @date 2024-12-16
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef _SPIKE_HOOKER_H_
#define _SPIKE_HOOKER_H_

#include <memory>
#include <iostream>

class simif_t;
using SimID = simif_t*;
class SpikeHooker {
public:
  virtual bool backupOn() const = 0;
  virtual char *getHostAddr(uint64_t vaddr, char *spike_host_addr, SimID sim) = 0;
  virtual void load(uint64_t addr, char *spike_host_addr, uint64_t len, uint8_t *bytes, SimID sim) = 0;
  virtual void store(uint64_t addr, char *spike_host_addr, uint64_t len, const uint8_t *bytes, SimID sim) = 0;
  virtual bool isMissPredict() const = 0;
  virtual bool exitHook(int code) = 0;
  virtual void decodeHook(void* in, uint64_t pc, uint64_t npc) = 0;
  virtual bool commitHook() = 0;
  virtual uint64_t getNpcHook(uint64_t spike_npc) = 0;
  virtual uint64_t excptionHook(void *in, uint64_t pc, trap_t &t) = 0;
  virtual void catchDataBeforeWriteHook(uint64_t addr, uint64_t data, uint32_t len, std::shared_ptr<bool> real_store) = 0;
  virtual void catchDataBeforeCsrHook(int which, uint64_t val, std::shared_ptr<bool> real_store) = 0;

  void setCid(int cid) { cid_ = cid; }
  void setDummyStored(bool val) { dummy_stored_ = val; }
  bool getDummyStored() const { return dummy_stored_; }
  void setDebug(bool val) { debug_ = val; }
  bool getDebug() const { return debug_; }
  void setSim(SimID sim1, SimID sim2) {
    sim_ = sim1 ? sim1 : sim_;
    sim_fast_ = sim2 ? sim2 : sim_fast_;
  }

protected:
  int cid_ = 0;
  bool dummy_stored_ = false;
  bool debug_ = false;
  SimID sim_ = nullptr;
  SimID sim_fast_ = nullptr;
};

inline std::shared_ptr<SpikeHooker> g_spike_hooker = nullptr;

#define HOOK_BOOL(boolfunc) (g_spike_hooker and g_spike_hooker->boolfunc())
#define BACKUP_ON (g_spike_hooker and g_spike_hooker->backupOn())
#define BACKUP_BOOL(boolfunc) (BACKUP_ON and g_spike_hooker->boolfunc())
#define BACKUP_EXEC(func) if (BACKUP_ON) g_spike_hooker->func()
#define BACKUP_EXEC_ARG(func, arg) if (BACKUP_ON) g_spike_hooker->func(arg)

#endif