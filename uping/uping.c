/*
 * uping - microsecond-precision ICMP ping for macOS
 *
 * Uses SOCK_DGRAM/IPPROTO_ICMP (no root required on macOS 10.14+),
 * falling back to SOCK_RAW if unavailable.
 * Timing via clock_gettime(CLOCK_MONOTONIC) gives sub-microsecond
 * resolution; results are reported in whole microseconds.
 *
 * Examples:
 *   ./uping 1.1.1.1
 *   ./uping -c 10 -i 0.5 google.com
 *   ./uping -W 1 -6 ipv6.google.com
 */

#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ---- ICMP6 header (not always in system headers) -------------------------*/
struct icmp6_hdr {
    uint8_t  icmp6_type;
    uint8_t  icmp6_code;
    uint16_t icmp6_cksum;
    uint16_t icmp6_id;
    uint16_t icmp6_seq;
};
#define ICMP6_ECHO_REQUEST 128
#define ICMP6_ECHO_REPLY   129

/* ---- Globals for signal handler ------------------------------------------*/
static volatile sig_atomic_t g_running = 1;
static long      g_sent     = 0;
static long      g_received = 0;
static long long g_sum_us   = 0;
static long long g_min_us   = -1;
static long long g_max_us   = 0;
static char      g_target[256];

/* ---- ANSI colours (set to empty strings when colour is off) --------------*/
static const char *COL_RED    = "";
static const char *COL_GREEN  = "";
static const char *COL_YELLOW = "";
static const char *COL_RESET  = "";

/* ---- Helpers -------------------------------------------------------------*/

static long long now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}

static uint16_t checksum(const void *data, size_t len)
{
    const uint16_t *buf = data;
    uint32_t sum = 0;
    while (len > 1) { sum += *buf++; len -= 2; }
    if (len)         sum += *(const uint8_t *)buf;
    sum  = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (uint16_t)~sum;
}

static void print_stats(void)
{
    printf("\n--- %s uping statistics ---\n", g_target);
    double loss = g_sent > 0
        ? (double)(g_sent - g_received) / (double)g_sent * 100.0 : 0.0;
    printf("%ld packets transmitted, %ld received, %.1f%% loss\n",
           g_sent, g_received, loss);
    if (g_received > 0) {
        double avg = (double)g_sum_us / (double)g_received;
        printf("rtt min/avg/max = %lld/%.1f/%lld µs\n",
               g_min_us, avg, g_max_us);
    }
}

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] <host>\n"
        "\n"
        "Options:\n"
        "  -c COUNT     Stop after COUNT pings (default: run until Ctrl+C)\n"
        "  -i INTERVAL  Seconds between pings, may be fractional (default: 1)\n"
        "  -W TIMEOUT   Per-ping timeout in seconds (default: 2)\n"
        "  -4           Force IPv4\n"
        "  -6           Force IPv6\n"
        "  -n           Disable colour\n"
        "  -h           Show this help\n",
        prog);
    exit(1);
}

/* ---- Packet structs -------------------------------------------------------*/
#pragma pack(push, 1)
struct icmp4_pkt {
    struct icmp hdr;
    long long   ts_us;   /* send timestamp — used to match reply */
};

struct icmp6_pkt {
    struct icmp6_hdr hdr;
    long long        ts_us;
};
#pragma pack(pop)

