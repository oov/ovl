#include <ovl/path.h>

char const *ovl_path_extract_file_name_char_const(char const *const path) {
  if (!path) {
    return NULL;
  }
  char const *sep = ovl_path_find_last_path_sep_char_const(path);
  if (!sep) {
    return path;
  }
  ++sep;
  return sep;
}

char *ovl_path_extract_file_name_char_mut(char *const path) {
  return ov_deconster_(ovl_path_extract_file_name_char_const(path));
}
