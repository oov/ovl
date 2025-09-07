#include <ovl/dialog.h>

#include <ovarray.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

#  ifndef COBJMACROS
#    define COBJMACROS
#  endif

#  include <shlobj.h>

NODISCARD error ovl_dialog_select_folder(void *const hwnd,
                                         NATIVE_CHAR const *const title,
                                         void const *const client_id,
                                         NATIVE_CHAR **const path) {
  error err = eok();
  IFileDialog *pfd = NULL;
  IShellItem *psiResult = NULL;
  PWSTR pszPath = NULL;
  HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&pfd);
  if (FAILED(hr)) {
    err = errhr(hr);
    goto cleanup;
  }
  DWORD dwOptions;
  hr = IFileDialog_GetOptions(pfd, &dwOptions);
  if (FAILED(hr)) {
    err = errhr(hr);
    goto cleanup;
  }
  hr = IFileDialog_SetOptions(pfd, dwOptions | FOS_PICKFOLDERS);
  if (FAILED(hr)) {
    err = errhr(hr);
    goto cleanup;
  }
  hr = IFileDialog_SetTitle(pfd, title);
  if (FAILED(hr)) {
    err = errhr(hr);
    goto cleanup;
  }
  hr = IFileDialog_SetClientGuid(pfd, client_id);
  if (FAILED(hr)) {
    err = errhr(hr);
    goto cleanup;
  }
  hr = IFileDialog_Show(pfd, hwnd);
  if (FAILED(hr)) {
    err = errhr(hr);
    goto cleanup;
  }
  hr = IFileDialog_GetResult(pfd, &psiResult);
  if (FAILED(hr)) {
    err = errhr(hr);
    goto cleanup;
  }
  hr = IShellItem_GetDisplayName(psiResult, SIGDN_FILESYSPATH, &pszPath);
  if (FAILED(hr)) {
    err = errhr(hr);
    goto cleanup;
  }
  err = OV_ARRAY_GROW(path, wcslen(pszPath) + 1);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  wcscpy(*path, pszPath);
cleanup:
  if (pszPath) {
    CoTaskMemFree(pszPath);
    pszPath = NULL;
  }
  if (psiResult) {
    IShellItem_Release(psiResult);
    psiResult = NULL;
  }
  if (pfd) {
    IFileDialog_Release(pfd);
    pfd = NULL;
  }
  return err;
}

#endif // _WIN32
