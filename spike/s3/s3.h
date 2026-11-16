/* Spike S3 (#33). Throwaway code: stays on branch spike/s3, never in main. */
#ifndef S3_H
#define S3_H

#include <devctl.h>
#include <stdint.h>
#include <sys/neutrino.h>

#define S3_PATH "/dev/s3"

struct s3_msg {
    int32_t value;
    uint32_t seq;
};

struct s3_close_info {
    uint64_t last_close_ns; /* CLOCK_MONOTONIC when the last OCB closed, 0 if none */
    uint32_t closes;
};

#define S3_LOG_MAX 64
struct s3_log {
    char events[S3_LOG_MAX]; /* 'P' pulse, 'D' echo devctl, 'S' stall start */
};

/* Q1: __DIOTF round trip. The server adds 1 to value. */
#define DCMD_S3_ECHO __DIOTF(_DCMD_MISC, 0x51, struct s3_msg)
/* Q3: when did the server see the last OCB close? */
#define DCMD_S3_LASTCLOSE __DIOF(_DCMD_MISC, 0x52, struct s3_close_info)
/* Q5: keep the single dispatch thread busy for 300 ms. */
#define DCMD_S3_STALL __DION(_DCMD_MISC, 0x53)
/* Q5: read and clear the event log. */
#define DCMD_S3_LOG __DIOF(_DCMD_MISC, 0x54, struct s3_log)

/* Delayed ack: server replies S3_SLOW_REPLY_MS later; client waits at most S3_SLOW_TIMEOUT_MS. */
#define DCMD_S3_SLOWECHO __DIOTF(_DCMD_MISC, 0x55, struct s3_msg)
#define S3_SLOW_REPLY_MS 10
#define S3_SLOW_TIMEOUT_MS 5

/* Q5: pulse code a client sends on its resmgr connection. */
#define S3_PULSE_CODE (_PULSE_CODE_MINAVAIL + 2)
/* Q2: a blocked read is replied to this long after it arrived. */
#define S3_READ_DELAY_MS 500

#endif
