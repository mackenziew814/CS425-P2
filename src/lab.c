/*
 * lab.c  –  CS 425 Project 2: Go-Back-N protocol library implementation
 *
 * Layer 1: pure packet encode / decode / validate (pkt_*)
 * Layer 2: Go-Back-N state machines (gbn_sender_*, gbn_recv_*)
 *
 * No sockets.  No clocks.  No file I/O.
 */

#include "lab.h"

#include <string.h>   /* memset, memcpy */
#include <stddef.h>

/* =========================================================================
 * Internal helpers: big-endian field read / write
 * ======================================================================= */

static void write16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xff);
}

static void write32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >>  8) & 0xff);
    p[3] = (uint8_t)(v & 0xff);
}

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

/* =========================================================================
 * LAYER 1 – pkt_checksum
 * ======================================================================= */

uint16_t pkt_checksum(const uint8_t *buf, size_t len)
{
    uint32_t sum = 0;
    size_t i;

    for (i = 0; i + 1 < len; i += 2)
        sum += ((uint32_t)buf[i] << 8) | buf[i + 1];

    if (i < len)                      /* odd byte: pad high, zero low */
        sum += (uint32_t)buf[i] << 8;

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return (uint16_t)(~sum);
}

/* =========================================================================
 * LAYER 1 – pkt_encode_*
 * ======================================================================= */

size_t pkt_encode_data(uint8_t *buf, uint32_t seq,
                       const uint8_t *payload, uint16_t paylen)
{
    buf[OFF_TYPE] = PKT_TYPE_DATA;
    buf[OFF_RSVD] = 0;
    buf[OFF_CKSUM]     = 0;
    buf[OFF_CKSUM + 1] = 0;
    write32(buf + OFF_SEQ, seq);
    write16(buf + OFF_LEN, paylen);
    memcpy(buf + OFF_PAYLOAD, payload, paylen);

    size_t total = (size_t)PKT_HDR_LEN + paylen;
    write16(buf + OFF_CKSUM, pkt_checksum(buf, total));
    return total;
}

size_t pkt_encode_ack(uint8_t *buf, uint32_t seq)
{
    buf[OFF_TYPE] = PKT_TYPE_ACK;
    buf[OFF_RSVD] = 0;
    buf[OFF_CKSUM]     = 0;
    buf[OFF_CKSUM + 1] = 0;
    write32(buf + OFF_SEQ, seq);
    write16(buf + OFF_LEN, 0);

    write16(buf + OFF_CKSUM, pkt_checksum(buf, PKT_HDR_LEN));
    return PKT_HDR_LEN;
}

size_t pkt_encode_fin(uint8_t *buf, uint32_t seq)
{
    buf[OFF_TYPE] = PKT_TYPE_FIN;
    buf[OFF_RSVD] = 0;
    buf[OFF_CKSUM]     = 0;
    buf[OFF_CKSUM + 1] = 0;
    write32(buf + OFF_SEQ, seq);
    write16(buf + OFF_LEN, 0);

    write16(buf + OFF_CKSUM, pkt_checksum(buf, PKT_HDR_LEN));
    return PKT_HDR_LEN;
}

/* =========================================================================
 * LAYER 1 – pkt_decode
 * ======================================================================= */

int pkt_decode(const uint8_t *buf, size_t len, pkt_t *out)
{
    /* Rule 1: minimum size */
    if (len < PKT_HDR_LEN)
        return 0;

    /* Rule 2: length field consistency – read before touching payload */
    uint16_t payload_len = read16(buf + OFF_LEN);
    if (payload_len > PKT_MAX_PAYLOAD)
        return 0;
    if (len != (size_t)PKT_HDR_LEN + payload_len)
        return 0;

    /* Rule 3: type and reserved */
    uint8_t type = buf[OFF_TYPE];
    if (type > PKT_TYPE_FIN)
        return 0;
    if (buf[OFF_RSVD] != 0)
        return 0;

    /* Rule 4: checksum – valid packet gives 0x0000 */
    if (pkt_checksum(buf, len) != 0x0000)
        return 0;

    out->type        = type;
    out->seq         = read32(buf + OFF_SEQ);
    out->payload_len = payload_len;
    out->payload     = (payload_len > 0) ? (buf + OFF_PAYLOAD) : NULL;
    return 1;
}

