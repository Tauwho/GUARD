#include "guard/qnx/sched.hpp"

#include <errno.h>
#include <sched.h>

namespace guard::qnx {

int set_fifo(pthread_t thread, int prio) {
    if (prio < 1 || prio > kMaxPriority) {
        return EINVAL;
    }
    struct sched_param param {};
    param.sched_priority = prio;
    return pthread_setschedparam(thread, SCHED_FIFO, &param);
}

int set_fifo(int prio) { return set_fifo(pthread_self(), prio); }

} // namespace guard::qnx