/* ---- Main ----------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    int    count    = 0;
    double interval = 1.0;
    double timeout  = 2.0;
    int    color    = isatty(STDOUT_FILENO);
    int    force4   = 0, force6 = 0;

    int opt;
    while ((opt = getopt(argc, argv, "c:i:W:46nh")) != -1) {
        switch (opt) {
        case 'c': count    = atoi(optarg);  break;
        case 'i': interval = atof(optarg);  break;
        case 'W': timeout  = atof(optarg);  break;
        case '4': force4   = 1;             break;
        case '6': force6   = 1;             break;
        case 'n': color    = 0;             break;
        case 'h': usage(argv[0]);           break;
        default:  usage(argv[0]);
        }
    }
    if (optind >= argc) usage(argv[0]);
    if (force4 && force6) {
        fprintf(stderr, "uping: -4 and -6 are mutually exclusive\n");
        return 1;
    }

    if (color) {
        COL_RED    = "\033[0;31m";
        COL_GREEN  = "\033[0;32m";
        COL_YELLOW = "\033[0;33m";
        COL_RESET  = "\033[0m";
    }

    const char *host = argv[optind];
    strncpy(g_target, host, sizeof(g_target) - 1);
    g_target[sizeof(g_target) - 1] = '\0';

    /* Resolve the target */
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = force6 ? AF_INET6 : (force4 ? AF_INET : AF_UNSPEC);
    hints.ai_socktype = SOCK_RAW;
    hints.ai_flags    = AI_ADDRCONFIG;

    int err = getaddrinfo(host, NULL, &hints, &res);
    if (err) {
        fprintf(stderr, "uping: %s: %s\n", host, gai_strerror(err));
        return 1;
    }

    int af = res->ai_family;  /* AF_INET or AF_INET6 */

    /* Choose addresses */
    struct sockaddr_storage dst_addr;
    socklen_t dst_len = (socklen_t)res->ai_addrlen;
    memcpy(&dst_addr, res->ai_addr, dst_len);
    freeaddrinfo(res);

    char dst_str[INET6_ADDRSTRLEN];
    if (af == AF_INET6) {
        inet_ntop(AF_INET6,
                  &((struct sockaddr_in6 *)&dst_addr)->sin6_addr,
                  dst_str, sizeof(dst_str));
    } else {
        inet_ntop(AF_INET,
                  &((struct sockaddr_in *)&dst_addr)->sin_addr,
                  dst_str, sizeof(dst_str));
    }

    /* Open socket — try unprivileged DGRAM first */
    int is_raw = 0;
    int proto  = (af == AF_INET6) ? IPPROTO_ICMPV6 : IPPROTO_ICMP;
    int sock   = socket(af, SOCK_DGRAM, proto);
    if (sock < 0) {
        sock = socket(af, SOCK_RAW, proto);
        if (sock < 0) {
            perror("uping: socket");
            fprintf(stderr, "       Try running with sudo for raw ICMP access.\n");
            return 1;
        }
        is_raw = 1;
    }

    /* Receive timeout */
    struct timeval tv;
    tv.tv_sec  = (time_t)timeout;
    tv.tv_usec = (suseconds_t)((timeout - (double)(time_t)timeout) * 1e6);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    uint16_t pid = (uint16_t)(getpid() & 0xffff);

    printf("UPING %s (%s): %s ICMP, timeout %.1fs\n",
           host, dst_str,
           af == AF_INET6 ? "ICMPv6" : "ICMPv4",
           timeout);

    int seq = 0;
    while (g_running && (count == 0 || seq < count)) {
        seq++;
        g_sent++;

        long long t0, t1;

        if (af == AF_INET) {
            /* ---- ICMPv4 --------------------------------------------------*/
            struct icmp4_pkt pkt;
            memset(&pkt, 0, sizeof(pkt));
            pkt.hdr.icmp_type  = ICMP_ECHO;
            pkt.hdr.icmp_code  = 0;
            pkt.hdr.icmp_id    = htons(pid);
            pkt.hdr.icmp_seq   = htons((uint16_t)seq);
            t0 = now_us();
            pkt.ts_us          = t0;
            pkt.hdr.icmp_cksum = checksum(&pkt, sizeof(pkt));

            if (sendto(sock, &pkt, sizeof(pkt), 0,
                       (struct sockaddr *)&dst_addr, dst_len) < 0) {
                printf("seq=%-4d %sFAIL%s  send: %s\n",
                       seq, COL_RED, COL_RESET, strerror(errno));
                goto next;
            }

            /* Receive loop — filter for our reply */
            while (1) {
                uint8_t buf[1500];
                struct sockaddr_storage from;
                socklen_t fromlen = sizeof(from);
                ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                                     (struct sockaddr *)&from, &fromlen);
                t1 = now_us();

                if (n < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        printf("seq=%-4d %sTIMEOUT%s\n",
                               seq, COL_RED, COL_RESET);
                    else
                        printf("seq=%-4d %sFAIL%s  recv: %s\n",
                               seq, COL_RED, COL_RESET, strerror(errno));
                    break;
                }

                /*
                 * On macOS both SOCK_RAW and SOCK_DGRAM include the IP
                 * header in received ICMP packets. Always strip it.
                 * In DGRAM mode the kernel also rewrites icmp_id, so
                 * skip the ID check and rely on sequence + type only.
                 */
                if (n < (ssize_t)sizeof(struct ip)) continue;
                struct ip *iph = (struct ip *)buf;
                int ihl = iph->ip_hl * 4;
                if (n < ihl + 8) continue;
                struct icmp *icmp = (struct icmp *)(buf + ihl);

                if (icmp->icmp_type != ICMP_ECHOREPLY)       continue;
                if (is_raw && ntohs(icmp->icmp_id) != pid)   continue;
                if (ntohs(icmp->icmp_seq) != (uint16_t)seq)  continue;

                long long rtt = t1 - t0;
                g_received++;
                g_sum_us += rtt;
                if (g_min_us < 0 || rtt < g_min_us) g_min_us = rtt;
                if (rtt > g_max_us) g_max_us = rtt;

                const char *col = rtt < 1000   ? COL_GREEN
                                : rtt < 10000  ? COL_YELLOW
                                               : COL_RED;
                char from_str[INET6_ADDRSTRLEN];
                const char *reply_from = dst_str;
                if (from.ss_family == AF_INET) {
                    struct sockaddr_in *from4 = (struct sockaddr_in *)&from;
                    if (inet_ntop(AF_INET, &from4->sin_addr,
                                  from_str, sizeof(from_str)) != NULL)
                        reply_from = from_str;
                } else if (from.ss_family == AF_INET6) {
                    struct sockaddr_in6 *from6 = (struct sockaddr_in6 *)&from;
                    if (inet_ntop(AF_INET6, &from6->sin6_addr,
                                  from_str, sizeof(from_str)) != NULL)
                        reply_from = from_str;
                }
                printf("seq=%-4d %s%lldµs%s  from %s\n",
                       seq, col, rtt, COL_RESET, reply_from);
                break;
            }

        } else {
            /* ---- ICMPv6 --------------------------------------------------*/
            struct icmp6_pkt pkt;
            memset(&pkt, 0, sizeof(pkt));
            pkt.hdr.icmp6_type = ICMP6_ECHO_REQUEST;
            pkt.hdr.icmp6_code = 0;
            pkt.hdr.icmp6_id   = htons(pid);
            pkt.hdr.icmp6_seq  = htons((uint16_t)seq);
            t0 = now_us();
            pkt.ts_us          = t0;
            /* Kernel computes ICMPv6 checksum (requires pseudo-header) */
            pkt.hdr.icmp6_cksum = 0;

            if (sendto(sock, &pkt, sizeof(pkt), 0,
                       (struct sockaddr *)&dst_addr, dst_len) < 0) {
                printf("seq=%-4d %sFAIL%s  send: %s\n",
                       seq, COL_RED, COL_RESET, strerror(errno));
                goto next;
            }

            while (1) {
                uint8_t buf[1500];
                struct sockaddr_storage from;
                socklen_t fromlen = sizeof(from);
                ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                                     (struct sockaddr *)&from, &fromlen);
                t1 = now_us();

                if (n < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        printf("seq=%-4d %sTIMEOUT%s\n",
                               seq, COL_RED, COL_RESET);
                    else
                        printf("seq=%-4d %sFAIL%s  recv: %s\n",
                               seq, COL_RED, COL_RESET, strerror(errno));
                    break;
                }

                /*
                 * ICMPv6: SOCK_DGRAM strips the IPv6 header; SOCK_RAW also
                 * does not include the IPv6 header (unlike IPv4 SOCK_RAW).
                 */
                if (n < (ssize_t)sizeof(struct icmp6_hdr)) continue;
                struct icmp6_hdr *icmp6 = (struct icmp6_hdr *)buf;

                if (icmp6->icmp6_type != ICMP6_ECHO_REPLY)              continue;
                /* In DGRAM mode the kernel rewrites icmp6_id — skip check */
                if (is_raw && ntohs(icmp6->icmp6_id) != pid)            continue;
                if (ntohs(icmp6->icmp6_seq) != (uint16_t)seq)           continue;

                long long rtt = t1 - t0;
                g_received++;
                g_sum_us += rtt;
                if (g_min_us < 0 || rtt < g_min_us) g_min_us = rtt;
                if (rtt > g_max_us) g_max_us = rtt;

                char from_str[NI_MAXHOST];
                if (getnameinfo((struct sockaddr *)&from, fromlen,
                                from_str, sizeof(from_str),
                                NULL, 0, NI_NUMERICHOST) != 0) {
                    snprintf(from_str, sizeof(from_str), "%s", dst_str);
                }

                const char *col = rtt < 1000   ? COL_GREEN
                                : rtt < 10000  ? COL_YELLOW
                                               : COL_RED;
                printf("seq=%-4d %s%lldµs%s  from %s\n",
                       seq, col, rtt, COL_RESET, from_str);
                break;
            }
        }

    next:
        if (g_running && (count == 0 || seq < count)) {
            struct timespec ts;
            ts.tv_sec  = (time_t)interval;
            ts.tv_nsec = (long)((interval - (double)(time_t)interval) * 1e9);
            nanosleep(&ts, NULL);
        }
    }

    close(sock);
    print_stats();
    return 0;
}
