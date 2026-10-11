/*
 * myapp.c  –  CS 425 Project 2: Go-Back-N over UDP
 *
 * Layer 3 (I/O) only.  All protocol logic lives in lab.c (lab.h).
 *
 * Build:  make release   →  ./build/release/myapp
 * Usage (no args):  prints usage and exits 0  (required by `make leak`)
 */

#include "lab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>

/* -------------------------------------------------------------------------
 * CLI constants
 * ---------------------------------------------------------------------- */
#define DEFAULT_WINDOW    8
#define DEFAULT_TIMEOUT   250
#define DEFAULT_LOSS      0.0
#define DEFAULT_CORRUPT   0.0
#define DEFAULT_DUP       0.0
#define DEFAULT_PORT      4250
#define MIN_WINDOW        1
#define MAX_WINDOW        64

/* I/O-layer constants */
#define HELLO_TIMEOUT_SEC  1
#define HELLO_MAX_TRIES    5
#define RECV_IDLE_TIMEOUT  30000LL   /* ms */
#define LINGER_MS          2000LL

/* -------------------------------------------------------------------------
 * Config
 * ---------------------------------------------------------------------- */
typedef enum { MODE_SEND, MODE_RECV } app_mode_t;

typedef struct {
    app_mode_t  mode;
    char       *session;
    int         window;
    int         timeout_ms;
    double      loss;
    double      corrupt;
    double      dup;
    int         port;
    char       *relay;
    char       *file;
} config_t;

/* -------------------------------------------------------------------------
 * Monotonic clock  (used only by Layer 3)
 * ---------------------------------------------------------------------- */
static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

/* -------------------------------------------------------------------------
 * print_usage
 * ---------------------------------------------------------------------- */
static void print_usage(void)
{
    puts("Usage: myapp send -s <session> [-w window] [-T timeout-ms] [-l loss]");
    puts("                  [-c corrupt] [-d dup] [-p port] <relay> <file>");
    puts("       myapp recv -s <session> [-p port] <relay> <file>");
    puts("");
    puts("  -s <session>     session name shared by the sender and the receiver");
    puts("  -w <window>      Go-Back-N window size in packets, 1 to 64 (default: 8)");
    puts("  -T <timeout-ms>  retransmission timeout in milliseconds (default: 250)");
    puts("  -l <loss>        probability the relay drops a packet (default: 0)");
    puts("  -c <corrupt>     probability the relay flips a bit (default: 0)");
    puts("  -d <dup>         probability the relay duplicates a packet (default: 0)");
    puts("  -p <port>        relay port (default: 4250)");
    puts("  <relay>          host name or address of the relay");
    puts("  <file>           file to send, or file to write what is received");
}

/* -------------------------------------------------------------------------
 * Argument parsers
 * ---------------------------------------------------------------------- */
static int parse_double_arg(const char *s, double *out, const char *flag)
{
    char *end;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') {
        fprintf(stderr, "myapp: %s: not a number: %s\n", flag, s);
        return -1;
    }
    if (v < 0.0 || v > 0.5) {
        fprintf(stderr, "myapp: %s: must be between 0 and 0.5: %s\n", flag, s);
        return -1;
    }
    *out = v;
    return 0;
}

