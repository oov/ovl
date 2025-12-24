#ifndef FIND_LAST_PATH_SEP_INC_C
#define FIND_LAST_PATH_SEP_INC_C
#include <ovl/path.h>

#ifndef T
#  include "find_last_path_sep_char.c"
#endif

T *FUNCNAME(T const *const path) {
  T *const sep1 = STRRCHR(path, CHR('\\'));
  T *const sep2 = STRRCHR(path, CHR('/'));
  if (!sep1) {
    return sep2;
  }
  if (!sep2) {
    return sep1;
  }
  return (sep1 > sep2) ? sep1 : sep2;
}

#undef T
#undef CHR
#undef STRRCHR
#undef FUNCNAME
#endif
