/* Spike S3 (#33) client. Run as root (it uses SCHED_FIFO 10..30, all <= 63).
 *   ./s3_client echo    Q1 __DIOTF round trip
 *   ./s3_client read    Q2 blocked read, late MsgReply
 *   ./s3_client kill    Q3 OCB close after kill -9 of a blocked client
 *   ./s3_client pi      Q4 default mutex protocol and observed inheritance
 *   ./s3_client order   Q5 pulse vs devctl order on one dispatch channel
 *   ./s3_client slow    delayed ack: 10 ms server reply vs 5 ms client TimerTimeout
 * Prints yes/no answers only, never timing numbers (AGENTS.md section 2). */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "s3.h"

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void sleep_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static int open_dev(void) {
    int fd = open(S3_PATH, O_RDWR);
    if (fd == -1) {
        perror("open " S3_PATH);
        exit(1);
    }
    return fd;
}

static int q1_echo(void) {
    int fd = open_dev();
    struct s3_msg m = {41, 0};
    int rc = devctl(fd, DCMD_S3_ECHO, &m, sizeof m, NULL);
    int ok = rc == EOK && m.value == 42 && m.seq > 0;
    printf("Q1 __DIOTF round trip works: %s (rc=%d value=%d)\n", ok ? "yes" : "no", rc, m.value);
    return ok ? 0 : 1;
}

static int q2_read(void) {
    int fd = open_dev();
    char buf[32] = {0};
    uint64_t t0 = now_ns();
    ssize_t n = read(fd, buf, sizeof buf - 1);
    uint64_t waited_ms = (now_ns() - t0) / 1000000u;
    int late = waited_ms >= S3_READ_DELAY_MS - 50;
    int ok = n > 0 && strcmp(buf, "late-reply\n") == 0 && late;
    printf("Q2 blocked read replied later via saved rcvid: %s\n", ok ? "yes" : "no");
    return ok ? 0 : 1;
}

static int q3_kill(void) {
    int fd = open_dev();
    struct s3_close_info before;
    devctl(fd, DCMD_S3_LASTCLOSE, &before, sizeof before, NULL);

    pid_t child = fork();
    if (child == 0) {
        int cfd = open_dev();
        char b[8];
        for (;;) {
            read(cfd, b, sizeof b); /* blocks in the server */
        }
    }
    sleep_ms(100); /* let the child block in read */
    uint64_t t_kill = now_ns();
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    sleep_ms(200);

    struct s3_close_info after;
    devctl(fd, DCMD_S3_LASTCLOSE, &after, sizeof after, NULL);
    int seen = after.closes > before.closes && after.last_close_ns >= t_kill;
    int within_100 = seen && after.last_close_ns - t_kill <= 100u * 1000000u;
    printf("Q3 resmgr saw OCB close after kill -9: %s\n", seen ? "yes" : "no");
    printf("Q3 close seen within 100 ms of the kill: %s\n", within_100 ? "yes" : "no");
    return seen ? 0 : 1;
}

/* Delayed ack (DECISIONS.md S3 row): server replies after 10 ms, client allows 5 ms. */
static int q_slow(void) {
    int fd = open_dev();
    struct s3_msg m = {1, 0};
    uint64_t limit = (uint64_t)S3_SLOW_TIMEOUT_MS * 1000000u;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, NULL, &limit, NULL);
    int rc = devctl(fd, DCMD_S3_SLOWECHO, &m, sizeof m, NULL);
    printf("ACK client devctl result: %s\n", rc == EOK ? "EOK (got the late reply)" : strerror(rc));
    printf("ACK client got ETIMEDOUT: %s\n", rc == ETIMEDOUT ? "yes" : "no");
    sleep_ms(50); /* let the server try its late MsgReply */
    return 0;
}

/* ---- Q4: mutex priority inheritance ---- */
static pthread_mutex_t g_mx;
static volatile int g_low_has_lock, g_high_waiting, g_low_seen_prio;

static void *low_thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&g_mx);
    g_low_has_lock = 1;
    while (!g_high_waiting) {
        sched_yield();
    }
    sleep_ms(50); /* high thread is now blocked on the mutex */
    struct sched_param sp;
    int policy;
    pthread_getschedparam(pthread_self(), &policy, &sp);
    g_low_seen_prio = sp.sched_curpriority; /* QNX: effective priority */
    pthread_mutex_unlock(&g_mx);
    return NULL;
}

