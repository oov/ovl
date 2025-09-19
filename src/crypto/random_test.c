#include <ovtest.h>

#include <ovl/crypto.h>

static void test_ovl_crypto_random(void) {
  unsigned char buffer[32];
  struct ov_error err = {0};

  TEST_CHECK(ovl_crypto_random(buffer, sizeof(buffer), &err));
  TEST_CHECK(err.stack[0].info.type == ov_error_type_invalid);

  bool all_zero = true;
  for (size_t i = 0; i < sizeof(buffer); ++i) {
    if (buffer[i] != 0) {
      all_zero = false;
      break;
    }
  }
  TEST_CHECK(!all_zero);

  OV_ERROR_DESTROY(&err);
}

static void test_ovl_crypto_random_null_args(void) {
  struct ov_error err = {0};

  TEST_CHECK(!ovl_crypto_random(NULL, 32, &err));
  TEST_CHECK(err.stack[0].info.type == ov_error_type_generic);
  TEST_CHECK(err.stack[0].info.code == ov_error_generic_invalid_argument);

  OV_ERROR_DESTROY(&err);
}

static void test_ovl_crypto_random_zero_size(void) {
  unsigned char buffer[1];
  struct ov_error err = {0};

  TEST_CHECK(ovl_crypto_random(buffer, 0, &err));
  TEST_CHECK(err.stack[0].info.type == ov_error_type_invalid);

  OV_ERROR_DESTROY(&err);
}

static void test_ovl_crypto_random_large_buffer(void) {
  enum { large_size = 4096 };
  unsigned char buffer[large_size];
  struct ov_error err = {0};

  TEST_CHECK(ovl_crypto_random(buffer, sizeof(buffer), &err));
  TEST_CHECK(err.stack[0].info.type == ov_error_type_invalid);

  bool all_zero = true;
  for (size_t i = 0; i < sizeof(buffer); ++i) {
    if (buffer[i] != 0) {
      all_zero = false;
      break;
    }
  }
  TEST_CHECK(!all_zero);

  OV_ERROR_DESTROY(&err);
}

TEST_LIST = {
    {"ovl_crypto_random", test_ovl_crypto_random},
    {"ovl_crypto_random_null_args", test_ovl_crypto_random_null_args},
    {"ovl_crypto_random_zero_size", test_ovl_crypto_random_zero_size},
    {"ovl_crypto_random_large_buffer", test_ovl_crypto_random_large_buffer},
    {NULL, NULL},
};
