#include "guard/qnx/resmgr.hpp"

#include <errno.h>
#include <string.h>

namespace guard::qnx {

ResMgr::ResMgr() { iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_, _RESMGR_IO_NFUNCS, &io_); }

ResMgr::~ResMgr() {
    if (id_ != -1) {
        resmgr_detach(dpp_, id_, _RESMGR_DETACH_ALL);
    }
    if (ctp_ != nullptr) {
        dispatch_context_free(ctp_);
    }
    if (dpp_ != nullptr) {
        dispatch_destroy(dpp_);
    }
}

int ResMgr::attach(const char *path, mode_t mode, unsigned flags) {
    if (dpp_ != nullptr) {
        return EBUSY;
    }
    dpp_ = dispatch_create();
    if (dpp_ == nullptr) {
        return errno;
    }
    iofunc_attr_init(&attr_, S_IFNAM | mode, nullptr, nullptr);
    resmgr_attr_t rattr;
    memset(&rattr, 0, sizeof rattr);
    rattr.nparts_max = 1;
    rattr.msg_max_size = 2048;
    id_ = resmgr_attach(dpp_, &rattr, path, _FTYPE_ANY, flags, &connect_, &io_, &attr_);
    if (id_ == -1) {
        return errno;
    }
    return EOK;
}

int ResMgr::on_pulse(int code, int (*fn)(message_context_t *, int, unsigned, void *),
                     void *handle) {
    if (dpp_ == nullptr) {
        return EINVAL;
    }
    return pulse_attach(dpp_, 0, code, fn, handle) == -1 ? errno : EOK;
}

int ResMgr::alloc_pulse(int &code, int (*fn)(message_context_t *, int, unsigned, void *),
                        void *handle) {
    if (dpp_ == nullptr) {
        return EINVAL;
    }
    code = pulse_attach(dpp_, MSG_FLAG_ALLOC_PULSE, 0, fn, handle);
    return code == -1 ? errno : EOK;
}

int ResMgr::self_coid(int &coid) {
    if (dpp_ == nullptr) {
        return EINVAL;
    }
    coid = message_connect(dpp_, MSG_FLAG_SIDE_CHANNEL);
    return coid == -1 ? errno : EOK;
}

int ResMgr::run_once() {
    if (dpp_ == nullptr) {
        return EINVAL;
    }
    if (ctp_ == nullptr && (ctp_ = dispatch_context_alloc(dpp_)) == nullptr) {
        return errno;
    }
    dispatch_context_t *next = dispatch_block(ctp_);
    if (next == nullptr) {
        return errno;
    }
    ctp_ = next;
    dispatch_handler(ctp_);
    return EOK;
}

} // namespace guard::qnx
