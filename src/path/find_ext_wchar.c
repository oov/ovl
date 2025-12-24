#include <ovl/path.h>
#include <wchar.h>

wchar_t const *ovl_path_find_ext_wchar_const(wchar_t const *const path) {
  if (!path) {
    return NULL;
  }
  // First, find the last path separator to avoid false positives
  // like "/home/user/temp.dir/LICENSE"
  wchar_t const *filename = ovl_path_extract_file_name_wchar_const(path);
  if (!filename) {
    return NULL;
  }
  wchar_t const *ext = wcsrchr(filename, L'.');
  return ext;
}

wchar_t *ovl_path_find_ext_wchar_mut(wchar_t *const path) {
  return (wchar_t *)ov_deconster_(ovl_path_find_ext_wchar_const(path));
}
