/* Spike S3 (#33) resource manager. Run as root: ./s3_server &
 * Single-threaded dispatch, like the gate resmgr in docs/architecture.md. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>
#include <time.h>
#include <unistd.h>

#include "s3.h"

static resmgr_connect_funcs_t cfuncs;
static resmgr_io_funcs_t iofuncs;
static iofunc_attr_t attr;

static uint32_t g_seq;
static struct s3_close_info g_close;
static struct s3_log g_log;
static unsigned g_log_len;

static int g_saved_rcvid = -1; /* Q2: blocked read waiting for a late reply */
static RESMGR_OCB_T *g_saved_ocb;
static timer_t g_timer;
static int g_timer_code;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void log_event(char c) {
    if (g_log_len < S3_LOG_MAX - 1) {
        g_log.events[g_log_len++] = c;
    }
}

static int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb) {
    int status = iofunc_devctl_default(ctp, msg, ocb);
    if (status != _RESMGR_DEFAULT) {
        return status;
    }
    void *data = _DEVCTL_DATA(msg->i);
    size_t out = 0;
    switch (msg->i.dcmd) {
    case DCMD_S3_ECHO: {
        if (msg->i.nbytes < sizeof(struct s3_msg)) {
            return EINVAL;
        }
        struct s3_msg *m = data;
        m->value += 1;
        m->seq = ++g_seq;
        log_event('D');
        out = sizeof *m;
        break;
    }
    case DCMD_S3_LASTCLOSE:
        memcpy(data, &g_close, sizeof g_close);
        out = sizeof g_close;
        break;
    case DCMD_S3_STALL: {
        log_event('S');
        struct timespec ts = {0, 300 * 1000000L};
        nanosleep(&ts, NULL);
        break;
    }
    case DCMD_S3_LOG:
        memcpy(data, &g_log, sizeof g_log);
        out = sizeof g_log;
        memset(&g_log, 0, sizeof g_log);
        g_log_len = 0;
        break;
    default:
        return ENOSYS;
    }
    memset(&msg->o, 0, sizeof msg->o);
    msg->o.nbytes = out;
    return _RESMGR_PTR(ctp, &msg->o, sizeof msg->o + out);
}

static int io_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
    int status = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (status != EOK) {
        return status;
    }
    if (g_saved_rcvid != -1) {
        return EBUSY; /* one blocked reader at a time is enough for the spike */
    }
    g_saved_rcvid = ctp->rcvid;
    g_saved_ocb = ocb;
    struct itimerspec its = {0};
    its.it_value.tv_nsec = S3_READ_DELAY_MS * 1000000L;
    timer_settime(g_timer, 0, &its, NULL);
    return _RESMGR_NOREPLY; /* reply later from the timer pulse */
}

static int on_timer(message_context_t *ctp, int code, unsigned flags, void *handle) {
    (void)ctp, (void)code, (void)flags, (void)handle;
    if (g_saved_rcvid == -1) {
        return 0;
    }
    static const char text[] = "late-reply\n";
    if (MsgReply(g_saved_rcvid, sizeof text - 1, text, sizeof text - 1) == -1) {
        printf("s3_server: late MsgReply failed: %s\n", strerror(errno));
    } else {
        printf("s3_server: late MsgReply ok\n");
    }
    g_saved_rcvid = -1;
    g_saved_ocb = NULL;
    return 0;
}

static int on_client_pulse(message_context_t *ctp, int code, unsigned flags, void *handle) {
    (void)ctp, (void)code, (void)flags, (void)handle;
    log_event('P');
    return 0;
}

static int io_close_ocb(resmgr_context_t *ctp, void *reserved, RESMGR_OCB_T *ocb) {
    if (ocb == g_saved_ocb) {
        g_saved_rcvid = -1; /* the blocked reader is gone */
        g_saved_ocb = NULL;
    }
    g_close.last_close_ns = now_ns();
    g_close.closes++;
    printf("s3_server: close_ocb #%u\n", g_close.closes);
    return iofunc_close_ocb_default(ctp, reserved, ocb);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    dispatch_t *dpp = dispatch_create();
    if (dpp == NULL) {
        perror("dispatch_create");
        return 1;
    }
    resmgr_attr_t rattr;
    memset(&rattr, 0, sizeof rattr);
    rattr.nparts_max = 1;
    rattr.msg_max_size = 2048;

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &cfuncs, _RESMGR_IO_NFUNCS, &iofuncs);
    iofuncs.devctl = io_devctl;
    iofuncs.read = io_read;
    iofuncs.close_ocb = io_close_ocb;
    iofunc_attr_init(&attr, S_IFNAM | 0666, NULL, NULL);

    if (resmgr_attach(dpp, &rattr, S3_PATH, _FTYPE_ANY, 0, &cfuncs, &iofuncs, &attr) == -1) {
        perror("resmgr_attach");
        return 1;
    }

    /* Timer pulse for the late read reply (Q2). */
    g_timer_code = pulse_attach(dpp, MSG_FLAG_ALLOC_PULSE, 0, on_timer, NULL);
    int self = message_connect(dpp, MSG_FLAG_SIDE_CHANNEL);
    if (g_timer_code == -1 || self == -1) {
        perror("pulse_attach/message_connect");
        return 1;
    }
    struct sigevent ev;
    SIGEV_PULSE_INIT(&ev, self, SIGEV_PULSE_PRIO_INHERIT, g_timer_code, 0);
    if (timer_create(CLOCK_MONOTONIC, &ev, &g_timer) == -1) {
        perror("timer_create");
        return 1;
    }

    /* Pulses clients send on their resmgr connection (Q5). */
    if (pulse_attach(dpp, 0, S3_PULSE_CODE, on_client_pulse, NULL) == -1) {
        perror("pulse_attach client");
        return 1;
    }

    dispatch_context_t *ctp = dispatch_context_alloc(dpp);
    printf("s3_server: ready on %s\n", S3_PATH);
    for (;;) {
        if ((ctp = dispatch_block(ctp)) == NULL) {
            perror("dispatch_block");
            return 1;
        }
        dispatch_handler(ctp);
    }
}
