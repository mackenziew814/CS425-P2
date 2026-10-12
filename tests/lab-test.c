/*
 * tests/test_lab.c  –  Unity test suite for lab.h (layers 1 and 2)
 *
 * Covers every public function declared in lab.h.
 * No sockets, no relay, no real clock.  Fully deterministic.
 *
 * Build & run:
 *   make check
 *
 * Required coverage (per project spec):
 *   Checksum: RFC 1071 example, odd-length, single flipped bit caught
 *   pkt_decode rejects: too-short, length mismatch, unknown type, rsvd!=0
 *   Receiver: duplicate pkt, beyond-gap pkt, repeated FIN
 *   Sender: window full, cumulative ACK slide, duplicate ACK,
 *           timeout retransmit, 10-timeout give-up
 *   Files: empty, exact multiple of 1024 bytes
 *   In-memory channel: 20% loss/corrupt/dup, fixed seed, bytes verified
 */

#include "harness/unity.h"
#include "src/lab.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* =========================================================================
 * Unity boilerplate
 * ======================================================================= */
void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * Minimal LCG for deterministic channel damage
 *
 * Knuth's constants; period 2^32.  Returns values in [0, 2^32).
 * ======================================================================= */
static uint32_t lcg_state;

static void lcg_seed(uint32_t s) { lcg_state = s; }

static uint32_t lcg_next(void)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

/* probability p in [0,1]: returns 1 with that probability */
static int lcg_prob(double p)
{
    return (double)(lcg_next() & 0xffffffff) < p * 4294967296.0;
}

/* =========================================================================
 * In-memory packet channel
 *
 * Holds up to CHAN_CAP raw packet copies.  Duplicates are stored as two
 * separate entries so the consumer sees them one at a time.
 * ======================================================================= */
#define CHAN_CAP  512

typedef struct {
    uint8_t  data[PKT_MAX_LEN];
    size_t   len;
} chan_entry_t;

typedef struct {
    chan_entry_t entries[CHAN_CAP];
    int          head;   /* next to read  */
    int          tail;   /* next to write */
    int          count;
} chan_t;

static void chan_init(chan_t *c)
{
    memset(c, 0, sizeof(*c));
}

/* push a packet into the channel (returns 0 if full) */
static int chan_push(chan_t *c, const uint8_t *buf, size_t len)
{
    if (c->count >= CHAN_CAP) return 0;
    memcpy(c->entries[c->tail].data, buf, len);
    c->entries[c->tail].len = len;
    c->tail = (c->tail + 1) % CHAN_CAP;
    c->count++;
    return 1;
}

/*
 * chan_deliver: take the front entry, apply damage, push result(s) into dst.
 * loss=0.2, corrupt=0.2, dup=0.2 means each independently.
 * Returns 0 if the source channel was empty.
 */
static int chan_deliver(chan_t *src, chan_t *dst,
                        double loss, double corrupt, double dup)
{
    if (src->count == 0) return 0;

    chan_entry_t e = src->entries[src->head];
    src->head = (src->head + 1) % CHAN_CAP;
    src->count--;

    /* loss: drop silently */
    if (lcg_prob(loss)) return 1;

    /* corrupt: flip one bit */
    if (lcg_prob(corrupt)) {
        size_t byte_off = lcg_next() % e.len;
        int    bit      = (int)(lcg_next() % 8);
        e.data[byte_off] ^= (uint8_t)(1u << bit);
    }

    /* dup: deliver twice */
    if (lcg_prob(dup))
        chan_push(dst, e.data, e.len);

    chan_push(dst, e.data, e.len);
    return 1;
}

/* =========================================================================
 * Helper: decode a packet from a wire buffer; TEST_ASSERT on failure
 * ======================================================================= */
static pkt_t must_decode(const uint8_t *buf, size_t len)
{
    pkt_t p;
    TEST_ASSERT_MESSAGE(pkt_decode(buf, len, &p), "pkt_decode failed unexpectedly");
    return p;
}

/* =========================================================================
 * Helper: flip one bit at byte_off, bit position bit (0=LSB)
 * ======================================================================= */
static void flip_bit(uint8_t *buf, size_t byte_off, int bit)
{
    buf[byte_off] ^= (uint8_t)(1u << bit);
}

/* =========================================================================
 * ── LAYER 1: pkt_checksum ──────────────────────────────────────────────
 * ======================================================================= */

/* RFC 1071 §1 worked example: 00 01 f2 03 f4 f5 f6 f7 → 0x220d */
void test_checksum_rfc1071_example(void)
{
    uint8_t buf[] = {0x00,0x01,0xf2,0x03,0xf4,0xf5,0xf6,0xf7};
    TEST_ASSERT_EQUAL_HEX16(0x220d, pkt_checksum(buf, sizeof(buf)));
}

/* Verification: adding a checksum into the buffer gives 0x0000 */
void test_checksum_verify_roundtrip(void)
{
    uint8_t buf[] = {0x00,0x01,0xf2,0x03,0xf4,0xf5,0xf6,0xf7};
    uint16_t cs = pkt_checksum(buf, sizeof(buf));
    /* store big-endian */
    uint8_t full[10];
    memcpy(full, buf, 8);
    full[8] = (uint8_t)(cs >> 8);
    full[9] = (uint8_t)(cs & 0xff);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(full, 10));
}

/* Odd-length: single byte 0x01 → high byte 0x01, low byte 0x00 → sum 0x0100
 * complement → 0xfeff */
void test_checksum_odd_length(void)
{
    uint8_t buf[] = {0x01};
    TEST_ASSERT_EQUAL_HEX16(0xfeff, pkt_checksum(buf, 1));
}

