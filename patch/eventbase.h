/**
 * @file eventbase.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-02-06
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef EXT_EVENTBASE_H_
#define EXT_EVENTBASE_H_

#include <string>

struct eventbase_t {
  virtual ~eventbase_t() = default;
  virtual std::string serialization() const = 0;
};

#endif // EXT_EVENTBASE_H_