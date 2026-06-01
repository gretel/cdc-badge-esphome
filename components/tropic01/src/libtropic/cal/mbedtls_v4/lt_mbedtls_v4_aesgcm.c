/**
 * @file lt_mbedtls_v4_aesgcm.c
 * @copyright Copyright (c) 2020-2025 Tropic Square s.r.o.
 *
 * @license For the license see file LICENSE.txt file in the root directory of this source tree.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#include "psa/crypto.h"
#pragma GCC diagnostic pop
#include "libtropic_common.h"
#include "libtropic_logging.h"
#include "libtropic_mbedtls_v4.h"
#include "lt_aesgcm.h"

/**
 * @brief Initializes MbedTLS AES-GCM context.
 *
 * @param ctx      AES-GCM context structure (MbedTLS specific)
 * @param key      Key to initialize with
 * @param key_len  Length of the key
 * @return         LT_OK if success, otherwise returns other error code.
 */
static lt_ret_t lt_aesgcm_init(lt_aesgcm_ctx_mbedtls_v4_t *ctx, const uint8_t *key, const uint32_t key_len)
{
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

    if (ctx->key_set) {
        LT_LOG_ERROR("AES-GCM context already initialized!");
        return LT_CRYPTO_ERR;
    }

    // Set up key attributes
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_GCM);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, key_len * 8);

    // Import the key
    status = psa_import_key(&attributes, key, key_len, &ctx->key_id);
    psa_reset_key_attributes(&attributes);

    if (status != PSA_SUCCESS) {
        LT_LOG_ERROR("Couldn't import AES-GCM key, status=%" PRId32 " (psa_status_t)", status);
        return LT_CRYPTO_ERR;
    }

    ctx->key_set = 1;
    return LT_OK;
}

/**
 * @brief Deinitializes MbedTLS AES-GCM context.
 *
 * @param ctx  AES-GCM context structure (MbedTLS specific)
 * @return     LT_OK if success, otherwise returns other error code.
 */
static lt_ret_t lt_aesgcm_deinit(lt_aesgcm_ctx_mbedtls_v4_t *ctx)
{
    if (ctx->key_set) {
        psa_status_t status = psa_destroy_key(ctx->key_id);
        if (status != PSA_SUCCESS) {
            LT_LOG_ERROR("Failed to destroy AES-GCM key, status=%" PRId32 " (psa_status_t)", status);
            return LT_CRYPTO_ERR;
        }
        ctx->key_set = 0;
    }
    return LT_OK;
}

lt_ret_t lt_aesgcm_encrypt_init(void *ctx, const uint8_t *key, const uint32_t key_len)
{
    lt_ctx_mbedtls_v4_t *_ctx = (lt_ctx_mbedtls_v4_t *)ctx;

    return lt_aesgcm_init(&_ctx->aesgcm_encrypt_ctx, key, key_len);
}

lt_ret_t lt_aesgcm_decrypt_init(void *ctx, const uint8_t *key, const uint32_t key_len)
{
    lt_ctx_mbedtls_v4_t *_ctx = (lt_ctx_mbedtls_v4_t *)ctx;

    return lt_aesgcm_init(&_ctx->aesgcm_decrypt_ctx, key, key_len);
}

lt_ret_t lt_aesgcm_encrypt(void *ctx, const uint8_t *iv, const uint32_t iv_len, const uint8_t *add,
                           const uint32_t add_len, const uint8_t *plaintext, const uint32_t plaintext_len,
                           uint8_t *ciphertext, const uint32_t ciphertext_len)
{
    lt_ctx_mbedtls_v4_t *_ctx = (lt_ctx_mbedtls_v4_t *)ctx;
    psa_status_t status;
    size_t resulting_length;

    if (ciphertext_len < PSA_AEAD_ENCRYPT_OUTPUT_SIZE(PSA_KEY_TYPE_AES, PSA_ALG_GCM, plaintext_len)) {
        LT_LOG_ERROR("AES-GCM output (ciphertext) buffer too small! Current: %" PRIu32 " bytes, required: %" PRIu32
                     " bytes",
                     ciphertext_len, PSA_AEAD_ENCRYPT_OUTPUT_SIZE(PSA_KEY_TYPE_AES, PSA_ALG_GCM, plaintext_len));
        return LT_PARAM_ERR;
    }

    if (!_ctx->aesgcm_encrypt_ctx.key_set) {
        LT_LOG_ERROR("AES-GCM context key not set!");
        return LT_CRYPTO_ERR;
    }

    // PSA AEAD encrypt operation
    status = psa_aead_encrypt(_ctx->aesgcm_encrypt_ctx.key_id, PSA_ALG_GCM, iv, iv_len, add, add_len, plaintext,
                              plaintext_len, ciphertext, ciphertext_len, &resulting_length);

    if (status != PSA_SUCCESS) {
        LT_LOG_ERROR("AES-GCM encryption failed, status=%" PRId32 " (psa_status_t)", status);
        return LT_CRYPTO_ERR;
    }

    if (resulting_length != PSA_AEAD_ENCRYPT_OUTPUT_SIZE(PSA_KEY_TYPE_AES, PSA_ALG_GCM, plaintext_len)) {
        LT_LOG_ERROR("AES-GCM encryption output length mismatch! Current: %zu bytes, expected: %" PRIu32 " bytes",
                     resulting_length, PSA_AEAD_ENCRYPT_OUTPUT_SIZE(PSA_KEY_TYPE_AES, PSA_ALG_GCM, plaintext_len));
        return LT_CRYPTO_ERR;
    }

    return LT_OK;
}

