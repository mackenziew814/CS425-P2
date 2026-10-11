/*
 * lab.h  –  CS 425 Project 2: testable Go-Back-N protocol library
 *
 * Three layers, cleanly separated:
 *
 *   Layer 1  pkt_*        Pure packet encode/decode/validate.
 *                         No state.  Bytes in, bytes or struct out.
 *
 *   Layer 2  gbn_sender_t  Go-Back-N sender state machine.
 *            gbn_recv_t    Go-Back-N receiver state machine.
 *                         No sockets, no clocks, no files.
 *                         The current time is always passed in (ms).
 *                         Each function returns what to do next.
 *
 *   Layer 3  run_send() / run_recv()  in myapp.c
 *                         Thin I/O shell: socket, poll, clock, fread/fwrite.
 *                         Calls layer 2, acts on what comes back.
 *
 * Tests drive layer 2 directly with a fake clock and an in-memory channel.
 * A "lost" packet is just one the test does not forward.
 * A "timeout" is just passing a larger value for `now_ms`.
 */

#ifndef LAB_H
#define LAB_H

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * LAYER 1 – Packet encode / decode / validate
 * ======================================================================= */

/* Wire-format constants */
#define PKT_TYPE_DATA    0
#define PKT_TYPE_ACK     1
#define PKT_TYPE_FIN     2

#define PKT_HDR_LEN      10
#define PKT_MAX_PAYLOAD  1024
#define PKT_MAX_LEN      (PKT_HDR_LEN + PKT_MAX_PAYLOAD)   /* 1034 */

/* Byte offsets within a flat wire buffer */
#define OFF_TYPE         0
#define OFF_RSVD         1
#define OFF_CKSUM        2   /* 2 bytes, big-endian */
#define OFF_SEQ          4   /* 4 bytes, big-endian */
#define OFF_LEN          8   /* 2 bytes, big-endian */
#define OFF_PAYLOAD      10

/* ------------------------------------------------------------------
 * pkt_checksum(buf, len)
 *
 * RFC 1071 Internet checksum: 16-bit one's-complement of the
 * one's-complement sum, treating the data as big-endian 16-bit words.
 * An odd-length buffer is padded with one zero byte (high byte) for
 * the calculation only.
 *
 * To COMPUTE: call with checksum field zeroed; store the result.
 * To VERIFY:  call over the whole packet including the stored checksum.
 *             Returns 0x0000 for an undamaged packet.
 * ------------------------------------------------------------------ */
uint16_t pkt_checksum(const uint8_t *buf, size_t len);

/* ------------------------------------------------------------------
 * pkt_encode_data(buf, seq, payload, paylen)  →  total bytes written
 *
 * Serialises a DATA packet into buf (which must be >= PKT_MAX_LEN).
 * paylen must be in [1, PKT_MAX_PAYLOAD].
 * ------------------------------------------------------------------ */
size_t pkt_encode_data(uint8_t *buf, uint32_t seq,
                       const uint8_t *payload, uint16_t paylen);

/* ------------------------------------------------------------------
 * pkt_encode_ack(buf, seq)  →  total bytes written (always PKT_HDR_LEN)
 *
 * seq is the next expected packet index (cumulative ACK).
 * ------------------------------------------------------------------ */
size_t pkt_encode_ack(uint8_t *buf, uint32_t seq);

/* ------------------------------------------------------------------
 * pkt_encode_fin(buf, seq)  →  total bytes written (always PKT_HDR_LEN)
 *
 * seq is the number of DATA packets (= index after last DATA).
 * ------------------------------------------------------------------ */
size_t pkt_encode_fin(uint8_t *buf, uint32_t seq);

/* ------------------------------------------------------------------
 * pkt_decode result: filled in by pkt_decode() on success.
 * ------------------------------------------------------------------ */
typedef struct {
    uint8_t         type;        /* PKT_TYPE_DATA / ACK / FIN */
    uint32_t        seq;
    uint16_t        payload_len; /* 0 for ACK / FIN */
    const uint8_t  *payload;     /* points into the original buffer */
} pkt_t;

