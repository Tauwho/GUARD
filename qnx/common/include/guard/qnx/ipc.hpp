// qnx/common: named channels and bounded MsgSend (B-3, #37). Adapter only, no GUARD decisions.
#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

namespace guard::qnx {

// A name_attach() server. Not copyable; detaches on destruction.
class NamedServer {
  public:
    NamedServer() = default;
    NamedServer(const NamedServer &) = delete;
    NamedServer &operator=(const NamedServer &) = delete;
    ~NamedServer();

    // name_attach(NULL, name, 0). Returns EOK or errno.
    int attach(const char *name);
    void detach();
    int chid() const;

    // MsgReceive that answers name_open()'s _IO_CONNECT itself and returns the next pulse
    // (rcvid 0) or message. `limit_ns` > 0 bounds the wait (ETIMEDOUT). Returns the rcvid,
    // or -1 with errno set.
    int receive(void *buf, std::size_t len, struct _msg_info *info, std::uint64_t limit_ns);

  private:
    name_attach_t *attach_ = nullptr;
};

// name_open(name, 0). Returns EOK and sets coid, or errno.
int open_named(const char *name, int &coid);

// MsgSend wrapped in TimerTimeout(_NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, SIGEV_UNBLOCK).
// Returns EOK, ETIMEDOUT, or the errno from MsgSend (including a server's MsgError).
// A timeout while reply-blocked on a channel with _NTO_CHF_UNBLOCK only ends when the
// server answers the unblock pulse (ADR 0017).
int send_bounded(int coid, const void *sbuf, std::size_t slen, void *rbuf, std::size_t rlen,
                 std::uint64_t timeout_ns);

// Side-channel connection to one of this process's own channels, e.g. for timer pulses.
// Returns EOK and sets coid, or errno.
int connect_self(int chid, int &coid);

} // namespace guard::qnx
