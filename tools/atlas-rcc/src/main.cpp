#include "atlas/rcc/resource_manifest.hpp"
#include "atlas/rcc/resource_table.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

// CLI entry point - not unit tested here (spawning a process from a test
// binary is a separate, more involved piece of test infrastructure this
// round doesn't build; see tools/atlas-rcc/README.md). The library
// functions this file wires together (parse_resource_manifest,
// compile_resource_table) are unit tested in tests/atlas-rcc/.
namespace {

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void print_usage() {
    std::cerr << "usage: atlas-rcc <resource-manifest.yaml>\n";
}

// Compiles a resource manifest and reports the result on stdout. This round
// deliberately stops at the in-memory ResourceTable (see README) rather than
// emitting a generated C++ header - the printed summary is this CLI's way of
// proving the compiled table for real, the same role atlas-cgen's own
// generated-file write serves for contract generation.
int run(const std::filesystem::path& manifest_path) {
    const auto manifest_text = read_file(manifest_path);
    if (!manifest_text) {
        std::cerr << "atlas-rcc: cannot open resource manifest file '" << manifest_path.string() << "'\n";
        return 1;
    }

    try {
        const auto entries = atlas::rcc::parse_resource_manifest(*manifest_text);
        const auto table = atlas::rcc::compile_resource_table(entries);

        std::cout << "atlas-rcc: compiled " << table.size() << " resource(s) from '" << manifest_path.string()
                  << "'\n";
        for (const auto& entry : entries) {
            const auto id = atlas::ResourceId::from_name(entry.name);
            std::cout << "  0x" << std::hex << std::setw(16) << std::setfill('0') << id.value << std::dec
                      << "  " << entry.type << "  " << entry.name << " -> " << entry.path << "\n";
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "atlas-rcc: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        print_usage();
        return 2;
    }
    return run(argv[1]);
}
