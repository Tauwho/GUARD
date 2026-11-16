#pragma once
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

namespace guard {
// Bounded JSON reader for schema-validated YAML normalized by host-side PyYAML.
// No allocation, filesystem access, clocks, or external C++ parser dependency.
class BufferJson {
  public:
    enum class Kind { object, array, string, number, boolean, null_value };
    struct Node {
        Kind kind;
        std::size_t begin = 0, end = 0, next = 0;
    };
    static constexpr std::size_t missing = 4096;
    explicit BufferJson(std::string_view source) : source_(source) {
        valid_ = value(0);
        whitespace();
        valid_ = valid_ && position_ == source_.size();
    }
    bool valid() const { return valid_; }
    const Node &node(std::size_t index) const { return nodes_[index < count_ ? index : missing]; }
    std::string_view text(std::size_t index) const {
        const auto &item = node(index);
        if (index >= count_)
            return {};
        return source_.substr(item.begin, item.end - item.begin);
    }
    std::size_t member(std::size_t index, std::string_view key) const {
        if (index >= count_ || node(index).kind != Kind::object)
            return missing;
        for (std::size_t child = index + 1; child < node(index).next;) {
            const std::size_t field = child + 1;
            if (text(child) == key)
                return field;
            child = node(field).next;
        }
        return missing;
    }
    bool number(std::size_t index, double &output) const {
        if (index >= count_ || node(index).kind != Kind::number)
            return false;
        const auto raw = text(index);
        if (raw.size() >= 64)
            return false;
        std::array<char, 64> buffer{};
        for (std::size_t i = 0; i < raw.size(); ++i)
            buffer[i] = raw[i];
        char *end = nullptr;
        const double parsed = std::strtod(buffer.data(), &end);
        if (end != buffer.data() + raw.size() || !std::isfinite(parsed))
            return false;
        output = parsed;
        return true;
    }
    bool unsigned_integer(std::size_t index, std::uint64_t &output) const {
        if (index >= count_ || node(index).kind != Kind::number)
            return false;
        const auto raw = text(index);
        const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), output);
        return parsed.ec == std::errc{} && parsed.ptr == raw.data() + raw.size();
    }
    bool boolean(std::size_t index, bool &output) const {
        if (index >= count_ || node(index).kind != Kind::boolean)
            return false;
        output = text(index) == "true";
        return true;
    }

  private:
    void whitespace() {
        while (position_ < source_.size() &&
               (source_[position_] == ' ' || source_[position_] == '\n' ||
                source_[position_] == '\r' || source_[position_] == '\t'))
            ++position_;
    }
    bool take(char character) {
        whitespace();
        if (position_ >= source_.size() || source_[position_] != character)
            return false;
        ++position_;
        return true;
    }
    bool string(std::size_t index) {
        nodes_[index].begin = ++position_;
        while (position_ < source_.size()) {
            const unsigned char c = source_[position_++];
            if (c == '"') {
                nodes_[index].end = position_ - 1;
                return true;
            }
            if (c < 32)
                return false;
            if (c == '\\') {
                if (position_ >= source_.size())
                    return false;
                const char escape = source_[position_++];
                if (escape == 'u') {
                    for (unsigned i = 0; i < 4; ++i) {
                        if (position_ >= source_.size() ||
                            std::string_view("0123456789abcdefABCDEF").find(source_[position_++]) ==
                                std::string_view::npos)
                            return false;
                    }
                } else if (std::string_view("\"\\/bfnrt").find(escape) == std::string_view::npos)
                    return false;
            }
        }
        return false;
    }
    bool value(unsigned depth) {
        whitespace();
        if (depth > 32 || count_ >= missing || position_ >= source_.size())
            return false;
        const std::size_t index = count_++;
        const char first = source_[position_];
        nodes_[index] = {Kind::null_value, position_, position_, count_};
        if (first == '{' || first == '[') {
            nodes_[index].kind = first == '{' ? Kind::object : Kind::array;
            ++position_;
            const char closing = first == '{' ? '}' : ']';
            if (!take(closing)) {
                do {
                    if (first == '{') {
                        whitespace();
                        if (position_ >= source_.size() || source_[position_] != '"')
                            return false;
                        const auto key = count_;
                        if (!value(depth + 1) || !take(':'))
                            return false;
                        for (std::size_t prior = index + 1; prior < key;
                             prior = nodes_[prior + 1].next)
                            if (text(prior) == text(key))
                                return false;
                    }
                    if (!value(depth + 1))
                        return false;
                } while (take(','));
                if (!take(closing))
                    return false;
            }
        } else if (first == '"') {
            nodes_[index].kind = Kind::string;
            if (!string(index))
                return false;
        } else if (first == 't' || first == 'f' || first == 'n') {
            const auto literal = first == 't' ? "true" : first == 'f' ? "false" : "null";
            if (source_.substr(position_, std::char_traits<char>::length(literal)) != literal)
                return false;
            position_ += std::char_traits<char>::length(literal);
            nodes_[index].kind = first == 'n' ? Kind::null_value : Kind::boolean;
        } else {
            nodes_[index].kind = Kind::number;
            if (first == '-')
                ++position_;
            if (position_ >= source_.size())
                return false;
            if (source_[position_] == '0')
                ++position_;
            else {
                if (source_[position_] < '1' || source_[position_] > '9')
                    return false;
                while (position_ < source_.size() && source_[position_] >= '0' &&
                       source_[position_] <= '9')
                    ++position_;
            }
            if (position_ < source_.size() && source_[position_] == '.') {
                ++position_;
                const auto begin = position_;
                while (position_ < source_.size() && source_[position_] >= '0' &&
                       source_[position_] <= '9')
                    ++position_;
                if (begin == position_)
                    return false;
            }
            if (position_ < source_.size() &&
                (source_[position_] == 'e' || source_[position_] == 'E')) {
                ++position_;
                if (position_ < source_.size() &&
                    (source_[position_] == '+' || source_[position_] == '-'))
                    ++position_;
                const auto begin = position_;
                while (position_ < source_.size() && source_[position_] >= '0' &&
                       source_[position_] <= '9')
                    ++position_;
                if (begin == position_)
                    return false;
            }
        }
        if (first != '"')
            nodes_[index].end = position_;
        nodes_[index].next = count_;
        return true;
    }
    std::string_view source_;
    std::array<Node, missing + 1> nodes_{};
    std::size_t count_ = 0, position_ = 0;
    bool valid_ = false;
};
} // namespace guard
