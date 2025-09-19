#include <ovl/crypto.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

#  include <wincrypt.h>

bool ovl_crypto_random(void *const buffer, size_t const buffer_bytes, struct ov_error *const err) {
  if (!buffer) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  HCRYPTPROV hCryptProv = 0;
  bool result = false;

  if (!CryptAcquireContextW(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }

  if (!CryptGenRandom(hCryptProv, (DWORD)buffer_bytes, (unsigned char *)buffer)) {
    OV_ERROR_SET_HRESULT(err, HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }

  result = true;

cleanup:
  if (hCryptProv) {
    CryptReleaseContext(hCryptProv, 0);
  }
  return result;
}

#else
#  include <fcntl.h>
#  include <unistd.h>

bool ovl_crypto_random(void *const buffer, size_t const buffer_bytes, struct ov_error *const err) {
  if (!buffer) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  int fd = -1;
  bool result = false;
  size_t bytes_read = 0;

  fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1) {
    OV_ERROR_SET_ERRNO(err, errno);
    goto cleanup;
  }

  while (bytes_read < buffer_bytes) {
    ssize_t const read_result = read(fd, (unsigned char *)buffer + bytes_read, buffer_bytes - bytes_read);
    if (read_result == -1) {
      OV_ERROR_SET_ERRNO(err, errno);
      goto cleanup;
    }
    bytes_read += (size_t)read_result;
  }

  result = true;

cleanup:
  if (fd != -1) {
    close(fd);
  }
  return result;
}

#endif
