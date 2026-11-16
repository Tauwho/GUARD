/* Spike S4 (#34) Q4: is the uid from ConnectClientInfo reliable?
 * Also used for Q2: run as a secpol type with no connect rule and read the errno.
 *   ./s4_uid            root: server plus one forked client per GUARD uid
 *   ./s4_uid client     one client as the current user (for the secpol denial test)
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/iomsg.h>
#include <sys/neutrino.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define S4_NAME "s4uid"
#define S4_MSG_TYPE (_IO_MAX + 0x40)

struct s4_req {
    uint16_t type;
    uint16_t subtype;
};
struct s4_rep {
    uint32_t ruid;
    uint32_t euid;
};

static int client_once(uint32_t *ruid, uint32_t *euid) {
    int coid = name_open(S4_NAME, 0);
    if (coid == -1) {
        return errno;
    }
    struct s4_req req = {S4_MSG_TYPE, 0};
    struct s4_rep rep = {0, 0};
    int rc = MsgSend(coid, &req, sizeof req, &rep, sizeof rep) == -1 ? errno : EOK;
    name_close(coid);
    *ruid = rep.ruid;
    *euid = rep.euid;
    return rc;
}

static int client_main(void) {
    uint32_t r = 0, e = 0;
    int rc = client_once(&r, &e);
    printf("client uid=%d: connect+send %s (errno %d)\n", (int)getuid(),
           rc == EOK ? "ok" : strerror(rc), rc);
    return rc == EOK ? 0 : 1;
}

/* mode 0: setuid(uid). mode 1: seteuid(uid) only, ruid stays 0. */
static void child(uid_t uid, int mode) {
    usleep(200 * 1000);
    if ((mode == 0 ? setuid(uid) : seteuid(uid)) != 0) {
        printf("child: set%suid(%d) failed: %s\n", mode ? "e" : "", (int)uid, strerror(errno));
        _exit(1);
    }
    uint32_t r = 0, e = 0;
    int rc = client_once(&r, &e);
    if (rc != EOK) {
        printf("%-16s uid=%-5d send failed: %s\n", mode ? "seteuid" : "setuid", (int)uid,
               strerror(rc));
        _exit(1);
    }
    int ok = mode == 0 ? (r == uid && e == uid) : (r == 0 && e == uid);
    printf("%-16s uid=%-5d server saw ruid=%-5u euid=%-5u %s\n", mode ? "seteuid" : "setuid",
           (int)uid, r, e, ok ? "match" : "MISMATCH");
    _exit(ok ? 0 : 1);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc > 1 && strcmp(argv[1], "client") == 0) {
        return client_main();
    }
    if (getuid() != 0) {
        fprintf(stderr, "run as root (or use: s4_uid client)\n");
        return 2;
    }
    name_attach_t *att = name_attach(NULL, S4_NAME, 0);
    if (att == NULL) {
        perror("name_attach");
        return 1;
    }
    static const uid_t uids[] = {1101, 1102, 1103, 1104, 1105, 1106};
    int nchild = 0;
    for (size_t i = 0; i < sizeof uids / sizeof uids[0]; i++, nchild++) {
        if (fork() == 0) {
            child(uids[i], 0);
        }
    }
    if (fork() == 0) {
        child(1104, 1);
    }
    nchild++;

    int served = 0;
    while (served < nchild) {
        union {
            struct _pulse pulse;
            struct _io_connect connect;
            struct s4_req req;
            uint16_t type;
        } msg;
        struct _msg_info info;
        uint64_t limit_ns = 5000000000ull; /* bounded: a failed child must not hang us */
        TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &limit_ns, NULL);
        int rcvid = MsgReceive(att->chid, &msg, sizeof msg, &info);
        if (rcvid == -1) {
            perror("MsgReceive (timeout means a client never sent)");
            break;
        }
        if (rcvid == 0) {
            continue; /* disconnect pulses */
        }
        if (msg.type == _IO_CONNECT) {
            MsgReply(rcvid, EOK, NULL, 0);
            continue;
        }
        if (msg.type != S4_MSG_TYPE) {
            MsgError(rcvid, ENOSYS);
            continue;
        }
        struct _client_info ci;
        struct s4_rep rep = {0, 0};
        if (ConnectClientInfo(info.scoid, &ci, 0) == -1) {
            MsgError(rcvid, errno);
            served++;
            continue;
        }
        rep.ruid = ci.cred.ruid;
        rep.euid = ci.cred.euid;
        MsgReply(rcvid, EOK, &rep, sizeof rep);
        served++;
    }
    int all_ok = 1, st;
    while (wait(&st) > 0) {
        all_ok &= WIFEXITED(st) && WEXITSTATUS(st) == 0;
    }
    printf("Q4 ConnectClientInfo uid matches every client: %s\n", all_ok ? "yes" : "no");
    name_detach(att, 0);
    return all_ok ? 0 : 1;
}
