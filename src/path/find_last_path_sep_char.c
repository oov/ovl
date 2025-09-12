#include <ovl/path.h>
#include <string.h>

char const *ovl_path_find_last_path_sep_char_const(char const *const path) {
  char const *sep = strrchr(path, '\\');
  if (!sep) {
    sep = strrchr(path, '/');
  }
  return sep;
}

char *ovl_path_find_last_path_sep_char_mut(char *const path) {
  return (char *)ov_deconster_(ovl_path_find_last_path_sep_char_const(path));
}
