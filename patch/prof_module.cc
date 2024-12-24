// See LICENSE for license details.
#include <cassert>
#include <fstream>
// #include <iostream>
#include "prof_module.h"
#include "riscv/sim.h"

// Fred note: why pinfo do not init on constructor
// Because the elf reading will start at sim_p->run() in spike.cc.
// when constructor run, the elf has not read, so elf info is empty.
// For convenience we init pinfo when first time run profiling().
prof_module_t::prof_module_t(sim_t *sim_i) : sim(sim_i) {
  //    printf("prof_module_t::prof_module_t\n");
  std::map<std::string, uint64_t> symbols_prof;
  reg_t entry;

  symbols_prof = load_prof_symbol(sim->get_targets()[0].c_str(), &(entry));
  // for (auto i : symbols_prof)
  // {
  //   fprintf(stderr, "symbols_prof.second: %lx ; first:  %s\n", i.second,
  //   i.first.c_str());
  // }
  text_info = get_text_info_from_elf(sim->get_targets()[0].c_str(), &elfIs32);
  // fprintf(stderr, "get elfIs32 = %d\n", elfIs32);
  // for (int ii = 0; ii < text_info.size(); ii ++)
  // {
  //   fprintf(stderr, "text_info[i].vma = %lx; size = %ld;\n",
  //   text_info[ii].vma, text_info[ii].size);
  // }
  for (std::map<std::string, uint64_t>::iterator it = symbols_prof.begin();
       it != symbols_prof.end(); ++it) {
    program_address[it->second] = it->first;
  }
}

prof_module_t::~prof_module_t() { printf("prof_module_t::~prof_module_t\n"); }

void prof_module_t::prof_enable() {
  // fprintf(stderr, "prof_module_t::prof_enable cycle: %ld\n",
  // sim->get_core(0)->get_cycle());
  //    fprintf(stderr, "Prof: enablen");
  en = true;
}

void prof_module_t::prof_disable() {
  //    fprintf(stderr, "Prof: disable\n");
  en = false;
}

// TODO: do we need to move all profiling function to this module?
void prof_module_t::prof_clear() {
  fprintf(stderr, "Prof: clear\n");
  if (pinfo.kcount) {
    delete (pinfo.kcount);
  }
  if (pinfo.froms) {
    delete (pinfo.froms);
  }
  // force reinit
  pinfo.init = false;
}

