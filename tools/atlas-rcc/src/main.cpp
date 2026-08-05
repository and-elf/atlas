#include "atlas/rcc/resource_blob.hpp"
#include "atlas/rcc/resource_manifest.hpp"
#include "atlas/rcc/resource_table.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

// CLI entry point - not unit tested here (spawning a process from a test
// binary is a separate, more involved piece of test infrastructure this
// round doesn't build; see tools/atlas-rcc/README.md). The library functions
// this file wires together (parse_resource_manifest, compile_resource_table,
// pack_resource_blob) are unit tested in tests/atlas-rcc/.
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

bool write_file(const std::filesystem::path& path, const std::vector<std::byte>& bytes) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - ostream::write needs a const char*.
    const auto* data = reinterpret_cast<const char*>(bytes.data());
    output.write(data, static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

void print_usage() {
    std::cerr << "usage: atlas-rcc <resource-manifest.yaml> <asset-root-dir> <output-dir>\n";
}

// Three same-typed paths as separate function parameters would be an
// easily-swapped-by-mistake hazard (bugprone-easily-swappable-parameters) -
// grouped into one named-field struct instead, so every call site names each
// path explicitly rather than relying on positional order.
struct RunPaths {
    std::filesystem::path manifest;
    std::filesystem::path asset_root;
    std::filesystem::path output_dir;
};

// Compiles a resource manifest, packs one blob per asset type (via
// pack_resource_blob), and writes each to <output-dir>/<type>.blob - the CLI
// wiring atlas-rcc's own README previously flagged as deliberately deferred.
// Per-type entries are built directly from `entries` (parse_resource_manifest's
// authored-order vector), not by iterating the compiled ResourceTable (an
// unordered_map) - preserves manifest order within each type's blob, so
// re-running this CLI against the same manifest/assets produces bit-identical
// blob files every time (spec §4: avoid unordered iteration anywhere it could
// affect output), even though which type's blob gets written/printed first
// still isn't deterministic - harmless, since it only affects console/write
// ordering, never a blob file's own contents.
int run(const RunPaths& paths) {
    const auto manifest_text = read_file(paths.manifest);
    if (!manifest_text) {
        std::cerr << "atlas-rcc: cannot open resource manifest file '" << paths.manifest.string() << "'\n";
        return 1;
    }

    try {
        const auto entries = atlas::rcc::parse_resource_manifest(*manifest_text);
        const auto table = atlas::rcc::compile_resource_table(entries);

        std::cout << "atlas-rcc: compiled " << table.size() << " resource(s) from '"
                  << paths.manifest.string() << "'\n";
        std::unordered_map<std::string, std::vector<atlas::rcc::CompiledResource>> entries_by_type;
        for (const auto& entry : entries) {
            const auto id = atlas::ResourceId::from_name(entry.name);
            std::cout << "  0x" << std::hex << std::setw(16) << std::setfill('0') << id.value << std::dec
                      << "  " << entry.type << "  " << entry.name << " -> " << entry.path << "\n";
            entries_by_type[entry.type].push_back(
                atlas::rcc::CompiledResource{id, entry.name, entry.type, entry.path, std::nullopt});
        }

        std::error_code error;
        std::filesystem::create_directories(paths.output_dir, error);
        if (error) {
            std::cerr << "atlas-rcc: cannot create output directory '" << paths.output_dir.string()
                      << "': " << error.message() << "\n";
            return 1;
        }

        for (const auto& [type, type_entries] : entries_by_type) {
            const auto blob = atlas::rcc::pack_resource_blob(type_entries, paths.asset_root);
            const auto blob_path = paths.output_dir / (type + ".blob");
            if (!write_file(blob_path, blob)) {
                std::cerr << "atlas-rcc: cannot write blob file '" << blob_path.string() << "'\n";
                return 1;
            }
            std::cout << "atlas-rcc: packed " << type_entries.size() << " " << type << " resource(s) into '"
                      << blob_path.string() << "' (" << blob.size() << " bytes)\n";
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "atlas-rcc: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        print_usage();
        return 2;
    }
    return run(RunPaths{.manifest = argv[1], .asset_root = argv[2], .output_dir = argv[3]});
}
