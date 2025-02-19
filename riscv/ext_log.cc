/**
 * @file ext_log.cc
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-01-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "ext_log.h"

log_mgr_t &log_mgr_t::instance() {
  static log_mgr_t ins;
  return ins;
}

log_mgr_t::~log_mgr_t() {
  for (auto [type, fp] : log_file_map) {
    fclose(fp);
  }
}

void log_mgr_t::add(log_flag_t::type_t type, const std::string &filename) {
  if (log_file_map.count(type)) {
    return;
  }
  FILE *fp = fopen(filename.c_str(), "w");
  if (fp) {
    log_file_map[type] = fp;
  }
}