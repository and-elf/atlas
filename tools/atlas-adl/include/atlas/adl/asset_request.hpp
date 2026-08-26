#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atlas::adl {

// Closed vocabulary for request.kind - see README's Scope for why this is a
// fixed enum, unlike e.g. atlas-rcc's open-ended resource `type` string.
enum class AssetRequestKind { Creature, Prop, Weapon, Environment, Vfx };

// A single, justified deviation from the referenced style guide, e.g.
//   - field: fur_color
//     value: "#4a3c2f"
//     rationale: "unique color for boss variant"
// All three fields are required - an override with no rationale is exactly
// the unreviewable freeform prose this ADL exists to replace (see the
// issue's Motivation).
struct VisualOverride {
    std::string field;
    std::string value;
    std::string rationale;
};

// visual: style_ref + optional, justified overrides.
struct VisualBlock {
    std::string style_ref;
    std::vector<VisualOverride> overrides;
};

// Closed vocabulary for an animation_set entry's `mode` - see README.
enum class AnimationMode { ProceduralAuto, HumanGated };

// One entry of rig.animation_set. `mode` gates which of the remaining
// fields are required:
//   - procedural_auto: `loop` + `duration_seconds`
//   - human_gated:      `sync_to` + `contact_frame_ratio`
// Fields belonging to the other mode are left unset (std::nullopt), never
// populated with a meaningless default - this struct is the parsed,
// already-mode-checked result, not a raw carry-through of whatever was
// authored.
struct AnimationSetEntry {
    std::string name;
    AnimationMode mode = AnimationMode::ProceduralAuto;
    std::optional<bool> loop;
    std::optional<double> duration_seconds;
    std::optional<std::string> sync_to;
    std::optional<double> contact_frame_ratio;
};

// rig: type (open-ended, like atlas-rcc's resource `type`) + skeleton_template
// + the animation_set list.
struct RigBlock {
    std::string type;
    std::string skeleton_template;
    std::vector<AnimationSetEntry> animation_set;
};

// composition: intended_role/existing_capabilities are always carried
// through; `rationale` is required iff requires_new_mechanism is true (left
// empty otherwise - never populated with a placeholder).
struct CompositionBlock {
    std::string intended_role;
    std::vector<std::string> existing_capabilities;
    bool requires_new_mechanism = false;
    std::string rationale;
};

// A fully parsed, already-validated asset/animation request - the
// reviewable intermediate artifact the issue's Motivation describes in
// place of freeform prose.
struct AssetRequest {
    AssetRequestKind kind = AssetRequestKind::Creature;
    std::string name;
    VisualBlock visual;
    RigBlock rig;
    CompositionBlock composition;
};

// Parses and validates asset/animation request YAML text (see README for
// the full schema and rejection list), returning a fully populated
// AssetRequest. Throws std::invalid_argument, with a message naming exactly
// what was wrong, for anything it can't confidently parse - mirroring
// atlas-rcc's parse_resource_manifest (same std::expected-unavailability
// rationale documented there applies here too).
[[nodiscard]] AssetRequest parse_asset_request(std::string_view yaml_text);

} // namespace atlas::adl
