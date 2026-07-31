#include "atlas/rcc/resource_manifest.hpp"

#include <stdexcept>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

namespace atlas::rcc {

namespace {

// Reads a required scalar string field from a resource entry mapping,
// rejecting a missing key or an empty value with a message naming both the
// entry's authored name (when known) and the missing field, rather than a
// generic "malformed entry" error - mirrors atlas-cgen's parse_manifest
// preference for errors that name exactly what was wrong.
std::string require_non_empty_field(const YAML::Node& entry,
                                    const std::string& field_name,
                                    const std::string& entry_label) {
    const YAML::Node field_node = entry[field_name];
    if (!field_node.IsDefined() || !field_node.IsScalar()) {
        throw std::invalid_argument("resource entry '" + entry_label + "' is missing required field '" +
                                    field_name + "'");
    }

    auto value = field_node.as<std::string>();
    if (value.empty()) {
        throw std::invalid_argument("resource entry '" + entry_label + "' has an empty '" + field_name +
                                    "' field");
    }
    return value;
}

} // namespace

std::vector<ResourceEntry> parse_resource_manifest(std::string_view yaml_text) {
    try {
        const YAML::Node root = YAML::Load(std::string(yaml_text));

        if (!root.IsMap()) {
            throw std::invalid_argument("resource manifest root must be a mapping");
        }

        const YAML::Node resources_node = root["resources"];
        if (!resources_node.IsDefined()) {
            throw std::invalid_argument("resource manifest is missing a 'resources:' list");
        }
        if (!resources_node.IsSequence()) {
            throw std::invalid_argument("resource manifest's 'resources:' key must be a sequence");
        }

        std::vector<ResourceEntry> entries;
        std::unordered_set<std::string> seen_names;

        for (const auto& entry_node : resources_node) {
            if (!entry_node.IsMap()) {
                throw std::invalid_argument(
                    "resource manifest entries must each be a mapping of name/type/path");
            }

            // 'name' is read first (and used as the error label for the
            // other two required fields below) since it is the entry's own
            // identity - an error about "type" or "path" is far more
            // actionable when it can say which resource it belongs to.
            const std::string name = require_non_empty_field(entry_node, "name", "<unnamed>");

            static const std::unordered_set<std::string> allowed_fields{"name", "type", "path"};
            for (const auto& field_entry : entry_node) {
                const auto key = field_entry.first.as<std::string>();
                if (!allowed_fields.contains(key)) {
                    std::string message = "resource entry '";
                    message += name;
                    message += "' has unrecognized field '";
                    message += key;
                    message += "'";
                    throw std::invalid_argument(message);
                }
            }

            if (!seen_names.insert(name).second) {
                throw std::invalid_argument("duplicate resource name '" + name + "'");
            }

            ResourceEntry entry;
            entry.name = name;
            entry.type = require_non_empty_field(entry_node, "type", name);
            entry.path = require_non_empty_field(entry_node, "path", name);
            entries.push_back(std::move(entry));
        }

        return entries;
    } catch (const YAML::Exception& e) {
        throw std::invalid_argument(std::string("YAML error: ") + e.what());
    }
}

} // namespace atlas::rcc
