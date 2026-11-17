#include "guard/qnx/tlm.hpp"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

namespace guard::qnx {

bool tlm_name(const char *src, char *out, std::size_t len) {
    int n = std::snprintf(out, len, "/guard_tlm_%s", src);
    return n > 0 && static_cast<std::size_t>(n) < len;
}

TlmWriter::~TlmWriter() {
    if (ring_ != nullptr)
        munmap(ring_, sizeof(TlmRing));
}

int TlmWriter::open(const char *src) {
    char name[48];
    if (ring_ != nullptr)
        return EBUSY;
    if (src == nullptr || std::strlen(src) >= sizeof src_ || !tlm_name(src, name, sizeof name))
        return EINVAL;
    int fd = shm_open(name, O_RDWR | O_CREAT, 0644);
    if (fd == -1)
        return errno;
    // A restarted writer may find the object owned by an earlier run; keep it readable.
    fchmod(fd, 0644);
    if (ftruncate(fd, sizeof(TlmRing)) == -1) {
        int e = errno;
        close(fd);
        return e;
    }
    void *p = mmap(nullptr, sizeof(TlmRing), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int e = errno;
    close(fd);
    if (p == MAP_FAILED)
        return e;
    ring_ = static_cast<TlmRing *>(p);
    // Reset: readers that see the new generation start again at line 0.
    ring_->magic = 0;
    for (auto &s : ring_->slot)
        s.stamp.store(0, std::memory_order_relaxed);
    ring_->head.store(0, std::memory_order_relaxed);
    ring_->slots = kTlmSlots;
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ring_->generation.store(static_cast<std::uint64_t>(ts.tv_sec) * 1000000000u + ts.tv_nsec + 1,
                            std::memory_order_release);
    ring_->magic = kTlmMagic;
    std::strcpy(src_, src);
    seq_ = 0;
    return EOK;
}

bool TlmWriter::emit(std::int64_t t_ns, const char *kind, const char *payload) {
    if (ring_ == nullptr || t_ns < 0)
        return false;
    std::uint64_t idx = ring_->head.load(std::memory_order_relaxed);
    TlmSlot &s = ring_->slot[idx % kTlmSlots];
    s.stamp.store(0, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    int n = std::snprintf(s.line, kTlmLine,
                          "{\"v\":1,\"t_ns\":%" PRId64 ",\"src\":\"%s\",\"seq\":%" PRIu64
                          ",\"kind\":\"%s\",\"payload\":{%s}}",
                          t_ns, src_, seq_, kind, payload);
    if (n <= 0 || static_cast<std::size_t>(n) >= kTlmLine) {
        ++too_long_;
        return false; // slot stays uncommitted; head does not move, so readers never see it
    }
    s.len = static_cast<std::uint32_t>(n);
    s.stamp.store(idx + 1, std::memory_order_release);
    ring_->head.store(idx + 1, std::memory_order_release);
    ++seq_;
    return true;
}

TlmReader::~TlmReader() {
    if (ring_ != nullptr)
        munmap(const_cast<TlmRing *>(ring_), sizeof(TlmRing));
}

int TlmReader::open(const char *src) {
    char name[48];
    if (ring_ != nullptr)
        return EBUSY;
    if (src == nullptr || !tlm_name(src, name, sizeof name))
        return EINVAL;
    int fd = shm_open(name, O_RDONLY, 0);
    if (fd == -1)
        return errno;
    void *p = mmap(nullptr, sizeof(TlmRing), PROT_READ, MAP_SHARED, fd, 0);
    int e = errno;
    close(fd);
    if (p == MAP_FAILED)
        return e;
    ring_ = static_cast<const TlmRing *>(p);
    generation_ = 0;
    read_ = 0;
    return EOK;
}

bool TlmReader::next(char *out, std::size_t len, std::size_t &n) {
    if (ring_ == nullptr || ring_->magic != kTlmMagic)
        return false;
    std::uint64_t gen = ring_->generation.load(std::memory_order_acquire);
    if (gen != generation_) {
        generation_ = gen;
        read_ = 0;
    }
    for (;;) {
        std::uint64_t head = ring_->head.load(std::memory_order_acquire);
        if (read_ >= head)
            return false;
        if (head - read_ > kTlmSlots) {
            dropped_ += head - kTlmSlots - read_;
            read_ = head - kTlmSlots;
        }
        const TlmSlot &s = ring_->slot[read_ % kTlmSlots];
        std::uint64_t before = s.stamp.load(std::memory_order_acquire);
        if (before != read_ + 1) {
            ++dropped_; // overwritten (or being overwritten) before we got to it
            ++read_;
            continue;
        }
        std::size_t got = s.len;
        if (got > kTlmLine || got > len) {
            ++dropped_;
            ++read_;
            continue;
        }
        std::memcpy(out, s.line, got);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (s.stamp.load(std::memory_order_relaxed) != before) {
            ++dropped_;
            ++read_;
            continue;
        }
        ++read_;
        n = got;
        return true;
    }
}

std::uint64_t TlmReader::take_dropped() {
    std::uint64_t d = dropped_ - reported_;
    reported_ = dropped_;
    return d;
}

} // namespace guard::qnx
