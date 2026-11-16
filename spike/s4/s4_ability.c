/* Spike S4 (#34) Q3: are procmgr_ability settings kept and locked across setuid?
 * Run as root: ./s4_ability   (uses uid 1104, guard_plan) */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <sys/procmgr.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_UID 1104

#ifndef _NTO_CHF_PUBLIC
#error "_NTO_CHF_PUBLIC missing: record this in the S4 findings"
#endif

static const char *rcs(int rc) { return rc == EOK ? "EOK" : strerror(rc); }

static int try_public_channel(void) {
    int chid = ChannelCreate(_NTO_CHF_PUBLIC);
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

/* A: allow + lock as root, then setuid. */
static void test_a(void) {
    int rc = procmgr_ability(
        0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AOP_LOCK | PROCMGR_AID_PUBLIC_CHANNEL,
        PROCMGR_AID_EOL);
    report("A", "root: allow+lock nonroot PUBLIC_CHANNEL", rc, 1);
    report("A", "setuid(1104)", setuid(TEST_UID) == 0 ? EOK : errno, 1);
    report("A", "after setuid: public channel (kept?)", try_public_channel(), 1);
    rc = procmgr_ability(0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_DENY | PROCMGR_AID_PUBLIC_CHANNEL,
                         PROCMGR_AID_EOL);
    report("A", "after setuid: change locked ability", rc, 0);
    rc = procmgr_ability(0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_SETUID,
                         PROCMGR_AID_EOL);
    report("A", "after setuid: widen unlocked SETUID", rc, 0);
    report("A", "after setuid: setuid(0)", setuid(0) == 0 ? EOK : errno, 0);
}

/* B: control. No ability change, so nonroot should not get a public channel. */
static void test_b(void) {
    report("B", "setuid(1104), no abilities set", setuid(TEST_UID) == 0 ? EOK : errno, 1);
    report("B", "after setuid: public channel", try_public_channel(), 0);
}

/* C: architecture order. Allow as root, setuid, then lock (docs/architecture.md). */
static void test_c(void) {
    int rc = procmgr_ability(
        0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_PUBLIC_CHANNEL, PROCMGR_AID_EOL);
    report("C", "root: allow nonroot PUBLIC_CHANNEL", rc, 1);
    report("C", "setuid(1104)", setuid(TEST_UID) == 0 ? EOK : errno, 1);
    rc = procmgr_ability(
        0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AOP_LOCK | PROCMGR_AID_PUBLIC_CHANNEL,
        PROCMGR_AID_EOL);
    report("C", "after setuid: lock it", rc, 1);
    report("C", "after lock: public channel", try_public_channel(), 1);
    rc = procmgr_ability(0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_DENY | PROCMGR_AID_PUBLIC_CHANNEL,
                         PROCMGR_AID_EOL);
    report("C", "after lock: change it", rc, 0);
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
