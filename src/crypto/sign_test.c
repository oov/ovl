#include <ovtest.h>

#include <ovl/crypto.h>
#include <string.h>

static void test_ovl_crypto_sign_generate_keypair(void) {
  uint8_t public_key[ovl_crypto_sign_publickey_size];
  uint8_t secret_key[ovl_crypto_sign_secretkey_size];
  struct ov_error err = {0};

  TEST_CHECK(ovl_crypto_sign_generate_keypair(public_key, secret_key, &err));

  bool all_zero_public = true;
  bool all_zero_secret = true;
  for (size_t i = 0; i < ovl_crypto_sign_publickey_size; ++i) {
    if (public_key[i] != 0) {
      all_zero_public = false;
      break;
    }
  }
  for (size_t i = 0; i < ovl_crypto_sign_secretkey_size; ++i) {
    if (secret_key[i] != 0) {
      all_zero_secret = false;
      break;
    }
  }

  TEST_CHECK(!all_zero_public);
  TEST_CHECK(!all_zero_secret);
}

static void test_ovl_crypto_keypair_generate_null_args(void) {
  uint8_t public_key[ovl_crypto_sign_publickey_size];
  uint8_t secret_key[ovl_crypto_sign_secretkey_size];
  struct ov_error err = {0};

  TEST_CHECK(!ovl_crypto_sign_generate_keypair(NULL, secret_key, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);

  TEST_CHECK(!ovl_crypto_sign_generate_keypair(public_key, NULL, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));

  OV_ERROR_DESTROY(&err);
}

static void test_ovl_crypto_sign_and_verify(void) {
  uint8_t public_key[ovl_crypto_sign_publickey_size];
  uint8_t secret_key[ovl_crypto_sign_secretkey_size];
  struct ov_error err = {0};

  TEST_CHECK(ovl_crypto_sign_generate_keypair(public_key, secret_key, &err));

  uint8_t const message[] = "Hello, Ed25519!";
  size_t const message_bytes = sizeof(message) - 1;

  uint8_t signature[ovl_crypto_sign_signature_size];

  TEST_CHECK(ovl_crypto_sign(signature, message, message_bytes, secret_key, &err));
  TEST_CHECK(ovl_crypto_sign_verify(signature, message, message_bytes, public_key, &err));
}

static void test_ovl_crypto_sign_verify_wrong_key(void) {
  uint8_t public_key1[ovl_crypto_sign_publickey_size];
  uint8_t secret_key1[ovl_crypto_sign_secretkey_size];
  uint8_t public_key2[ovl_crypto_sign_publickey_size];
  uint8_t secret_key2[ovl_crypto_sign_secretkey_size];
  struct ov_error err = {0};

  TEST_CHECK(ovl_crypto_sign_generate_keypair(public_key1, secret_key1, &err));
  TEST_CHECK(ovl_crypto_sign_generate_keypair(public_key2, secret_key2, &err));

  uint8_t const message[] = "Hello, Ed25519!";
  size_t const message_bytes = sizeof(message) - 1;

  uint8_t signature[ovl_crypto_sign_signature_size];

  TEST_CHECK(ovl_crypto_sign(signature, message, message_bytes, secret_key1, &err));

  TEST_CHECK(!ovl_crypto_sign_verify(signature, message, message_bytes, public_key2, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_fail));

  OV_ERROR_DESTROY(&err);
}

static void test_ovl_crypto_sign_verify_wrong_message(void) {
  uint8_t public_key[ovl_crypto_sign_publickey_size];
  uint8_t secret_key[ovl_crypto_sign_secretkey_size];
  struct ov_error err = {0};

  TEST_CHECK(ovl_crypto_sign_generate_keypair(public_key, secret_key, &err));

  uint8_t const message1[] = "Hello, sign!";
  uint8_t const message2[] = "Hello, World!";
  size_t const message_bytes = sizeof(message1) - 1;

  uint8_t signature[ovl_crypto_sign_signature_size];

  TEST_CHECK(ovl_crypto_sign(signature, message1, message_bytes, secret_key, &err));

  TEST_CHECK(!ovl_crypto_sign_verify(signature, message2, message_bytes, public_key, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_fail));

  OV_ERROR_DESTROY(&err);
}

static void test_ovl_crypto_sign_null_args(void) {
  uint8_t public_key[ovl_crypto_sign_publickey_size];
  uint8_t secret_key[ovl_crypto_sign_secretkey_size];
  struct ov_error err = {0};
  uint8_t signature[ovl_crypto_sign_signature_size];
  uint8_t const message[] = "test";

  TEST_CHECK(ovl_crypto_sign_generate_keypair(public_key, secret_key, &err));

  TEST_CHECK(!ovl_crypto_sign(NULL, message, sizeof(message) - 1, secret_key, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);

  TEST_CHECK(!ovl_crypto_sign(signature, NULL, sizeof(message) - 1, secret_key, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);

  TEST_CHECK(!ovl_crypto_sign(signature, message, sizeof(message) - 1, NULL, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);
}

static void test_ovl_crypto_verify_null_args(void) {
  uint8_t public_key[ovl_crypto_sign_publickey_size];
  uint8_t secret_key[ovl_crypto_sign_secretkey_size];
  struct ov_error err = {0};
  uint8_t signature[ovl_crypto_sign_signature_size];
  uint8_t const message[] = "test";

  TEST_CHECK(ovl_crypto_sign_generate_keypair(public_key, secret_key, &err));

  TEST_CHECK(!ovl_crypto_sign_verify(NULL, message, sizeof(message) - 1, public_key, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);

  TEST_CHECK(!ovl_crypto_sign_verify(signature, NULL, sizeof(message) - 1, public_key, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);

  TEST_CHECK(!ovl_crypto_sign_verify(signature, message, sizeof(message) - 1, NULL, &err));
  TEST_CHECK(ov_error_is(&err, ov_error_type_generic, ov_error_generic_invalid_argument));
  OV_ERROR_DESTROY(&err);
}

static void test_ovl_crypto_sign_large_message(void) {
  uint8_t public_key[ovl_crypto_sign_publickey_size];
  uint8_t secret_key[ovl_crypto_sign_secretkey_size];
  struct ov_error err = {0};

  TEST_CHECK(ovl_crypto_sign_generate_keypair(public_key, secret_key, &err));

  enum { large_message_size = 10000 };
  uint8_t large_message[large_message_size];
  for (size_t i = 0; i < large_message_size; ++i) {
    large_message[i] = (uint8_t)(i % 256);
  }

  uint8_t signature[ovl_crypto_sign_signature_size];

  TEST_CHECK(ovl_crypto_sign(signature, large_message, large_message_size, secret_key, &err));

  TEST_CHECK(ovl_crypto_sign_verify(signature, large_message, large_message_size, public_key, &err));
}

TEST_LIST = {
    {"ovl_crypto_sign_generate_keypair", test_ovl_crypto_sign_generate_keypair},
    {"ovl_crypto_keypair_generate_null_args", test_ovl_crypto_keypair_generate_null_args},
    {"ovl_crypto_sign_and_verify", test_ovl_crypto_sign_and_verify},
    {"ovl_crypto_sign_verify_wrong_key", test_ovl_crypto_sign_verify_wrong_key},
    {"ovl_crypto_sign_verify_wrong_message", test_ovl_crypto_sign_verify_wrong_message},
    {"ovl_crypto_sign_null_args", test_ovl_crypto_sign_null_args},
    {"ovl_crypto_verify_null_args", test_ovl_crypto_verify_null_args},
    {"ovl_crypto_sign_large_message", test_ovl_crypto_sign_large_message},
    {NULL, NULL},
};
