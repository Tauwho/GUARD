// qnx/common: single-threaded resource manager skeleton and delayed replies (B-3, #37).
// Adapter only, no GUARD decisions.
#pragma once

#include <cstddef>
#include <sys/dispatch.h>
#include <sys/iofunc.h>
#include <sys/neutrino.h>

namespace guard::qnx {

// One dispatch loop on one channel (ADR 0015). Set handlers through io() before attach().
class ResMgr {
  public:
    ResMgr();
    ResMgr(const ResMgr &) = delete;
    ResMgr &operator=(const ResMgr &) = delete;
    ~ResMgr();

    // iofunc defaults; replace the entries you handle (devctl, read, unblock, close_ocb, ...).
    resmgr_io_funcs_t &io() { return io_; }
    resmgr_connect_funcs_t &connect() { return connect_; }
    iofunc_attr_t &attr() { return attr_; }

    // dispatch_create + resmgr_attach(path) with `mode` (e.g. 0660) and resmgr `flags`
    // (_RESMGR_FLAG_SELF lets this process open its own path, e.g. in tests).
    // Returns EOK or errno.
    int attach(const char *path, mode_t mode, unsigned flags = 0);
    // A further path on the same dispatch (one loop, one channel), with its own handlers and
    // attribute, which the caller owns and keeps alive. Call after attach(). EOK or errno.
    int add_path(const char *path, resmgr_connect_funcs_t *connect, resmgr_io_funcs_t *io,
                 iofunc_attr_t *attr, unsigned flags = 0);
    // pulse_attach for `code`. Returns EOK or errno.
    int on_pulse(int code, int (*fn)(message_context_t *, int, unsigned, void *), void *handle);
    // pulse_attach with an allocated code, written to `code`. Returns EOK or errno.
    int alloc_pulse(int &code, int (*fn)(message_context_t *, int, unsigned, void *), void *handle);
    // Side-channel connection to this manager's channel, for timer pulses. Returns EOK or errno.
    int self_coid(int &coid);

    // Handles one message or pulse. Returns EOK or errno.
    int run_once();

  private:
    dispatch_t *dpp_ = nullptr;
    dispatch_context_t *ctp_ = nullptr;
    int id_ = -1;
    resmgr_connect_funcs_t connect_{};
    resmgr_io_funcs_t io_{};
    iofunc_attr_t attr_{};
};

// Fixed-capacity table of clients whose reply is sent later (MsgReply on a saved rcvid).
// On a channel with _NTO_CHF_UNBLOCK, a client whose TimerTimeout fires or who is killed
// stays blocked until the server answers its unblock pulse (spike S3, ADR 0017): call
// unblock() from the io unblock handler.
template <std::size_t N> class DelayedReplies {
  public:
    // Saves `rcvid` for `ocb`. Returns EOK, or EAGAIN if the table is full.
    int save(int rcvid, const void *ocb) {
        for (auto &s : slots_) {
            if (s.rcvid == -1) {
                s = {rcvid, ocb};
                return EOK;
            }
        }
        return EAGAIN;
    }
    // MsgReply(status, data) to the saved client of `ocb`; for a read, status is the byte
    // count. Returns EOK, ENOENT if none, or MsgReply's errno.
    int reply(const void *ocb, long status, const void *data, std::size_t len) {
        for (auto &s : slots_) {
            if (s.rcvid != -1 && s.ocb == ocb) {
                int rcvid = s.rcvid;
                s.rcvid = -1;
                return MsgReply(rcvid, status, data, len) == -1 ? errno : EOK;
            }
        }
        return ENOENT;
    }
    // If `rcvid` is saved, MsgError(rcvid, err), free the slot, and return true.
    bool unblock(int rcvid, int err) {
        for (auto &s : slots_) {
            if (s.rcvid == rcvid) {
                s.rcvid = -1;
                MsgError(rcvid, err);
                return true;
            }
        }
        return false;
    }
    // Forget clients of a closing OCB without replying (call from close_ocb).
    void drop(const void *ocb) {
        for (auto &s : slots_) {
            if (s.ocb == ocb) {
                s.rcvid = -1;
            }
        }
    }
    std::size_t pending() const {
        std::size_t n = 0;
        for (const auto &s : slots_) {
            n += s.rcvid != -1 ? 1 : 0;
        }
        return n;
    }

  private:
    struct Slot {
        int rcvid = -1;
        const void *ocb = nullptr;
    };
    Slot slots_[N]{};
};

} // namespace guard::qnx
