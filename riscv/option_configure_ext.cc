/**
 * @file option_configure_ext.cc
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2025-02-12
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "option_configure_ext.h"

void option_configure_ext(option_parser_t &parser, cfg_t &cfg) {
  parser.option(0, "deepctrl", 1, [&](const char* s){cfg.deepctrl = strtoull(s, nullptr, 10);});
}