// qnx/common: client uid checks and privilege drop (B-3, #37). Adapter only, no GUARD decisions.
#pragma once

#include <cstddef>
#include <sys/types.h>

namespace guard::qnx {

// Effective uid of the client behind `scoid` (struct _msg_info::scoid), via ConnectClientInfo.
// Use euid: after seteuid() the real uid can still be 0 (spike S4). Returns EOK or errno.
int client_euid(int scoid, uid_t &euid);

// EOK if the client's euid is in allowed[0..n), EPERM if not, or ConnectClientInfo's errno.
// Which uids are allowed is the caller's decision.
int require_euid(int scoid, const uid_t *allowed, std::size_t n);

// Privilege drop for a process that started as root and has already attached its path or
// channel and set its priority (docs/architecture.md):
//  1. for non-root, deny and lock SETUID, SETGID, SPAWN_SETUID, SPAWN_SETGID, PATHSPACE,
//     PUBLIC_CHANNEL and PRIORITY, except the ids in keep[0..n), which are allowed and locked;
//  2. setgid(gid), setuid(uid);
//  3. check the effect: setuid(0) must fail (skipped when GUARD_SECPOL_DEVELOP is set: a
//     secpol develop image grants and records every request). procmgr_ability() returns EOK even
//     when a locked ability is not changed (spike S4), so the return code alone proves nothing.
// Returns EOK, the errno of the failing step, or EIO if the check in step 3 fails.
// Irreversible for the calling process.
int drop_and_lock(uid_t uid, gid_t gid, const unsigned *keep, std::size_t n);

} // namespace guard::qnx
