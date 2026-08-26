#include "atlas/adl/asset_request.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// CLI entry point - not unit tested here, matching every other atlas-*
// tool's main.cpp (see tools/atlas-rcc/README.md for the rationale: argv/
// file-I/O error paths need subprocess-spawning test infrastructure this
// project doesn't build yet). parse_asset_request itself is fully unit
// tested in tests/atlas-adl/.
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: atlas-adl <asset-request.yaml>\n";
        return 1;
    }

    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "atlas-adl: could not open '" << argv[1] << "'\n";
        return 1;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();

    try {
        const atlas::adl::AssetRequest request = atlas::adl::parse_asset_request(buffer.str());
        std::cout << "valid asset request '" << request.name << "'\n";
        return 0;
    } catch (const std::invalid_argument& e) {
        std::cerr << "atlas-adl: " << e.what() << '\n';
        return 1;
    }
}
