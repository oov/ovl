#include <ovtest.h>

#ifndef TESTDATADIR
#  define TESTDATADIR NSTR(".")
#endif

#include <ovarray.h>

#include <ovl/file.h>
#include <ovl/path.h>

static void test_file_open_read_close(void) {
  struct ovl_file *file = NULL;
  error err = ovl_file_open(TESTDATADIR NSTR("/test_hello.txt"), &file);
  if (!TEST_SUCCEEDED_F(err)) {
    return;
  }

  char buffer[16] = {0};
  size_t read_bytes = 0;
  err = ovl_file_read(file, buffer, sizeof(buffer) - 1, &read_bytes);
  TEST_SUCCEEDED_F(err);
  TEST_CHECK(read_bytes == 5);
  TEST_CHECK(strcmp(buffer, "hello") == 0);

  ovl_file_close(file);
}

static void test_file_create_write_close(void) {
  wchar_t *temp_path = NULL;
  error err = ovl_path_get_temp_file(&temp_path, NSTR("ovl_test_write.txt"));
  if (!TEST_SUCCEEDED_F(err)) {
    return;
  }

  struct ovl_file *file = NULL;
  err = ovl_file_create(temp_path, &file);
  if (!TEST_SUCCEEDED_F(err)) {
    OV_ARRAY_DESTROY(&temp_path);
    return;
  }

  char const test_data[] = "test write data";
  size_t written_bytes = 0;
  err = ovl_file_write(file, test_data, strlen(test_data), &written_bytes);
  TEST_SUCCEEDED_F(err);
  TEST_CHECK(written_bytes == strlen(test_data));

  ovl_file_close(file);
  file = NULL;

  err = ovl_file_open(temp_path, &file);
  if (!TEST_SUCCEEDED_F(err)) {
    OV_ARRAY_DESTROY(&temp_path);
    return;
  }

  char read_buffer[32] = {0};
  size_t read_bytes = 0;
  err = ovl_file_read(file, read_buffer, sizeof(read_buffer) - 1, &read_bytes);
  TEST_SUCCEEDED_F(err);
  TEST_CHECK(read_bytes == strlen(test_data));
  TEST_CHECK(strcmp(read_buffer, test_data) == 0);

  ovl_file_close(file);
  DeleteFileW(temp_path);
  OV_ARRAY_DESTROY(&temp_path);
}

static void test_file_seek_tell(void) {
  struct ovl_file *file = NULL;
  error err = ovl_file_open(TESTDATADIR NSTR("/test_hello.txt"), &file);
  if (!TEST_SUCCEEDED_F(err)) {
    return;
  }

  int64_t pos = 0;
  err = ovl_file_tell(file, &pos);
  TEST_SUCCEEDED_F(err);
  TEST_CHECK(pos == 0);

  err = ovl_file_seek(file, 2, ovl_file_seek_method_set);
  TEST_SUCCEEDED_F(err);

  err = ovl_file_tell(file, &pos);
  TEST_SUCCEEDED_F(err);
  TEST_CHECK(pos == 2);

  char buffer[4] = {0};
  size_t read_bytes = 0;
  err = ovl_file_read(file, buffer, 3, &read_bytes);
  TEST_SUCCEEDED_F(err);
  TEST_CHECK(read_bytes == 3);
  TEST_CHECK(strcmp(buffer, "llo") == 0);

  err = ovl_file_seek(file, -2, ovl_file_seek_method_cur);
  TEST_SUCCEEDED_F(err);

  err = ovl_file_tell(file, &pos);
  TEST_SUCCEEDED_F(err);
  TEST_CHECK(pos == 3);

  err = ovl_file_seek(file, -1, ovl_file_seek_method_end);
  TEST_SUCCEEDED_F(err);

  err = ovl_file_tell(file, &pos);
  TEST_SUCCEEDED_F(err);
  TEST_CHECK(pos == 4);

  ovl_file_close(file);
}

static void test_file_size(void) {
  struct ovl_file *file = NULL;
  error err = ovl_file_open(TESTDATADIR NSTR("/test_hello.txt"), &file);
  if (!TEST_SUCCEEDED_F(err)) {
    return;
  }

  uint64_t size = 0;
  err = ovl_file_size(file, &size);
  TEST_SUCCEEDED_F(err);
  TEST_CHECK(size == 5);

  ovl_file_close(file);
}

static void test_file_error_handling(void) {
  struct ovl_file *file = NULL;

  error err = ovl_file_open(TESTDATADIR NSTR("/nonexistent_file.txt"), &file);
  TEST_EIS_F(err, err_type_hresult, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  err = ovl_file_create(TESTDATADIR NSTR("/*.txt"), &file);
  TEST_EIS_F(err, err_type_hresult, HRESULT_FROM_WIN32(ERROR_INVALID_NAME));

  err = ovl_file_read(NULL, NULL, 0, NULL);
  TEST_EISG_F(err, err_invalid_arugment);

  err = ovl_file_write(NULL, NULL, 0, NULL);
  TEST_EISG_F(err, err_invalid_arugment);

  size_t dummy = 0;
  err = ovl_file_read(NULL, &dummy, 1, &dummy);
  TEST_EISG_F(err, err_invalid_arugment);

  err = ovl_file_write(NULL, &dummy, 1, &dummy);
  TEST_EISG_F(err, err_invalid_arugment);

  err = ovl_file_open(TESTDATADIR NSTR("/test_hello.txt"), &file);
  if (TEST_SUCCEEDED_F(err)) {
    err = ovl_file_seek(file, -1000, ovl_file_seek_method_set);
    TEST_EIS_F(err, err_type_hresult, HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK));
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
