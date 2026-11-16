#include "guard/qnx/sched.hpp"

#include <doctest/doctest.h>
#include <errno.h>
#include <sched.h>

using guard::qnx::set_fifo;

TEST_CASE("set_fifo rejects priorities outside 1..63") {
    CHECK(set_fifo(0) == EINVAL);
    CHECK(set_fifo(64) == EINVAL);
    CHECK(set_fifo(255) == EINVAL);
}

TEST_CASE("set_fifo sets SCHED_FIFO and the priority") {
    REQUIRE(set_fifo(10) == EOK);
    int policy = 0;
    struct sched_param param {};
    REQUIRE(pthread_getschedparam(pthread_self(), &policy, &param) == EOK);
    CHECK(policy == SCHED_FIFO);
    CHECK(param.sched_priority == 10);
}
