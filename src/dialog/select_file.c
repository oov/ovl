#include <ovl/dialog.h>

#include <ovarray.h>
#include <ovl/path.h>

#include <string.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

#  ifndef COBJMACROS
#    define COBJMACROS
#  endif

#  include <shlobj.h>

NODISCARD bool ovl_dialog_select_file(void *const hwnd,
                                      NATIVE_CHAR const *const title,
                                      NATIVE_CHAR const *const filter,
                                      void const *const client_id,
                                      NATIVE_CHAR const *const default_path,
                                      NATIVE_CHAR **const path,
                                      struct ov_error *const err) {
  if (!path) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  IFileDialog *pfd = NULL;
  COMDLG_FILTERSPEC *fs = NULL;
  IShellItem *psiResult = NULL;
  IShellItem *psiDefaultFolder = NULL;
  PWSTR pszPath = NULL;
  bool result = false;
  WCHAR const *f = NULL;
  size_t filter_count = 0;
  wchar_t *folder_path = NULL;

  HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&pfd);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }
  hr = IFileDialog_SetTitle(pfd, title);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }

  // Split default_path into folder and filename
  if (default_path) {
    wchar_t const *filename = ovl_path_find_last_path_sep(default_path);
    if (filename) {
      filename++; // Skip the separator
      hr = IFileDialog_SetFileName(pfd, filename);
      if (FAILED(hr)) {
        OV_ERROR_SET_HRESULT(err, hr);
        goto cleanup;
      }

      size_t const folder_len = (size_t)(filename - default_path - 1);
      if (!OV_ARRAY_GROW(&folder_path, folder_len + 1)) {
        OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
        goto cleanup;
      }
      wcsncpy(folder_path, default_path, folder_len);
      folder_path[folder_len] = L'\0';

      hr = SHCreateItemFromParsingName(folder_path, NULL, &IID_IShellItem, (void **)&psiDefaultFolder);
      if (SUCCEEDED(hr) && psiDefaultFolder) {
        hr = IFileDialog_SetDefaultFolder(pfd, psiDefaultFolder);
        if (FAILED(hr)) {
          OV_ERROR_SET_HRESULT(err, hr);
          goto cleanup;
        }
      }
    } else {
      // No path separator, treat entire string as filename
      hr = IFileDialog_SetFileName(pfd, default_path);
      if (FAILED(hr)) {
        OV_ERROR_SET_HRESULT(err, hr);
        goto cleanup;
      }
    }
  }

  f = filter;
  while (*f) {
    ++filter_count;
    f += wcslen(f) + 1;
  }
  filter_count /= 2;
  if (!OV_ARRAY_GROW(&fs, filter_count + 1)) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
    goto cleanup;
  }
  f = filter;
  for (size_t i = 0; i < filter_count; ++i) {
    fs[i].pszName = f;
    f += wcslen(f) + 1;
    fs[i].pszSpec = f;
    f += wcslen(f) + 1;
  }
  hr = IFileDialog_SetFileTypes(pfd, (UINT)filter_count, fs);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }
  hr = IFileDialog_SetClientGuid(pfd, (GUID const *)client_id);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }
  hr = IFileDialog_Show(pfd, (HWND)hwnd);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }
  hr = IFileDialog_GetResult(pfd, &psiResult);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }
  hr = IShellItem_GetDisplayName(psiResult, SIGDN_FILESYSPATH, &pszPath);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }
  if (!OV_ARRAY_GROW(path, wcslen(pszPath) + 1)) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_out_of_memory);
    goto cleanup;
  }
  wcscpy(*path, pszPath);
  result = true;

cleanup:
  if (pszPath) {
    CoTaskMemFree(pszPath);
    pszPath = NULL;
  }
  if (psiDefaultFolder) {
    IShellItem_Release(psiDefaultFolder);
    psiDefaultFolder = NULL;
  }
  if (psiResult) {
    IShellItem_Release(psiResult);
    psiResult = NULL;
  }
  if (fs) {
    OV_ARRAY_DESTROY(&fs);
  }
  if (folder_path) {
    OV_ARRAY_DESTROY(&folder_path);
  }
  if (pfd) {
    IFileDialog_Release(pfd);
    pfd = NULL;
  }
  return result;
}

#endif // _WIN32
