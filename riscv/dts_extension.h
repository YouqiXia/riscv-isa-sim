#ifndef _RISCV_DTS_EXTENSION_H
#define _RISCV_DTS_EXTENSION_H

#include "dts.h"

int fdt_parse_rivai_uart_z1(const void* fdt, reg_t* uart_z1_addr, const char* compatible);

#endif