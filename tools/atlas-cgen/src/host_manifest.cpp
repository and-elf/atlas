#include "atlas/cgen/host_manifest.hpp"

#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace atlas::cgen {

HostManifest parse_host_manifest(std::string_view yaml_text) {
    try {
        const YAML::Node root = YAML::Load(std::string(yaml_text));

        if (!root.IsMap()) {
            throw std::invalid_argument("host manifest root must be a mapping");
        }

        const YAML::Node host_node = root["host"];
        if (!host_node.IsDefined() || !host_node.IsScalar()) {
            throw std::invalid_argument("host manifest is missing a scalar 'host:' key");
        }

        HostManifest manifest;
        manifest.host_name = host_node.as<std::string>();

        const YAML::Node composes_node = root["composes"];
        if (composes_node.IsDefined() && !composes_node.IsNull()) {
            if (!composes_node.IsSequence()) {
                throw std::invalid_argument("host manifest's 'composes:' key must be a sequence");
            }
            for (const auto& capability : composes_node) {
                manifest.composes.push_back(capability.as<std::string>());
            }
        }

        return manifest;
    } catch (const YAML::Exception& e) {
        throw std::invalid_argument(std::string("YAML error: ") + e.what());
    }
}

} // namespace atlas::cgen
