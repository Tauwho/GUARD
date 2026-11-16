// guard-hog <prio> <ms> [-c <cpu>]: busy loop at SCHED_FIFO <prio> for <ms> (fault F2).
// Test image only. Exit 0 on success, 2 on bad arguments, 1 otherwise.
#include "guard/hog.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
    guard::hog::Options opt;
    if (guard::hog::parse(argc, argv, opt) != EOK) {
        std::fprintf(stderr, "usage: guard-hog <prio 1..63> <ms 1..60000> [-c <cpu>]\n");
        return 2;
    }
    int rc = guard::hog::run(opt);
    if (rc != EOK) {
        std::fprintf(stderr, "guard-hog: %s\n", std::strerror(rc));
        return 1;
    }
    return 0;
}
