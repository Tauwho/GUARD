#include "guard/buffer_json.hpp"
#include "guard/config.hpp"
#include <clocale>
#include <doctest/doctest.h>
#include <string>

TEST_CASE("configuration decimal parsing is independent of the process numeric locale") {
    const std::string previous = std::setlocale(LC_NUMERIC, nullptr);
    struct Restore {
        std::string value;
        ~Restore() { std::setlocale(LC_NUMERIC, value.c_str()); }
    } restore{previous};
    for (const auto *locale : {"de_DE.UTF-8", "German_Germany.1252", "French_France.1252"})
        if (std::setlocale(LC_NUMERIC, locale))
            break;
    guard::Config config;
    REQUIRE(guard::load_config(R"({"version":1,"mode":"B2","params":{"margin_m":0.25}})", config));
    CHECK(config.margin_m == 0.25);
}

TEST_CASE("bounded JSON parser rejects malformed or duplicate objects") {
    for (auto invalid :
         {"", "{", "[1,]", "{\"a\":1,\"a\":2}", "01", "1.", "1e", "{\"a\":truex}", "\"bad\\q\""})
        CHECK_FALSE(guard::BufferJson(invalid).valid());
    CHECK(guard::BufferJson("{\"a\":[true,false,null, -1.25e2]}").valid());
}

TEST_CASE("configuration buffer loader commits only validated settings") {
    guard::Config config;
    REQUIRE(guard::load_config(
        R"({"version":1,"mode":"B2","params":{"v_max":0.9,"t_lease_ms":100}})", config));
    CHECK(config.v_max == 0.9);
    CHECK(config.t_lease_ms == 100);
    for (auto invalid : {R"({"version":1,"mode":"B2","params":{"t_tick_ms":0}})",
                         R"({"version":1,"mode":"B0","ablate":{}})",
                         R"({"version":1,"mode":"B2","params":{"t_tick_ms":4294967296}})",
                         R"({"version":1,"mode":"B2","params":{"v_max":1e999}})",
                         R"({"version":1,"mode":"B2","params":{"surprise":5}})"}) {
        CHECK_FALSE(guard::load_config(invalid, config));
        CHECK(config.v_max == 0.9);
    }
}
