#include <ovtest.h>

#include <ovarray.h>

#include <ovl/path.h>

static void test_path_get_temp_file(void) {
  wchar_t *path = NULL;
  error err = ovl_path_get_temp_file(NULL, NSTR("test"));
  TEST_EISG_F(err, err_invalid_arugment);
  err = ovl_path_get_temp_file(&path, NSTR(""));
  if (TEST_SUCCEEDED_F(err)) {
    TEST_CHECK(path != NULL);
    wchar_t golden[MAX_PATH * 2];
    GetTempPathW(MAX_PATH + 1, golden);
    TEST_CHECK(wcscmp(path, golden) == 0);
    OV_ARRAY_DESTROY(&path);
  }
  err = ovl_path_get_temp_file(&path, NSTR("hello.txt"));
  if (TEST_SUCCEEDED_F(err)) {
    TEST_CHECK(path != NULL);
    wchar_t golden[MAX_PATH * 2];
    GetTempPathW(MAX_PATH + 1, golden);
    wcscat(golden, NSTR("hello.txt"));
    TEST_CHECK(wcscmp(path, golden) == 0);
    OV_ARRAY_DESTROY(&path);
  }
}

static void test_path_extract_file_name(void) {
  NATIVE_CHAR const path1[] = NSTR("C:\\Users\\test\\hello_world.txt");
#ifdef WIN32
  TEST_CHECK(ovl_path_extract_file_name(path1) == wcsrchr(path1, L'\\') + 1);
#else
  TEST_CHECK(ovl_path_extract_file_name(path1) == strrchr(path1, L'\\') + 1);
#endif
}

TEST_LIST = {
    {"test_path_get_temp_file", test_path_get_temp_file},
    {"test_path_extract_file_name", test_path_extract_file_name},
    {NULL, NULL},
};
