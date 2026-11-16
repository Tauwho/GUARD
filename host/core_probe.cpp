// Host-only property-test driver. It calls the production codecs and gate FSM.
#include "guard/codec.hpp"
#include "guard/gate_fsm.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int nibble(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}
void codec(const std::string &hex) {
    if (hex.size() % 2 || hex.size() > 320)
        throw std::runtime_error("hex input must contain at most 160 bytes");
    std::vector<std::uint8_t> bytes(hex.size() / 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto high = nibble(hex[2 * i]), low = nibble(hex[2 * i + 1]);
        if (high < 0 || low < 0)
            throw std::runtime_error("invalid hex");
        bytes[i] = static_cast<std::uint8_t>(high * 16 + low);
    }
    guard::Lease value;
    if (guard::decode(bytes.data(), bytes.size(), value) != guard::DecodeError::none) {
        std::cout << "{\"valid\":false}\n";
        return;
    }
    std::array<std::uint8_t, sizeof(guard::Lease)> encoded{};
    if (!guard::encode(value, encoded))
        throw std::runtime_error("decoded lease failed encode");
    std::cout << "{\"valid\":true,\"wire\":\"";
    for (const auto byte : encoded)
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(byte);
    std::cout << "\"}\n";
}
double speed_argument(const char *text) {
    char *end = nullptr;
    const auto value = std::strtod(text, &end);
    if (end == text || *end || !std::isfinite(value))
        throw std::runtime_error("invalid speed argument");
    return value; // Preserve representable subnormals even when libc sets ERANGE.
}
void gate_trace(double speed, unsigned duration, bool delivered) {
    if (speed < 0 || speed > 1.2 || duration < 50 || duration > 1000)
        throw std::runtime_error("invalid gate parameters");
    guard::Gate gate;
    guard::RegisterReply registration;
    if (!gate.register_supervisor(registration))
        throw std::runtime_error("register failed");
    for (unsigned tick = 0; tick < 5; ++tick)
        gate.tick(tick * 10'000'000, 0);
    guard::RecoverRequest recover;
    recover.request_id = 1;
    if (gate.recover(recover, {true, true, true}).reason != 512)
        throw std::runtime_error("recover failed");
    guard::Lease lease;
    lease.lease_id = lease.lease_seq = lease.proposal_id = lease.epoch = 1;
    lease.v_cmd = lease.v_cap = speed;
    lease.issued_ns = 50'000'000;
    lease.expires_ns = lease.issued_ns + duration * 1'000'000LL;
    if (gate.prepare_lease(lease, lease.issued_ns).reason != 500)
        throw std::runtime_error("prepare failed");
    const bool active = gate.commit_lease(1, lease.issued_ns, delivered);
    auto rejected = lease;
    rejected.lease_seq = rejected.lease_id = 2;
    rejected.issued_ns = 0;
    rejected.expires_ns = 50'000'000;
    const auto refused = gate.prepare_lease(rejected, 50'000'000);
    std::cout << std::setprecision(17) << "{\"active\":" << (active ? "true" : "false")
              << ",\"refused\":" << refused.reason
              << ",\"sequence\":" << gate.status(50'000'000).lease_seq << ",\"ticks\":[";
    double measured = 0;
    for (unsigned ms = 50; ms <= 50 + duration + 1500; ms += 10) {
        const auto motor = gate.tick(ms * 1'000'000LL, measured);
        if (ms != 50)
            std::cout << ',';
        std::cout << '[' << ms << ',' << motor.gate_mode << ',' << motor.v << ']';
        measured = motor.v;
    }
    std::cout << "]}\n";
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "lease")
            codec(argv[2]);
        else if (argc == 5 && std::string(argv[1]) == "gate")
            gate_trace(speed_argument(argv[2]), static_cast<unsigned>(std::stoul(argv[3])),
                       std::string(argv[4]) == "1");
        else
            throw std::runtime_error("usage: guard-core-probe lease HEX | gate SPEED DURATION ACK");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
