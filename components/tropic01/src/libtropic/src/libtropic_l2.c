/**
 * @file libtropic_l2.c
 * @brief Layer 2 functions definitions
 * @copyright Copyright (c) 2020-2026 Tropic Square s.r.o.
 *
 * @license For the license see LICENSE.md in the root directory of this source tree.
 */

#include "libtropic_l2.h"

#include <inttypes.h>
#include <string.h>

#include "libtropic_common.h"
#include "libtropic_logging.h"
#include "lt_crc16.h"
#include "lt_l1.h"
#include "lt_l2_api_structs.h"
#include "lt_l2_frame_check.h"

/** Safety number. Limit number of loops during L3 chunks reception. TROPIC01 divides data into 128B
 *  chunks, length of L3 buffer is (2 + 4096 + 16).
 *  Divided by typical chunk length: (2 + 4096 + 16) / 128 => 32,
 *  with a few added loops it is set to 42.
 */
#define LT_L2_RECV_ENC_RES_MAX_CHUNKS 42

lt_ret_t lt_l2_send(lt_l2_state_t *s2)
{
    if (!s2) {
        return LT_PARAM_ERR;
    }

    add_crc(s2->buff);

    uint8_t len = s2->buff[1];

    return lt_l1_write(s2, len + 4, LT_L1_SPI_TIMEOUT_MS);
}

lt_ret_t lt_l2_resend_response(lt_l2_state_t *s2)
{
    // Setup a request pointer to l2 buffer, which is placed in handle
    struct lt_l2_resend_req_t *p_l2_req = (struct lt_l2_resend_req_t *)s2->buff;
    p_l2_req->req_id = TR01_L2_RESEND_REQ_ID;
    p_l2_req->req_len = TR01_L2_RESEND_REQ_LEN;

    lt_ret_t ret = lt_l2_send(s2);
    if (ret != LT_OK) {
        return ret;
    }

    ret = lt_l1_read(s2, TR01_L1_LEN_MAX, LT_L1_SPI_TIMEOUT_MS);
    if (ret != LT_OK) {
        return ret;
    }

    return lt_l2_frame_check(s2->buff);
}

lt_ret_t lt_l2_receive(lt_l2_state_t *s2)
{
    if (!s2) {
        return LT_PARAM_ERR;
    }

    lt_ret_t ret = lt_l1_read(s2, TR01_L1_LEN_MAX, LT_L1_SPI_TIMEOUT_MS);
    if (ret != LT_OK) {
        return ret;
    }

    // Fix of the chip FW bug, where several last bits of the frame may be missing
    // if the chip started to reboot. See Erratum CI_TR01_ERR_2025091800.
    //
    // If the reboot was successful, we only check the frame up to the first CRC byte.
    if (s2->startup_req_sent && s2->buff[TR01_L2_STATUS_OFFSET] == TR01_L2_STATUS_REQUEST_OK &&
        s2->buff[TR01_L2_RSP_LEN_OFFSET] == 0x00 &&
        s2->buff[TR01_L2_RSP_DATA_RSP_CRC_OFFSET] == 0x03) {
        return LT_OK;
    }

    ret = lt_l2_frame_check(s2->buff);

    // Rest of errors are reported directly to upper layers, without trying to resend response.
    return ret;
}

lt_ret_t lt_l2_transfer(lt_l2_state_t *s2)
{
    if (!s2) {
        return LT_PARAM_ERR;
    }

    uint8_t req_buff_backup[LT_SIZE_OF_L2_BUFF];
    memcpy(req_buff_backup, s2->buff, LT_SIZE_OF_L2_BUFF);

    lt_ret_t ret;
    bool retry_communication;
    int total_retry_count = 0;

    do {
        retry_communication = false;

        ret = lt_l2_send(s2);
        if (ret != LT_OK) {
            return ret;
        }

        ret = lt_l2_receive(s2);

        // After receiving Response frame with correct CRC (or exceeded count of attempts),
        // check if TROPIC01 received our Request OK. If not, and total number
        // of retries was not exhausted yet, retry whole communication.
        if (ret == LT_L2_CRC_ERR) {
            s2->l2_crc_error_count++;

            if (total_retry_count < LT_CRC_ERR_RETRY_ATTEMPTS) {
                memcpy(s2->buff, req_buff_backup, LT_SIZE_OF_L2_BUFF);
                total_retry_count++;
                retry_communication = true;
            }
        }
        // If CRC of incoming Response is invalid, try to get a new Response.
        else if (ret == LT_L2_IN_CRC_ERR) {
            s2->l2_in_crc_error_count++;
            while (total_retry_count < LT_CRC_ERR_RETRY_ATTEMPTS) {
                total_retry_count++;

                ret = lt_l2_resend_response(s2);
                if (ret == LT_L2_IN_CRC_ERR) {
                    s2->l2_in_crc_error_count++;
                }
                // If we receive LT_L2_CRC_ERR to Resend_Req, we should not retry whole communication,
                // rather the Resend_Req again, as we cannot be sure that the CRC error appeared in the
                // original frame or only later in the Resend_Req frame.
                else if (ret == LT_L2_CRC_ERR) {
                    s2->l2_crc_error_count++;
                }
                else {
                    break;
                }
            }
        }

    } while (retry_communication);

    return ret;
}

