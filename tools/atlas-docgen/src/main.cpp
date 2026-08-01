#include "atlas/cgen/manifest.hpp"
#include "atlas/docgen/markdown_writer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

// CLI entry point - not unit tested here, matching tools/atlas-cgen/src/main.cpp's
// own convention (see tools/atlas-docgen/README.md): argv/file-I/O error
// paths need subprocess-spawning test infrastructure this project doesn't
// build yet. The library logic this file wires together (parse_manifest,
// generate_markdown_doc) is fully unit-tested in tests/atlas-docgen/.
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
    std::cerr << "usage: atlas-docgen <output-dir> <manifest.yaml>...\n";
}

// One capability manifest in, one Markdown page out (named after the
// capability, e.g. health.md) - mirroring atlas-cgen's own "one manifest in,
// one generated artifact out" single-capability mode. Called once per
// manifest path passed on the command line, so a single invocation can
// document an arbitrary number of capabilities into the same output
// directory.
// A manifest *file* path and an output *directory* path are never actually
// confusable at a call site: swapping them fails loudly and immediately
// (read_file opens a directory and fails, or create_directories/ofstream is
// handed a YAML file path) rather than silently misbehaving, unlike the
// genuine same-role swap risk this check exists to catch.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int generate_one(const std::filesystem::path& manifest_path, const std::filesystem::path& output_dir_path) {
    const auto manifest_text = read_file(manifest_path);
    if (!manifest_text) {
        std::cerr << "atlas-docgen: cannot open manifest file '" << manifest_path.string() << "'\n";
        return 1;
    }

    try {
        const auto manifest = atlas::cgen::parse_manifest(*manifest_text);
        const auto output_path = output_dir_path / (manifest.capability_name + ".md");
        const auto doc = atlas::docgen::generate_markdown_doc(
            manifest, output_path.filename().string(), manifest_path.filename().string());

        std::ofstream output(output_path);
        if (!output) {
            std::cerr << "atlas-docgen: cannot write output file '" << output_path.string() << "'\n";
            return 1;
        }
        output << doc;
    } catch (const std::invalid_argument& e) {
        std::cerr << "atlas-docgen: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 2;
    }

    const std::filesystem::path output_dir(argv[1]);
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);

    for (int i = 2; i < argc; ++i) {
        if (const auto result = generate_one(argv[i], output_dir); result != 0) {
            return result;
        }
    }

    return 0;
}
