#include <ovl/file.h>

#include <ovarray.h>
#include <ovl/path.h>

#ifdef _WIN32

NODISCARD error ovl_file_create_temp(NATIVE_CHAR const *const base_name,
                                     struct ovl_file **const file,
                                     NATIVE_CHAR **const created_path) {
  if (!file || *file || !created_path || *created_path) {
    return errg(err_invalid_arugment);
  }

  NATIVE_CHAR *temp_dir = NULL;
  error err = ovl_path_get_temp_directory(&temp_dir);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  err = ovl_file_create_unique(temp_dir, base_name, file, created_path);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

cleanup:
  if (temp_dir) {
    OV_ARRAY_DESTROY(&temp_dir);
  }
  return err;
}

#endif