lt_ret_t lt_aesgcm_decrypt(void *ctx, const uint8_t *iv, const uint32_t iv_len, const uint8_t *add,
                           const uint32_t add_len, const uint8_t *ciphertext, const uint32_t ciphertext_len,
                           uint8_t *plaintext, const uint32_t plaintext_len)
{
    lt_ctx_mbedtls_v4_t *_ctx = (lt_ctx_mbedtls_v4_t *)ctx;
    psa_status_t status;
    size_t resulting_length;

    // DEBUG: Log all parameters
    LT_LOG_DEBUG("lt_aesgcm_decrypt: iv=%p iv_len=%" PRIu32 " add=%p add_len=%" PRIu32,
                 (void*)iv, iv_len, (void*)add, add_len);
    LT_LOG_DEBUG("lt_aesgcm_decrypt: ciphertext=%p ciphertext_len=%" PRIu32 " plaintext=%p plaintext_len=%" PRIu32,
                 (void*)ciphertext, ciphertext_len, (void*)plaintext, plaintext_len);
    LT_LOG_DEBUG("lt_aesgcm_decrypt: PSA_AEAD_DECRYPT_OUTPUT_SIZE=%" PRIu32,
                 (uint32_t)PSA_AEAD_DECRYPT_OUTPUT_SIZE(PSA_KEY_TYPE_AES, PSA_ALG_GCM, ciphertext_len));

    if (plaintext_len < PSA_AEAD_DECRYPT_OUTPUT_SIZE(PSA_KEY_TYPE_AES, PSA_ALG_GCM, ciphertext_len)) {
        LT_LOG_ERROR("AES-GCM output (plaintext) buffer too small! Current: %" PRIu32 " bytes, required: %" PRIu32
                     " bytes",
                     plaintext_len, PSA_AEAD_DECRYPT_OUTPUT_SIZE(PSA_KEY_TYPE_AES, PSA_ALG_GCM, ciphertext_len));
        return LT_PARAM_ERR;
    }

    if (!_ctx->aesgcm_decrypt_ctx.key_set) {
        LT_LOG_ERROR("AES-GCM context key not set!");
        return LT_CRYPTO_ERR;
    }

    LT_LOG_DEBUG("lt_aesgcm_decrypt: calling psa_aead_decrypt with key_id=%" PRIu32, (uint32_t)_ctx->aesgcm_decrypt_ctx.key_id);

    // ESP-IDF workaround: Hardware GCM requires valid non-NULL buffer even for zero-length plaintext.
    // If plaintext pointer is NULL or points to empty string literal, use a dummy buffer.
    uint8_t dummy_buf[16];
    uint8_t *actual_plaintext = plaintext;
    size_t actual_plaintext_len = plaintext_len;

    // Check if we need the workaround (plaintext is NULL, empty string literal, or length suggests no data)
    uint32_t expected_output = PSA_AEAD_DECRYPT_OUTPUT_SIZE(PSA_KEY_TYPE_AES, PSA_ALG_GCM, ciphertext_len);
    if (expected_output == 0 || plaintext == NULL || plaintext == (uint8_t*)"") {
        actual_plaintext = dummy_buf;
        actual_plaintext_len = sizeof(dummy_buf);
        LT_LOG_DEBUG("lt_aesgcm_decrypt: using dummy buffer workaround for ESP-IDF GCM");
    }

    // PSA AEAD decrypt operation
    status = psa_aead_decrypt(_ctx->aesgcm_decrypt_ctx.key_id, PSA_ALG_GCM, iv, iv_len, add, add_len, ciphertext,
                              ciphertext_len, actual_plaintext, actual_plaintext_len, &resulting_length);

    LT_LOG_DEBUG("lt_aesgcm_decrypt: psa_aead_decrypt returned status=%" PRId32, status);

    if (status != PSA_SUCCESS) {
        LT_LOG_ERROR("AES-GCM decryption failed, status=%" PRId32 " (psa_status_t)", status);
        return LT_CRYPTO_ERR;
    }

    // For auth-only operations (no plaintext), skip the length check
    if (expected_output > 0) {
        if (resulting_length != expected_output) {
            LT_LOG_ERROR("AES-GCM decryption output length mismatch! Current: %zu bytes, expected: %" PRIu32 " bytes",
                         resulting_length, expected_output);
            return LT_CRYPTO_ERR;
        }
    }

    return LT_OK;
}

lt_ret_t lt_aesgcm_encrypt_deinit(void *ctx)
{
    lt_ctx_mbedtls_v4_t *_ctx = (lt_ctx_mbedtls_v4_t *)ctx;

    return lt_aesgcm_deinit(&_ctx->aesgcm_encrypt_ctx);
}

lt_ret_t lt_aesgcm_decrypt_deinit(void *ctx)
{
    lt_ctx_mbedtls_v4_t *_ctx = (lt_ctx_mbedtls_v4_t *)ctx;

    return lt_aesgcm_deinit(&_ctx->aesgcm_decrypt_ctx);
}