#include "guard/qnx/ipc.hpp"

#include <atomic>
#include <cstdint>
#include <doctest/doctest.h>
#include <errno.h>
#include <pthread.h>
#include <sys/iomsg.h>
#include <unistd.h>

using namespace guard::qnx;

namespace {
constexpr std::uint64_t kMs = 1000000u;

struct Msg {
    std::uint16_t type;
    std::uint16_t subtype;
    std::int32_t value;
};
constexpr std::uint16_t kType = _IO_MAX + 0x50;

enum class Mode { echo, hold_then_error };

struct ServerArgs {
    NamedServer *server;
    Mode mode;
    std::atomic<int> unblock_pulses{0};
};

// One-message server: echo value+1, or hold the reply and answer the unblock pulse.
void *serve_one(void *p) {
    auto *args = static_cast<ServerArgs *>(p);
    union {
        struct _pulse pulse;
        Msg msg;
    } buf;
    struct _msg_info info;
    int held = -1;
    for (int i = 0; i < 4; ++i) {
        int rcvid = args->server->receive(&buf, sizeof buf, &info, 2000 * kMs);
        if (rcvid == -1) {
            break;
        }
        if (rcvid == 0) {
            if (buf.pulse.code == _PULSE_CODE_UNBLOCK && held != -1) {
                args->unblock_pulses++;
                MsgError(held, ETIMEDOUT);
                break;
            }
            continue;
        }
        if (args->mode == Mode::echo) {
            Msg reply = buf.msg;
            reply.value += 1;
            MsgReply(rcvid, EOK, &reply, sizeof reply);
            break;
        }
        held = rcvid; // hold_then_error: never reply, wait for the unblock pulse
    }
    return nullptr;
}
} // namespace

TEST_CASE("named server and bounded send: round trip") {
    NamedServer server;
    REQUIRE(server.attach("guard_test_ipc") == EOK);
    CHECK(server.chid() != -1);
    CHECK(server.attach("again") == EBUSY);
    ServerArgs args{&server, Mode::echo};
    pthread_t t;
    REQUIRE(pthread_create(&t, nullptr, serve_one, &args) == EOK);
    int coid = -1;
    REQUIRE(open_named("guard_test_ipc", coid) == EOK);
    Msg msg{kType, 0, 41};
    Msg reply{};
    CHECK(send_bounded(coid, &msg, sizeof msg, &reply, sizeof reply, 50 * kMs) == EOK);
    CHECK(reply.value == 42);
    name_close(coid);
    pthread_join(t, nullptr);
}

TEST_CASE("bounded send: ETIMEDOUT while send-blocked") {
    NamedServer server;
    REQUIRE(server.attach("guard_test_ipc_idle") == EOK);
    // Connect by name; nobody calls MsgReceive for the message, so it stays send-blocked.
    int coid = -1;
    pthread_t t;
    ServerArgs args{&server, Mode::echo};
    // Answer the name_open() handshake (receive() does), then stop receiving.
    struct HandshakeOnly {
        static void *run(void *p) {
            auto *a = static_cast<ServerArgs *>(p);
            union {
                struct _pulse pulse;
                Msg msg;
            } buf;
            while (a->server->receive(&buf, sizeof buf, nullptr, 200 * kMs) == 0) {
                // pulses only; returns -1 (ETIMEDOUT) once the handshake is over
            }
            return nullptr;
        }
    };
    REQUIRE(pthread_create(&t, nullptr, HandshakeOnly::run, &args) == EOK);
    REQUIRE(open_named("guard_test_ipc_idle", coid) == EOK);
    pthread_join(t, nullptr);
    Msg msg{kType, 0, 1};
    Msg reply{};
    CHECK(send_bounded(coid, &msg, sizeof msg, &reply, sizeof reply, 5 * kMs) == ETIMEDOUT);
    name_close(coid);
}

TEST_CASE("bounded send: reply-blocked timeout ends via the server's unblock answer") {
    NamedServer server;
    REQUIRE(server.attach("guard_test_ipc_hold") == EOK);
    ServerArgs args{&server, Mode::hold_then_error};
    pthread_t t;
    REQUIRE(pthread_create(&t, nullptr, serve_one, &args) == EOK);
    int coid = -1;
    REQUIRE(open_named("guard_test_ipc_hold", coid) == EOK);
    Msg msg{kType, 0, 1};
    Msg reply{};
    CHECK(send_bounded(coid, &msg, sizeof msg, &reply, sizeof reply, 5 * kMs) == ETIMEDOUT);
    name_close(coid);
    pthread_join(t, nullptr);
    CHECK(args.unblock_pulses.load() == 1);
}

TEST_CASE("open_named fails for an unknown name") {
    int coid = -1;
    CHECK(open_named("guard_test_no_such_name", coid) != EOK);
}

TEST_CASE("connect_self reaches the process's own channel") {
    int chid = ChannelCreate(_NTO_CHF_PRIVATE);
    REQUIRE(chid != -1);
    int coid = -1;
    REQUIRE(connect_self(chid, coid) == EOK);
    CHECK(MsgSendPulse(coid, 10, _PULSE_CODE_MINAVAIL, 7) == 0);
    struct _pulse pulse {};
    std::uint64_t ns = 100 * kMs;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, nullptr, &ns, nullptr);
    CHECK(MsgReceivePulse(chid, &pulse, sizeof pulse, nullptr) == 0);
    CHECK(pulse.value.sival_int == 7);
    ConnectDetach(coid);
    ChannelDestroy(chid);
}
