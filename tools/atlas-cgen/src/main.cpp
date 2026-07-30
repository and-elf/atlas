#include "atlas/cgen/contract_writer.hpp"
#include "atlas/cgen/host_composition.hpp"
#include "atlas/cgen/host_composition_writer.hpp"
#include "atlas/cgen/host_manifest.hpp"
#include "atlas/cgen/manifest.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

// CLI entry point - not unit tested here (spawning a process from a test
// binary is a separate, more involved piece of test infrastructure this
// round doesn't build; see tools/atlas-cgen/README.md). The library
// functions this file wires together (parse_manifest, parse_host_manifest,
// resolve_host_composition, generate_contract, generate_host_composition,
// and the template engine underneath) are unit tested in tests/atlas-cgen/,
// and tests/atlas-cgen/compile_check/ proves this tool's actual output
// compiles as a real build step.
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

// atlas-cgen <manifest.yaml> <output.hpp> - the original, single-capability
// mode: one manifest in, one generated contract header out.
int run_single_manifest_mode(const std::filesystem::path& manifest_path,
                             const std::filesystem::path& output_path) {
    const auto manifest_text = read_file(manifest_path);
    if (!manifest_text) {
        std::cerr << "atlas-cgen: cannot open manifest file '" << manifest_path.string() << "'\n";
        return 1;
    }

    try {
        const auto manifest = atlas::cgen::parse_manifest(*manifest_text);
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

// atlas-cgen --host <host.yaml> <output.hpp> <capability.yaml>... - host
// composition mode (spec §14): resolves host.yaml's `composes:` against
// every capability manifest listed, validates the dependency graph
// (cycle detection, spec §5), and emits the generated PropertyStore
// registration header (see host_composition_writer.hpp).
int run_host_composition_mode(const std::filesystem::path& host_path,
                              const std::filesystem::path& output_path,
                              const std::vector<std::filesystem::path>& capability_paths) {
    const auto host_text = read_file(host_path);
    if (!host_text) {
        std::cerr << "atlas-cgen: cannot open host manifest file '" << host_path.string() << "'\n";
        return 1;
    }

    std::vector<atlas::cgen::Manifest> capability_manifests;
    capability_manifests.reserve(capability_paths.size());
    for (const auto& capability_path : capability_paths) {
        const auto capability_text = read_file(capability_path);
        if (!capability_text) {
            std::cerr << "atlas-cgen: cannot open manifest file '" << capability_path.string() << "'\n";
            return 1;
        }
        try {
            capability_manifests.push_back(atlas::cgen::parse_manifest(*capability_text));
        } catch (const std::invalid_argument& e) {
            std::cerr << "atlas-cgen: " << e.what() << "\n";
            return 1;
        }
    }

    try {
        const auto host_manifest = atlas::cgen::parse_host_manifest(*host_text);
        const auto composition = atlas::cgen::resolve_host_composition(host_manifest, capability_manifests);
        const auto generated = atlas::cgen::generate_host_composition(
            composition, output_path.filename().string(), host_path.filename().string());

        std::ofstream output(output_path);
        if (!output) {
            std::cerr << "atlas-cgen: cannot write output file '" << output_path.string() << "'\n";
            return 1;
        }
        output << generated;
    } catch (const std::invalid_argument& e) {
        std::cerr << "atlas-cgen: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

void print_usage() {
    std::cerr << "usage: atlas-cgen <manifest.yaml> <output.hpp>\n"
                 "       atlas-cgen --host <host.yaml> <output.hpp> <capability.yaml>...\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string_view(argv[1]) == "--host") {
        if (argc < 5) {
            print_usage();
            return 2;
        }
        std::vector<std::filesystem::path> capability_paths;
        capability_paths.reserve(static_cast<std::size_t>(argc - 4));
        for (int i = 4; i < argc; ++i) {
            capability_paths.emplace_back(argv[i]);
        }
        return run_host_composition_mode(argv[2], argv[3], capability_paths);
    }

    if (argc != 3) {
        print_usage();
        return 2;
    }
    return run_single_manifest_mode(argv[1], argv[2]);
}
