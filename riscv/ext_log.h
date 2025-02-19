/**
 * @file ext_log.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-01-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef _EXT_LOG_H_
#define _EXT_LOG_H_

#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <string>

struct log_flag_t {
  enum type_t : uint64_t {
    MCC = 0x1,
  };
  uint64_t val = MCC;

  uint64_t operator&(uint64_t that) const { return val & that; }
};

class log_mgr_t {
public:
  static log_mgr_t &instance();

private:
  log_mgr_t() = default;
  ~log_mgr_t();

public:
  void add(log_flag_t::type_t type, const std::string &filename);

  log_flag_t flags;
  std::unordered_map<log_flag_t::type_t, FILE *> log_file_map;
};

#define EXT_LOG(flag, Fmt, Arg...)                                             \
  do {                                                                         \
    auto &g_log_mgr = log_mgr_t::instance();                                   \
    if ((g_log_mgr.flags & flag) and g_log_mgr.log_file_map.count(flag)) {     \
      fprintf(g_log_mgr.log_file_map.at(flag), Fmt, ##Arg);                    \
      fflush(g_log_mgr.log_file_map.at(flag));                                 \
    }                                                                          \
  } while (0)

#define MCC_LOG(Fmt, Arg...) EXT_LOG(log_flag_t::MCC, Fmt, ##Arg)

#endif // _EXT_LOG_H_