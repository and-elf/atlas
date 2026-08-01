#include "atlas/refl/manifest.hpp"
#include "atlas/refl/reflection_writer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

// CLI entry point - not unit tested here (spawning a process from a test
// binary is a separate, more involved piece of test infrastructure this
// round doesn't build; see tools/atlas-cgen/src/main.cpp's own header
// comment, and this tool's README). The library functions this file wires
// together (parse_manifest, generate_reflection_metadata, and the template
// engine underneath) are unit tested in tests/atlas-refl/, and
// tests/atlas-refl/compile_check/ proves this tool's actual output compiles
// and is genuinely consumable alongside atlas-reflection's runtime
// primitives.
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
    std::cerr << "usage: atlas-refl <manifest.yaml> <output.hpp>\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        print_usage();
        return 2;
    }

    const std::filesystem::path manifest_path(argv[1]);
    const std::filesystem::path output_path(argv[2]);

    const auto manifest_text = read_file(manifest_path);
    if (!manifest_text) {
        std::cerr << "atlas-refl: cannot open manifest file '" << manifest_path.string() << "'\n";
        return 1;
    }

    try {
        const auto manifest = atlas::refl::parse_manifest(*manifest_text);
        const auto metadata = atlas::refl::generate_reflection_metadata(
            manifest, output_path.filename().string(), manifest_path.filename().string());

        std::ofstream output(output_path);
        if (!output) {
            std::cerr << "atlas-refl: cannot write output file '" << output_path.string() << "'\n";
            return 1;
        }
        output << metadata;
    } catch (const std::invalid_argument& e) {
        std::cerr << "atlas-refl: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
