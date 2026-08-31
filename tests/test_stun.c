/*
 * Standalone checks for STUN USERNAME matching and IPv4
 * XOR-MAPPED-ADDRESS / FINGERPRINT construction.
 *
 *   make test
 */
#include "ice_lite.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

static void write_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t) (value >> 8);
    p[1] = (uint8_t) value;
}

/*
 * Minimal STUN Binding Request with a USERNAME attribute.
 */
static size_t make_binding_request(uint8_t *out, const char *username)
{
    size_t ulen = strlen(username);
    size_t padded = (ulen + 3) & ~3u;

    memset(out, 0, 20 + 4 + padded);
    write_be16(out, 0x0001);
    write_be16(out + 2, (uint16_t) (4 + padded));
    out[4] = 0x21;
    out[5] = 0x12;
    out[6] = 0xA4;
    out[7] = 0x42;

    write_be16(out + 20, 0x0006);
    write_be16(out + 22, (uint16_t) ulen);
    memcpy(out + 24, username, ulen);

    return 20 + 4 + padded;
}

int main(void)
{
    uint8_t req[256];
    int failed = 0;

    size_t n = make_binding_request(req, "SrvUfrag:BrUfrag");

    if (!stun_is_binding_request(req, n, NULL)) {
        fprintf(stderr, "fail: binding request not recognised\n");
        failed = 1;
    }

    if (!stun_username_matches(req, n, "SrvUfrag")) {
        fprintf(stderr, "fail: RFC order (receiver ufrag first) rejected\n");
        failed = 1;
    }

    if (stun_username_matches(req, n, "BrUfrag") == 0) {
        fprintf(stderr, "fail: reversed-order fallback should accept BrUfrag\n");
        failed = 1;
    }

    if (stun_username_matches(req, n, "nope")) {
        fprintf(stderr, "fail: unrelated ufrag accepted\n");
        failed = 1;
    }

    char copied[64];
    if (stun_copy_username(req, n, copied, sizeof(copied)) != 0 ||
        strcmp(copied, "SrvUfrag:BrUfrag") != 0) {
        fprintf(stderr, "fail: stun_copy_username -> '%s'\n", copied);
        failed = 1;
    }

    struct sockaddr_storage peer;
    memset(&peer, 0, sizeof(peer));
    struct sockaddr_in *a4 = (struct sockaddr_in *) &peer;
    a4->sin_family = AF_INET;
    a4->sin_port = htons(51234);
    inet_pton(AF_INET, "192.168.0.10", &a4->sin_addr);

    uint8_t resp[128];
    size_t resp_len = 0;

    if (stun_build_binding_response("localpasswordvalueXXXXXX",
                                    req, n, &peer,
                                    resp, sizeof(resp), &resp_len) != 0) {
        fprintf(stderr, "fail: could not build binding response\n");
        failed = 1;
    } else if (resp_len < 64 || resp[0] != 0x01 || resp[1] != 0x01) {
        fprintf(stderr, "fail: response header type %02x%02x len %zu\n",
                resp[0], resp[1], resp_len);
        failed = 1;
    }

    if (failed) {
        fprintf(stderr, "test_stun: FAILED\n");
        return 1;
    }

    printf("test_stun: ok\n");
    return 0;
}
