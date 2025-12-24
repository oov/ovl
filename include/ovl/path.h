#pragma once

#include <ovbase.h>

/**
 * @brief Find the last path separator in a path string
 *
 * Searches for the last occurrence of '/' or '\' in the string.
 *
 * @param path Path string
 * @return Pointer to the last separator, or NULL if not found
 */
#define ovl_path_find_last_path_sep(path)                                                                              \
  _Generic((path),                                                                                                     \
      char const *: ovl_path_find_last_path_sep_char_const,                                                            \
      wchar_t const *: ovl_path_find_last_path_sep_wchar_const,                                                        \
      char *: ovl_path_find_last_path_sep_char_mut,                                                                    \
      wchar_t *: ovl_path_find_last_path_sep_wchar_mut)(path)

char *ovl_path_find_last_path_sep_char(char const *const path);
wchar_t *ovl_path_find_last_path_sep_wchar(wchar_t const *const path);

static inline char const *ovl_path_find_last_path_sep_char_const(char const *const path) {
  return ovl_path_find_last_path_sep_char(path);
}
static inline char *ovl_path_find_last_path_sep_char_mut(char *const path) {
  return ovl_path_find_last_path_sep_char(path);
}
static inline wchar_t const *ovl_path_find_last_path_sep_wchar_const(wchar_t const *const path) {
  return ovl_path_find_last_path_sep_wchar(path);
}
static inline wchar_t *ovl_path_find_last_path_sep_wchar_mut(wchar_t *const path) {
  return ovl_path_find_last_path_sep_wchar(path);
}

/**
 * @brief Extract the file name from a path string
 *
 * Returns a pointer to the character following the last path separator.
 * If no separator is found, returns the original path.
 *
 * @param path Path string
 * @return Pointer to the file name part of the path
 */
#define ovl_path_extract_file_name(path)                                                                               \
  _Generic((path),                                                                                                     \
      char const *: ovl_path_extract_file_name_char_const,                                                             \
      wchar_t const *: ovl_path_extract_file_name_wchar_const,                                                         \
      char *: ovl_path_extract_file_name_char_mut,                                                                     \
      wchar_t *: ovl_path_extract_file_name_wchar_mut)(path)

char *ovl_path_extract_file_name_char(char const *const path);
wchar_t *ovl_path_extract_file_name_wchar(wchar_t const *const path);

static inline char const *ovl_path_extract_file_name_char_const(char const *const path) {
  return ovl_path_extract_file_name_char(path);
}
static inline char *ovl_path_extract_file_name_char_mut(char *const path) {
  return ovl_path_extract_file_name_char(path);
}
static inline wchar_t const *ovl_path_extract_file_name_wchar_const(wchar_t const *const path) {
  return ovl_path_extract_file_name_wchar(path);
}
static inline wchar_t *ovl_path_extract_file_name_wchar_mut(wchar_t *const path) {
  return ovl_path_extract_file_name_wchar(path);
}

/**
 * @brief Find the file extension in a path string
 *
 * Searches for the last '.' in the file name.
 *
 * @param path Path string
 * @return Pointer to the extension (including '.'), or NULL if not found
 */
#define ovl_path_find_ext(path)                                                                                        \
  _Generic((path),                                                                                                     \
      char const *: ovl_path_find_ext_char_const,                                                                      \
      wchar_t const *: ovl_path_find_ext_wchar_const,                                                                  \
      char *: ovl_path_find_ext_char_mut,                                                                              \
      wchar_t *: ovl_path_find_ext_wchar_mut)(path)

char *ovl_path_find_ext_char(char const *const path);
wchar_t *ovl_path_find_ext_wchar(wchar_t const *const path);

static inline char const *ovl_path_find_ext_char_const(char const *const path) { return ovl_path_find_ext_char(path); }
static inline char *ovl_path_find_ext_char_mut(char *const path) { return ovl_path_find_ext_char(path); }
static inline wchar_t const *ovl_path_find_ext_wchar_const(wchar_t const *const path) {
  return ovl_path_find_ext_wchar(path);
}
static inline wchar_t *ovl_path_find_ext_wchar_mut(wchar_t *const path) { return ovl_path_find_ext_wchar(path); }

NODISCARD bool ovl_path_get_temp_directory(NATIVE_CHAR **const path, struct ov_error *const err);

NODISCARD bool ovl_path_get_module_name(NATIVE_CHAR **const module_path, void *const hinst, struct ov_error *const err);
