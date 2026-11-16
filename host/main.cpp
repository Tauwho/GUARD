#include "replay.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>
#include <locale>
#include <map>
#include <stdexcept>
#include <string>

namespace {
std::string read(const std::string &path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() > 1'048'576 || input.tellg() < 0)
        throw std::runtime_error("invalid input file: " + path);
    input.seekg(0);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::map<std::string, std::string> args;
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 >= argc || !args.emplace(argv[i], argv[i + 1]).second)
                throw std::runtime_error("expected unique option/value pairs");
        }
        if (args.size() != 5 || !args.count("--map") || !args.count("--scenario") ||
            !args.count("--config") || !args.count("--seed") || !args.count("--out"))
            throw std::runtime_error(
                "required: --map --scenario --config --seed --out (normalized JSON input)");
        std::uint64_t seed = 0;
        const auto &text = args.at("--seed");
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), seed);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            throw std::runtime_error("invalid seed");
        guard::Config config;
        guard::MapSpec map;
        guard::ScenarioSpec scenario;
        if (!guard::load_config(read(args.at("--config")), config) ||
            !guard::load_map(read(args.at("--map")), map) ||
            !guard::load_scenario(read(args.at("--scenario")), map, scenario))
            throw std::runtime_error("invalid normalized input");
        std::ofstream output(args.at("--out"), std::ios::binary);
        output.imbue(std::locale::classic());
        if (!output)
            throw std::runtime_error("cannot open output");
        guard::host::replay(map, scenario, config, seed, output);
        output.close();
        if (!output)
            throw std::runtime_error("cannot flush output");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "guard-replay-core: " << error.what() << '\n';
        return 2;
    }
}
