#pragma once
#include "guard/buffer_json.hpp"
#include "guard/scenario.hpp"
#include <algorithm>

namespace guard::json_fields {
inline bool number(const BufferJson &json, std::size_t object, std::string_view key,
                   double &value) {
    return json.number(json.member(object, key), value);
}
inline bool bounded(double value, double low = -100, double high = 100) {
    return std::isfinite(value) && value >= low && value <= high;
}
inline bool name(const BufferJson &json, std::size_t index, Name &output) {
    const auto text = json.text(index);
    if (json.node(index).kind != BufferJson::Kind::string || text.empty() || text.size() > 32)
        return false;
    for (char c : text)
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9') &&
            c != '_')
            return false;
    std::copy(text.begin(), text.end(), output.begin());
    return true;
}
inline bool point(const BufferJson &json, std::size_t index, Point &output) {
    if (json.node(index).kind != BufferJson::Kind::array || json.node(index).next != index + 3)
        return false;
    return json.number(index + 1, output.x) && json.number(index + 2, output.y) &&
           bounded(output.x) && bounded(output.y);
}
inline bool pose(const BufferJson &json, std::size_t index, Pose &output) {
    return number(json, index, "x", output.x) && number(json, index, "y", output.y) &&
           number(json, index, "theta", output.theta) && bounded(output.x) && bounded(output.y) &&
           bounded(output.theta, -3.1416, 3.1416);
}
inline bool rectangle(const BufferJson &json, std::size_t index, Rect &output) {
    return point(json, json.member(index, "min"), output.min) &&
           point(json, json.member(index, "max"), output.max) && output.min.x <= output.max.x &&
           output.min.y <= output.max.y;
}
template <class Function>
bool array(const BufferJson &json, std::size_t index, std::size_t capacity, Function function,
           std::size_t &count, bool optional = false) {
    if (index == BufferJson::missing)
        return optional;
    if (json.node(index).kind != BufferJson::Kind::array)
        return false;
    count = 0;
    for (std::size_t child = index + 1; child < json.node(index).next;
         child = json.node(child).next) {
        if (count >= capacity || !function(child, count))
            return false;
        ++count;
    }
    return true;
}
inline bool integer(const BufferJson &json, std::size_t object, std::string_view key,
                    std::uint32_t &value, bool optional = false) {
    const auto index = json.member(object, key);
    if (index == BufferJson::missing)
        return optional;
    std::uint64_t parsed;
    if (!json.unsigned_integer(index, parsed) || parsed > 0xffffffffu)
        return false;
    value = static_cast<std::uint32_t>(parsed);
    return true;
}
} // namespace guard::json_fields