static int parse_int_arg(const char *s, int *out, const char *flag)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v <= 0) {
        fprintf(stderr, "myapp: %s: must be a positive integer: %s\n", flag, s);
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int parse_args(int argc, char *argv[], config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->window     = DEFAULT_WINDOW;
    cfg->timeout_ms = DEFAULT_TIMEOUT;
    cfg->loss       = DEFAULT_LOSS;
    cfg->corrupt    = DEFAULT_CORRUPT;
    cfg->dup        = DEFAULT_DUP;
    cfg->port       = DEFAULT_PORT;

    if (argc < 2) return -1;

    if (strcmp(argv[1], "send") == 0)
        cfg->mode = (app_mode_t)MODE_SEND;
    else if (strcmp(argv[1], "recv") == 0)
        cfg->mode = (app_mode_t)MODE_RECV;
    else {
        fprintf(stderr, "myapp: unknown mode '%s'\n", argv[1]);
        return -1;
    }

    int rem_argc = argc - 1;
    char **rem_argv = argv + 1;
    optind = 1;

    int opt;
    while ((opt = getopt(rem_argc, rem_argv, "s:w:T:l:c:d:p:")) != -1) {
        switch (opt) {
        case 's': cfg->session = optarg; break;
        case 'w':
            if (parse_int_arg(optarg, &cfg->window, "-w") != 0) return -1;
            if (cfg->window < MIN_WINDOW || cfg->window > MAX_WINDOW) {
                fprintf(stderr, "myapp: -w: window must be %d-%d\n",
                        MIN_WINDOW, MAX_WINDOW);
                return -1;
            }
            break;
        case 'T':
            if (parse_int_arg(optarg, &cfg->timeout_ms, "-T") != 0) return -1;
            break;
        case 'l':
            if (parse_double_arg(optarg, &cfg->loss, "-l") != 0) return -1;
            break;
        case 'c':
            if (parse_double_arg(optarg, &cfg->corrupt, "-c") != 0) return -1;
            break;
        case 'd':
            if (parse_double_arg(optarg, &cfg->dup, "-d") != 0) return -1;
            break;
        case 'p':
            if (parse_int_arg(optarg, &cfg->port, "-p") != 0) return -1;
            if (cfg->port < 1 || cfg->port > 65535) {
                fprintf(stderr, "myapp: -p: port out of range\n");
                return -1;
            }
            break;
        default: return -1;
        }
    }

    if (!cfg->session) {
        fprintf(stderr, "myapp: -s <session> is required\n");
        return -1;
    }

    int npos = rem_argc - optind;
    if (npos < 2) {
        fprintf(stderr, "myapp: missing positional argument%s\n",
                npos == 1 ? " <file>" : "s (<relay> <file>)");
        return -1;
    }
    if (npos > 2) {
        fprintf(stderr, "myapp: too many positional arguments\n");
        return -1;
    }
    cfg->relay = rem_argv[optind];
    cfg->file  = rem_argv[optind + 1];
    return 0;
}

/* -------------------------------------------------------------------------
 * open_udp_socket: resolve relay host:port, return bound UDP fd.
 * ---------------------------------------------------------------------- */
static int open_udp_socket(const char *host, int port,
                            struct sockaddr_storage *dest, socklen_t *destlen)
{
    char portstr[8];
    struct addrinfo hints, *res, *rp;
    int fd = -1;

    snprintf(portstr, sizeof(portstr), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int rc = getaddrinfo(host, portstr, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "myapp: getaddrinfo(%s): %s\n", host, gai_strerror(rc));
        return -1;
    }

    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd >= 0) {
            memcpy(dest, rp->ai_addr, rp->ai_addrlen);
            *destlen = (socklen_t)rp->ai_addrlen;
            break;
        }
    }
    freeaddrinfo(res);

    if (fd < 0) {
        fprintf(stderr, "myapp: could not create UDP socket for %s:%d\n",
                host, port);
    }
    return fd;
}

/* -------------------------------------------------------------------------
 * register_with_relay: send HELLO, wait for OK.  Up to 5 attempts.
 * ---------------------------------------------------------------------- */
