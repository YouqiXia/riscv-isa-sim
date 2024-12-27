/**
 * @file ext_utils.h
 * @author qiubinglin (qiubinglin@outlook.com)
 * @brief utilities for patch
 * @version 0.1
 * @date 2024-12-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef EXT_UTILS_H
#define EXT_UTILS_H

#include <string>
#include <vector>

/**
 * @brief State machine to check if pattern is matched in stream.
 */
class stream_kmp_t {
public:
  stream_kmp_t(const std::string &_pattern) : pattern(_pattern) {
    gen_lps();
  }
  bool check(char c) {
    size_t m = pattern.size();
    if (m == 0) {
      return true;
    }

    if (pattern[pidx] == c) {
      ++pidx;
    } else {
      if (pidx == 0) {
        return false;
      }
      pidx = lps[pidx - 1];
      return check(c);
    }
    if (pidx == m) {
      pidx = lps[pidx - 1];
      return true;
    }
    return false;
  }

private:
  // Function to compute the LPS array
  void gen_lps() {
    size_t m = pattern.size();
    lps.resize(m);
    if (m == 0) {
      return;
    }
    size_t length = 0;
    lps[0] = 0;
    size_t i = 1;

    while (i < m) {
      if (pattern[i] == pattern[length]) {
        length++;
        lps[i] = length;
        i++;
      } else {
        if (length != 0) {
          length = lps[length - 1];
        } else {
          lps[i] = 0;
          i++;
        }
      }
    }
  }

private:
  size_t pidx = 0;
  std::string pattern;
  std::vector<int> lps;
};

#endif