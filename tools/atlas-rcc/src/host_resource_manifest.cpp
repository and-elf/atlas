#include "atlas/rcc/host_resource_manifest.hpp"

#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace atlas::rcc {

HostResourceManifest parse_host_resource_manifest(std::string_view yaml_text) {
    try {
        const YAML::Node root = YAML::Load(std::string(yaml_text));

        if (!root.IsMap()) {
            throw std::invalid_argument("host resource manifest root must be a mapping");
        }

        const YAML::Node host_node = root["host"];
        if (!host_node.IsDefined() || !host_node.IsScalar()) {
            throw std::invalid_argument("host resource manifest is missing a scalar 'host:' key");
        }

        return HostResourceManifest{.host_name = host_node.as<std::string>()};
    } catch (const YAML::Exception& e) {
        throw std::invalid_argument(std::string("YAML error: ") + e.what());
    }
}

} // namespace atlas::rcc
