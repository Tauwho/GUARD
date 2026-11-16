#include "guard/buffer_json.hpp"
#include "guard/codec.hpp"
#include "guard/reason_codes.hpp"
#include "guard/route.hpp"
#include "guard/scenario.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
std::string read(const char *path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 0 || input.tellg() > 1'048'576)
        throw std::runtime_error("invalid demo input file");
    input.seekg(0);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 4)
            throw std::runtime_error(
                "usage: guard-route-demo MAP_JSON SCENARIO_JSON SINGLE_RECORD_JSON");
        guard::MapSpec map;
        guard::ScenarioSpec scenario;
        if (!guard::load_map(read(argv[1]), map) ||
            !guard::load_scenario(read(argv[2]), map, scenario))
            throw std::runtime_error("invalid map/scenario");
        const auto recording = read(argv[3]);
        guard::BufferJson json(recording);
        if (!json.valid())
            throw std::runtime_error("invalid recorded JSON");
        const auto origin = json.text(json.member(0, "source"));
        if (origin != "ollama-recording" && origin != "synthetic-test")
            throw std::runtime_error("record source missing");
        const auto request = json.member(0, "request");
        const auto observed = json.member(request, "obs");
        guard::Observation source;
        std::uint64_t stamp = 0, version = 0;
        if (!json.unsigned_integer(json.member(observed, "obs_id"), source.obs_id) ||
            !json.unsigned_integer(json.member(observed, "stamp_ns"), stamp) ||
            stamp > std::uint64_t(std::numeric_limits<std::int64_t>::max()) ||
            !json.unsigned_integer(json.member(observed, "map_version"), version) ||
            version > 0xffffffffU || !json.number(json.member(observed, "x"), source.x) ||
            !json.number(json.member(observed, "y"), source.y) ||
            !json.number(json.member(observed, "theta"), source.theta))
            throw std::runtime_error("invalid retained source");
        source.seq = source.obs_id;
        source.stamp_ns = static_cast<std::int64_t>(stamp);
        source.map_version = static_cast<std::uint32_t>(version);
        source.sensor_ok = 1;
        if (!guard::valid(source) || source.obs_id == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("invalid source payload");
        guard::RouteProposal route;
        if (!guard::load_route(json.text(json.member(0, "response")), route))
            throw std::runtime_error("invalid C9 route");
        guard::World world(map, scenario);
        if (!world.update(source.stamp_ns) || world.scene().map_version != source.map_version)
            throw std::runtime_error("source map does not match the scenario");
        if (!world.update(route.created_ns))
            throw std::runtime_error("response time precedes source time");
        auto current = source;
        ++current.obs_id;
        ++current.seq;
        current.stamp_ns = route.created_ns;
        current.map_version = world.scene().map_version;
        const auto reason =
            guard::check_route(route, source, current, world.scene(), {}, route.created_ns);
        std::cout << "{\"source\":\"" << origin
                  << "\",\"source_map_version\":" << source.map_version
                  << ",\"current_map_version\":" << current.map_version
                  << ",\"source_age_ns\":" << route.created_ns - source.stamp_ns
                  << ",\"usable\":" << (reason == 0 ? "true" : "false") << ",\"reason\":\""
                  << (reason == 0 ? "ROUTE_USABLE" : guard::reason_name(reason)) << "\"}\n";
        return reason == 0 ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "guard-route-demo: " << error.what() << '\n';
        return 2;
    }
}
