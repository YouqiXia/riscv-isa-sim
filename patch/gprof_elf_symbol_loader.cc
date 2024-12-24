#include "gprof_elf_symbol_loader.h"
// #include "gprof_struct_define.h"
#include "fesvr/elf.h"
// #include "fesvr/memif.h"

#include "byteorder.h"
#include <assert.h>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#define LOAD_ELF_PROF_SYMBOLS_ONLY(ehdr_t, phdr_t, shdr_t, sym_t)                                                      \
  do {                                                                                                                 \
    ehdr_t *eh = (ehdr_t *)buf;                                                                                        \
    phdr_t *ph = (phdr_t *)(buf + eh->e_phoff);                                                                        \
    *entry = eh->e_entry;                                                                                              \
    assert(size >= eh->e_phoff + eh->e_phnum * sizeof(*ph));                                                           \
    for (unsigned i = 0; i < eh->e_phnum; i++) {                                                                       \
      if (ph[i].p_type == PT_LOAD && ph[i].p_memsz) {                                                                  \
        if (ph[i].p_filesz) {                                                                                          \
          assert(size >= ph[i].p_offset + ph[i].p_filesz);                                                             \
        }                                                                                                              \
        zeros.resize(ph[i].p_memsz - ph[i].p_filesz);                                                                  \
      }                                                                                                                \
    }                                                                                                                  \
    shdr_t *sh = (shdr_t *)(buf + eh->e_shoff);                                                                        \
    assert(size >= eh->e_shoff + eh->e_shnum * sizeof(*sh));                                                           \
    assert(eh->e_shstrndx < eh->e_shnum);                                                                              \
    assert(size >= sh[eh->e_shstrndx].sh_offset + sh[eh->e_shstrndx].sh_size);                                         \
    char *shstrtab = buf + sh[eh->e_shstrndx].sh_offset;                                                               \
    unsigned strtabidx = 0, symtabidx = 0;                                                                             \
    for (unsigned i = 0; i < eh->e_shnum; i++) {                                                                       \
      unsigned max_len = sh[eh->e_shstrndx].sh_size - sh[i].sh_name;                                                   \
      assert(sh[i].sh_name < sh[eh->e_shstrndx].sh_size);                                                              \
      assert(strnlen(shstrtab + sh[i].sh_name, max_len) < max_len);                                                    \
      (void)max_len;                                                                                                   \
      if (sh[i].sh_type & SHT_NOBITS)                                                                                  \
        continue;                                                                                                      \
      assert(size >= sh[i].sh_offset + sh[i].sh_size);                                                                 \
      if (strcmp(shstrtab + sh[i].sh_name, ".strtab") == 0)                                                            \
        strtabidx = i;                                                                                                 \
      if (strcmp(shstrtab + sh[i].sh_name, ".symtab") == 0)                                                            \
        symtabidx = i;                                                                                                 \
    }                                                                                                                  \
    if (strtabidx && symtabidx) {                                                                                      \
      char *strtab = buf + sh[strtabidx].sh_offset;                                                                    \
      sym_t *sym = (sym_t *)(buf + sh[symtabidx].sh_offset);                                                           \
      for (unsigned i = 0; i < sh[symtabidx].sh_size / sizeof(sym_t); i++) {                                           \
        unsigned max_len = sh[strtabidx].sh_size - sym[i].st_name;                                                     \
        assert(sym[i].st_name < sh[strtabidx].sh_size);                                                                \
        assert(strnlen(strtab + sym[i].st_name, max_len) < max_len);                                                   \
        (void)max_len;                                                                                                 \
        if (ELF_ST_TYPE(sym[i].st_info) == STT_FUNC)                                                                   \
          symbols_prof[strtab + sym[i].st_name] = sym[i].st_value;                                                     \
      }                                                                                                                \
    }                                                                                                                  \
  } while (0)
