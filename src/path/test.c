#include <ovtest.h>

#include <ovarray.h>

#include <ovl/file.h>
#include <ovl/path.h>

static void test_file_create_temp(void) {
  struct ovl_file *file = NULL;
  wchar_t *path = NULL;
  error err = ovl_file_create_temp(NSTR("test.txt"), &file, &path);
  if (TEST_SUCCEEDED_F(err)) {
    TEST_CHECK(file != NULL);
    TEST_CHECK(path != NULL);
    TEST_CHECK(wcsstr(path, NSTR("test_")) != NULL);
    TEST_CHECK(wcsstr(path, NSTR(".txt")) != NULL);
    TEST_MSG("%ls", path);
    ovl_file_close(file);
    DeleteFileW(path);
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
    {"test_file_create_temp", test_file_create_temp},
    {"test_path_extract_file_name", test_path_extract_file_name},
    {NULL, NULL},
};
