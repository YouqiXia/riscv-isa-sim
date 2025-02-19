#ifndef _EASY_ARGS_H_
#define _EASY_ARGS_H_

// todo. Remove usage from insn_template.
struct easy_args_t {
  bool vmaskone = false; // vector usage

  bool specify_proc = false; // to be removed
};

inline easy_args_t g_easy_args = easy_args_t();

#endif // _EASY_ARGS_H_