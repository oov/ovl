#pragma once

#include <ovbase.h>

#define ovl_path_find_last_path_sep(path)                                                                              \
  _Generic((path),                                                                                                     \
      char const *: ovl_path_find_last_path_sep_char_const,                                                            \
      wchar_t const *: ovl_path_find_last_path_sep_wchar_const,                                                        \
      char *: ovl_path_find_last_path_sep_char_mut,                                                                    \
      wchar_t *: ovl_path_find_last_path_sep_wchar_mut)(path)

char const *ovl_path_find_last_path_sep_char_const(char const *const path);
wchar_t const *ovl_path_find_last_path_sep_wchar_const(wchar_t const *const path);
char *ovl_path_find_last_path_sep_char_mut(char *const path);
wchar_t *ovl_path_find_last_path_sep_wchar_mut(wchar_t *const path);

#define ovl_path_extract_file_name(path)                                                                               \
  _Generic((path),                                                                                                     \
      char const *: ovl_path_extract_file_name_char_const,                                                             \
      wchar_t const *: ovl_path_extract_file_name_wchar_const,                                                         \
      char *: ovl_path_extract_file_name_char_mut,                                                                     \
      wchar_t *: ovl_path_extract_file_name_wchar_mut)(path)

char const *ovl_path_extract_file_name_char_const(char const *const path);
wchar_t const *ovl_path_extract_file_name_wchar_const(wchar_t const *const path);
char *ovl_path_extract_file_name_char_mut(char *const path);
wchar_t *ovl_path_extract_file_name_wchar_mut(wchar_t *const path);

#define ovl_path_find_ext(path)                                                                                        \
  _Generic((path),                                                                                                     \
      char const *: ovl_path_find_ext_char_const,                                                                      \
      wchar_t const *: ovl_path_find_ext_wchar_const,                                                                  \
      char *: ovl_path_find_ext_char_mut,                                                                              \
      wchar_t *: ovl_path_find_ext_wchar_mut)(path)

char const *ovl_path_find_ext_char_const(char const *const path);
wchar_t const *ovl_path_find_ext_wchar_const(wchar_t const *const path);
char *ovl_path_find_ext_char_mut(char *const path);
wchar_t *ovl_path_find_ext_wchar_mut(wchar_t *const path);

NODISCARD bool ovl_path_get_temp_directory(NATIVE_CHAR **const path, struct ov_error *const err);

NODISCARD bool ovl_path_get_module_name(NATIVE_CHAR **const module_path, void *const hinst, struct ov_error *const err);