/* Odd-length two-byte block: 0x01 0x02 = word 0x0102,
 * with a third byte 0x03 padded → 0x0300 added → sum = 0x0402,
 * complement = 0xfbfd */
void test_checksum_odd_three_bytes(void)
{
    uint8_t buf[] = {0x01, 0x02, 0x03};
    /* manually: 0x0102 + 0x0300 = 0x0402, ~0x0402 = 0xfbfd */
    TEST_ASSERT_EQUAL_HEX16(0xfbfd, pkt_checksum(buf, 3));
}

/* Spec Example 2: DATA seq=2 payload "Hi!" → checksum 0x9691 */
void test_checksum_data_hi(void)
{
    uint8_t buf[13] = {0};
    buf[0] = 0x00; buf[1] = 0x00;                          /* type, rsvd */
    /* checksum zeroed */
    buf[4]=0x00; buf[5]=0x00; buf[6]=0x00; buf[7]=0x02;   /* seq=2 BE */
    buf[8]=0x00; buf[9]=0x03;                               /* len=3 */
    buf[10]='H'; buf[11]='i'; buf[12]='!';
    TEST_ASSERT_EQUAL_HEX16(0x9691, pkt_checksum(buf, 13));
}

/* Spec Example 2: ACK 3 full packet → verification gives 0x0000 */
void test_checksum_ack3_verify(void)
{
    uint8_t buf[] = {0x01,0x00,0xfe,0xfc,0x00,0x00,0x00,0x03,0x00,0x00};
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(buf, sizeof(buf)));
}

/* A single flipped bit must change the checksum */
void test_checksum_detects_single_bit_flip(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[8] = {0xDE,0xAD,0xBE,0xEF,0xCA,0xFE,0xBA,0xBE};
    size_t len = pkt_encode_data(buf, 42, pay, 8);

    /* flip each bit in the payload and check detection */
    int detected = 1;
    size_t i;
    for (i = 0; i < len; i++) {
        int b;
        for (b = 0; b < 8; b++) {
            uint8_t tmp[PKT_MAX_LEN];
            memcpy(tmp, buf, len);
            flip_bit(tmp, i, b);
            if (pkt_checksum(tmp, len) == 0x0000) {
                detected = 0;  /* this bit position evaded detection */
            }
        }
    }
    TEST_ASSERT_MESSAGE(detected,
        "a single flipped bit should always be caught by the checksum");
}

/* =========================================================================
 * ── LAYER 1: pkt_encode_* ──────────────────────────────────────────────
 * ======================================================================= */

void test_encode_data_length(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[5] = {1,2,3,4,5};
    size_t len = pkt_encode_data(buf, 0, pay, 5);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN + 5, len);
}

void test_encode_data_fields(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[3] = {0xAA,0xBB,0xCC};
    pkt_encode_data(buf, 7, pay, 3);
    pkt_t p = must_decode(buf, PKT_HDR_LEN + 3);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_DATA, p.type);
    TEST_ASSERT_EQUAL_UINT32(7, p.seq);
    TEST_ASSERT_EQUAL_UINT16(3, p.payload_len);
    TEST_ASSERT_EQUAL_UINT8(0xAA, p.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, p.payload[2]);
}

void test_encode_data_max_payload(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[PKT_MAX_PAYLOAD];
    memset(pay, 0x5A, PKT_MAX_PAYLOAD);
    size_t len = pkt_encode_data(buf, 0, pay, PKT_MAX_PAYLOAD);
    TEST_ASSERT_EQUAL_size_t(PKT_MAX_LEN, len);
    pkt_t p = must_decode(buf, len);
    TEST_ASSERT_EQUAL_UINT16(PKT_MAX_PAYLOAD, p.payload_len);
    TEST_ASSERT_EQUAL_HEX8(0x5A, p.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x5A, p.payload[PKT_MAX_PAYLOAD - 1]);
}

void test_encode_ack_length_and_fields(void)
{
    uint8_t buf[PKT_HDR_LEN];
    size_t len = pkt_encode_ack(buf, 99);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, len);
    pkt_t p = must_decode(buf, len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_ACK, p.type);
    TEST_ASSERT_EQUAL_UINT32(99, p.seq);
    TEST_ASSERT_EQUAL_UINT16(0, p.payload_len);
}

void test_encode_fin_length_and_fields(void)
{
    uint8_t buf[PKT_HDR_LEN];
    size_t len = pkt_encode_fin(buf, 42);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, len);
    pkt_t p = must_decode(buf, len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, p.type);
    TEST_ASSERT_EQUAL_UINT32(42, p.seq);
    TEST_ASSERT_EQUAL_UINT16(0, p.payload_len);
}

/* seq field round-trips at boundary values */
void test_encode_seq_boundary_values(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_t p;

    pkt_encode_ack(buf, 0);
    p = must_decode(buf, PKT_HDR_LEN);
    TEST_ASSERT_EQUAL_UINT32(0, p.seq);

    pkt_encode_ack(buf, 0xffffffff);
    p = must_decode(buf, PKT_HDR_LEN);
    TEST_ASSERT_EQUAL_UINT32(0xffffffff, p.seq);
}

/* Every encoded packet verifies (checksum over full packet == 0x0000) */
void test_encode_checksum_embeds_correctly(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[7] = {1,2,3,4,5,6,7};

    size_t len = pkt_encode_data(buf, 3, pay, 7);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(buf, len));

    len = pkt_encode_ack(buf, 5);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(buf, len));

    len = pkt_encode_fin(buf, 5);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(buf, len));
}

/* =========================================================================
 * ── LAYER 1: pkt_decode ─────────────────────────────────────────────────
 * ======================================================================= */

