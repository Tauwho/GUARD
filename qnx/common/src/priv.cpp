#include "guard/qnx/priv.hpp"

#include <cstdio>
#include <cstring>

#include <errno.h>
#include <sys/neutrino.h>
#include <sys/procmgr.h>
#include <unistd.h>

namespace guard::qnx {

int client_euid(int scoid, uid_t &euid) {
    struct _client_info info;
    if (ConnectClientInfo(scoid, &info, 0) == -1) {
        return errno;
    }
    euid = info.cred.euid;
    return EOK;
}

int require_euid(int scoid, const uid_t *allowed, std::size_t n) {
    uid_t euid = 0;
    int rc = client_euid(scoid, euid);
    if (rc != EOK) {
        return rc;
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (allowed[i] == euid) {
            return EOK;
        }
    }
    return EPERM;
}

namespace {
constexpr unsigned kLocked[] = {PROCMGR_AID_SETUID,       PROCMGR_AID_SETGID,
                                PROCMGR_AID_SPAWN_SETUID, PROCMGR_AID_SPAWN_SETGID,
                                PROCMGR_AID_PATHSPACE,    PROCMGR_AID_PUBLIC_CHANNEL,
                                PROCMGR_AID_PRIORITY};

bool secpol_develop() {
    char mode[16] = "";
    std::FILE *f = std::fopen("/system/etc/guard-secpol", "r");
    if (f == nullptr)
        return false;
    bool ok = std::fgets(mode, sizeof mode, f) != nullptr;
    std::fclose(f);
    return ok && std::strncmp(mode, "develop", 7) == 0;
}

bool kept(unsigned id, const unsigned *keep, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        if (keep[i] == id) {
            return true;
        }
    }
    return false;
}
} // namespace

int drop_and_lock(uid_t uid, gid_t gid, const unsigned *keep, std::size_t n) {
    if (uid == 0) {
        return EINVAL;
    }
    for (unsigned id : kLocked) {
        unsigned op = kept(id, keep, n) ? PROCMGR_AOP_ALLOW : PROCMGR_AOP_DENY;
        int rc =
            procmgr_ability(0, PROCMGR_ADN_NONROOT | op | PROCMGR_AOP_LOCK | id, PROCMGR_AID_EOL);
        if (rc != EOK) {
            return rc;
        }
    }
    if (setgid(gid) == -1 || setuid(uid) == -1) {
        return errno;
    }
    // secpol develop mode grants and records every request, so the setuid(0) probe would
    // both succeed and put setuid-to-root into the generated policy (image/secpol). Only the
    // image itself can say "develop": /system/etc/guard-secpol is read-only, written by
    // image/build-image.sh.
    if (secpol_develop()) {
        return getuid() == uid && geteuid() == uid ? EOK : EIO;
    }
    if (setuid(0) == 0 || getuid() != uid || geteuid() != uid) {
        return EIO;
    }
    return EOK;
}

} // namespace guard::qnx
