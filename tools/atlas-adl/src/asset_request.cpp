#include "atlas/adl/asset_request.hpp"

#include <stdexcept>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

namespace atlas::adl {

namespace {

std::string require_non_empty_string(const YAML::Node& parent, const std::string& field_path) {
    if (!parent.IsDefined() || !parent.IsScalar()) {
        throw std::invalid_argument("asset request is missing required field '" + field_path + "'");
    }
    auto value = parent.as<std::string>();
    if (value.empty()) {
        throw std::invalid_argument("asset request has an empty '" + field_path + "' field");
    }
    return value;
}

void reject_unrecognized_fields(const YAML::Node& map_node,
                                const std::unordered_set<std::string>& allowed_fields,
                                const std::string& block_label) {
    for (const auto& field_entry : map_node) {
        const auto key = field_entry.first.as<std::string>();
        if (!allowed_fields.contains(key)) {
            throw std::invalid_argument("asset request's '" + block_label +
                                        "' block has unrecognized field '" + key + "'");
        }
    }
}

AssetRequestKind parse_kind(const YAML::Node& request_node) {
    const std::string kind = require_non_empty_string(request_node["kind"], "request.kind");
    if (kind == "creature") {
        return AssetRequestKind::Creature;
    }
    if (kind == "prop") {
        return AssetRequestKind::Prop;
    }
    if (kind == "weapon") {
        return AssetRequestKind::Weapon;
    }
    if (kind == "environment") {
        return AssetRequestKind::Environment;
    }
    if (kind == "vfx") {
        return AssetRequestKind::Vfx;
    }
    throw std::invalid_argument("asset request has an unrecognized 'request.kind' value '" + kind + "'");
}

VisualOverride parse_visual_override(const YAML::Node& override_node, std::size_t index) {
    if (!override_node.IsMap()) {
        throw std::invalid_argument("asset request's 'visual.overrides[" + std::to_string(index) +
                                    "]' must be a mapping");
    }

    static const std::unordered_set<std::string> allowed_fields{"field", "value", "rationale"};
    reject_unrecognized_fields(
        override_node, allowed_fields, "visual.overrides[" + std::to_string(index) + "]");

    VisualOverride override_entry;
    override_entry.field = require_non_empty_string(override_node["field"],
                                                    "visual.overrides[" + std::to_string(index) + "].field");
    override_entry.value = require_non_empty_string(override_node["value"],
                                                    "visual.overrides[" + std::to_string(index) + "].value");
    override_entry.rationale = require_non_empty_string(
        override_node["rationale"], "visual.overrides[" + std::to_string(index) + "].rationale");
    return override_entry;
}

VisualBlock parse_visual_block(const YAML::Node& root) {
    const YAML::Node visual_node = root["visual"];
    if (!visual_node.IsDefined() || !visual_node.IsMap()) {
        throw std::invalid_argument("asset request is missing a 'visual' mapping");
    }

    static const std::unordered_set<std::string> allowed_fields{"style_ref", "overrides"};
    reject_unrecognized_fields(visual_node, allowed_fields, "visual");

    VisualBlock visual;
    visual.style_ref = require_non_empty_string(visual_node["style_ref"], "visual.style_ref");

    const YAML::Node overrides_node = visual_node["overrides"];
    if (overrides_node.IsDefined()) {
        if (!overrides_node.IsSequence()) {
            throw std::invalid_argument("asset request's 'visual.overrides' must be a sequence");
        }
        std::size_t index = 0;
        for (const auto& override_node : overrides_node) {
            visual.overrides.push_back(parse_visual_override(override_node, index));
            ++index;
        }
    }

    return visual;
}

AnimationMode parse_animation_mode(const YAML::Node& entry_node, const std::string& entry_label) {
    const std::string mode = require_non_empty_string(entry_node["mode"], entry_label + ".mode");
    if (mode == "procedural_auto") {
        return AnimationMode::ProceduralAuto;
    }
    if (mode == "human_gated") {
        return AnimationMode::HumanGated;
    }
    throw std::invalid_argument("asset request's '" + entry_label + ".mode' has an unrecognized value '" +
                                mode + "'");
}

bool parse_required_bool(const YAML::Node& parent, const std::string& field_path) {
    const YAML::Node field_node = parent;
    if (!field_node.IsDefined() || !field_node.IsScalar()) {
        throw std::invalid_argument("asset request is missing required field '" + field_path + "'");
    }
    try {
        return field_node.as<bool>();
    } catch (const YAML::BadConversion&) {
        throw std::invalid_argument("asset request's '" + field_path + "' field is not a boolean");
    }
}

double parse_required_number(const YAML::Node& parent, const std::string& field_path) {
    if (!parent.IsDefined() || !parent.IsScalar()) {
        throw std::invalid_argument("asset request is missing required field '" + field_path + "'");
    }
    try {
        return parent.as<double>();
    } catch (const YAML::BadConversion&) {
        throw std::invalid_argument("asset request's '" + field_path + "' field is not a number");
    }
}

AnimationSetEntry parse_animation_set_entry(const YAML::Node& entry_node, std::size_t index) {
    if (!entry_node.IsMap()) {
        throw std::invalid_argument("asset request's 'rig.animation_set[" + std::to_string(index) +
                                    "]' must be a mapping");
    }

    const std::string entry_label = "rig.animation_set[" + std::to_string(index) + "]";

    static const std::unordered_set<std::string> allowed_fields{
        "name", "mode", "loop", "duration_seconds", "sync_to", "contact_frame_ratio"};
    reject_unrecognized_fields(entry_node, allowed_fields, entry_label);

    AnimationSetEntry entry;
    entry.name = require_non_empty_string(entry_node["name"], entry_label + ".name");
    entry.mode = parse_animation_mode(entry_node, entry_label);

    if (entry.mode == AnimationMode::ProceduralAuto) {
        entry.loop = parse_required_bool(entry_node["loop"], entry_label + ".loop");
        entry.duration_seconds =
            parse_required_number(entry_node["duration_seconds"], entry_label + ".duration_seconds");
    } else {
        entry.sync_to = require_non_empty_string(entry_node["sync_to"], entry_label + ".sync_to");
        const double contact_frame_ratio =
            parse_required_number(entry_node["contact_frame_ratio"], entry_label + ".contact_frame_ratio");
        if (contact_frame_ratio < 0.0 || contact_frame_ratio > 1.0) {
            throw std::invalid_argument("asset request's '" + entry_label +
                                        ".contact_frame_ratio' must be within [0, 1]");
        }
        entry.contact_frame_ratio = contact_frame_ratio;
    }

    return entry;
}

RigBlock parse_rig_block(const YAML::Node& root) {
    const YAML::Node rig_node = root["rig"];
    if (!rig_node.IsDefined() || !rig_node.IsMap()) {
        throw std::invalid_argument("asset request is missing a 'rig' mapping");
    }

    static const std::unordered_set<std::string> allowed_fields{"type", "skeleton_template", "animation_set"};
    reject_unrecognized_fields(rig_node, allowed_fields, "rig");

    RigBlock rig;
    rig.type = require_non_empty_string(rig_node["type"], "rig.type");
    rig.skeleton_template = require_non_empty_string(rig_node["skeleton_template"], "rig.skeleton_template");

    const YAML::Node animation_set_node = rig_node["animation_set"];
    if (!animation_set_node.IsDefined() || !animation_set_node.IsSequence()) {
        throw std::invalid_argument("asset request is missing a 'rig.animation_set' sequence");
    }

    std::size_t index = 0;
    for (const auto& entry_node : animation_set_node) {
        rig.animation_set.push_back(parse_animation_set_entry(entry_node, index));
        ++index;
    }

    return rig;
}

CompositionBlock parse_composition_block(const YAML::Node& root) {
    const YAML::Node composition_node = root["composition"];
    if (!composition_node.IsDefined() || !composition_node.IsMap()) {
        throw std::invalid_argument("asset request is missing a 'composition' mapping");
    }

    static const std::unordered_set<std::string> allowed_fields{
        "intended_role", "existing_capabilities", "requires_new_mechanism", "rationale"};
    reject_unrecognized_fields(composition_node, allowed_fields, "composition");

    CompositionBlock composition;
    composition.intended_role =
        require_non_empty_string(composition_node["intended_role"], "composition.intended_role");

    const YAML::Node existing_capabilities_node = composition_node["existing_capabilities"];
    if (existing_capabilities_node.IsDefined()) {
        if (!existing_capabilities_node.IsSequence()) {
            throw std::invalid_argument(
                "asset request's 'composition.existing_capabilities' must be a sequence");
        }
        for (const auto& capability_node : existing_capabilities_node) {
            if (!capability_node.IsScalar()) {
                throw std::invalid_argument(
                    "asset request's 'composition.existing_capabilities' entries must be strings");
            }
            composition.existing_capabilities.push_back(capability_node.as<std::string>());
        }
    }

    const YAML::Node requires_new_mechanism_node = composition_node["requires_new_mechanism"];
    if (requires_new_mechanism_node.IsDefined()) {
        composition.requires_new_mechanism =
            parse_required_bool(requires_new_mechanism_node, "composition.requires_new_mechanism");
    }

    const YAML::Node rationale_node = composition_node["rationale"];
    if (composition.requires_new_mechanism) {
        composition.rationale = require_non_empty_string(rationale_node, "composition.rationale");
    } else if (rationale_node.IsDefined()) {
        composition.rationale = require_non_empty_string(rationale_node, "composition.rationale");
    }

    return composition;
}

} // namespace

AssetRequest parse_asset_request(std::string_view yaml_text) {
    try {
        const YAML::Node root = YAML::Load(std::string(yaml_text));

        if (!root.IsMap()) {
            throw std::invalid_argument("asset request root must be a mapping");
        }

        const YAML::Node request_node = root["request"];
        if (!request_node.IsDefined() || !request_node.IsMap()) {
            throw std::invalid_argument("asset request is missing a 'request' mapping");
        }

        static const std::unordered_set<std::string> allowed_request_fields{"kind", "name"};
        reject_unrecognized_fields(request_node, allowed_request_fields, "request");

        AssetRequest asset_request;
        asset_request.kind = parse_kind(request_node);
        asset_request.name = require_non_empty_string(request_node["name"], "request.name");

        static const std::unordered_set<std::string> allowed_root_fields{
            "request", "visual", "rig", "composition"};
        reject_unrecognized_fields(root, allowed_root_fields, "<root>");

        asset_request.visual = parse_visual_block(root);
        asset_request.rig = parse_rig_block(root);
        asset_request.composition = parse_composition_block(root);

        return asset_request;
    } catch (const YAML::Exception& e) {
        throw std::invalid_argument(std::string("YAML error: ") + e.what());
    }
}

} // namespace atlas::adl
