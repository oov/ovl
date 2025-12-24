#include <ovl/path.h>
#include <string.h>

char const *ovl_path_find_ext_char_const(char const *const path) {
  if (!path) {
    return NULL;
  }
  // First, find the last path separator to avoid false positives
  // like "/home/user/temp.dir/LICENSE"
  char const *filename = ovl_path_extract_file_name_char_const(path);
  if (!filename) {
    return NULL;
  }
  char const *ext = strrchr(filename, '.');
  return ext;
}

char *ovl_path_find_ext_char_mut(char *const path) { return (char *)ov_deconster_(ovl_path_find_ext_char_const(path)); }