/* Happy path: DATA packet */
void test_decode_valid_data(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[4] = {9,8,7,6};
    size_t len = pkt_encode_data(buf, 5, pay, 4);
    pkt_t p;
    TEST_ASSERT_TRUE(pkt_decode(buf, len, &p));
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_DATA, p.type);
    TEST_ASSERT_EQUAL_UINT32(5, p.seq);
    TEST_ASSERT_EQUAL_UINT16(4, p.payload_len);
    /* payload pointer must be inside the original buffer */
    TEST_ASSERT_TRUE(p.payload >= buf && p.payload < buf + len);
    TEST_ASSERT_EQUAL_UINT8(9, p.payload[0]);
}

/* Happy path: ACK */
void test_decode_valid_ack(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_ack(buf, 17);
    pkt_t p;
    TEST_ASSERT_TRUE(pkt_decode(buf, PKT_HDR_LEN, &p));
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_ACK, p.type);
    TEST_ASSERT_EQUAL_UINT32(17, p.seq);
    TEST_ASSERT_EQUAL_UINT16(0, p.payload_len);
    TEST_ASSERT_NULL(p.payload);
}

/* Happy path: FIN */
void test_decode_valid_fin(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_fin(buf, 8);
    pkt_t p;
    TEST_ASSERT_TRUE(pkt_decode(buf, PKT_HDR_LEN, &p));
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, p.type);
}

/* Rule 1: datagram shorter than 10 bytes is rejected */
void test_decode_rejects_too_short(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[1] = {0};
    pkt_encode_data(buf, 0, pay, 1);
    pkt_t p;
    /* try every length from 0 to 9 */
    size_t i;
    for (i = 0; i < PKT_HDR_LEN; i++)
        TEST_ASSERT_FALSE_MESSAGE(pkt_decode(buf, i, &p), "should reject short datagram");
}

/* Rule 2: length field says 100 bytes but datagram is only 13 bytes */
void test_decode_rejects_length_mismatch_short(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[3] = {1,2,3};
    size_t len = pkt_encode_data(buf, 0, pay, 3);  /* total 13 */
    /* tamper: set length field to 100 without updating checksum */
    buf[OFF_LEN]   = 0x00;
    buf[OFF_LEN+1] = 100;
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, len, &p));
}

/* Rule 2: length field says 0 but datagram has extra payload bytes */
void test_decode_rejects_length_mismatch_long(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[5] = {1,2,3,4,5};
    size_t len = pkt_encode_data(buf, 0, pay, 5);  /* total 15 */
    /* claim length = 0 */
    buf[OFF_LEN] = 0x00; buf[OFF_LEN+1] = 0x00;
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, len, &p));
}

/* Rule 2: length field > 1024 */
void test_decode_rejects_length_over_max(void)
{
    /* Craft a minimal valid-looking 10-byte header then set length=1025 */
    uint8_t buf[PKT_HDR_LEN + 1024 + 2] = {0};
    buf[OFF_TYPE]   = PKT_TYPE_DATA;
    buf[OFF_LEN]    = 0x04;   /* 0x0401 = 1025 */
    buf[OFF_LEN+1]  = 0x01;
    pkt_t p;
    /* datagram size = PKT_HDR_LEN + 1025 bytes, length field = 1025 → rule 2 */
    TEST_ASSERT_FALSE(pkt_decode(buf, PKT_HDR_LEN + 1025, &p));
}

/* Rule 3: unknown type (3, 127, 255) */
void test_decode_rejects_unknown_type(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_ack(buf, 0);
    pkt_t p;

    uint8_t bad_types[] = {3, 4, 127, 255};
    size_t i;
    for (i = 0; i < sizeof(bad_types); i++) {
        uint8_t tmp[PKT_HDR_LEN];
        memcpy(tmp, buf, PKT_HDR_LEN);
        tmp[OFF_TYPE] = bad_types[i];
        TEST_ASSERT_FALSE(pkt_decode(tmp, PKT_HDR_LEN, &p));
    }
}

/* Rule 3: reserved byte != 0 */
void test_decode_rejects_nonzero_reserved(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_ack(buf, 0);
    buf[OFF_RSVD] = 1;
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, PKT_HDR_LEN, &p));
}

/* Rule 4: single flipped bit in payload is caught */
void test_decode_rejects_corrupted_payload(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[8] = {1,2,3,4,5,6,7,8};
    size_t len = pkt_encode_data(buf, 0, pay, 8);
    flip_bit(buf, OFF_PAYLOAD + 3, 5);
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, len, &p));
}

/* Rule 4: single flipped bit in header is caught */
void test_decode_rejects_corrupted_header(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_fin(buf, 0);
    /* flip a bit in the seq field */
    flip_bit(buf, OFF_SEQ + 2, 0);
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, PKT_HDR_LEN, &p));
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_init / gbn_sender_window_full ──────────────────
 * ======================================================================= */

void test_sender_init_zeroes_state(void)
{
    gbn_sender_t s;
    /* poison memory first */
    memset(&s, 0xFF, sizeof(s));
    gbn_sender_init(&s, 4, 250);
    TEST_ASSERT_EQUAL_INT(4,   s.winsize);
    TEST_ASSERT_EQUAL_INT(250, (int)s.timeout_ms);
    TEST_ASSERT_EQUAL_UINT32(0, s.base);
    TEST_ASSERT_EQUAL_UINT32(0, s.next);
    TEST_ASSERT_EQUAL_INT(0, s.eof_seen);
    TEST_ASSERT_EQUAL_INT(0, s.fin_sent);
    TEST_ASSERT_EQUAL_INT(0, s.fin_acked);
    TEST_ASSERT_EQUAL_INT(0, s.consec_timeouts);
    TEST_ASSERT_EQUAL_UINT64(0, (unsigned long long)s.timer_deadline_ms);
}

