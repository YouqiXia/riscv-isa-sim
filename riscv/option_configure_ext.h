/**
 * @file option_configure_ext.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2025-02-12
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef EXT_OPTION_CONFIGURE_EXT_H
#define EXT_OPTION_CONFIGURE_EXT_H

#include "option_parser.h"
#include "cfg.h"

#define OPTION_HELP_PRINT \
  fprintf(stderr, "  --deepctrl=<val>      Set if in deepctrl mode, val=0 or 1.\n");

void option_configure_ext(option_parser_t &parser, cfg_t &cfg);

#endif // EXT_OPTION_CONFIGURE_EXT_H