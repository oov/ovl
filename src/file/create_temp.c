#include <ovl/file.h>

#include <ovarray.h>
#include <ovl/path.h>

#ifdef _WIN32

NODISCARD bool ovl_file_create_temp(NATIVE_CHAR const *const base_name,
                                    struct ovl_file **const file,
                                    NATIVE_CHAR **const created_path,
                                    struct ov_error *const err) {
  if (!file || *file || !created_path || *created_path) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  NATIVE_CHAR *temp_dir = NULL;
  bool result = false;

  if (!ovl_path_get_temp_directory(&temp_dir, err)) {
    OV_ERROR_ADD_TRACE(err);
    goto cleanup;
  }

  if (!ovl_file_create_unique(temp_dir, base_name, file, created_path, err)) {
    OV_ERROR_ADD_TRACE(err);
    goto cleanup;
  }

  result = true;

cleanup:
  if (temp_dir) {
    OV_ARRAY_DESTROY(&temp_dir);
  }
  return result;
}

#endif