/*
  if you want to add .S file's symbols to 'symbols_prof'
  example: lable_bss_clear_start in rtos/start.S
  if   ((ELF_ST_TYPE(sym[i].st_info) == STT_FUNC) \
  || ( (strcmp(strtab + sym[i].st_name, "$x") != 0) && ( \
    ((sym[i].st_info == 0x10) || (sym[i].st_info == 0x0)) \
  && ((sym[i].st_shndx == 0x1) || (sym[i].st_shndx == 0x2)))) \
  ){ \
  symbols_prof[strtab + sym[i].st_name] = sym[i].st_value; \
  fprintf(stderr, "sym[i].st_info: %lx, name: %s, st_value: %lx, st_size:
    %ld, st_other: %lx, st_shndx: %lx\n", sym[i].st_info, strtab +
    sym[i].st_name, sym[i].st_value, sym[i].st_size, sym[i].st_other,
    sym[i].st_shndx); \
  } \
*/

std::map<std::string, uint64_t> load_prof_symbol(const char *fn, reg_t *entry) {
  //   fprintf(stderr, "--------- load_prof_symbol, fn = %s\n", fn);
  int fd = open(fn, O_RDONLY);
  struct stat s;
  assert(fd != -1);
  if (fstat(fd, &s) < 0)
    abort();
  size_t size = s.st_size;

  //   char* buf = (char*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

  // not encrypt
  char *buf;
  buf = (char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

  assert(buf != MAP_FAILED);
  close(fd);

  assert(size >= sizeof(Elf64_Ehdr));
  const Elf64_Ehdr *eh64 = (const Elf64_Ehdr *)buf;
  assert(IS_ELF32(*eh64) || IS_ELF64(*eh64));

  std::vector<uint8_t> zeros;
  std::map<std::string, uint64_t> symbols_prof;

  if (IS_ELF32(*eh64))
    LOAD_ELF_PROF_SYMBOLS_ONLY(Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr, Elf32_Sym);
  else
    LOAD_ELF_PROF_SYMBOLS_ONLY(Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr, Elf64_Sym);

  // not encrypt
  munmap(buf, size);

  return symbols_prof;
}

std::vector<elf_text_info> get_text_info_from_elf(const char *fn,
                                                  bool *elfIs32) {
  //   fprintf(stderr, "--------- get_text_info, fn = %s\n", fn);
  int fd = open(fn, O_RDONLY);

  struct stat s;
  assert(fd != -1);
  if (fstat(fd, &s) < 0)
    abort();
  size_t size = s.st_size;

  // decryption
  char *buf;
  buf = (char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

  //   char* buf = (char*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  assert(buf != MAP_FAILED);
  close(fd);

  const Elf64_Ehdr *eh64 = (const Elf64_Ehdr *)buf;

  std::vector<elf_text_info> text_info;
  elf_text_info tmp;

  if (IS_ELF32(*eh64)) {
    *elfIs32 = true;
    // LOAD_ELF(Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr, Elf32_Sym);
    Elf32_Ehdr *eh = (Elf32_Ehdr *)buf;
    Elf32_Shdr *sh = (Elf32_Shdr *)(buf + eh->e_shoff);
    for (unsigned i = 0; i < eh->e_shnum; i++) {
      if (sh[i].sh_flags & SHF_EXECINSTR) {
        tmp.vma = sh[i].sh_addr;
        tmp.size = sh[i].sh_size;
        // fprintf(stderr, "tmp.vma: %lx; size: %lx\n", tmp.vma, tmp.size);
        text_info.push_back(tmp);
      }
    }
  } else {
    *elfIs32 = false;
    Elf64_Ehdr *eh = (Elf64_Ehdr *)buf;
    Elf64_Shdr *sh = (Elf64_Shdr *)(buf + eh->e_shoff);
    for (unsigned i = 0; i < eh->e_shnum; i++) {
      if (sh[i].sh_flags & SHF_EXECINSTR) {
        tmp.vma = sh[i].sh_addr;
        tmp.size = sh[i].sh_size;
        // fprintf(stderr, "__tmp.vma: %lx; size: %lx\n", tmp.vma, tmp.size);
        text_info.push_back(tmp);
      }
    }
  }
  munmap(buf, size);
  // printf("\e[32mprof symbols: \e[0m\n");
  // for (elf_text_info& elem : text_info)
  // {
  //   printf("vma=%#lx, size=%lx\n", elem.vma, elem.size);
  // }

  return text_info;
}
