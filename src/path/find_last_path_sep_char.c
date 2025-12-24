#include <ovl/path.h>
#include <string.h>

char const *ovl_path_find_last_path_sep_char_const(char const *const path) {
  char const *sep1 = strrchr(path, L'\\');
  char const *sep2 = strrchr(path, L'/');
  if (!sep1) {
    return sep2;
  }
  if (!sep2) {
    return sep1;
  }
  return (sep1 > sep2) ? sep1 : sep2;
}

char *ovl_path_find_last_path_sep_char_mut(char *const path) {
  return (char *)ov_deconster_(ovl_path_find_last_path_sep_char_const(path));
}
