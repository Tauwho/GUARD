// qnx/common: SCHED_FIFO helper (B-3, #37). Adapter only, no GUARD decisions.
#pragma once

#include <pthread.h>

namespace guard::qnx {

// tracelogger user events (B11, #48): one code per step of a lease, shown on the timeline.
// trace_logf() costs nothing visible when no tracelogger runs.
enum TraceCode : int { kTraceProposal = 1, kTraceDecision = 2, kTraceLease = 3, kTraceExpiry = 4 };

// Highest priority GUARD may use (docs/architecture.md REF-PRIORITIES).
constexpr int kMaxPriority = 63;

// Sets SCHED_FIFO at `prio` for `thread`. Returns EOK, EINVAL for prio outside 1..63,
// or the errno from pthread_setschedparam.
int set_fifo(pthread_t thread, int prio);

// Same, for the calling thread.
int set_fifo(int prio);

} // namespace guard::qnx
