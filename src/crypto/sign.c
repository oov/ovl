#include <ovl/crypto.h>

#include <ed25519.h>
#include <edsign.h>

bool ovl_crypto_sign_generate_keypair(uint8_t public_key[ovl_crypto_sign_publickey_size],
                                      uint8_t secret_key[ovl_crypto_sign_secretkey_size],
                                      struct ov_error *const err) {
  if (!public_key || !secret_key) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  bool result = false;

  if (!ovl_crypto_random(secret_key, ovl_crypto_sign_secretkey_size, err)) {
    OV_ERROR_ADD_TRACE(err);
    goto cleanup;
  }
  ed25519_prepare(secret_key);
  edsign_sec_to_pub(public_key, secret_key);
  result = true;

cleanup:
  return result;
}

bool ovl_crypto_sign(uint8_t signature[ovl_crypto_sign_signature_size],
                     void const *const message,
                     size_t const message_bytes,
                     uint8_t const secret_key[ovl_crypto_sign_secretkey_size],
                     struct ov_error *const err) {
  if (!signature || !message || !secret_key) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  uint8_t public_key[ovl_crypto_sign_publickey_size];
  edsign_sec_to_pub(public_key, secret_key);
  edsign_sign(signature, public_key, secret_key, (uint8_t const *)message, message_bytes);
  return true;
}

bool ovl_crypto_sign_verify(uint8_t const signature[ovl_crypto_sign_signature_size],
                            void const *const message,
                            size_t const message_bytes,
                            uint8_t const public_key[ovl_crypto_sign_publickey_size],
                            struct ov_error *const err) {
  if (!signature || !message || !public_key) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_invalid_argument);
    return false;
  }

  bool result = false;
  if (!edsign_verify(signature, public_key, (uint8_t const *)message, message_bytes)) {
    OV_ERROR_SET_GENERIC(err, ov_error_generic_fail);
    goto cleanup;
  }
  result = true;

cleanup:
  return result;
}
