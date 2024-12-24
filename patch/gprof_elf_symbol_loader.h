#ifndef _GPROF_ELF_SYMBOL_LOADER_H
#define _GPROF_ELF_SYMBOL_LOADER_H

#include "fesvr/elf.h"
#include "fesvr/memif.h"
#include <map>
#include <string>
#include <vector>

#include "gprof_struct_define.h"

class memif_t;
std::map<std::string, uint64_t> load_prof_symbol(const char *fn, reg_t *entry);
std::vector<elf_text_info> get_text_info_from_elf(const char *fn,
                                                  bool *elfIs32);

#endif