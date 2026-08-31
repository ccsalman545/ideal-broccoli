/*
 * rtcp.c
 *
 * See rtcp.h.
 */
#include "rtcp.h"

#include <string.h>

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t) ((uint16_t) p[0] << 8 | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) |
           ((uint32_t) p[2] << 8) | (uint32_t) p[3];
}

static void write_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t) (value >> 8);
    p[1] = (uint8_t) value;
}

static void write_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t) (value >> 24);
    p[1] = (uint8_t) (value >> 16);
    p[2] = (uint8_t) (value >> 8);
    p[3] = (uint8_t) value;
}

size_t rtcp_build_sender_report(uint8_t *out,
                                uint32_t ssrc,
                                uint64_t ntp_wall_us,
                                uint32_t rtp_ts,
                                uint32_t packet_count,
                                uint32_t octet_count)
{
    /*
     * NTP timestamp: seconds since 1900-01-01 plus fraction.
     */
    const uint64_t ntp_secs = ntp_wall_us / 1000000ULL + 2208988800ULL;
    const uint32_t ntp_frac =
        (uint32_t) (((ntp_wall_us % 1000000ULL) << 32) / 1000000ULL);

    out[0] = 0x80;               /* V=2, P=0, RC=0 */
    out[1] = 200;                /* PT=SR */
    write_be16(out + 2, 6);      /* length in 32 bit words minus one */
    write_be32(out + 4, ssrc);

    write_be32(out + 8, (uint32_t) (ntp_secs & 0xFFFFFFFFUL));
    write_be32(out + 12, ntp_frac);

    write_be32(out + 16, rtp_ts);
    write_be32(out + 20, packet_count);
    write_be32(out + 24, octet_count);

    return RTCP_SR_SIZE;
}

void rtcp_parse(const uint8_t *buffer,
                size_t length,
                RtcpFeedback *feedback)
{
    memset(feedback, 0, sizeof(*feedback));

    size_t offset = 0;

    while (offset + 4 <= length) {
        const uint8_t version = buffer[offset] >> 6;

        if (version != 2) {
            return;
        }

        const uint8_t count_or_fmt = buffer[offset] & 0x1F;
        const uint8_t packet_type = buffer[offset + 1];
        const size_t words = read_be16(buffer + offset + 2);
        const size_t packet_len = (words + 1) * 4;

        if (offset + packet_len > length || packet_len == 0) {
            return;
        }


        switch (packet_type) {
        case 205: {
            /*
             * Generic NACK (RTPFB PT=205, FMT=1). PT 200 is
             * Sender Report and must not be parsed as feedback.
             * FCI entries are 4-byte (PID, bitmask of the 16
             * following seqs).
             */
            if (count_or_fmt == 1) {
                const size_t fci = offset + 12;

                for (size_t i = fci;
                     i + 4 <= offset + packet_len &&
                     feedback->nack_seqs < 128;
                     i += 4) {
                    uint16_t pid = read_be16(buffer + i);
                    uint16_t mask = read_be16(buffer + i + 2);

                    if (feedback->nack_seqs < 128) {
                        feedback->nack_seq[feedback->nack_seqs++] = pid;
                    }

                    for (int bit = 0;
                         bit < 16 &&
                         feedback->nack_seqs < 128;
                         bit++) {
                        if (mask & (1u << bit)) {
                            feedback->nack_seq[feedback->nack_seqs++] =
                                (uint16_t) (pid + bit + 1);
                        }
                    }
                }
            }
            break;
        }

        case 201: {
            /*
             * Receiver Report: first report block only.
             */
            if (count_or_fmt >= 1 && packet_len >= 8 + 24) {
                feedback->has_rr = 1;
                feedback->rr_fraction_lost = buffer[offset + 12];
                feedback->rr_highest_seq = read_be32(buffer + offset + 16);
                feedback->rr_jitter = read_be32(buffer + offset + 20);
            }
            break;
        }

        case 203:
            feedback->bye = 1;
            break;

        case 206: {
            /*
             * Payload-specific feedback.
             */
            if (count_or_fmt == 1) {
                feedback->pli++;
            } else if (count_or_fmt == 4) {
                feedback->fir++;
            }
            break;
        }

        default:
            break;
        }

        offset += packet_len;
    }
}