static int register_with_relay(int fd,
                                const struct sockaddr_storage *relay,
                                socklen_t relaylen,
                                const char *hello_msg)
{
    struct timeval tv = { HELLO_TIMEOUT_SEC, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char reply[256];
    int attempt;
    for (attempt = 0; attempt < HELLO_MAX_TRIES; attempt++) {
        if (sendto(fd, hello_msg, strlen(hello_msg), 0,
                   (const struct sockaddr *)relay, relaylen) < 0) {
            perror("myapp: sendto HELLO");
            return -1;
        }

        ssize_t n = recvfrom(fd, reply, sizeof(reply) - 1, 0, NULL, NULL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("myapp: recvfrom HELLO reply");
            return -1;
        }
        reply[n] = '\0';

        if (strncmp(reply, "OK", 2) == 0) {
            /* clear the receive timeout – data phase uses poll */
            tv.tv_sec = tv.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            return 0;
        }
        if (strncmp(reply, "ERR", 3) == 0) {
            /* strip trailing whitespace */
            while (n > 0 && (reply[n-1] == '\n' || reply[n-1] == '\r'
                              || reply[n-1] == ' '))
                reply[--n] = '\0';
            fprintf(stderr, "myapp: relay refused: %s\n", reply);
            return -1;
        }
    }

    fprintf(stderr, "myapp: no reply from relay after %d attempts\n",
            HELLO_MAX_TRIES);
    return -1;
}

/* -------------------------------------------------------------------------
 * do_send: send all packets in a snd_action_t to the relay.
 * ---------------------------------------------------------------------- */
static void do_send(int fd,
                    const struct sockaddr_storage *relay, socklen_t relaylen,
                    const snd_action_t *a)
{
    int i;
    for (i = 0; i < a->send_count; i++)
        sendto(fd, a->send_pkts[i].buf, a->send_pkts[i].len, 0,
               (const struct sockaddr *)relay, relaylen);
}

/* =========================================================================
 * run_recv  –  Layer 3 receiver I/O shell
 * ======================================================================= */
static int run_recv(const config_t *cfg)
{
    struct sockaddr_storage relay_addr;
    socklen_t relay_addrlen;

    int fd = open_udp_socket(cfg->relay, cfg->port, &relay_addr, &relay_addrlen);
    if (fd < 0) return 2;

    char hello[128];
    snprintf(hello, sizeof(hello), "HELLO %s recv", cfg->session);
    if (register_with_relay(fd, &relay_addr, relay_addrlen, hello) != 0) {
        close(fd);
        return 2;
    }

    FILE *outf = fopen(cfg->file, "wb");
    if (!outf) {
        fprintf(stderr, "myapp: cannot open '%s': %s\n",
                cfg->file, strerror(errno));
        close(fd);
        return 2;
    }

    /* ---- state machine ---- */
    gbn_recv_t r;
    gbn_recv_init(&r, LINGER_MS);

    uint8_t  rawbuf[PKT_MAX_LEN];
    long long deadline = now_ms() + RECV_IDLE_TIMEOUT;

    for (;;) {
        /* set SO_RCVTIMEO to remaining idle budget */
        long long left = deadline - now_ms();
        if (left <= 0) {
            fprintf(stderr, "myapp: idle timeout\n");
            fclose(outf);
            close(fd);
            return 2;
        }
        struct timeval tv;
        tv.tv_sec  = left / 1000;
        tv.tv_usec = (long)(left % 1000) * 1000L;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_storage src;
        socklen_t srclen = sizeof(src);
        ssize_t n = recvfrom(fd, rawbuf, sizeof(rawbuf), 0,
                             (struct sockaddr *)&src, &srclen);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "myapp: idle timeout\n");
                fclose(outf);
                close(fd);
                return 2;
            }
            perror("myapp: recvfrom");
            fclose(outf);
            close(fd);
            return 2;
        }

        pkt_t pkt;
        if (!pkt_decode(rawbuf, (size_t)n, &pkt)) continue;

        deadline = now_ms() + RECV_IDLE_TIMEOUT;

        rcv_action_t a = gbn_recv_pkt(&r, now_ms(), &pkt);

        /* write payload */
        if (a.payload_len > 0) {
            if (fwrite(a.payload, 1, a.payload_len, outf) != a.payload_len) {
                fprintf(stderr, "myapp: fwrite: %s\n", strerror(errno));
                fclose(outf);
                close(fd);
                return 2;
            }
        }

        /* send ACK */
        if (a.ack_len > 0)
            sendto(fd, a.ack_buf, a.ack_len, 0,
                   (const struct sockaddr *)&relay_addr, relay_addrlen);

        if (!a.done) continue;

        /* ---- FIN processed: close file, enter linger ---- */
        fclose(outf);

        /* Linger: re-ACK repeated FINs until the deadline */
        long long linger_end = a.linger_deadline_ms;
        for (;;) {
            long long ll = linger_end - now_ms();
            if (ll <= 0) break;

            struct timeval ltv;
            ltv.tv_sec  = ll / 1000;
            ltv.tv_usec = (long)(ll % 1000) * 1000L;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &ltv, sizeof(ltv));

            ssize_t ln = recvfrom(fd, rawbuf, sizeof(rawbuf), 0,
                                  (struct sockaddr *)&src, &srclen);
            if (ln < 0) break;

            pkt_t lp;
            if (!pkt_decode(rawbuf, (size_t)ln, &lp)) continue;

            /* Re-ACK the FIN: pass it back through the state machine.
             * r.expected is already past the FIN, so a repeated FIN
             * with seq == r.expected-1 will be treated as out-of-order
             * and generate ACK r.expected again. */
            rcv_action_t la = gbn_recv_pkt(&r, now_ms(), &lp);
            if (la.ack_len > 0)
                sendto(fd, la.ack_buf, la.ack_len, 0,
                       (const struct sockaddr *)&relay_addr, relay_addrlen);
        }

        close(fd);
        return 0;
    }
}