lt_ret_t lt_l2_send_encrypted_cmd(lt_l2_state_t *s2, uint8_t *buff, uint16_t buff_len)
{
    if (!s2 || !buff) {
        return LT_PARAM_ERR;
    }

    lt_ret_t ret = LT_FAIL;

    // There is L3 payload in provided buffer (buff).
    // First check how much data are to be send and if it actually fits into that buffer,
    // there must be a space for 2B of size value, ?B of command (ID + data) and 16B of TAG.
    struct lt_l3_gen_frame_t *p_frame = (struct lt_l3_gen_frame_t *)buff;
    uint16_t packet_size = (TR01_L3_SIZE_SIZE + p_frame->cmd_size + TR01_L3_TAG_SIZE);

    // Prevent sending more data than is the max size of L3 packet.
    if (packet_size > TR01_L3_PACKET_MAX_SIZE) {
        LT_LOG_ERROR("Packet size %" PRIu16 "exceeds maximum L3 packet size %u", packet_size,
                     TR01_L3_PACKET_MAX_SIZE);
        return LT_L3_DATA_LEN_ERROR;
    }
    // Prevent sending more data than is the size of the provided buffer.
    if (packet_size > buff_len) {
        LT_LOG_ERROR("Packet size %" PRIu16 "exceeds L3 buffer size %" PRIu16, packet_size, buff_len);
        return LT_PARAM_ERR;
    }

    // Setup a request pointer to L2 buffer, which is placed in handle
    struct lt_l2_encrypted_cmd_req_t *req = (struct lt_l2_encrypted_cmd_req_t *)s2->buff;

    // Calculate number of chunks to send.
    // First, get the number of full chunks.
    uint16_t full_chunk_num = (packet_size / TR01_L2_CHUNK_MAX_DATA_SIZE);
    // Second, if packet_size is not divisible by the maximum chunk size, one additional smaller chunk
    // will be created, which we add to the total count.
    uint16_t chunk_num = (packet_size % TR01_L2_CHUNK_MAX_DATA_SIZE) == 0 ? full_chunk_num
                                                                          : full_chunk_num + 1;
    // Calculate the length of the last chunk
    uint16_t last_chunk_len = packet_size - ((chunk_num - 1) * TR01_L2_CHUNK_MAX_DATA_SIZE);

    uint16_t buff_offset = 0;

    // Count of retries to resend the frame on LT_L2_CRC_ERR or LT_L2_IN_CRC_ERR.
    // This counter is reset on each successful chunk (frame).
    uint32_t frame_resend_counter = 0;

    // req->req_len gets overwritten, so we need to keep copy to increment
    // buff_offset at the end of the loop.
    uint16_t req_len;

    // Split encrypted buffer into chunks and proceed them into L2 transfers:
    for (int i = 0; i < chunk_num; i++) {
        req->req_id = TR01_L2_ENCRYPTED_CMD_REQ_ID;
        // If the currently processed chunk is the last one, get its length (may be shorter than
        // L2_CHUNK_MAX_DATA_SIZE)
        if (i == (chunk_num - 1)) {
            req_len = last_chunk_len;
        }
        else {
            req_len = TR01_L2_CHUNK_MAX_DATA_SIZE;
        }
        req->req_len = req_len;
        memcpy(req->l3_chunk, buff + buff_offset, req_len);
        add_crc(req);

        // Send L2 request containing a chunk from L3 buff
        ret = lt_l1_write(s2, 2 + req_len + 2, LT_L1_SPI_TIMEOUT_MS);
        if (ret != LT_OK) {
            return ret;
        }

        // Read a response on this L2 request
        ret = lt_l1_read(s2, TR01_L1_LEN_MAX, LT_L1_SPI_TIMEOUT_MS);
        if (ret != LT_OK) {
            return ret;
        }

        // Check status byte of this frame
        ret = lt_l2_frame_check(s2->buff);

        // In case of IN_CRC error, request to resend the last frame if there's still some retry
        // attempts.
        if (ret == LT_L2_IN_CRC_ERR) {
            s2->l2_in_crc_error_count++;
            while (frame_resend_counter < LT_CRC_ERR_RETRY_ATTEMPTS) {
                frame_resend_counter++;

                ret = lt_l2_resend_response(s2);
                if (ret == LT_L2_IN_CRC_ERR) {
                    s2->l2_in_crc_error_count++;
                }
                // If we receive LT_L2_CRC_ERR to Resend_Req, we should not retry whole communication,
                // rather the Resend_Req again, as we cannot be sure that the CRC error appeared in the
                // original frame or only later in the Resend_Req frame.
                else if (ret == LT_L2_CRC_ERR) {
                    s2->l2_crc_error_count++;
                }
                // Do not retry on OK or other errors.
                else {
                    break;
                }
            }
        }
        // In case of CRC error, resend the last frame if there're still some retry attempts.
        else if (ret == LT_L2_CRC_ERR) {
            s2->l2_crc_error_count++;  // Counter has to be increased even if retry attempts were
                                       // depleted.

            if (frame_resend_counter < LT_CRC_ERR_RETRY_ATTEMPTS) {
                frame_resend_counter++;
                i--;
                continue;
            }
        }

        // In other cases, return immediately.
        if (ret != LT_OK && ret != LT_L2_REQ_CONT) {
            return ret;
        }
        // OK - we can move forward in buffer and reset retry counter.
        else {
            buff_offset += req_len;    // Move offset for next chunk
            frame_resend_counter = 0;  // Frame sent successfully, reset the counter.
        }
    }

    return LT_OK;
}

