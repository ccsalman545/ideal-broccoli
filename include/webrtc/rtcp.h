/*
 * rtcp.h
 *
 * RTCP compound packet handling for the video sender:
 *   - build Sender Reports for receiver statistics
 *   - parse inbound feedback: PLI, FIR, Generic NACK, BYE
 */
#ifndef WEBRTC_RTCP_H
#define WEBRTC_RTCP_H

#include <stddef.h>
#include <stdint.h>

#define RTCP_SR_SIZE 28

typedef struct {
    int pli;                        /* picture loss indications */
    int fir;                        /* full intra requests */
    int bye;

    size_t nack_seqs;               /* sequence numbers to retransmit */
    uint16_t nack_seq[128];

    /* First receiver report block, when present. */
    int has_rr;
    uint8_t rr_fraction_lost;       /* times 256 */
    uint32_t rr_jitter;             /* RTP clock units */
    uint32_t rr_highest_seq;
} RtcpFeedback;

/*
 * Build one Sender Report packet.
 *
 * ntp_wall_us: wall clock (CLOCK_REALTIME) in microseconds.
 * rtp_ts: RTP timestamp that corresponds to ntp_wall_us.
 */
size_t rtcp_build_sender_report(uint8_t *out,
                                uint32_t ssrc,
                                uint64_t ntp_wall_us,
                                uint32_t rtp_ts,
                                uint32_t packet_count,
                                uint32_t octet_count);

/*
 * Parse a compound RTCP packet (already SRTP unprotected).
 * Fields not present stay zero.
 */
void rtcp_parse(const uint8_t *buffer,
                size_t length,
                RtcpFeedback *feedback);

#endif