/* =========================================================================
 * run_send  –  Layer 3 sender I/O shell
 * ======================================================================= */
static int run_send(const config_t *cfg)
{
    struct sockaddr_storage relay_addr;
    socklen_t relay_addrlen;

    int fd = open_udp_socket(cfg->relay, cfg->port, &relay_addr, &relay_addrlen);
    if (fd < 0) return 2;

    char hello[256];
    snprintf(hello, sizeof(hello), "HELLO %s send %.4f %.4f %.4f",
             cfg->session, cfg->loss, cfg->corrupt, cfg->dup);
    if (register_with_relay(fd, &relay_addr, relay_addrlen, hello) != 0) {
        close(fd);
        return 2;
    }

    FILE *inf = fopen(cfg->file, "rb");
    if (!inf) {
        fprintf(stderr, "myapp: cannot open '%s': %s\n",
                cfg->file, strerror(errno));
        close(fd);
        return 2;
    }

    /* ---- state machine ---- */
    gbn_sender_t s;
    gbn_sender_init(&s, cfg->window, (long long)cfg->timeout_ms);

    uint8_t  payload[PKT_MAX_PAYLOAD];
    uint8_t  rawbuf[PKT_MAX_LEN];

    struct pollfd pfd = { fd, POLLIN, 0 };

    for (;;) {
        /* ---- fill window from file ---- */
        while (!gbn_sender_window_full(&s) && !s.eof_seen) {
            size_t nr = fread(payload, 1, PKT_MAX_PAYLOAD, inf);
            if (nr > 0) {
                snd_action_t a = gbn_sender_data(&s, now_ms(),
                                                 payload, (uint16_t)nr);
                do_send(fd, &relay_addr, relay_addrlen, &a);
                if (nr < PKT_MAX_PAYLOAD) {
                    /* short read → EOF */
                    snd_action_t ea = gbn_sender_eof(&s, now_ms());
                    do_send(fd, &relay_addr, relay_addrlen, &ea);
                    break;
                }
            } else {
                snd_action_t ea = gbn_sender_eof(&s, now_ms());
                do_send(fd, &relay_addr, relay_addrlen, &ea);
                break;
            }
        }

        if (s.fin_acked) {
            fclose(inf);
            close(fd);
            return 0;
        }

        /* ---- poll: wait for ACK or timer ---- */
        int poll_ms = -1;
        if (s.timer_deadline_ms > 0) {
            long long left = s.timer_deadline_ms - now_ms();
            poll_ms = (left > 0) ? (int)left : 0;
        }

        int rc = poll(&pfd, 1, poll_ms);
        if (rc < 0) {
            if (errno == EINTR) continue;
            perror("myapp: poll");
            fclose(inf);
            close(fd);
            return 2;
        }

        /* ---- timeout ---- */
        if (rc == 0 || (s.timer_deadline_ms > 0
                        && now_ms() >= s.timer_deadline_ms)) {
            snd_action_t a = gbn_sender_timeout(&s, now_ms());
            do_send(fd, &relay_addr, relay_addrlen, &a);
            if (a.give_up) {
                fprintf(stderr,
                        "myapp: gave up after %d consecutive timeouts\n",
                        GBN_MAX_TIMEOUTS);
                fclose(inf);
                close(fd);
                return 2;
            }
            continue;
        }

        /* ---- ACK received ---- */
        if (!(pfd.revents & POLLIN)) continue;

        ssize_t n = recvfrom(fd, rawbuf, sizeof(rawbuf), 0, NULL, NULL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("myapp: recvfrom");
            fclose(inf);
            close(fd);
            return 2;
        }

        pkt_t pkt;
        if (!pkt_decode(rawbuf, (size_t)n, &pkt)) continue;
        if (pkt.type != PKT_TYPE_ACK) continue;

        snd_action_t a = gbn_sender_ack(&s, now_ms(), pkt.seq);
        do_send(fd, &relay_addr, relay_addrlen, &a);
        if (a.done) {
            fclose(inf);
            close(fd);
            return 0;
        }
    }
}

/* =========================================================================
 * main
 * ======================================================================= */
int main(int argc, char *argv[])
{
    if (argc == 1) { print_usage(); return 0; }

    config_t cfg;
    if (parse_args(argc, argv, &cfg) != 0) {
        print_usage();
        return 1;
    }

    return cfg.mode == (app_mode_t)MODE_RECV
           ? run_recv(&cfg)
           : run_send(&cfg);
}