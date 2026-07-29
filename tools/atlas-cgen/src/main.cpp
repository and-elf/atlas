#include "atlas/cgen/contract_writer.hpp"
#include "atlas/cgen/manifest.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

// CLI entry point - not unit tested here (spawning a process from a test
// binary is a separate, more involved piece of test infrastructure this
// round doesn't build; see tools/atlas-cgen/README.md). The three
// components it wires together (parse_manifest, generate_contract, and the
// template engine underneath) are unit tested in tests/atlas-cgen/, and
// tests/atlas-cgen/compile_check/ proves this tool's actual output
// compiles and satisfies atlas-contracts's concepts as a real build step.
int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: atlas-cgen <manifest.yaml> <output.hpp>\n";
        return 2;
    }

    const std::filesystem::path manifest_path = argv[1];
    const std::filesystem::path output_path = argv[2];

    std::ifstream input(manifest_path);
    if (!input) {
        std::cerr << "atlas-cgen: cannot open manifest file '" << manifest_path.string() << "'\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    try {
        const auto manifest = atlas::cgen::parse_manifest(buffer.str());
        const auto contract = atlas::cgen::generate_contract(
            manifest, output_path.filename().string(), manifest_path.filename().string());

        std::ofstream output(output_path);
        if (!output) {
            std::cerr << "atlas-cgen: cannot write output file '" << output_path.string() << "'\n";
            return 1;
        }
        output << contract;
    } catch (const std::invalid_argument& e) {
        std::cerr << "atlas-cgen: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