/* =========================================================================
 * LAYER 2 – gbn_sender helpers
 * ======================================================================= */

/* empty action – caller fills in what it needs */
static snd_action_t make_empty_action(const gbn_sender_t *s)
{
    snd_action_t a;
    memset(&a, 0, sizeof(a));
    a.timer_deadline_ms = s->timer_deadline_ms;
    return a;
}

/*
 * queue_send: add one window slot to a's send list.
 * slot must be in [0, GBN_MAX_WINDOW).
 */
static void queue_send(snd_action_t *a, gbn_sender_t *s, int slot)
{
    int i = a->send_count;
    a->send_pkts[i].buf = s->winbuf[slot];
    a->send_pkts[i].len = s->winlen[slot];
    a->send_count++;
}

/*
 * try_send_fin: if EOF has been seen and all DATA sent, build+queue the FIN.
 * Returns 1 if FIN was queued, 0 otherwise.
 */
static int try_send_fin(gbn_sender_t *s, snd_action_t *a, long long now_ms)
{
    if (!s->eof_seen || s->fin_sent)
        return 0;
    /* Invariant: fin_seq == next at the moment eof_seen is set, and next
     * is never incremented again until try_send_fin itself sets it to
     * fin_seq+1.  So next == fin_seq always holds here. */

    int slot = (int)(s->fin_seq % (uint32_t)s->winsize);
    s->winlen[slot] = pkt_encode_fin(s->winbuf[slot], s->fin_seq);
    s->fin_sent = 1;
    s->next     = s->fin_seq + 1;

    if (s->timer_deadline_ms == 0)
        s->timer_deadline_ms = now_ms + s->timeout_ms;

    queue_send(a, s, slot);
    a->timer_deadline_ms = s->timer_deadline_ms;
    return 1;
}

/* =========================================================================
 * LAYER 2 – gbn_sender_* public API
 * ======================================================================= */

void gbn_sender_init(gbn_sender_t *s, int winsize, long long timeout_ms)
{
    memset(s, 0, sizeof(*s));
    s->winsize    = winsize;
    s->timeout_ms = timeout_ms;
}

int gbn_sender_window_full(const gbn_sender_t *s)
{
    return (int)(s->next - s->base) >= s->winsize;
}

snd_action_t gbn_sender_data(gbn_sender_t *s, long long now_ms,
                              const uint8_t *payload, uint16_t paylen)
{
    snd_action_t a = make_empty_action(s);

    /* Window full or EOF already signalled: caller must not advance file */
    if (gbn_sender_window_full(s) || s->eof_seen)
        return a;

    int slot = (int)(s->next % (uint32_t)s->winsize);
    s->winlen[slot] = pkt_encode_data(s->winbuf[slot], s->next,
                                      payload, paylen);

    /* Start timer on first in-flight packet */
    if (s->timer_deadline_ms == 0)
        s->timer_deadline_ms = now_ms + s->timeout_ms;

    s->next++;
    queue_send(&a, s, slot);
    a.timer_deadline_ms = s->timer_deadline_ms;

    /* If this was a short read the caller will follow with gbn_sender_eof */
    return a;
}

snd_action_t gbn_sender_eof(gbn_sender_t *s, long long now_ms)
{
    snd_action_t a = make_empty_action(s);

    if (s->eof_seen)
        return a;   /* idempotent */

    s->eof_seen = 1;
    s->fin_seq  = s->next;  /* FIN seq = number of DATA packets */

    /* If the window has room, send the FIN immediately */
    if (!gbn_sender_window_full(s))
        try_send_fin(s, &a, now_ms);

    return a;
}

