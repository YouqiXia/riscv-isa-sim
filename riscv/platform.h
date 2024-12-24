// See LICENSE for license details.
#ifndef _RISCV_PLATFORM_H
#define _RISCV_PLATFORM_H
#if (0)
#define DEFAULT_KERNEL_BOOTARGS "console=ttyS0 earlycon"
#define DEFAULT_RSTVEC     0x00001000
#define DEFAULT_ISA        "rv64imafdc_zicntr_zihpm"
#define DEFAULT_PRIV       "MSU"
#define CLINT_BASE         0x02000000
#define CLINT_SIZE         0x000c0000
#define PLIC_BASE          0x0c000000
#define PLIC_SIZE          0x01000000
#define PLIC_NDEV          31
#define PLIC_PRIO_BITS     4
#define NS16550_BASE       0x10000000
#define NS16550_SIZE       0x100
#define NS16550_REG_SHIFT  0
#define NS16550_REG_IO_WIDTH 1
#define NS16550_INTERRUPT_ID 1
#define EXT_IO_BASE        0x40000000
#define DRAM_BASE          0x80000000
#endif

// rivai beg
struct platform_cfg_t {
  enum cfg_mode_t {
    MEMSIZE_2G, //current default for elf runs
    MEMSIZE_4G, //support elf runs in mem 0x200000-0xff000000
    MEMSIZE_8G, //support elf runs in mem 0x4000000000-0x4200000000
  };

  platform_cfg_t() {
    reinit(cfg_mode_t::MEMSIZE_2G);
  }
  void reinit(cfg_mode_t mode) {
    switch (mode) {
      case cfg_mode_t::MEMSIZE_2G:
        rstvec = 0x00001000;
        clintbs = 0x02000000;
        clintsz = 0x000c0000;
        plicbs = 0x0c000000;
        plicsz = 0x01000000;
        plic_ndev = 31;
        plic_prio_bits = 4;
        ns16550_base = 0x10000000;
        ns16550_size = 0x100;
        ns16550_reg_shift = 0;
        ns16550_reg_io_width = 1;
        ns16550_interrupt_id = 1;
        ext_io_base = 0x40000000;
        dram_base = 0x80000000;

        simpoint_mainram_base = 0x80000000;
        break;

      case cfg_mode_t::MEMSIZE_4G:
        rstvec = 0x100000000;
        clintbs = 0x02000000;
        clintsz = 0x000c0000;
        plicbs = 0x0c000000;
        plicsz = 0x01000000;
        plic_ndev = 31;
        plic_prio_bits = 4;
        ns16550_base = 0x110000000;
        ns16550_size = 0x100;
        ns16550_reg_shift = 0;
        ns16550_reg_io_width = 1;
        ns16550_interrupt_id = 1;
        ext_io_base = 0x140000000;
        dram_base = 0x00020000;

        simpoint_mainram_base = 0x200000;
        break;

      case cfg_mode_t::MEMSIZE_8G:
      default:
        rstvec = 0x1000;
        clintbs = 0x02000000;
        clintsz = 0x000c0000;
        plicbs = 0x0c000000;
        plicsz = 0x01000000;
        plic_ndev = 31;
        plic_prio_bits = 4;
        ns16550_base = 0xd0087000;
        ns16550_size = 0x1000;
        ns16550_reg_shift = 0;
        ns16550_reg_io_width = 1;
        ns16550_interrupt_id = 1;
        ext_io_base = 0x40000000;
        dram_base = 0x4000000000;

        simpoint_mainram_base = 0x4000000000;
        break;
    }
  }

  uint64_t rstvec;
  uint64_t clintbs;
  uint64_t clintsz;
  uint64_t plicbs;
  uint64_t plicsz;
  uint64_t plic_ndev;
  uint64_t plic_prio_bits;
  uint64_t ns16550_base;
  uint64_t ns16550_size;
  uint64_t ns16550_reg_shift;
  uint64_t ns16550_reg_io_width;
  uint64_t ns16550_interrupt_id;
  uint64_t ext_io_base;
  uint64_t dram_base;

  uint64_t simpoint_mainram_base;
};

inline platform_cfg_t s_platform_cfg = platform_cfg_t();

#define DEFAULT_KERNEL_BOOTARGS "console=ttyS0 earlycon"
#define DEFAULT_RSTVEC s_platform_cfg.rstvec
#define DEFAULT_ISA        "rv64imafdc_zicntr_zihpm"
#define DEFAULT_PRIV       "MSU"
// CLINT and PLIC will be set by dtb file, do not need to modify
#define CLINT_BASE s_platform_cfg.clintbs // P600 CLINT基地址 = 0x62ff800000
#define CLINT_SIZE s_platform_cfg.clintsz
#define PLIC_BASE s_platform_cfg.plicbs
#define PLIC_SIZE s_platform_cfg.plicsz
#define PLIC_NDEV s_platform_cfg.plic_ndev
#define PLIC_PRIO_BITS s_platform_cfg.plic_prio_bits
#define NS16550_BASE s_platform_cfg.ns16550_base
#define NS16550_SIZE s_platform_cfg.ns16550_size
#define NS16550_REG_SHIFT s_platform_cfg.ns16550_reg_shift
#define NS16550_REG_IO_WIDTH s_platform_cfg.ns16550_reg_io_width
#define NS16550_INTERRUPT_ID s_platform_cfg.ns16550_interrupt_id
#define EXT_IO_BASE s_platform_cfg.ext_io_base
#define DRAM_BASE s_platform_cfg.dram_base

// UART-Z1 在P600的基地址是0xff027000
#define UART_Z1_BASE 0xff027000
#define UART_Z1_SIZE 0x00001000

// rivai add: add PROC_START_PC_ADDR
// #define DISABLE_ROM

#ifndef DISABLE_ROM
#define PROC_START_PC_ADDR DEFAULT_RSTVEC
#else
#define PROC_START_PC_ADDR DRAM_BASE
#endif
// rivai end

#endif
