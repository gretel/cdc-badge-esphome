/**
 * @file lt_l2_frame_check.c
 * @brief Layer 2 frame check functions definitions
 * @copyright Copyright (c) 2020-2026 Tropic Square s.r.o.
 *
 * @license For the license see LICENSE.md in the root directory of this source tree.
 */

#include "lt_l2_frame_check.h"

#include "libtropic_common.h"
#include "lt_crc16.h"

lt_ret_t lt_l2_frame_check(const uint8_t *frame)
{
#ifdef LT_REDUNDANT_ARG_CHECK
    if (!frame) {
        return LT_PARAM_ERR;
    }
#endif

    // Extract STATUS, RSP_LEN and RSP_CRC fields. Verify that RSP_LEN is in bounds.
    uint8_t status = frame[TR01_L2_STATUS_OFFSET];
    uint8_t len = frame[TR01_L2_RSP_LEN_OFFSET];

    if (len > TR01_L2_CHUNK_MAX_DATA_SIZE) {
        return LT_L2_RSP_LEN_ERROR;
    }

    uint16_t frame_crc = frame[len + TR01_L2_RSP_DATA_RSP_CRC_OFFSET + 1] |
                         frame[len + TR01_L2_RSP_DATA_RSP_CRC_OFFSET] << 8;

    switch (status) {
        // Valid frames, or CRC errors in incoming frames are handled here:
        case TR01_L2_STATUS_REQUEST_OK:
        case TR01_L2_STATUS_RESULT_OK:
            if (frame_crc != crc16(frame + TR01_L2_STATUS_OFFSET,
                                   TR01_L2_STATUS_SIZE + TR01_L2_REQ_RSP_LEN_SIZE + len)) {
                return LT_L2_IN_CRC_ERR;
            }
            return LT_OK;

        // L2 statuses returned by Tropic chip are handled here
        case TR01_L2_STATUS_REQUEST_CONT:
            return LT_L2_REQ_CONT;
        case TR01_L2_STATUS_RESULT_CONT:
            return LT_L2_RES_CONT;
        case TR01_L2_STATUS_HSK_ERR:
            return LT_L2_HSK_ERR;
        case TR01_L2_STATUS_NO_SESSION:
            return LT_L2_NO_SESSION;
        case TR01_L2_STATUS_TAG_ERR:
            return LT_L2_TAG_ERR;
        case TR01_L2_STATUS_CRC_ERR:
            return LT_L2_CRC_ERR;
        case TR01_L2_STATUS_GEN_ERR:
            return LT_L2_GEN_ERR;
        case TR01_L2_STATUS_NO_RESP:
            return LT_L2_NO_RESP;
        case TR01_L2_STATUS_UNKNOWN_ERR:
            return LT_L2_UNKNOWN_REQ;
        case TR01_L2_STATUS_RESP_DISABLED:
            return LT_L2_RESP_DISABLED;
        default:
            return LT_L2_STATUS_UNKNOWN;
    }
}
