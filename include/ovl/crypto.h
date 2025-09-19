#pragma once

#include <ovbase.h>

enum {
  ovl_crypto_sign_publickey_size = 32,
  ovl_crypto_sign_secretkey_size = 32,
  ovl_crypto_sign_signature_size = 64,
};

NODISCARD bool ovl_crypto_sign_generate_keypair(uint8_t public_key[ovl_crypto_sign_publickey_size],
                                                uint8_t secret_key[ovl_crypto_sign_secretkey_size],
                                                struct ov_error *const err);

NODISCARD bool ovl_crypto_sign(uint8_t signature[ovl_crypto_sign_signature_size],
                               void const *const message,
                               size_t const message_bytes,
                               uint8_t const secret_key[ovl_crypto_sign_secretkey_size],
                               struct ov_error *const err);

NODISCARD bool ovl_crypto_sign_verify(uint8_t const signature[ovl_crypto_sign_signature_size],
                                      void const *const message,
                                      size_t const message_bytes,
                                      uint8_t const public_key[ovl_crypto_sign_publickey_size],
                                      struct ov_error *const err);

NODISCARD bool ovl_crypto_random(void *const buffer, size_t const buffer_bytes, struct ov_error *const err);