snd_action_t gbn_sender_ack(gbn_sender_t *s, long long now_ms,
                             uint32_t ack_seq)
{
    snd_action_t a = make_empty_action(s);

    /* Duplicate or stale: ignore */
    if (ack_seq <= s->base)
        return a;

    /* Advance base */
    s->base            = ack_seq;
    s->consec_timeouts = 0;

    /* Check for FIN ACK */
    if (s->fin_sent && ack_seq == s->fin_seq + 1) {
        s->fin_acked          = 1;
        s->timer_deadline_ms  = 0;
        a.timer_deadline_ms   = 0;
        a.done                = 1;
        return a;
    }

    /* Anything still in flight? */
    int in_flight = s->fin_sent
                    ? (s->base <= s->fin_seq)
                    : (s->base < s->next);

    if (in_flight) {
        s->timer_deadline_ms = now_ms + s->timeout_ms;
    } else {
        /* Window empty and FIN not yet sent: stop timer; the FIN send
         * (triggered when the I/O layer calls gbn_sender_eof or the next
         * gbn_sender_data pass) will restart it. */
        s->timer_deadline_ms = 0;
    }
    a.timer_deadline_ms = s->timer_deadline_ms;

    /*
     * If EOF has been seen and the window just drained below capacity,
     * try to send the FIN now.
     */
    if (!gbn_sender_window_full(s))
        try_send_fin(s, &a, now_ms);

    return a;
}

snd_action_t gbn_sender_timeout(gbn_sender_t *s, long long now_ms)
{
    snd_action_t a = make_empty_action(s);

    s->consec_timeouts++;
    if (s->consec_timeouts >= GBN_MAX_TIMEOUTS) {
        a.give_up           = 1;
        a.timer_deadline_ms = 0;
        s->timer_deadline_ms = 0;
        return a;
    }

    /* Retransmit base … next-1 (next is fin_seq+1 when FIN is in flight) */
    uint32_t end = s->fin_sent ? s->fin_seq + 1 : s->next;
    uint32_t seq;
    for (seq = s->base; seq < end; seq++) {
        int slot = (int)(seq % (uint32_t)s->winsize);
        queue_send(&a, s, slot);
    }

    /* Restart timer */
    s->timer_deadline_ms = now_ms + s->timeout_ms;
    a.timer_deadline_ms  = s->timer_deadline_ms;
    return a;
}

/* =========================================================================
 * LAYER 2 – gbn_recv_* public API
 * ======================================================================= */

void gbn_recv_init(gbn_recv_t *r, long long linger_ms)
{
    memset(r, 0, sizeof(*r));
    r->linger_ms = linger_ms;
}

rcv_action_t gbn_recv_pkt(gbn_recv_t *r, long long now_ms,
                           const pkt_t *pkt)
{
    rcv_action_t a;
    memset(&a, 0, sizeof(a));

    if (pkt->type == PKT_TYPE_DATA) {
        if (pkt->seq == r->expected) {
            /* In-order: deliver payload and advance expected */
            a.payload     = pkt->payload;
            a.payload_len = pkt->payload_len;
            r->expected++;
        }
        /* Out-of-order or duplicate: fall through, ACK current expected */
        a.ack_len = pkt_encode_ack(a.ack_buf, r->expected);

    } else if (pkt->type == PKT_TYPE_FIN) {
        if (pkt->seq == r->expected) {
            /* Expected FIN: transfer complete */
            r->expected++;
            r->done = 1;
            a.ack_len            = pkt_encode_ack(a.ack_buf, r->expected);
            a.done               = 1;
            a.linger_deadline_ms = now_ms + r->linger_ms;
        } else {
            /* Out-of-order FIN: re-ACK what we're waiting for */
            a.ack_len = pkt_encode_ack(a.ack_buf, r->expected);
        }
    }
    /* ACK packets arriving at the receiver are silently ignored (ack_len=0) */

    return a;
}