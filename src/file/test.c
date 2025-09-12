#include <ovtest.h>

#ifndef TESTDATADIR
#  define TESTDATADIR NSTR(".")
#endif

#include <ovarray.h>

#include <ovl/file.h>
#include <ovl/path.h>

static void test_file_open_read_close(void) {
  struct ovl_file *file = NULL;
  struct ov_error err = {0};
  if (!ovl_file_open(TESTDATADIR NSTR("/test_hello.txt"), &file, &err)) {
    OV_ERROR_DESTROY(&err);
    return;
  }

  char buffer[16] = {0};
  size_t read_bytes = 0;
  bool ok = ovl_file_read(file, buffer, sizeof(buffer) - 1, &read_bytes, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }
  TEST_CHECK(read_bytes == 5);
  TEST_CHECK(strcmp(buffer, "hello") == 0);

  ovl_file_close(file);
}

static void test_file_create_write_close(void) {
  wchar_t *temp_path = NULL;
  struct ovl_file *file = NULL;
  struct ov_error err = {0};
  if (!ovl_file_create_temp(NSTR("ovl_test_write.txt"), &file, &temp_path, &err)) {
    OV_ERROR_DESTROY(&err);
    return;
  }

  char const test_data[] = "test write data";
  size_t written_bytes = 0;
  bool ok = ovl_file_write(file, test_data, strlen(test_data), &written_bytes, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }
  TEST_CHECK(written_bytes == strlen(test_data));

  ovl_file_close(file);
  file = NULL;

  ok = ovl_file_open(temp_path, &file, &err);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
    OV_ARRAY_DESTROY(&temp_path);
    return;
  }

  char read_buffer[32] = {0};
  size_t read_bytes = 0;
  ok = ovl_file_read(file, read_buffer, sizeof(read_buffer) - 1, &read_bytes, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }
  TEST_CHECK(read_bytes == strlen(test_data));
  TEST_CHECK(strcmp(read_buffer, test_data) == 0);

  ovl_file_close(file);
  DeleteFileW(temp_path);
  OV_ARRAY_DESTROY(&temp_path);
}

static void test_file_seek_tell(void) {
  struct ovl_file *file = NULL;
  struct ov_error err = {0};
  if (!ovl_file_open(TESTDATADIR NSTR("/test_hello.txt"), &file, &err)) {
    OV_ERROR_DESTROY(&err);
    return;
  }

  int64_t pos = 0;
  bool ok = ovl_file_tell(file, &pos, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }
  TEST_CHECK(pos == 0);

  ok = ovl_file_seek(file, 2, ovl_file_seek_method_set, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }

  ok = ovl_file_tell(file, &pos, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }
  TEST_CHECK(pos == 2);

  char buffer[4] = {0};
  size_t read_bytes = 0;
  ok = ovl_file_read(file, buffer, 3, &read_bytes, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }
  TEST_CHECK(read_bytes == 3);
  TEST_CHECK(strcmp(buffer, "llo") == 0);

  ok = ovl_file_seek(file, -2, ovl_file_seek_method_cur, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }

  ok = ovl_file_tell(file, &pos, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }
  TEST_CHECK(pos == 3);

  ok = ovl_file_seek(file, -1, ovl_file_seek_method_end, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }

  ok = ovl_file_tell(file, &pos, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }
  TEST_CHECK(pos == 4);

  ovl_file_close(file);
}

static void test_file_size(void) {
  struct ovl_file *file = NULL;
  struct ov_error err = {0};
  if (!ovl_file_open(TESTDATADIR NSTR("/test_hello.txt"), &file, &err)) {
    OV_ERROR_DESTROY(&err);
    return;
  }

  uint64_t size = 0;
  bool ok = ovl_file_size(file, &size, &err);
  TEST_CHECK(ok);
  if (!ok) {
    OV_ERROR_DESTROY(&err);
  }
  TEST_CHECK(size == 5);

  ovl_file_close(file);
}

static void test_file_error_handling(void) {
  struct ovl_file *file = NULL;
  struct ov_error err = {0};

  bool ok = ovl_file_open(TESTDATADIR NSTR("/nonexistent_file.txt"), &file, &err);
  TEST_CHECK(!ok);
  TEST_CHECK(ov_error_is(&err, ov_error_type_hresult, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)));
  OV_ERROR_DESTROY(&err);

  ok = ovl_file_create(TESTDATADIR NSTR("/*.txt"), &file, &err);
  TEST_CHECK(!ok);
  TEST_CHECK(ov_error_is(&err, ov_error_type_hresult, HRESULT_FROM_WIN32(ERROR_INVALID_NAME)));
  OV_ERROR_DESTROY(&err);

  size_t dummy = 0;
  ok = ovl_file_read(NULL, NULL, 0, NULL, &err);
  TEST_CHECK(!ok);
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);

  ok = ovl_file_write(NULL, NULL, 0, NULL, &err);
  TEST_CHECK(!ok);
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);

  ok = ovl_file_read(NULL, &dummy, 1, &dummy, &err);
  TEST_CHECK(!ok);
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);

  ok = ovl_file_write(NULL, &dummy, 1, &dummy, &err);
  TEST_CHECK(!ok);
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);

  ok = ovl_file_open(TESTDATADIR NSTR("/test_hello.txt"), &file, &err);
  if (ok) {
    ok = ovl_file_seek(file, -1000, ovl_file_seek_method_set, &err);
    TEST_CHECK(!ok);
    TEST_CHECK(ov_error_is(&err, ov_error_type_hresult, HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK)));
    OV_ERROR_DESTROY(&err);
    ovl_file_close(file);
  }
}

TEST_LIST = {
    {"test_file_open_read_close", test_file_open_read_close},
    {"test_file_create_write_close", test_file_create_write_close},
    {"test_file_seek_tell", test_file_seek_tell},
    {"test_file_size", test_file_size},
    {"test_file_error_handling", test_file_error_handling},
    {NULL, NULL},
};
