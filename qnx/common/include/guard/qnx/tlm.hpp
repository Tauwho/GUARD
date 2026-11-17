// qnx/common: telemetry ring in shared memory, the source side of C8 (B3, #38).
// One ring per process (src), one writer, any number of read-only readers (guard-bridge).
// The writer never blocks and never waits for a reader: a full ring overwrites the oldest
// line and the reader counts what it missed (AGENTS.md: the critical path never blocks on
// telemetry). Adapter only, no GUARD decisions.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace guard::qnx {

constexpr std::size_t kTlmSlots = 512;
constexpr std::size_t kTlmLine = 2040;          // one C8 JSON line, without the newline
constexpr std::uint32_t kTlmMagic = 0x474c4d54; // "TMLG"

struct TlmSlot {
    std::atomic<std::uint64_t> stamp; // index + 1 once committed; 0 while being written
    std::uint32_t len;
    char line[kTlmLine];
};

struct TlmRing {
    std::atomic<std::uint32_t> magic;
    std::uint32_t slots;
    std::atomic<std::uint64_t> head;       // lines written so far
    std::atomic<std::uint64_t> generation; // new value each time a writer (re)creates it
    TlmSlot slot[kTlmSlots];
};

// Shared-memory object name for a source, e.g. "/guard_tlm_sim".
// `src` is one of the C8 src values; writes at most `len` bytes. Returns false if too long.
bool tlm_name(const char *src, char *out, std::size_t len);

class TlmWriter {
  public:
    TlmWriter() = default;
    TlmWriter(const TlmWriter &) = delete;
    TlmWriter &operator=(const TlmWriter &) = delete;
    ~TlmWriter();

    // shm_open(O_CREAT, 0644) + mmap, then resets the ring. Call before dropping root.
    // `label` is the C8 src written in each line (default: the ring name). With `append`, a
    // valid ring is kept and seq continues from its line count (short-lived writers such as
    // guardctl). Returns EOK or errno.
    int open(const char *src, const char *label = nullptr, bool append = false);
    // Writes one C8 line {"v":1,"t_ns":..,"src":..,"seq":..,"kind":..,"payload":{<payload>}}.
    // `payload` is the JSON object body without braces. Returns false (and counts a local
    // drop) if the ring is not open or the line does not fit.
    bool emit(std::int64_t t_ns, const char *kind, const char *payload);
    std::uint64_t seq() const { return seq_; }
    std::uint64_t too_long() const { return too_long_; }

  private:
    TlmRing *ring_ = nullptr;
    char src_[16]{};
    std::uint64_t seq_ = 0, too_long_ = 0;
};

class TlmReader {
  public:
    TlmReader() = default;
    TlmReader(const TlmReader &) = delete;
    TlmReader &operator=(const TlmReader &) = delete;
    ~TlmReader();

    // Maps the ring of `src` read-only. Returns EOK, ENOENT if no writer created it yet, or errno.
    int open(const char *src);
    bool is_open() const { return ring_ != nullptr; }
    // Copies the next committed line into out[0..len) (no NUL), sets n, returns true.
    // Returns false when nothing new is there. Lines a writer overwrote before they were read
    // are added to dropped(); a writer restart starts again at its first line.
    bool next(char *out, std::size_t len, std::size_t &n);
    std::uint64_t dropped() const { return dropped_; }
    // Returns and clears the drops counted since the last call.
    std::uint64_t take_dropped();

  private:
    const TlmRing *ring_ = nullptr;
    std::uint64_t read_ = 0, generation_ = 0, dropped_ = 0, reported_ = 0;
};

} // namespace guard::qnx
