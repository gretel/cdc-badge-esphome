/**
 * @file   lt_hkdf.c
 * @brief  HKDF functions definitions
 * @copyright Copyright (c) 2020-2026 Tropic Square s.r.o.
 *
 * @license For the license see LICENSE.md in the root directory of this source tree.
 */

#include "lt_hkdf.h"

#include <stdint.h>
#include <string.h>

#include "libtropic_common.h"
#include "libtropic_secure_memzero.h"
#include "lt_hmac_sha256.h"

lt_ret_t lt_hkdf(const uint8_t *ck, const uint32_t ck_len, const uint8_t *input,
                 const uint32_t input_len, const uint8_t nouts, uint8_t *output_1, uint8_t *output_2)
{
    LT_UNUSED(nouts);

    uint8_t tmp[LT_HMAC_SHA256_HASH_LEN] = {0};
    uint8_t helper[LT_HMAC_SHA256_HASH_LEN + 1] = {0};
    uint8_t one = 0x01;
    lt_ret_t ret;

    ret = lt_hmac_sha256(ck, ck_len, input, input_len, tmp);
    if (ret != LT_OK) {
        goto cleanup;
    }

    ret = lt_hmac_sha256(tmp, sizeof(tmp), &one, 1, output_1);
    if (ret != LT_OK) {
        goto cleanup;
    }

    memcpy(helper, output_1, LT_HMAC_SHA256_HASH_LEN);  // Copy whole output of SHA256 HMAC.
    helper[LT_HMAC_SHA256_HASH_LEN] = 2;

    ret = lt_hmac_sha256(tmp, sizeof(tmp), helper, sizeof(helper), output_2);
    lt_secure_memzero(helper, sizeof(helper));

cleanup:
    lt_secure_memzero(tmp, sizeof(tmp));

    return ret;
}
