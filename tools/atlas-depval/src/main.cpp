#include "atlas/depval/validator.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// CLI entry point - not unit tested here, matching tools/atlas-cgen's and
// tools/atlas-rcc's own convention (see tools/atlas-depval/README.md):
// spawning a process from a test binary is separate test infrastructure
// this project doesn't build yet, and argv/file-I/O error paths are
// integration-level, not library logic. The library function this file
// wires together (validate_composition, format_report) is fully unit
// tested in tests/atlas-depval/.
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
    std::cerr << "usage: atlas-depval <host.yaml> <capability.yaml>...\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 2;
    }

    const std::filesystem::path host_path{argv[1]};
    const auto host_text = read_file(host_path);
    if (!host_text) {
        std::cerr << "atlas-depval: cannot open host manifest file '" << host_path.string() << "'\n";
        return 1;
    }

    // Capability file contents must outlive the string_view span passed to
    // validate_composition, so they're read into this vector first and
    // referenced from it - not read and discarded per-iteration.
    std::vector<std::string> capability_texts;
    capability_texts.reserve(static_cast<std::size_t>(argc - 2));
    for (int i = 2; i < argc; ++i) {
        const std::filesystem::path capability_path{argv[i]};
        const auto capability_text = read_file(capability_path);
        if (!capability_text) {
            std::cerr << "atlas-depval: cannot open capability manifest file '" << capability_path.string()
                      << "'\n";
            return 1;
        }
        capability_texts.push_back(*capability_text);
    }

    std::vector<std::string_view> capability_views;
    capability_views.reserve(capability_texts.size());
    for (const auto& text : capability_texts) {
        capability_views.emplace_back(text);
    }

    try {
        const auto report = atlas::depval::validate_composition(*host_text, capability_views);
        std::cout << atlas::depval::format_report(report);
    } catch (const std::invalid_argument& e) {
        std::cerr << "atlas-depval: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
