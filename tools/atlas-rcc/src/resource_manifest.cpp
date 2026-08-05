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

// Reads an optional `loop` boolean out of an already-validated `animation:`
// mapping, defaulting to false (AnimationMetadata::loop's own default) when
// absent - loop is one of the two animation fields with a sensible default,
// unlike `skeleton` which has no useful default and is required.
bool parse_optional_loop_field(const YAML::Node& animation_node, const std::string& entry_label) {
    const YAML::Node loop_node = animation_node["loop"];
    if (!loop_node.IsDefined()) {
        return false;
    }
    if (!loop_node.IsScalar()) {
        throw std::invalid_argument("resource entry '" + entry_label +
                                    "' has an 'animation.loop' field that is not a boolean");
    }
    try {
        return loop_node.as<bool>();
    } catch (const YAML::BadConversion&) {
        // yaml-cpp throws BadConversion for a scalar it can't parse as bool
        // (e.g. the string "sideways") - re-thrown as the same
        // std::invalid_argument a non-scalar value gets above, since both
        // are the same "not actually a boolean" failure from the caller's
        // point of view.
        throw std::invalid_argument("resource entry '" + entry_label +
                                    "' has an 'animation.loop' field that is not a boolean");
    }
}

// Reads an optional `playback_rate` number out of an already-validated
// `animation:` mapping, defaulting to 1.0 (AnimationMetadata::playback_rate's
// own default) when absent. A non-positive rate is rejected outright - it
// has no sensible interpretation (issue #45: "a non-positive playback rate
// is nonsensical"), so there is no clamping or silent coercion here.
double parse_optional_playback_rate_field(const YAML::Node& animation_node, const std::string& entry_label) {
    const YAML::Node rate_node = animation_node["playback_rate"];
    if (!rate_node.IsDefined()) {
        return 1.0;
    }
    if (!rate_node.IsScalar()) {
        throw std::invalid_argument("resource entry '" + entry_label +
                                    "' has an 'animation.playback_rate' field that is not a number");
    }

    double value = 0.0;
    try {
        value = rate_node.as<double>();
    } catch (const YAML::BadConversion&) {
        // Same re-throw rationale as parse_optional_loop_field above.
        throw std::invalid_argument("resource entry '" + entry_label +
                                    "' has an 'animation.playback_rate' field that is not a number");
    }
    if (value <= 0.0) {
        throw std::invalid_argument("resource entry '" + entry_label +
                                    "' has a non-positive 'animation.playback_rate' field");
    }
    return value;
}

// Parses an entry's `animation:` block once its presence has already been
// established by the caller - deliberately unconditional on `type` (see
// AnimationMetadata's own comment for why: this tool has no closed
// vocabulary to gate that on).
AnimationMetadata parse_animation_metadata(const YAML::Node& animation_node, const std::string& entry_label) {
    if (!animation_node.IsMap()) {
        throw std::invalid_argument("resource entry '" + entry_label +
                                    "' has an 'animation' field that is not a mapping");
    }

    static const std::unordered_set<std::string> allowed_animation_fields{
        "skeleton", "loop", "playback_rate"};
    for (const auto& field_entry : animation_node) {
        const auto key = field_entry.first.as<std::string>();
        if (!allowed_animation_fields.contains(key)) {
            std::string message = "resource entry '";
            message += entry_label;
            message += "' has unrecognized field 'animation.";
            message += key;
            message += "'";
            throw std::invalid_argument(message);
        }
    }

    AnimationMetadata animation;
    animation.skeleton = require_non_empty_field(animation_node, "skeleton", entry_label);
    animation.loop = parse_optional_loop_field(animation_node, entry_label);
    animation.playback_rate = parse_optional_playback_rate_field(animation_node, entry_label);
    return animation;
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

            static const std::unordered_set<std::string> allowed_fields{"name", "type", "path", "animation"};
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

            const YAML::Node animation_node = entry_node["animation"];
            if (animation_node.IsDefined()) {
                entry.animation = parse_animation_metadata(animation_node, name);
            }

            entries.push_back(std::move(entry));
        }

        return entries;
    } catch (const YAML::Exception& e) {
        throw std::invalid_argument(std::string("YAML error: ") + e.what());
    }
}

} // namespace atlas::rcc