static void *high_thread(void *arg) {
    (void)arg;
    g_high_waiting = 1;
    pthread_mutex_lock(&g_mx);
    pthread_mutex_unlock(&g_mx);
    return NULL;
}

static pthread_t start_fifo(void *(*fn)(void *), int prio) {
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&a, SCHED_FIFO);
    struct sched_param sp = {0};
    sp.sched_priority = prio;
    pthread_attr_setschedparam(&a, &sp);
    pthread_t t;
    int rc = pthread_create(&t, &a, fn, NULL);
    if (rc != EOK) {
        fprintf(stderr, "pthread_create prio %d: %s\n", prio, strerror(rc));
        exit(1);
    }
    return t;
}

static int q4_pi(void) {
    pthread_mutexattr_t ma;
    int proto = -1;
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_getprotocol(&ma, &proto);
    printf("Q4 default mutexattr protocol: %s\n", proto == PTHREAD_PRIO_INHERIT ? "PRIO_INHERIT"
                                                  : proto == PTHREAD_PRIO_NONE  ? "PRIO_NONE"
                                                                                : "other");
    pthread_mutex_init(&g_mx, NULL); /* default attributes, as GUARD code would use */
    pthread_t lo = start_fifo(low_thread, 10);
    while (!g_low_has_lock) {
        sched_yield();
    }
    pthread_t hi = start_fifo(high_thread, 30);
    pthread_join(hi, NULL);
    pthread_join(lo, NULL);
    int inherited = g_low_seen_prio == 30;
    printf("Q4 default mutex boosted holder 10 -> 30: %s (seen %d)\n", inherited ? "yes" : "no",
           g_low_seen_prio);
    return 0;
}

/* ---- Q5: pulse vs devctl order ---- */
static int g_fd;

static void *stall_thread(void *arg) {
    (void)arg;
    devctl(g_fd, DCMD_S3_STALL, NULL, 0, NULL);
    return NULL;
}

static void *echo_thread(void *arg) {
    (void)arg;
    struct s3_msg m = {0, 0};
    devctl(g_fd, DCMD_S3_ECHO, &m, sizeof m, NULL);
    return NULL;
}

/* While the server is stalled, queue a pulse and an echo devctl, then read the order. */
static void order_case(const char *name, int pulse_first, int pulse_prio, int echo_prio) {
    struct s3_log lg;
    devctl(g_fd, DCMD_S3_LOG, &lg, sizeof lg, NULL); /* clear */
    pthread_t st = start_fifo(stall_thread, 10);
    sleep_ms(50); /* server is inside STALL now */
    pthread_t et = 0;
    if (pulse_first) {
        MsgSendPulse(g_fd, pulse_prio, S3_PULSE_CODE, 0);
        sleep_ms(10);
        et = start_fifo(echo_thread, echo_prio);
    } else {
        et = start_fifo(echo_thread, echo_prio);
        sleep_ms(10);
        MsgSendPulse(g_fd, pulse_prio, S3_PULSE_CODE, 0);
    }
    pthread_join(st, NULL);
    pthread_join(et, NULL);
    sleep_ms(20);
    memset(&lg, 0, sizeof lg);
    devctl(g_fd, DCMD_S3_LOG, &lg, sizeof lg, NULL);
    printf("Q5 %-34s order=%s\n", name, lg.events);
}

static int q5_order(void) {
    g_fd = open_dev();
    order_case("pulse(10) sent before devctl(10)", 1, 10, 10);
    order_case("devctl(10) sent before pulse(10)", 0, 10, 10);
    order_case("devctl(10) sent before pulse(20)", 0, 20, 10);
    order_case("pulse(10) sent before devctl(20)", 1, 10, 20);
    printf("Q5 key: S=stall started, then events in the order the server handled them\n");
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "echo") == 0) {
        return q1_echo();
    }
    if (strcmp(mode, "read") == 0) {
        return q2_read();
    }
    if (strcmp(mode, "kill") == 0) {
        return q3_kill();
    }
    if (strcmp(mode, "pi") == 0) {
        return q4_pi();
    }
    if (strcmp(mode, "order") == 0) {
        return q5_order();
    }
    if (strcmp(mode, "slow") == 0) {
        return q_slow();
    }
    fprintf(stderr, "usage: %s echo|read|kill|pi|order|slow\n", argv[0]);
    return 2;
}
