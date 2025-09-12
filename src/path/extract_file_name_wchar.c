#include <ovl/path.h>

wchar_t const *ovl_path_extract_file_name_wchar_const(wchar_t const *const path) {
  if (!path) {
    return NULL;
  }
  wchar_t const *sep = ovl_path_find_last_path_sep_wchar_const(path);
  if (!sep) {
    return path;
  }
  ++sep;
  return sep;
}

wchar_t *ovl_path_extract_file_name_wchar_mut(wchar_t *const path) {
  return (wchar_t *)ov_deconster_(ovl_path_extract_file_name_wchar_const(path));
}
