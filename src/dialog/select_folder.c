#include <ovl/dialog.h>

#include <ovarray.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

#  ifndef COBJMACROS
#    define COBJMACROS
#  endif

#  include <shlobj.h>

NODISCARD bool ovl_dialog_select_folder(void *const hwnd,
                                        NATIVE_CHAR const *const title,
                                        void const *const client_id,
                                        NATIVE_CHAR const *const default_path,
                                        NATIVE_CHAR **const path,
                                        struct ov_error *const err) {
  if (!path) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  IFileDialog *pfd = NULL;
  IShellItem *psiResult = NULL;
  IShellItem *psiDefaultFolder = NULL;
  PWSTR pszPath = NULL;
  bool result = false;

  HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&pfd);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }
  DWORD dwOptions;
  hr = IFileDialog_GetOptions(pfd, &dwOptions);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }
  hr = IFileDialog_SetOptions(pfd, dwOptions | FOS_PICKFOLDERS);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }
  hr = IFileDialog_SetTitle(pfd, title);
  if (FAILED(hr)) {
    OV_ERROR_SET_HRESULT(err, hr);
    goto cleanup;
  }

  // Set default folder if provided
  if (default_path) {
    hr = SHCreateItemFromParsingName(default_path, NULL, &IID_IShellItem, (void **)&psiDefaultFolder);
    if (SUCCEEDED(hr) && psiDefaultFolder) {
      hr = IFileDialog_SetDefaultFolder(pfd, psiDefaultFolder);
      if (FAILED(hr)) {
        OV_ERROR_SET_HRESULT(err, hr);
        goto cleanup;
      }
    }
  }

  hr = IFileDialog_SetClientGuid(pfd, (const GUID *)client_id);
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
  if (pfd) {
    IFileDialog_Release(pfd);
    pfd = NULL;
  }
  return result;
}

#endif // _WIN32
