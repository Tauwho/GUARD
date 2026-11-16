#include "guard/codec.hpp"
#include <doctest/doctest.h>
#include <limits>

namespace {
guard::Proposal proposal() {
    guard::Proposal value;
    value.proposal_id = 1;
    value.planner_id = 1;
    value.seq = 1;
    value.based_on = {1, 1, 100, 1, 0};
    value.created_ns = 100;
    return value;
}
} // namespace

TEST_CASE("all truncated proposal lengths reject without changing output") {
    const auto source = proposal();
    std::array<std::uint8_t, sizeof(source)> bytes{};
    REQUIRE(guard::encode(source, bytes));
    auto output = source;
    output.proposal_id = 999;
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        CHECK(guard::decode(bytes.data(), size, output) != guard::DecodeError::none);
        CHECK(output.proposal_id == 999);
    }
    CHECK(guard::decode<guard::Proposal>(nullptr, bytes.size(), output) ==
          guard::DecodeError::bad_message);
}

TEST_CASE("numeric validation rejects nonfinite and out of bound values") {
    auto value = proposal();
    std::array<std::uint8_t, sizeof(value)> bytes{};
    for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(), 1.50001, -1.50001}) {
        value.v_cmd = invalid;
        CHECK_FALSE(guard::encode(value, bytes));
    }
    value.v_cmd = -1.5;
    CHECK(guard::encode(value, bytes));
}

TEST_CASE("sender zeroes reserved bytes and receiver ignores them") {
    auto source = proposal();
    source.based_on.reserved0 = 1234;
    std::array<std::uint8_t, sizeof(source)> bytes{};
    REQUIRE(guard::encode(source, bytes));
    CHECK(bytes[60] == 0);
    bytes[60] = 255;
    guard::Proposal output;
    CHECK(guard::decode(bytes.data(), bytes.size(), output) == guard::DecodeError::none);
}

TEST_CASE("lease duration handles integer boundaries without overflow") {
    guard::Lease value;
    value.lease_id = value.lease_seq = value.proposal_id = value.epoch = 1;
    value.issued_ns = std::numeric_limits<std::int64_t>::max() - 50'000'000;
    value.expires_ns = std::numeric_limits<std::int64_t>::max();
    CHECK(guard::valid(value));
    value.issued_ns = -1;
    CHECK_FALSE(guard::valid(value));
}