void test_sender_window_not_full_initially(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    TEST_ASSERT_FALSE(gbn_sender_window_full(&s));
}

/* Window becomes full at exactly winsize in-flight packets */
void test_sender_window_full_at_capacity(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 3, 250);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);
    TEST_ASSERT_FALSE(gbn_sender_window_full(&s));
    gbn_sender_data(&s, t, pay, 1);
    TEST_ASSERT_FALSE(gbn_sender_window_full(&s));
    gbn_sender_data(&s, t, pay, 1);
    TEST_ASSERT_TRUE(gbn_sender_window_full(&s));   /* exactly winsize=3 */
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_data ───────────────────────────────────────────
 * ======================================================================= */

void test_sender_data_sends_one_packet(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[5] = {1,2,3,4,5};

    snd_action_t a = gbn_sender_data(&s, 0, pay, 5);
    TEST_ASSERT_EQUAL_INT(1, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(1, s.next);
    TEST_ASSERT_EQUAL_INT(250, (int)a.timer_deadline_ms); /* t=0+250 */
}

void test_sender_data_packet_decodes_correctly(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[3] = {0xAA, 0xBB, 0xCC};

    snd_action_t a = gbn_sender_data(&s, 0, pay, 3);
    pkt_t p = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_DATA, p.type);
    TEST_ASSERT_EQUAL_UINT32(0, p.seq);
    TEST_ASSERT_EQUAL_UINT16(3, p.payload_len);
    TEST_ASSERT_EQUAL_UINT8(0xBB, p.payload[1]);
}

/* window full → data call returns send_count == 0 */
void test_sender_data_ignored_when_window_full(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 2, 250);
    uint8_t pay[1] = {0};

    gbn_sender_data(&s, 0, pay, 1);
    gbn_sender_data(&s, 0, pay, 1);
    TEST_ASSERT_TRUE(gbn_sender_window_full(&s));

    snd_action_t a = gbn_sender_data(&s, 0, pay, 1);
    TEST_ASSERT_EQUAL_INT(0, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(2, s.next);   /* did not advance */
}

/* Timer starts on first data, does not reset on subsequent data */
void test_sender_data_timer_starts_on_first(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 100);
    uint8_t pay[1] = {0};

    snd_action_t a0 = gbn_sender_data(&s, 1000, pay, 1);
    TEST_ASSERT_EQUAL_INT(1100, (int)a0.timer_deadline_ms);

    snd_action_t a1 = gbn_sender_data(&s, 2000, pay, 1);
    /* timer was already running; deadline must NOT be reset */
    TEST_ASSERT_EQUAL_INT(1100, (int)a1.timer_deadline_ms);
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_eof ────────────────────────────────────────────
 * ======================================================================= */

/* Empty file: eof immediately → FIN seq 0 sent */
void test_sender_eof_empty_file(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 8, 250);

    snd_action_t a = gbn_sender_eof(&s, 0);
    TEST_ASSERT_EQUAL_INT(1, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(0, s.fin_seq);
    TEST_ASSERT_TRUE(s.fin_sent);
    TEST_ASSERT_EQUAL_INT(250, (int)a.timer_deadline_ms);

    pkt_t fp = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, fp.type);
    TEST_ASSERT_EQUAL_UINT32(0, fp.seq);
}

/* EOF with window full: FIN deferred until ACK opens a slot */
void test_sender_eof_deferred_when_window_full(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 2, 250);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);
    gbn_sender_data(&s, t, pay, 1);
    TEST_ASSERT_TRUE(gbn_sender_window_full(&s));

    snd_action_t a = gbn_sender_eof(&s, t);
    TEST_ASSERT_EQUAL_INT(0, a.send_count);   /* FIN not sent yet */
    TEST_ASSERT_TRUE(s.eof_seen);
    TEST_ASSERT_FALSE(s.fin_sent);

    /* ACK 1 slides base; FIN should appear now */
    a = gbn_sender_ack(&s, t+10, 1);
    TEST_ASSERT_EQUAL_INT(1, a.send_count);   /* FIN */
    pkt_t fp = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, fp.type);
    TEST_ASSERT_EQUAL_UINT32(2, fp.seq);      /* fin_seq = 2 DATA pkts */
}

/* EOF is idempotent */
void test_sender_eof_idempotent(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 8, 250);

    snd_action_t a0 = gbn_sender_eof(&s, 0);
    snd_action_t a1 = gbn_sender_eof(&s, 0);
    TEST_ASSERT_EQUAL_INT(1, a0.send_count);
    TEST_ASSERT_EQUAL_INT(0, a1.send_count);  /* second call is no-op */
}

/* Exact multiple of 1024 bytes: FIN seq must equal number of DATA packets */
void test_sender_exact_multiple_of_1024(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 16, 250);
    uint8_t pay[PKT_MAX_PAYLOAD];
    memset(pay, 0xAB, PKT_MAX_PAYLOAD);
    long long t = 0;
    int i;

    /* Send 4 full packets (= 4096 bytes) */
    for (i = 0; i < 4; i++)
        gbn_sender_data(&s, t, pay, PKT_MAX_PAYLOAD);

    /* ACK all of them */
    gbn_sender_ack(&s, t+10, 4);

    /* Signal EOF */
    snd_action_t a = gbn_sender_eof(&s, t+10);
    TEST_ASSERT_EQUAL_INT(1, a.send_count);
    pkt_t fp = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, fp.type);
    TEST_ASSERT_EQUAL_UINT32(4, fp.seq);   /* 4 DATA packets */
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_ack ────────────────────────────────────────────
 * ======================================================================= */

