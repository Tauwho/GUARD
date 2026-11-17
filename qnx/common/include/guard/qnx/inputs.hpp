// qnx/common: finds and reads the normalized input files on the target (B3, #38, #141).
// host/prepare_inputs.py writes JSON-subset YAML with the same layout as scenarios/:
//   <root>/F0.yaml, <root>/maps/<map id>.yaml, <root>/config.yaml
// The core loaders parse the buffers; this file only locates and reads them.
#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace guard::qnx {

constexpr std::size_t kInputMax = 1 << 20; // same 1 MiB cap as host/main.cpp

// Reads the whole file into buf[0..cap). Returns EOK, ENOENT, EFBIG, or errno.
inline int read_file(const char *path, char *buf, std::size_t cap, std::size_t &n) {
    std::FILE *f = std::fopen(path, "rb");
    if (f == nullptr)
        return errno == 0 ? ENOENT : errno;
    n = std::fread(buf, 1, cap, f);
    bool more = std::fgetc(f) != EOF;
    bool bad = std::ferror(f) != 0;
    std::fclose(f);
    return more ? EFBIG : bad ? EIO : EOK;
}

// The value of the top-level string key `"map"` in a normalized scenario, e.g.
// "hospital_corridor". Only [a-z0-9_] is accepted. Returns false if missing or invalid.
inline bool scenario_map_id(const char *text, std::size_t len, char *out, std::size_t outlen) {
    const char key[] = "\"map\":";
    for (std::size_t i = 0; i + sizeof key - 1 < len; ++i) {
        if (std::memcmp(text + i, key, sizeof key - 1) != 0)
            continue;
        std::size_t j = i + sizeof key - 1;
        while (j < len && text[j] == ' ')
            ++j;
        if (j >= len || text[j] != '"')
            return false;
        std::size_t k = ++j, n = 0;
        while (k < len && text[k] != '"') {
            char c = text[k];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') || n + 1 >= outlen)
                return false;
            out[n++] = c;
            ++k;
        }
        if (k >= len || n == 0)
            return false;
        out[n] = '\0';
        return true;
    }
    return false;
}

// <dir of scenario_path>/<rest>, e.g. rest "maps/x.yaml" or "config.yaml".
inline bool sibling_path(const char *scenario_path, const char *rest, char *out,
                         std::size_t outlen) {
    const char *slash = std::strrchr(scenario_path, '/');
    if (slash == nullptr)
        return false;
    int n = std::snprintf(out, outlen, "%.*s/%s", static_cast<int>(slash - scenario_path),
                          scenario_path, rest);
    return n > 0 && static_cast<std::size_t>(n) < outlen;
}

} // namespace guard::qnx