// TODO: do we need to move all profiling function to this module?
void prof_module_t::prof_write_file() {
  fprintf(stderr, "prof_module_t::prof_write_file cycle: %ld\n",
          sim->get_core(0)->get_cycle());

  if (prof_is_enable() == false) {
    fprintf(stderr, "Prof: file write not enable.\n");
    return;
  }

  if (pinfo.init == false) {
    fprintf(stderr, "Prof: module not init.\n");
    return;
  }
  fprintf(stderr, "Prof: file writing...\n");

  std::ofstream gprof_out;
  // gprof_out.open("gmon1.out", std::ios::trunc);
  gprof_out.open("gmon.out", std::ios::binary | std::ios::trunc);

  gmon_hdr ghdr = {{'g', 'm', 'o', 'n'}, 1, {0}};
  gprof_out.write((char *)&ghdr, sizeof(ghdr)); // 20

  hist_hdr hhdr;
  hhdr.lowpc = pinfo.lowpc;
  hhdr.highpc = pinfo.highpc;
  hhdr.hist_size = pinfo.textsize >> 1;
  // hhdr.prof_rate = 1000000;
  strncpy(hhdr.dimen, "seconds", sizeof(hhdr.dimen));
  hhdr.dimen_abbrev = 's';

  // instruction sampling scale
  uint64_t kcount_max = 0;
  for (auto i : *(pinfo.kcount)) {
    if (i > kcount_max) {
      kcount_max = i;
    }
  }

  uint64_t scale_coef = 1;
  while (kcount_max > UINT16_MAX) {
    scale_coef *= 10;
    kcount_max /= scale_coef;
  }
  hhdr.prof_rate = 1000000 / scale_coef;
  // in case prof_rate more than 1s
  if (hhdr.prof_rate == 0)
    hhdr.prof_rate = 1;

  // instruction sampling ( histogram )
  char tag = GMON_TAG_TIME_HIST;
  gprof_out.write(&tag, sizeof(tag)); // 21

  if (elfIs32 == true) {
    // fprintf(stderr, "Prof: now is 32 bit elf.\n");
    hist_hdr32 hhdr32;
    // hhdr32.lowpc = (uint32_t)((hhdr.lowpc << 32) >> 32);
    // hhdr32.highpc = (uint32_t)((hhdr.highpc << 32) >> 32);
    hhdr32.lowpc = (uint32_t)(hhdr.lowpc);
    hhdr32.highpc = (uint32_t)(hhdr.highpc);
    hhdr32.hist_size = hhdr.hist_size;
    hhdr32.prof_rate = hhdr.prof_rate;
    strncpy(hhdr32.dimen, "seconds", sizeof(hhdr32.dimen));
    hhdr32.dimen_abbrev = 's';
    // fprintf(stderr, "32dr: lowpc: %lx, highpc: %lx, hist_size: %d, prof_rate:
    // %d, dimen: %s, dimen_abbrev: %s\n", hhdr32.lowpc, hhdr32.highpc,
    // hhdr32.hist_size, hhdr32.prof_rate, hhdr32.dimen, &hhdr32.dimen_abbrev);
    gprof_out.write((char *)&hhdr32, sizeof(hhdr32));
  } else {
    // fprintf(stderr, "Prof: now is 64 bit elf.\n");
    // fprintf(stderr, "Prof: hhdr: lowpc: %lx, highpc: %lx, hist_size: %d,
    // prof_rate: %d, dimen: %s, dimen_abbrev: %s\n", hhdr.lowpc, hhdr.highpc,
    // hhdr.hist_size, hhdr.prof_rate, hhdr.dimen, &hhdr.dimen_abbrev);
    gprof_out.write((char *)&hhdr, sizeof(hhdr));
  }

  for (auto i : *(pinfo.kcount)) {
    uint16_t kcount_value = i / scale_coef;
    // fprintf(stderr, "Prof: kcount_value: %d\n", kcount_value);
    gprof_out.write((char *)&kcount_value, sizeof(uint16_t));
  }

  // call graph
  arc_hdr ahdr;
  ahdr.tag = GMON_TAG_CG_ARC;

  arc_hdr32 ahdr32;
  ahdr32.tag = ahdr.tag;
  for (auto to = pinfo.froms->begin(); to != pinfo.froms->end(); to++) {
    if (to->size() == 0) {
      continue;
    }
    // fprintf(stderr, "in for 1\n");
    uint32_t frompc = (distance(pinfo.froms->begin(), to) << 1) + pinfo.lowpc;

    for (auto i = to->begin(); i != to->end(); ++i) {
      if (elfIs32 == true) {
        ahdr32.from_pc = (uint32_t)(frompc);
        ahdr32.self_pc = (uint32_t)(i->selfpc);
        ahdr32.count = i->count;
        gprof_out.write((char *)&ahdr32, sizeof(ahdr32));
        // fprintf(stderr, "Prof: 32dr, from_pc = %x; self_pc = %x; count =
        // %d\n", ahdr32.from_pc, ahdr32.self_pc, ahdr32.count);
      } else {
        ahdr.from_pc = frompc;
        ahdr.self_pc = i->selfpc;
        ahdr.count = i->count;
        gprof_out.write((char *)&ahdr, sizeof(ahdr));
        // fprintf(stderr, "Prof: ahdr, from_pc = %x; self_pc = %x; count =
        // %d\n", ahdr.from_pc, ahdr.self_pc, ahdr.count);
      }
    }
  }
  gprof_out.close();
  fprintf(stderr, "Prof: file writing done!\n");
}