/* Duplicate ACK (ack_seq <= base) is silently ignored */
void test_sender_ack_duplicate_ignored(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[1] = {0};

    gbn_sender_data(&s, 0, pay, 1);   /* pkt 0 */
    gbn_sender_data(&s, 0, pay, 1);   /* pkt 1 */
    gbn_sender_ack(&s, 10, 1);        /* base → 1 */

    snd_action_t a = gbn_sender_ack(&s, 20, 1);  /* duplicate */
    TEST_ASSERT_EQUAL_INT(0, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(1, s.base);

    a = gbn_sender_ack(&s, 20, 0);   /* stale */
    TEST_ASSERT_EQUAL_INT(0, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(1, s.base);
}

/* Cumulative ACK slides window by several at once */
void test_sender_ack_cumulative_slide(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 8, 250);
    uint8_t pay[1] = {0};
    int i;

    for (i = 0; i < 5; i++)
        gbn_sender_data(&s, 0, pay, 1);

    /* ACKs 1,2 lost; ACK 3 arrives: slides base from 0 to 3 at once */
    snd_action_t a = gbn_sender_ack(&s, 100, 3);
    TEST_ASSERT_EQUAL_UINT32(3, s.base);
    TEST_ASSERT_EQUAL_INT(0, s.consec_timeouts);
    /* still have pkts 3,4 in flight: timer must be running */
    TEST_ASSERT_TRUE(a.timer_deadline_ms > 0);
}

/* ACK that drains the window stops the timer */
void test_sender_ack_drains_window_stops_timer(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[1] = {0};

    gbn_sender_data(&s, 0, pay, 1);
    gbn_sender_data(&s, 0, pay, 1);

    /* ACK 2: all DATA acked, no EOF yet → window empty, timer stops */
    snd_action_t a = gbn_sender_ack(&s, 50, 2);
    TEST_ASSERT_EQUAL_UINT32(2, s.base);
    TEST_ASSERT_EQUAL_INT(0, (int)a.timer_deadline_ms);
}

/* ACK resets consec_timeouts */
void test_sender_ack_resets_timeout_counter(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 100);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);

    /* burn 3 timeouts */
    gbn_sender_timeout(&s, t+=100);
    gbn_sender_timeout(&s, t+=100);
    gbn_sender_timeout(&s, t+=100);
    TEST_ASSERT_EQUAL_INT(3, s.consec_timeouts);

    /* ACK arrives: counter must reset */
    gbn_sender_ack(&s, t+10, 1);
    TEST_ASSERT_EQUAL_INT(0, s.consec_timeouts);
}

/* FIN ACK sets done and fin_acked flags */
void test_sender_ack_fin_sets_done(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);

    gbn_sender_eof(&s, 0);            /* empty file, FIN seq=0 */
    snd_action_t a = gbn_sender_ack(&s, 10, 1);   /* ACK fin_seq+1 = 1 */
    TEST_ASSERT_TRUE(a.done);
    TEST_ASSERT_TRUE(s.fin_acked);
    TEST_ASSERT_EQUAL_INT(0, (int)a.timer_deadline_ms);
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_timeout ────────────────────────────────────────
 * ======================================================================= */

/* Timeout retransmits every in-flight packet */
void test_sender_timeout_retransmits_window(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[3] = {1,2,3};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 3);   /* pkt 0 */
    pay[0]=4; gbn_sender_data(&s, t, pay, 3);   /* pkt 1 */
    pay[0]=7; gbn_sender_data(&s, t, pay, 3);   /* pkt 2 */

    /* ACK 1: base slides to 1, pkts 1,2 still in flight */
    gbn_sender_ack(&s, t+50, 1);

    /* Timeout: should retransmit pkts 1 and 2 only */
    snd_action_t a = gbn_sender_timeout(&s, t+250);
    TEST_ASSERT_EQUAL_INT(2, a.send_count);
    TEST_ASSERT_EQUAL_INT(1, (int)s.consec_timeouts);
    TEST_ASSERT_EQUAL_INT((int)(t+250+250), (int)a.timer_deadline_ms);

    /* Verify the retransmitted packet seq numbers */
    pkt_t p0 = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    pkt_t p1 = must_decode(a.send_pkts[1].buf, a.send_pkts[1].len);
    TEST_ASSERT_EQUAL_UINT32(1, p0.seq);
    TEST_ASSERT_EQUAL_UINT32(2, p1.seq);
}

/* Timeout includes the FIN when it is in flight */
void test_sender_timeout_includes_fin(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);   /* pkt 0 */
    gbn_sender_eof(&s, t);            /* FIN seq=1, immediately sent */

    /* Both pkt 0 and FIN are unACKed */
    snd_action_t a = gbn_sender_timeout(&s, t+250);
    TEST_ASSERT_EQUAL_INT(2, a.send_count);

    pkt_t p0 = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    pkt_t p1 = must_decode(a.send_pkts[1].buf, a.send_pkts[1].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_DATA, p0.type);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN,  p1.type);
}

/* consec_timeouts increments; give_up fires at exactly GBN_MAX_TIMEOUTS */
void test_sender_timeout_give_up(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 1, 100);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);

    snd_action_t a;
    int i;
    for (i = 0; i < GBN_MAX_TIMEOUTS - 1; i++) {
        t += 100;
        a = gbn_sender_timeout(&s, t);
        TEST_ASSERT_FALSE_MESSAGE(a.give_up, "should not give up yet");
        TEST_ASSERT_EQUAL_INT(i + 1, s.consec_timeouts);
    }

    t += 100;
    a = gbn_sender_timeout(&s, t);
    TEST_ASSERT_TRUE(a.give_up);
    TEST_ASSERT_EQUAL_INT(0, a.send_count);         /* no retransmit on give-up */
    TEST_ASSERT_EQUAL_INT(0, (int)a.timer_deadline_ms); /* timer stopped */
}

