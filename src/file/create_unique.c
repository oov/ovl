#include <ovl/file.h>

#include <ovarray.h>
#include <ovl/path.h>

#include "../i18n.h"

#ifdef _WIN32

#  define STRLEN wcslen
#  define STRCPY wcscpy

static size_t extract_file_extension_pos(NATIVE_CHAR const *const filename) {
  if (!filename) {
    return 0;
  }
  size_t const len = STRLEN(filename);
  for (size_t i = len; i > 0; --i) {
    if (filename[i - 1] == L'.') {
      return i - 1;
    }
    if (filename[i - 1] == L'\\' || filename[i - 1] == L'/') {
      break;
    }
  }
  return len;
}

static void format_hex_string(NATIVE_CHAR *dest, uint64_t const value) {
  static NATIVE_CHAR const hex_chars[] = NSTR("0123456789abcdef");
  for (int i = 15; i >= 0; --i) {
    dest[15 - i] = hex_chars[(value >> (i * 4)) & 0xf];
  }
}

NODISCARD error ovl_file_create_unique(NATIVE_CHAR const *const dir,
                                       NATIVE_CHAR const *const base_name,
                                       struct ovl_file **const file,
                                       NATIVE_CHAR **const created_path) {
  if (!dir || !file || *file || !created_path || *created_path) {
    return errg(err_invalid_arugment);
  }

  NATIVE_CHAR const *actual_base_name = base_name;
  if (!base_name || base_name[0] == NSTR('\0')) {
    actual_base_name = NSTR("tmp.bin");
  }

  NATIVE_CHAR *path = NULL;
  HANDLE h = INVALID_HANDLE_VALUE;
  error err = eok();

  size_t const dir_len = STRLEN(dir);
  size_t const base_name_len = STRLEN(actual_base_name);
  size_t const ext_pos = extract_file_extension_pos(actual_base_name);
  size_t const ext_len = base_name_len - ext_pos;

  NATIVE_CHAR const *basename = actual_base_name;
  size_t basename_len = ext_pos;
  if (base_name_len > 0 && basename_len == 0) {
    basename = L"tmp";
    basename_len = 3;
  }

  bool has_trailing_sep = (dir_len > 0 && (dir[dir_len - 1] == L'\\' || dir[dir_len - 1] == L'/'));
  size_t sep_len = has_trailing_sep ? 0 : 1;
  size_t const path_len = dir_len + sep_len + basename_len + 1 + 16 + ext_len;

  err = OV_ARRAY_GROW(&path, path_len + 1);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  STRCPY(path, dir);
  if (!has_trailing_sep) {
    path[dir_len] = L'\\';
  }
  STRCPY(path + dir_len + sep_len, basename);
  path[dir_len + sep_len + basename_len] = NSTR('_');
  STRCPY(path + dir_len + sep_len + basename_len + 1 + 16, actual_base_name + ext_pos);

  NATIVE_CHAR *digits = path + dir_len + sep_len + basename_len + 1;
  uint64_t rng_state = ov_splitmix64(get_global_hint() + GetTickCount64());

  for (int attempt = 0; attempt < 10; ++attempt) {
    rng_state = ov_splitmix64(rng_state);
    format_hex_string(digits, rng_state);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
      *file = (struct ovl_file *)h;
      *created_path = path;
      OV_ARRAY_SET_LENGTH(path, path_len);
      path = NULL;
      h = INVALID_HANDLE_VALUE;
      goto cleanup;
    }
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    if (hr != HRESULT_FROM_WIN32(ERROR_FILE_EXISTS)) {
      err = errhr(hr);
      goto cleanup;
    }
  }
  err = emsg_i18n(err_type_generic, err_fail, gettext("Failed to create unique file after multiple attempts"));

cleanup:
  if (h != INVALID_HANDLE_VALUE) {
    CloseHandle(h);
    h = INVALID_HANDLE_VALUE;
  }
  if (path) {
    OV_ARRAY_DESTROY(&path);
  }
  return err;
}

#endif
