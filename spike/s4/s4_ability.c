/* Spike S4 (#34) Q3: are procmgr_ability settings kept and locked across setuid?
 * Run as root: ./s4_ability   (uses uid 1104, guard_plan)
 *
 * SDP 8.0: a channel is public unless created with _NTO_CHF_PRIVATE, and a public
 * channel needs PROCMGR_AID_PUBLIC_CHANNEL. The default allows it for root and non-root,
 * so these tests DENY it for non-root and check the denial survives setuid. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <sys/procmgr.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_UID 1104

static const char *rcs(int rc) { return rc == EOK ? "EOK" : strerror(rc); }

static int try_channel(unsigned flags) {
    int chid = ChannelCreate(flags);
    if (chid == -1) {
        return errno;
    }
    ChannelDestroy(chid);
    return EOK;
}

static void report(const char *test, const char *what, int rc, int want_ok) {
    int ok = (rc == EOK) == want_ok;
    printf("%s %-46s %-24s %s\n", test, what, rcs(rc), ok ? "as-expected" : "UNEXPECTED");
}

static int deny_public_nonroot(unsigned extra) {
    return procmgr_ability(
        0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_DENY | extra | PROCMGR_AID_PUBLIC_CHANNEL,
        PROCMGR_AID_EOL);
}

/* A: deny+lock as root, then setuid. */
static void test_a(void) {
    report("A", "root: deny+lock nonroot PUBLIC_CHANNEL", deny_public_nonroot(PROCMGR_AOP_LOCK), 1);
    report("A", "root: public channel still ok", try_channel(0), 1);
    report("A", "setuid(1104)", setuid(TEST_UID) == 0 ? EOK : errno, 1);
    report("A", "after setuid: public channel (deny kept?)", try_channel(0), 0);
    report("A", "after setuid: private channel", try_channel(_NTO_CHF_PRIVATE), 1);
    int rc = procmgr_ability(
        0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_PUBLIC_CHANNEL, PROCMGR_AID_EOL);
    report("A", "after setuid: re-allow locked ability", rc, 0);
    rc = procmgr_ability(0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_SETUID,
                         PROCMGR_AID_EOL);
    report("A", "after setuid: widen SETUID", rc, 0);
    report("A", "after setuid: setuid(0)", setuid(0) == 0 ? EOK : errno, 0);
}

/* B: control. No ability change: default allows a non-root public channel. */
static void test_b(void) {
    report("B", "setuid(1104), no abilities set", setuid(TEST_UID) == 0 ? EOK : errno, 1);
    report("B", "after setuid: public channel", try_channel(0), 1);
}

/* C: architecture order. Make the channel as root, setuid, then deny+lock. */
static void test_c(void) {
    int chid = ChannelCreate(0);
    report("C", "root: create public channel", chid == -1 ? errno : EOK, 1);
    report("C", "setuid(1104)", setuid(TEST_UID) == 0 ? EOK : errno, 1);
    report("C", "after setuid: deny+lock PUBLIC_CHANNEL", deny_public_nonroot(PROCMGR_AOP_LOCK), 1);
    report("C", "after lock: new public channel", try_channel(0), 0);
    int rc = procmgr_ability(
        0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_PUBLIC_CHANNEL, PROCMGR_AID_EOL);
    report("C", "after lock: re-allow it", rc, 0);
    if (chid != -1) {
        report("C", "channel made as root still usable", ChannelDestroy(chid) == -1 ? errno : EOK,
               1);
    }
}

static void run(void (*fn)(void)) {
    pid_t pid = fork();
    if (pid == 0) {
        fn();
        fflush(stdout);
        _exit(0);
    }
    waitpid(pid, NULL, 0);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (getuid() != 0) {
        fprintf(stderr, "run as root\n");
        return 2;
    }
    run(test_a);
    run(test_b);
    run(test_c);
    return 0;
}