/* =========================================================================
 * ── LAYER 2: gbn_recv_init / gbn_recv_pkt ──────────────────────────────
 * ======================================================================= */

void test_recv_init_zeroes_state(void)
{
    gbn_recv_t r;
    memset(&r, 0xFF, sizeof(r));
    gbn_recv_init(&r, 2000);
    TEST_ASSERT_EQUAL_UINT32(0, r.expected);
    TEST_ASSERT_EQUAL_INT(0, r.done);
    TEST_ASSERT_EQUAL_INT(2000, (int)r.linger_ms);
}

/* In-order DATA: payload delivered, expected advances, ACK sent */
void test_recv_inorder_data(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_MAX_LEN];
    uint8_t pay[4] = {1,2,3,4};
    pkt_t pkt;

    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 4), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);

    TEST_ASSERT_EQUAL_UINT16(4, a.payload_len);
    TEST_ASSERT_EQUAL_UINT8(1, a.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(4, a.payload[3]);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, a.ack_len);
    TEST_ASSERT_EQUAL_UINT32(1, r.expected);

    pkt_t ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_ACK, ap.type);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);   /* ACK expected=1 */
    TEST_ASSERT_FALSE(a.done);
}

/* Duplicate packet (seq < expected): discard payload, re-ACK expected */
void test_recv_duplicate_packet(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_MAX_LEN];
    uint8_t pay[2] = {5,6};
    pkt_t pkt;

    /* deliver pkt 0 */
    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 2), &pkt);
    gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_UINT32(1, r.expected);

    /* deliver pkt 0 again (duplicate) */
    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 2), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);   /* discarded */
    pkt_t ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);           /* re-ACK expected=1 */
    TEST_ASSERT_EQUAL_UINT32(1, r.expected);       /* did not advance */
}

/* Packet beyond gap (seq > expected): discard, re-ACK expected */
void test_recv_beyond_gap(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_MAX_LEN];
    uint8_t pay[2] = {1,2};
    pkt_t pkt;

    /* pkt 0 delivered fine */
    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 2), &pkt);
    gbn_recv_pkt(&r, 0, &pkt);

    /* pkt 2 arrives (gap at 1) */
    pkt_decode(raw, pkt_encode_data(raw, 2, pay, 2), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);    /* discarded */
    pkt_t ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);            /* re-ACK expected=1 */
    TEST_ASSERT_EQUAL_UINT32(1, r.expected);        /* did not advance */

    /* pkt 5 arrives (even further ahead) */
    pkt_decode(raw, pkt_encode_data(raw, 5, pay, 2), &pkt);
    a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);
    ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);
}

/* FIN at the right seq: done set, correct ACK, linger deadline */
void test_recv_fin_sets_done(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_HDR_LEN];
    pkt_t pkt;

    pkt_decode(raw, pkt_encode_fin(raw, 0), &pkt);   /* FIN seq=0 (empty file) */
    rcv_action_t a = gbn_recv_pkt(&r, 1000, &pkt);

    TEST_ASSERT_TRUE(a.done);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);
    TEST_ASSERT_EQUAL_INT(3000, (int)a.linger_deadline_ms); /* 1000+2000 */

    pkt_t ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_ACK, ap.type);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);   /* ACK fin_seq+1 = 1 */
}

/* Repeated FIN after done: re-ACK the same seq (linger behaviour) */
void test_recv_repeated_fin(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_MAX_LEN];
    uint8_t pay[3] = {1,2,3};
    pkt_t pkt;

    /* deliver two DATA pkts then a FIN */
    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 3), &pkt);
    gbn_recv_pkt(&r, 0, &pkt);
    pkt_decode(raw, pkt_encode_data(raw, 1, pay, 3), &pkt);
    gbn_recv_pkt(&r, 0, &pkt);

    pkt_decode(raw, pkt_encode_fin(raw, 2), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_TRUE(a.done);
    TEST_ASSERT_EQUAL_UINT32(3, r.expected);

    /* FIN arrives again (relay resent it): receiver re-ACKs */
    pkt_decode(raw, pkt_encode_fin(raw, 2), &pkt);
    rcv_action_t a2 = gbn_recv_pkt(&r, 500, &pkt);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, a2.ack_len);
    pkt_t ap = must_decode(a2.ack_buf, a2.ack_len);
    TEST_ASSERT_EQUAL_UINT32(3, ap.seq);   /* still ACK 3 */
}

/* ACK packets arriving at receiver are silently ignored */
void test_recv_ignores_ack_packets(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_HDR_LEN];
    pkt_t pkt;

    pkt_decode(raw, pkt_encode_ack(raw, 5), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_size_t(0, a.ack_len);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);
    TEST_ASSERT_FALSE(a.done);
}

/* =========================================================================
 * ── FULL IN-MEMORY TRANSFER with simulated channel damage ───────────────
 *
 * Sender and receiver state machines connected by two chan_t queues.
 * The channel applies 20% loss, 20% corrupt, 20% dup independently to
 * every packet in both directions.  A fixed LCG seed makes it deterministic.
 *
 * We run the protocol to completion with a fake monotonic clock that
 * advances by cfg.timeout_ms every time the sender would block waiting for
 * an ACK, simulating timer expiry without sleeping.
 * ======================================================================= */

/* Simulate one complete file transfer through a damaging channel.
 * file_bytes: the "file" (caller-allocated).
 * file_len:   total bytes.
 * winsize:    sender window.
 * seed:       LCG seed for reproducibility.
 * Returns 1 if the received bytes exactly match the sent bytes, 0 on failure
 * (give-up or data corruption). */