/* ------------------------------------------------------------------
 * pkt_decode(buf, len, out)  →  1 if valid, 0 if invalid (discard)
 *
 * Validates all four spec rules and fills *out on success:
 *   1. datagram >= 10 bytes
 *   2. 10 + length == datagram size, length <= 1024
 *   3. type in {0,1,2}, reserved == 0
 *   4. checksum verifies (== 0x0000 when re-computed over the packet)
 * ------------------------------------------------------------------ */
int pkt_decode(const uint8_t *buf, size_t len, pkt_t *out);


/* =========================================================================
 * LAYER 2 – Go-Back-N state machines
 *
 * Neither machine calls sendto, recvfrom, poll, clock_gettime, fread,
 * or fwrite.  The current wall-clock time is always passed in as `now_ms`
 * (a long long, milliseconds from an arbitrary epoch – use CLOCK_MONOTONIC).
 * ======================================================================= */

#define GBN_MAX_WINDOW     64
#define GBN_MAX_TIMEOUTS   10   /* give-up threshold */

/* ------------------------------------------------------------------
 * snd_action_t: what the sender wants the I/O layer to do.
 *
 * The I/O layer inspects this after every gbn_sender_* call and acts
 * on it before calling again.
 *
 * send_count packets starting at send_pkts[0] should be sent (in order).
 * Each entry is a (buf, len) pair pointing into the sender's own window
 * buffer – the I/O layer must not free them.
 *
 * timer_deadline_ms:
 *   > 0  start/restart the timer; fire at this absolute time (ms)
 *   = 0  stop the timer (nothing in flight)
 *
 * done:     transfer complete; I/O layer should exit 0.
 * give_up:  10 consecutive timeouts; I/O layer should exit 2.
 * ------------------------------------------------------------------ */
typedef struct {
    uint8_t  *buf;
    size_t    len;
} snd_iovec_t;

typedef struct {
    snd_iovec_t   send_pkts[GBN_MAX_WINDOW + 1]; /* +1 for the FIN */
    int           send_count;
    long long     timer_deadline_ms;              /* 0 = stop */
    int           done;
    int           give_up;
} snd_action_t;

/* ------------------------------------------------------------------
 * gbn_sender_t: all sender state.  Zero-initialise, then call
 * gbn_sender_init() before any other function.
 * ------------------------------------------------------------------ */
typedef struct {
    /* configuration (set by init, never changed) */
    int       winsize;
    long long timeout_ms;

    /* sliding-window state */
    uint32_t  base;          /* oldest unACK'd seq */
    uint32_t  next;          /* next seq never sent */
    uint32_t  fin_seq;       /* seq of the FIN packet */
    int       eof_seen;      /* file data is exhausted */
    int       fin_sent;      /* FIN has been transmitted at least once */
    int       fin_acked;     /* ACK fin_seq+1 received */

    /* timer */
    long long timer_deadline_ms;  /* 0 = stopped */
    int       consec_timeouts;

    /* window buffer: circular, indexed by seq % winsize */
    uint8_t   winbuf[GBN_MAX_WINDOW][PKT_MAX_LEN];
    size_t    winlen[GBN_MAX_WINDOW];
} gbn_sender_t;

/* ------------------------------------------------------------------
 * gbn_sender_init(s, winsize, timeout_ms)
 *
 * Zeroes s and stores configuration.  Call once before anything else.
 * winsize must be in [1, GBN_MAX_WINDOW].
 * ------------------------------------------------------------------ */
void gbn_sender_init(gbn_sender_t *s, int winsize, long long timeout_ms);

/* ------------------------------------------------------------------
 * gbn_sender_data(s, now_ms, payload, paylen)  →  snd_action_t
 *
 * Offer one chunk of file data (up to PKT_MAX_PAYLOAD bytes) to the
 * sender.  If the window has room the packet is built, stored, and
 * included in the returned action.  If the window is full the payload
 * is ignored – the I/O layer must not advance the file position.
 *
 * The I/O layer calls this repeatedly until send_count == 0, which
 * means the window is full and the layer should wait for ACKs.
 * ------------------------------------------------------------------ */
