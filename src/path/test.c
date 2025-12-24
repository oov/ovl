#include <ovtest.h>

#include <ovarray.h>

#include <ovl/file.h>
#include <ovl/path.h>

static void test_file_create_temp(void) {
  struct ovl_file *file = NULL;
  wchar_t *path = NULL;
  struct ov_error err = {0};
  if (!TEST_SUCCEEDED(ovl_file_create_temp(NSTR("test.txt"), &file, &path, &err), &err)) {
    goto cleanup;
  }
  TEST_CHECK(file != NULL);
  TEST_CHECK(path != NULL);
  TEST_CHECK(wcsstr(path, NSTR("test_")) != NULL);
  TEST_CHECK(wcsstr(path, NSTR(".txt")) != NULL);
  TEST_MSG("%ls", path);

cleanup:
  if (file) {
    ovl_file_close(file);
  }
  if (path) {
    DeleteFileW(path);
    OV_ARRAY_DESTROY(&path);
  }
}

static void test_path_extract_file_name(void) {
  {
    NATIVE_CHAR const path[] = NSTR("C:\\Users\\test\\hello_world.txt");
#ifdef WIN32
    TEST_CHECK(ovl_path_extract_file_name(path) == wcsrchr(path, L'\\') + 1);
#else
    TEST_CHECK(ovl_path_extract_file_name(path) == strrchr(path, L'\\') + 1);
#endif
  }
  {
    char const path[] = "C:\\Users/test\\hello_world.txt";
    TEST_CHECK(ovl_path_extract_file_name(path) == strrchr(path, '\\') + 1);
  }
  {
    char const path[] = "C:\\Users\\test/hello_world.txt";
    TEST_CHECK(ovl_path_extract_file_name(path) == strrchr(path, '/') + 1);
  }
  {
    wchar_t const path[] = L"C:\\Users/test\\hello_world.txt";
    TEST_CHECK(ovl_path_extract_file_name(path) == wcsrchr(path, L'\\') + 1);
  }
  {
    wchar_t const path[] = L"C:\\Users\\test/hello_world.txt";
    TEST_CHECK(ovl_path_extract_file_name(path) == wcsrchr(path, L'/') + 1);
  }
}

static void test_path_find_ext(void) {
  // Normal case: file with extension
  NATIVE_CHAR const path1[] = NSTR("C:\\Users\\test\\hello_world.txt");
  NATIVE_CHAR const *ext1 = ovl_path_find_ext(path1);
  TEST_CHECK(ext1 != NULL);
#ifdef WIN32
  TEST_CHECK(wcscmp(ext1, L".txt") == 0);
#else
  TEST_CHECK(strcmp(ext1, ".txt") == 0);
#endif

  // Case with dot in directory name (should not falsely detect)
  NATIVE_CHAR const path2[] = NSTR("/home/user/temp.dir/LICENSE");
  NATIVE_CHAR const *ext2 = ovl_path_find_ext(path2);
  TEST_CHECK(ext2 == NULL);

  // Case with dot in directory name but file also has extension
  NATIVE_CHAR const path3[] = NSTR("/home/user/temp.dir/readme.md");
  NATIVE_CHAR const *ext3 = ovl_path_find_ext(path3);
  TEST_CHECK(ext3 != NULL);
#ifdef WIN32
  TEST_CHECK(wcscmp(ext3, L".md") == 0);
#else
  TEST_CHECK(strcmp(ext3, ".md") == 0);
#endif

  // File starting with dot (hidden file in Unix)
  NATIVE_CHAR const path4[] = NSTR("/home/user/.gitignore");
  NATIVE_CHAR const *ext4 = ovl_path_find_ext(path4);
  TEST_CHECK(ext4 != NULL);
#ifdef WIN32
  TEST_CHECK(wcscmp(ext4, L".gitignore") == 0);
#else
  TEST_CHECK(strcmp(ext4, ".gitignore") == 0);
#endif

  // Multiple dots in filename
  NATIVE_CHAR const path5[] = NSTR("C:\\test\\file.tar.gz");
  NATIVE_CHAR const *ext5 = ovl_path_find_ext(path5);
  TEST_CHECK(ext5 != NULL);
#ifdef WIN32
  TEST_CHECK(wcscmp(ext5, L".gz") == 0);
#else
  TEST_CHECK(strcmp(ext5, ".gz") == 0);
#endif

  // No extension
  NATIVE_CHAR const path6[] = NSTR("C:\\test\\Makefile");
  NATIVE_CHAR const *ext6 = ovl_path_find_ext(path6);
  TEST_CHECK(ext6 == NULL);

  // NULL input
  NATIVE_CHAR const *ext7 = ovl_path_find_ext((NATIVE_CHAR const *)NULL);
  TEST_CHECK(ext7 == NULL);
}

static void test_path_is_same_ext(void) {
  // wchar_t
  TEST_CHECK(ovl_path_is_same_ext(L".TXT", L".txt") == true);
  TEST_CHECK(ovl_path_is_same_ext(L".txt", L".TXT") == true);
  TEST_CHECK(ovl_path_is_same_ext(L".doc", L".txt") == false);
  TEST_CHECK(ovl_path_is_same_ext((wchar_t const *)NULL, L".txt") == false);
  TEST_CHECK(ovl_path_is_same_ext(L".txt", (wchar_t const *)NULL) == false);
  TEST_CHECK(ovl_path_is_same_ext(L".txt", L".txt") == true);
  TEST_CHECK(ovl_path_is_same_ext(L".txt", L".tx") == false);
  TEST_CHECK(ovl_path_is_same_ext(L".tx", L".txt") == false);

  // char
  TEST_CHECK(ovl_path_is_same_ext(".TXT", ".txt") == true);
  TEST_CHECK(ovl_path_is_same_ext(".txt", ".TXT") == true);
  TEST_CHECK(ovl_path_is_same_ext(".doc", ".txt") == false);
  TEST_CHECK(ovl_path_is_same_ext((char const *)NULL, ".txt") == false);
  TEST_CHECK(ovl_path_is_same_ext(".txt", (char const *)NULL) == false);
  TEST_CHECK(ovl_path_is_same_ext(".txt", ".txt") == true);
  TEST_CHECK(ovl_path_is_same_ext(".txt", ".tx") == false);
  TEST_CHECK(ovl_path_is_same_ext(".tx", ".txt") == false);
}

TEST_LIST = {
    {"test_file_create_temp", test_file_create_temp},
    {"test_path_extract_file_name", test_path_extract_file_name},
    {"test_path_find_ext", test_path_find_ext},
    {"test_path_is_same_ext", test_path_is_same_ext},
    {NULL, NULL},
};