static int run_transfer(const uint8_t *file_bytes, size_t file_len,
                        int winsize, uint32_t seed)
{
    lcg_seed(seed);

    gbn_sender_t snd;
    gbn_recv_t   rcv;
    gbn_sender_init(&snd, winsize, 250);
    gbn_recv_init(&rcv, 2000);

    /* Channels: snd→rcv (data_chan) and rcv→snd (ack_chan) */
    chan_t data_chan, ack_chan;
    chan_init(&data_chan);
    chan_init(&ack_chan);

    /* Buffer to collect what the receiver delivers */
    uint8_t *received = (uint8_t *)malloc(file_len + PKT_MAX_PAYLOAD);
    if (!received) return 0;
    size_t   recv_len = 0;

    long long t         = 0;
    size_t    file_pos  = 0;
    int       eof_done  = 0;

    /* Safety valve: a correct implementation finishes in well under
     * file_len/1024 * MAX_TIMEOUTS * 2 iterations. */
    int max_iter = (int)((file_len / PKT_MAX_PAYLOAD + 2)
                         * (GBN_MAX_TIMEOUTS + 1) * 100);
    if (max_iter < 10000) max_iter = 10000;

    int iter;
    for (iter = 0; iter < max_iter; iter++) {

        /* ── Sender: fill window from file ── */
        while (!gbn_sender_window_full(&snd) && !snd.eof_seen) {
            size_t remaining = file_len - file_pos;
            if (remaining == 0) {
                snd_action_t ea = gbn_sender_eof(&snd, t);
                int k;
                for (k = 0; k < ea.send_count; k++)
                    chan_push(&data_chan, ea.send_pkts[k].buf,
                              ea.send_pkts[k].len);
                eof_done = 1;
                break;
            }
            uint16_t chunk = (uint16_t)(remaining > PKT_MAX_PAYLOAD
                                        ? PKT_MAX_PAYLOAD : remaining);
            snd_action_t da = gbn_sender_data(&snd, t,
                                              file_bytes + file_pos, chunk);
            if (da.send_count == 0) break;  /* window unexpectedly full */
            int k;
            for (k = 0; k < da.send_count; k++)
                chan_push(&data_chan, da.send_pkts[k].buf,
                          da.send_pkts[k].len);
            file_pos += chunk;
            if (chunk < PKT_MAX_PAYLOAD && !eof_done) {
                snd_action_t ea = gbn_sender_eof(&snd, t);
                for (k = 0; k < ea.send_count; k++)
                    chan_push(&data_chan, ea.send_pkts[k].buf,
                              ea.send_pkts[k].len);
                eof_done = 1;
                break;
            }
        }

        /* ── Channel: deliver one data packet to receiver ── */
        if (data_chan.count > 0) {
            uint8_t raw[PKT_MAX_LEN];
            size_t  rawlen;

            /* peek at the front, run through damage, deliver to ack side */
            chan_entry_t front = data_chan.entries[data_chan.head];
            data_chan.head  = (data_chan.head + 1) % CHAN_CAP;
            data_chan.count--;

            /* apply damage into a temporary entry */
            chan_t tmp_src, tmp_dst;
            chan_init(&tmp_src); chan_init(&tmp_dst);
            chan_push(&tmp_src, front.data, front.len);
            chan_deliver(&tmp_src, &tmp_dst, 0.20, 0.20, 0.20);

            /* drain tmp_dst: feed each surviving copy to the receiver */
            while (tmp_dst.count > 0) {
                chan_entry_t e = tmp_dst.entries[tmp_dst.head];
                tmp_dst.head  = (tmp_dst.head + 1) % CHAN_CAP;
                tmp_dst.count--;

                rawlen = e.len;
                memcpy(raw, e.data, rawlen);

                pkt_t pkt;
                if (!pkt_decode(raw, rawlen, &pkt)) continue;

                rcv_action_t ra = gbn_recv_pkt(&rcv, t, &pkt);
                if (ra.payload_len > 0) {
                    memcpy(received + recv_len, ra.payload, ra.payload_len);
                    recv_len += ra.payload_len;
                }
                if (ra.ack_len > 0)
                    chan_push(&ack_chan, ra.ack_buf, ra.ack_len);
            }
        }

        /* ── Channel: deliver one ACK to sender ── */
        if (ack_chan.count > 0) {
            chan_t tmp_src, tmp_dst;
            chan_init(&tmp_src); chan_init(&tmp_dst);

            chan_entry_t front = ack_chan.entries[ack_chan.head];
            ack_chan.head  = (ack_chan.head + 1) % CHAN_CAP;
            ack_chan.count--;
            chan_push(&tmp_src, front.data, front.len);
            chan_deliver(&tmp_src, &tmp_dst, 0.20, 0.20, 0.20);

            while (tmp_dst.count > 0) {
                chan_entry_t e = tmp_dst.entries[tmp_dst.head];
                tmp_dst.head  = (tmp_dst.head + 1) % CHAN_CAP;
                tmp_dst.count--;

                pkt_t pkt;
                if (!pkt_decode(e.data, e.len, &pkt)) continue;
                if (pkt.type != PKT_TYPE_ACK) continue;

                snd_action_t sa = gbn_sender_ack(&snd, t, pkt.seq);
                int k;
                for (k = 0; k < sa.send_count; k++)
                    chan_push(&data_chan, sa.send_pkts[k].buf,
                              sa.send_pkts[k].len);
                if (sa.done) {
                    int ok = (recv_len == file_len) &&
                             (file_len == 0 ||
                              memcmp(received, file_bytes, file_len) == 0);
                    free(received);
                    return ok;
                }
            }
        }

        /* ── Timer: if sender has nothing pending and both channels are
         *    empty, advance the clock to trigger a timeout ── */
        if (data_chan.count == 0 && ack_chan.count == 0
                && snd.timer_deadline_ms > 0) {
            t = snd.timer_deadline_ms;
            snd_action_t ta = gbn_sender_timeout(&snd, t);
            if (ta.give_up) { free(received); return 0; }
            int k;
            for (k = 0; k < ta.send_count; k++)
                chan_push(&data_chan, ta.send_pkts[k].buf,
                          ta.send_pkts[k].len);
        }
    }

    /* Did not finish within max_iter */
    free(received);
    return 0;
}

