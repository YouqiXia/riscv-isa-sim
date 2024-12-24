
#ifndef _GPROF_STRUCT_DEFINE_H
#define _GPROF_STRUCT_DEFINE_H

#define SHF_ALLOC (1 << 1)     /* Occupies memory during execution */
#define SHF_EXECINSTR (1 << 2) /* Executable */

#define ELF_ST_TYPE(info) ((uint32_t)(info)&0xf)
#define STT_NOTYPE (0)
#define STT_OBJECT (1)
#define STT_FUNC (2)
#define STT_SECTION (3)
#define STT_FILE (4)
#define STT_LOPROC (13)
#define STT_HIPROC (15)

#include <string>
#include <vector>

struct to_struct {
  size_t selfpc;
  size_t count;
};
typedef std::vector<to_struct> to_type;
struct profiling_param {
  bool init;
  size_t lowpc;
  size_t highpc;
  size_t textsize;
  std::vector<uint16_t> *kcount;
  std::vector<to_type> *froms;
};

struct elf_text_info {
  uint64_t vma;
  uint64_t size;
};

#endif