/*
 * ice_lite.h
 *
 * Minimal ICE-lite transport support:
 *   - non blocking UDP socket helper
 *   - RFC 7983 packet demultiplexing
 *     (STUN / DTLS / RTP / other by first byte)
 *   - RFC 5389 STUN binding request parsing and authenticated
 *     binding response building (MESSAGE-INTEGRITY HMAC-SHA1
 *     with the local ice-pwd, plus FINGERPRINT CRC32)
 *
 * An ICE-lite agent never gathers candidates and never sends
 * connectivity checks itself. It only answers the checks of
 * the full agent (the browser) and remembers the validated
 * peer address.
 */
#ifndef WEBRTC_ICE_LITE_H
#define WEBRTC_ICE_LITE_H

#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

#define STUN_MAGIC_COOKIE 0x2112A442UL
#define STUN_HEADER_SIZE 20

typedef enum {
    RTC_PKT_STUN = 1,       /* first byte 0 to 3 */
    RTC_PKT_DTLS = 2,       /* first byte 20 to 63 */
    RTC_PKT_RTP = 3,        /* first byte 128 to 191, RTP or RTCP */
    RTC_PKT_OTHER = 4
} RtcPacketClass;

/*
 * Classify a datagram per RFC 7983.
 */
RtcPacketClass rtc_classify_packet(const uint8_t *buf, size_t len);

/*
 * Create a non blocking UDP socket bound to requested_port
 * (port 0 means auto assign). Returns fd or -1. The actual
 * port is written to *actual_port.
 */
int udp_socket_create(uint16_t requested_port, uint16_t *actual_port);

/*
 * A STUN binding request has type 0x0001, a valid magic
 * cookie and a sane length. Copies the 12 byte transaction id
 * into tid (may be NULL).
 */
int stun_is_binding_request(const uint8_t *buf, size_t len,
                            uint8_t tid[12]);

/*
 * Copy the STUN USERNAME attribute into a C string (truncated
 * to out_size - 1). Returns 0 on success, -1 when absent.
 */
int stun_copy_username(const uint8_t *buf, size_t len,
                       char *out, size_t out_size);

/*
 * RFC 8445 §7.3: a connectivity-check USERNAME is
 * "<receiver-ufrag>:<sender-ufrag>". For a browser check aimed
 * at this ICE-lite agent that means the attribute MUST start
 * with "<local-ufrag>:". The reversed form is also accepted so
 * a peer that swapped the fragments still gets an answer.
 */
int stun_username_matches(const uint8_t *buf, size_t len,
                          const char *local_ufrag);

/*
 * Build the binding response for a received request:
 *   XOR-MAPPED-ADDRESS + MESSAGE-INTEGRITY + FINGERPRINT.
 * local_pwd is the ice-pwd of this ( lite ) agent.
 * Returns 0 on success.
 */
int stun_build_binding_response(const char *local_pwd,
                                const uint8_t *request,
                                size_t request_len,
                                const struct sockaddr_storage *peer,
                                uint8_t *out,
                                size_t out_capacity,
                                size_t *out_len);

#endif