/* Small file, seed 42 */
void test_channel_small_file_seed42(void)
{
    uint8_t file[500];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i & 0xff);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 8, 42));
}

/* Small file, seed 1234 – different damage pattern */
void test_channel_small_file_seed1234(void)
{
    uint8_t file[500];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i * 3);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 8, 1234));
}

/* Exact multiple of 1024: no partial last packet, FIN immediately follows */
void test_channel_exact_1024_multiple(void)
{
    uint8_t file[4096];   /* exactly 4 DATA packets */
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i ^ 0xA5);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 8, 7));
}

/* Empty file: just a FIN, no DATA */
void test_channel_empty_file(void)
{
    TEST_ASSERT_TRUE(run_transfer(NULL, 0, 8, 99));
}

/* Larger file, window=1 (stop-and-wait mode) */
void test_channel_large_file_window1(void)
{
    uint8_t file[5000];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i * 7);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 1, 13));
}

/* Larger file, window=16 */
void test_channel_large_file_window16(void)
{
    uint8_t file[16000];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i * 3 + 1);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 16, 2025));
}

/* Different seed to vary damage positions */
void test_channel_seed_999(void)
{
    uint8_t file[3000];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 4, 999));
}

/* =========================================================================
 * main – Unity test runner
 * ======================================================================= */
int main(void)
{
    UNITY_BEGIN();

    /* pkt_checksum */
    RUN_TEST(test_checksum_rfc1071_example);
    RUN_TEST(test_checksum_verify_roundtrip);
    RUN_TEST(test_checksum_odd_length);
    RUN_TEST(test_checksum_odd_three_bytes);
    RUN_TEST(test_checksum_data_hi);
    RUN_TEST(test_checksum_ack3_verify);
    RUN_TEST(test_checksum_detects_single_bit_flip);

    /* pkt_encode_* */
    RUN_TEST(test_encode_data_length);
    RUN_TEST(test_encode_data_fields);
    RUN_TEST(test_encode_data_max_payload);
    RUN_TEST(test_encode_ack_length_and_fields);
    RUN_TEST(test_encode_fin_length_and_fields);
    RUN_TEST(test_encode_seq_boundary_values);
    RUN_TEST(test_encode_checksum_embeds_correctly);

    /* pkt_decode */
    RUN_TEST(test_decode_valid_data);
    RUN_TEST(test_decode_valid_ack);
    RUN_TEST(test_decode_valid_fin);
    RUN_TEST(test_decode_rejects_too_short);
    RUN_TEST(test_decode_rejects_length_mismatch_short);
    RUN_TEST(test_decode_rejects_length_mismatch_long);
    RUN_TEST(test_decode_rejects_length_over_max);
    RUN_TEST(test_decode_rejects_unknown_type);
    RUN_TEST(test_decode_rejects_nonzero_reserved);
    RUN_TEST(test_decode_rejects_corrupted_payload);
    RUN_TEST(test_decode_rejects_corrupted_header);

    /* gbn_sender_init / window_full */
    RUN_TEST(test_sender_init_zeroes_state);
    RUN_TEST(test_sender_window_not_full_initially);
    RUN_TEST(test_sender_window_full_at_capacity);

    /* gbn_sender_data */
    RUN_TEST(test_sender_data_sends_one_packet);
    RUN_TEST(test_sender_data_packet_decodes_correctly);
    RUN_TEST(test_sender_data_ignored_when_window_full);
    RUN_TEST(test_sender_data_timer_starts_on_first);

    /* gbn_sender_eof */
    RUN_TEST(test_sender_eof_empty_file);
    RUN_TEST(test_sender_eof_deferred_when_window_full);
    RUN_TEST(test_sender_eof_idempotent);
    RUN_TEST(test_sender_exact_multiple_of_1024);

    /* gbn_sender_ack */
    RUN_TEST(test_sender_ack_duplicate_ignored);
    RUN_TEST(test_sender_ack_cumulative_slide);
    RUN_TEST(test_sender_ack_drains_window_stops_timer);
    RUN_TEST(test_sender_ack_resets_timeout_counter);
    RUN_TEST(test_sender_ack_fin_sets_done);

    /* gbn_sender_timeout */
    RUN_TEST(test_sender_timeout_retransmits_window);
    RUN_TEST(test_sender_timeout_includes_fin);
    RUN_TEST(test_sender_timeout_give_up);

    /* gbn_recv_init / gbn_recv_pkt */
    RUN_TEST(test_recv_init_zeroes_state);
    RUN_TEST(test_recv_inorder_data);
    RUN_TEST(test_recv_duplicate_packet);
    RUN_TEST(test_recv_beyond_gap);
    RUN_TEST(test_recv_fin_sets_done);
    RUN_TEST(test_recv_repeated_fin);
    RUN_TEST(test_recv_ignores_ack_packets);

    /* in-memory channel transfers */
    RUN_TEST(test_channel_small_file_seed42);
    RUN_TEST(test_channel_small_file_seed1234);
    RUN_TEST(test_channel_exact_1024_multiple);
    RUN_TEST(test_channel_empty_file);
    RUN_TEST(test_channel_large_file_window1);
    RUN_TEST(test_channel_large_file_window16);
    RUN_TEST(test_channel_seed_999);

    return UNITY_END();
}