snd_action_t gbn_sender_data(gbn_sender_t *s, long long now_ms,
                              const uint8_t *payload, uint16_t paylen);

/* ------------------------------------------------------------------
 * gbn_sender_eof(s, now_ms)  →  snd_action_t
 *
 * Signal end-of-file.  The sender will build and send the FIN once
 * all preceding DATA packets have been sent (and the window allows).
 * May be called before gbn_sender_data drains the window; the FIN
 * will be queued and sent when gbn_sender_timeout or gbn_sender_ack
 * next opens a slot.
 * ------------------------------------------------------------------ */
snd_action_t gbn_sender_eof(gbn_sender_t *s, long long now_ms);

/* ------------------------------------------------------------------
 * gbn_sender_ack(s, now_ms, ack_seq)  →  snd_action_t
 *
 * Feed a received, validated ACK seq number.  Slides base, resets the
 * consecutive-timeout counter, and may open room in the window.
 * The action's send_count will be 0 (the I/O layer fills new slots
 * from the file on its next pass); timer_deadline_ms reflects the new
 * in-flight state.
 * ------------------------------------------------------------------ */
snd_action_t gbn_sender_ack(gbn_sender_t *s, long long now_ms,
                             uint32_t ack_seq);

/* ------------------------------------------------------------------
 * gbn_sender_timeout(s, now_ms)  →  snd_action_t
 *
 * The timer has fired.  Retransmits everything from base to next-1
 * (including the FIN if sent), increments consec_timeouts, and
 * restarts the timer.  Sets give_up when consec_timeouts reaches
 * GBN_MAX_TIMEOUTS.
 * ------------------------------------------------------------------ */
snd_action_t gbn_sender_timeout(gbn_sender_t *s, long long now_ms);

/* ------------------------------------------------------------------
 * gbn_sender_window_full(s)  →  1 if the window is full, 0 if there
 * is room.  Convenience for the I/O layer's read loop.
 * ------------------------------------------------------------------ */
int gbn_sender_window_full(const gbn_sender_t *s);

/* ------------------------------------------------------------------
 * rcv_action_t: what the receiver wants the I/O layer to do.
 *
 * If ack_len > 0, send ack_buf back to the relay.
 * If payload_len > 0, write payload_len bytes starting at payload
 *   to the output file.
 * done: the FIN has been processed and the file closed; the I/O layer
 *   should now enter the 2-second linger.
 * linger_deadline_ms: absolute time (ms) when the linger ends.
 *   Meaningful only when done == 1.
 * ------------------------------------------------------------------ */
typedef struct {
    uint8_t        ack_buf[PKT_HDR_LEN];
    size_t         ack_len;            /* 0 = nothing to send */

    const uint8_t *payload;            /* points into the caller's buffer */
    uint16_t       payload_len;        /* 0 = nothing to write */

    int            done;               /* FIN processed */
    long long      linger_deadline_ms; /* meaningful when done == 1 */
} rcv_action_t;

/* ------------------------------------------------------------------
 * gbn_recv_t: all receiver state.  Zero-initialise, then call
 * gbn_recv_init() before any other function.
 * ------------------------------------------------------------------ */
typedef struct {
    uint32_t expected;          /* next seq the receiver wants */
    int      done;              /* FIN has been processed */
    long long linger_ms;        /* linger duration (ms) */
} gbn_recv_t;

/* ------------------------------------------------------------------
 * gbn_recv_init(r, linger_ms)
 *
 * Zeroes r and stores the linger duration.  Call once before anything.
 * ------------------------------------------------------------------ */
void gbn_recv_init(gbn_recv_t *r, long long linger_ms);

/* ------------------------------------------------------------------
 * gbn_recv_pkt(r, now_ms, pkt)  →  rcv_action_t
 *
 * Feed one already-validated and decoded packet to the receiver.
 * The caller must have called pkt_decode() first (layer 1).
 * Returns what to ACK, what to write, and whether the transfer is done.
 * ------------------------------------------------------------------ */
rcv_action_t gbn_recv_pkt(gbn_recv_t *r, long long now_ms,
                           const pkt_t *pkt);

#endif /* LAB_H */