lt_ret_t lt_l2_recv_encrypted_res(lt_l2_state_t *s2, uint8_t *buff, uint16_t max_len)
{
    if (!s2
        // Max len must be definitively smaller than size of L3 buffer
        || max_len > TR01_L3_PACKET_MAX_SIZE || !buff) {
        return LT_PARAM_ERR;
    }

    lt_ret_t ret = LT_FAIL;
    // Setup a response pointer to L2 buffer, which is placed in handle
    struct lt_l2_encrypted_cmd_rsp_t *resp = (struct lt_l2_encrypted_cmd_rsp_t *)s2->buff;

    // Position into L3 buffer where processed L2 chunk will be copied into
    uint16_t offset = 0;
    // Tropic can respond with various counts and lengths of chunks.
    // This counter tracks number of chunks received, so we can terminate the communication
    // if it exceeds a certain amount.
    uint16_t chunks_received = 0;

    // Count of retries to resend the frame on LT_L2_CRC_ERR or LT_L2_IN_CRC_ERR.
    // This counter is reset on each successful chunk (frame).
    uint32_t frame_resend_counter = 0;

    // Whether to resend previous chunk or continue with the next one.
    bool resend_response = false;

    do {
        // Get one L2 frame of a device's response.
        // If previous frame was received OK, read a next frame. Otherwise, ask to resend
        // the previous frame.
        if (!resend_response) {
            ret = lt_l1_read(s2, TR01_L1_LEN_MAX, LT_L1_SPI_TIMEOUT_MS);
            if (ret != LT_OK) {
                return ret;
            }

            // Check status byte of this frame
            ret = lt_l2_frame_check(s2->buff);
        }
        else {
            ret = lt_l2_resend_response(s2);
            resend_response = false;
        }

        // Prevent receiving more data than is the size of the provided L3 buffer.
        if (ret == LT_OK || ret == LT_L2_RES_CONT) {
            if (offset + resp->rsp_len > max_len) {
                return LT_L2_RSP_LEN_ERROR;
            }
        }

        switch (ret) {
            case LT_L2_RES_CONT:
                // Frame received OK, resetting resend counter.
                frame_resend_counter = 0;
                // Copy content of L2 into current offset of the L3 buffer
                memcpy(buff + offset, (struct l2_encrypted_rsp_t *)resp->l3_chunk, resp->rsp_len);
                offset += resp->rsp_len;
                chunks_received++;
                break;
            case LT_L2_IN_CRC_ERR:
                // Host received corrupted frame. Ask for resend.
                s2->l2_in_crc_error_count++;
                if (frame_resend_counter >= LT_CRC_ERR_RETRY_ATTEMPTS) {
                    return ret;
                }
                else {
                    frame_resend_counter++;
                    resend_response = true;
                }
                break;
            case LT_L2_CRC_ERR:
                // CRC_ERR can be returned only as a response to Resend_Req.
                // Ask for resend again.
                s2->l2_crc_error_count++;
                if (frame_resend_counter >= LT_CRC_ERR_RETRY_ATTEMPTS) {
                    return ret;
                }
                else {
                    frame_resend_counter++;
                    resend_response = true;
                }
                break;
            case LT_OK:
                // This was last L2 frame of L3 packet, copy it and return
                memcpy(buff + offset, (struct l2_encrypted_rsp_t *)resp->l3_chunk, resp->rsp_len);
                return LT_OK;
            default:
                // Any other frame status is not expected
                return ret;
        }

    } while (chunks_received < LT_L2_RECV_ENC_RES_MAX_CHUNKS);

    LT_LOG_ERROR("Maximal number of received chunks (%d) exceeded!", LT_L2_RECV_ENC_RES_MAX_CHUNKS);
    return LT_L3_DATA_LEN_ERROR;
}
