#include "guard/qnx/ipc.hpp"
#include "guard/qnx/priv.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <errno.h>
#include <pthread.h>
#include <sys/iomsg.h>
#include <sys/procmgr.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace guard::qnx;

namespace {
constexpr std::uint64_t kMs = 1000000u;
constexpr std::uint16_t kType = _IO_MAX + 0x51;
struct Req {
    std::uint16_t type;
    std::uint16_t subtype;
};
struct Rep {
    std::int32_t require_rc;
    std::uint32_t euid;
};

// Runs fn in a forked child (privilege drops are irreversible) and returns its exit code.
int in_child(int (*fn)()) {
    pid_t pid = fork();
    if (pid == 0) {
        _exit(fn());
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 255;
}

int public_channel() {
    int chid = ChannelCreate(0);
    if (chid == -1) {
        return errno;
    }
    ChannelDestroy(chid);
    return EOK;
}

// Exit codes: 0 ok, 1..9 = which check failed.
int drop_default() {
    int pre = ChannelCreate(0); // made as root before the drop
    if (pre == -1)
        return 1;
    if (drop_and_lock(1104, 1104, nullptr, 0) != EOK)
        return 2;
    if (getuid() != 1104 || geteuid() != 1104)
        return 3;
    if (setuid(0) == 0)
        return 4;
    if (public_channel() != EPERM)
        return 5;
    if (ChannelCreate(_NTO_CHF_PRIVATE) == -1)
        return 6;
    procmgr_ability(0, PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_PUBLIC_CHANNEL,
                    PROCMGR_AID_EOL); // returns EOK but must not take effect (spike S4)
    if (public_channel() != EPERM)
        return 7;
    if (ChannelDestroy(pre) == -1)
        return 8;
    return 0;
}

int drop_keep_public() {
    const unsigned keep[] = {PROCMGR_AID_PUBLIC_CHANNEL};
    if (drop_and_lock(1102, 1102, keep, 1) != EOK)
        return 2;
    if (public_channel() != EOK)
        return 5;
    return 0;
}

int drop_root_rejected() { return drop_and_lock(0, 0, nullptr, 0) == EINVAL ? 0 : 1; }

NamedServer *g_server;
void *uid_server(void *) {
    union {
        struct _pulse pulse;
        Req req;
    } buf;
    struct _msg_info info;
    const uid_t allowed[] = {1103};
    for (int served = 0; served < 2;) {
        int rcvid = g_server->receive(&buf, sizeof buf, &info, 3000 * kMs);
        if (rcvid == -1)
            break;
        if (rcvid == 0 || buf.req.type != kType)
            continue;
        Rep rep{require_euid(info.scoid, allowed, 1), 0};
        uid_t euid = 0;
        client_euid(info.scoid, euid);
        rep.euid = euid;
        MsgReply(rcvid, EOK, &rep, sizeof rep);
        served++;
    }
    return nullptr;
}

int ask_as(uid_t uid, int want_rc) {
    if (seteuid(uid) != 0)
        return 1;
    int coid = -1;
    if (open_named("guard_test_priv", coid) != EOK)
        return 2;
    Req req{kType, 0};
    Rep rep{};
    if (send_bounded(coid, &req, sizeof req, &rep, sizeof rep, 100 * kMs) != EOK)
        return 3;
    if (rep.euid != static_cast<std::uint32_t>(uid))
        return 4;
    return rep.require_rc == want_rc ? 0 : 5;
}
int ask_as_sup() { return ask_as(1103, EOK); }
int ask_as_plan() { return ask_as(1104, EPERM); }
} // namespace

TEST_CASE("drop_and_lock: drops uid, locks abilities, lock holds against re-allow") {
    CHECK(in_child(drop_default) == 0);
}

TEST_CASE("drop_and_lock: a kept ability stays allowed after the drop") {
    CHECK(in_child(drop_keep_public) == 0);
}

TEST_CASE("drop_and_lock: refuses uid 0") { CHECK(in_child(drop_root_rejected) == 0); }

TEST_CASE("require_euid: allows a listed euid, EPERM otherwise") {
    NamedServer server;
    REQUIRE(server.attach("guard_test_priv") == EOK);
    g_server = &server;
    pthread_t t;
    REQUIRE(pthread_create(&t, nullptr, uid_server, nullptr) == EOK);
    CHECK(in_child(ask_as_sup) == 0);
    CHECK(in_child(ask_as_plan) == 0);
    pthread_join(t, nullptr);
}
