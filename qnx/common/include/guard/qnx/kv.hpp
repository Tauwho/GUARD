// qnx/common: reads one value from a C4-style "key: value" snapshot (B3, #38).
// Used for the /dev/guard/status and /dev/sim/world text. Adapter only, no GUARD decisions.
#pragma once

#include <cstddef>
#include <cstring>

namespace guard::qnx {

// Copies the value of the first line "<key>: <value>" in text[0..len) into out (NUL-ended).
// Returns false if the key is missing or the value does not fit.
inline bool kv_get(const char *text, std::size_t len, const char *key, char *out,
                   std::size_t outlen) {
    const std::size_t klen = std::strlen(key);
    std::size_t i = 0;
    while (i < len) {
        std::size_t end = i;
        while (end < len && text[end] != '\n')
            ++end;
        if (end - i > klen + 1 && std::strncmp(text + i, key, klen) == 0 && text[i + klen] == ':') {
            std::size_t v = i + klen + 1;
            while (v < end && text[v] == ' ')
                ++v;
            if (end - v >= outlen)
                return false;
            std::memcpy(out, text + v, end - v);
            out[end - v] = '\0';
            return true;
        }
        i = end + 1;
    }
    return false;
}

} // namespace guard::qnx
