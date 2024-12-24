#ifndef _EASY_ARGS_H
#define _EASY_ARGS_H

struct easy_args_t {
  bool vmaskone = false;

  bool specify_proc = false;
};

inline easy_args_t g_easy_args = easy_args_t();

#endif // _EASY_ARGS_H