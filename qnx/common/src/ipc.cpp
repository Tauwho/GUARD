#include "guard/qnx/ipc.hpp"

#include <errno.h>
#include <sys/iomsg.h>

namespace guard::qnx {

NamedServer::~NamedServer() { detach(); }

int NamedServer::attach(const char *name) {
    if (attach_ != nullptr) {
        return EBUSY;
    }
    attach_ = name_attach(nullptr, name, 0);
    return attach_ == nullptr ? errno : EOK;
}

void NamedServer::detach() {
    if (attach_ != nullptr) {
        name_detach(attach_, 0);
        attach_ = nullptr;
    }
}

int NamedServer::chid() const { return attach_ == nullptr ? -1 : attach_->chid; }

int NamedServer::receive(void *buf, std::size_t len, struct _msg_info *info,
                         std::uint64_t limit_ns) {
    if (attach_ == nullptr || len < sizeof(struct _pulse)) {
        errno = EINVAL;
        return -1;
    }
    for (;;) {
        if (limit_ns > 0) {
            std::uint64_t ns = limit_ns;
            TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, nullptr, &ns, nullptr);
        }
        int rcvid = MsgReceive(attach_->chid, buf, len, info);
        if (rcvid <= 0) {
            return rcvid; // pulse (0) or error (-1, errno set)
        }
        if (static_cast<struct _io_connect *>(buf)->type == _IO_CONNECT) {
            MsgReply(rcvid, EOK, nullptr, 0); // name_open handshake
            continue;
        }
        return rcvid;
    }
}

int open_named(const char *name, int &coid) {
    coid = name_open(name, 0);
    return coid == -1 ? errno : EOK;
}

int send_bounded(int coid, const void *sbuf, std::size_t slen, void *rbuf, std::size_t rlen,
                 std::uint64_t timeout_ns) {
    struct sigevent unblock;
    SIGEV_UNBLOCK_INIT(&unblock);
    std::uint64_t ns = timeout_ns;
    if (TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &unblock, &ns,
                     nullptr) == -1) {
        return errno;
    }
    return MsgSend(coid, sbuf, slen, rbuf, rlen) == -1 ? errno : EOK;
}

int connect_self(int chid, int &coid) {
    coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
    return coid == -1 ? errno : EOK;
}

} // namespace guard::qnx