void prof_module_t::profiling_pc(uint64_t pc) {
  // info for profilling
  uint64_t current_pc = pc;
  if (elfIs32 == true) {
    current_pc = (uint32_t)((pc << 32) >>
                            32); // for 32bit elf, the pc is ffffffff8*******
  }
  unsigned long current_cycle =
      sim->get_core(0)->get_cycle(); // TODO: other core
  // Here we use instance not ptr as *cur_insn maybe deleted in outer procedure
  static uint64_t prev_insn_data_pc;
  static unsigned long prev_cycle;

  if (prof_is_enable() == false) {
    return;
  }
  if (pinfo.init == false) {
    if (text_info.size() < 1) {
      // fprintf(stderr, "text_info is empty.\n");
      return;
    }
    size_t lowpc = text_info[0].vma;
    size_t highpc = text_info[0].vma + text_info[0].size;
    for (auto i : text_info) {
      if (i.vma < lowpc) {
        lowpc = i.vma;
      }
      if ((i.vma + i.size) > highpc) {
        highpc = i.vma + i.size;
      }
    }
    // fprintf(stderr, "gprof lowpc test2 = %lx\n", lowpc);
    // fprintf(stderr, "gprof highpc test2 = %lx\n", highpc);
    // printf("33 5\n");
    prof_env_setup(lowpc, highpc);
    // printf("33 6\n");
    prev_insn_data_pc = 0;
    prev_cycle = current_cycle;
    pinfo.init = true;
  }
  if (current_pc >= pinfo.highpc || current_pc < pinfo.lowpc) {
    // fprintf(stderr, "curr pc out of range.\n");
    return;
  }
  // fprintf(stderr, "profiling, current_pc: %lx\n", current_pc);
  // insn sampling
  // if (pinfo.kcount && current_pc != pinfo.lowpc)  //if you want to add .S
  // file's symbols to 'symbols_prof'
  if (pinfo.kcount) {
    size_t idx = (current_pc - pinfo.lowpc) >> 1;
    // fprintf(stderr, "pinfo.kcount idx = %lx; current_cycle = %ld; prev_cycle
    // = %ld\n", idx, current_cycle, prev_cycle);
    pinfo.kcount->at(idx) = pinfo.kcount->at(idx) + current_cycle - prev_cycle;
  }

  // call graph
  auto iter = program_address.find(current_pc);
  // if (iter != program_address.end() && prev_insn_data_pc != 0 && current_pc
  // != pinfo.lowpc) { //if you want to add .S file's symbols to 'symbols_prof'
  if (iter != program_address.end() && prev_insn_data_pc != 0) {
    // fprintf(stderr, "curr pc out of range 4. prev_insn_data_pc: %lx,
    // pinfo.lowpc: %lx\n", prev_insn_data_pc, pinfo.lowpc);
    size_t froms_idx = (prev_insn_data_pc - pinfo.lowpc) >> 1;
    size_t selfpc, count;
    bool find_callee = false;
    // fprintf(stderr, "Prof: call graph froms_idx = %lx; pc = %lx; lowpc =
    // %lx\n", froms_idx, prev_insn_data_pc, pinfo.lowpc);

    // check is this call graph has entry in table
    tos_type &to = pinfo.froms->at(froms_idx);
    for (auto j = to.begin(); j != to.end(); ++j) {
      if (j->selfpc == current_pc) {
        selfpc = j->selfpc;
        count = j->count + 1; // advance call count
        to.erase(j);
        find_callee = true;
        // fprintf(stderr, "Prof: call graph find_callee: %d, j->selfpc = %lx;
        // j->count = %ld\n", find_callee, j->selfpc, j->count);
        break;
      }
    }
    if (find_callee) {
      pinfo.froms->at(froms_idx).push_back({selfpc, count});
    } else {
      pinfo.froms->at(froms_idx).push_back({current_pc, 1});
    }
  }

  if (elfIs32 == true) {
    prev_insn_data_pc = (uint32_t)((pc << 32) >> 32);
    ; // for 32bit elf, the pc is ffffffff8*******
  } else {
    prev_insn_data_pc = pc;
  }
  prev_cycle = current_cycle;
}
