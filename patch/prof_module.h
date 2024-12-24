// See LICENSE for license details.
#ifndef _RISCV_PROF_MODULE_H
#define _RISCV_PROF_MODULE_H

// #include "instruction_data.h"
#include "gprof_elf_symbol_loader.h"
#include "gprof_struct_define.h"
#include <cstdint>
#include <map>
#include <vector>

class sim_t;

// struct to record profiling procedure
struct tos_struct {
  size_t selfpc;
  size_t count;
};
typedef std::vector<tos_struct> tos_type;
struct prof_info {
  bool init = false;
  size_t lowpc;
  size_t highpc;
  size_t textsize;
  std::vector<uint64_t> *kcount = nullptr;
  std::vector<tos_type> *froms = nullptr;
};

// struct to form gmon file
// TODO: only core0 cared now.
// TODO2: prof_rate need a macro
struct gmon_hdr {
  char cookie[4];
  uint32_t version;
  char spare[12];
} __attribute__((packed));

struct hist_hdr {
  uint64_t lowpc;
  uint64_t highpc;
  uint32_t hist_size;
  uint32_t prof_rate;
  char dimen[15];
  char dimen_abbrev;
} __attribute__((packed));

struct hist_hdr32 {
  uint32_t lowpc;
  uint32_t highpc;
  uint32_t hist_size;
  uint32_t prof_rate;
  char dimen[15];
  char dimen_abbrev;
} __attribute__((packed));

struct arc_hdr {
  char tag;         /* Set to GMON_TAG_CG_ARC.  */
  uint64_t from_pc; /* Address within caller's body.  */
  uint64_t self_pc; /* Address within callee's body.  */
  uint32_t count;   /* Number of arc traversals.  */
} __attribute__((packed));

struct arc_hdr32 {
  char tag;         /* Set to GMON_TAG_CG_ARC.  */
  uint32_t from_pc; /* Address within caller's body.  */
  uint32_t self_pc; /* Address within callee's body.  */
  uint32_t count;   /* Number of arc traversals.  */
} __attribute__((packed));

typedef enum {
  GMON_TAG_TIME_HIST = 0,
  GMON_TAG_CG_ARC = 1,
} GMON_Record_Tag;

// Fred: this module is used for profiling.
//  interfaced to remote_bitbang
class prof_module_t {
public:
  prof_module_t(sim_t *sim);
  ~prof_module_t();
  void prof_enable();
  void prof_disable();
  void prof_clear();
  void prof_write_file();
  bool prof_is_enable() { return en; }
  void profiling_pc(uint64_t pc);

private:
  sim_t *sim = nullptr;
  bool en = false;
  prof_info pinfo;
  std::vector<elf_text_info> text_info;
  bool elfIs32 = 0;
  // void prof_enable(bool isEn) {prof_en = isEn;}
  // bool prof_get_status() {return prof_en;}
  std::map<uint64_t, std::string> program_address;
  void prof_env_setup(size_t lowpc, size_t highpc) {
    pinfo.lowpc = lowpc;
    pinfo.highpc = highpc;
    pinfo.textsize = highpc - lowpc;
    pinfo.kcount = new std::vector<uint64_t>; // remember to delete later
    pinfo.kcount->resize(pinfo.textsize >> 1);
    pinfo.froms = new std::vector<tos_type>; // remember to delete later
    pinfo.froms->resize(pinfo.textsize >> 1);
  }
  void prof_clean() {
    if (pinfo.kcount && pinfo.kcount->size()) {
      pinfo.kcount->clear();
    }
    if (pinfo.froms && pinfo.froms->size()) {
      pinfo.froms->clear();
    }
  }
};

#endif
