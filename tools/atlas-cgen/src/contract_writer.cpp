#include "atlas/cgen/contract_writer.hpp"

#include "atlas/cgen/template_engine.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <span>
#include <stdexcept>

#include "contract_file_template.hpp"
#include "struct_decl_template.hpp"

namespace atlas::cgen {

namespace {

std::string render_fields(const StructDecl& decl) {
    std::string fields;
    for (std::size_t i = 0; i < decl.fields.size(); ++i) {
        const Field& field = decl.fields[i];
        const auto mapped = map_field_type(field.type);
        if (!mapped) {
            throw std::invalid_argument("field '" + decl.name + "." + field.name +
                                        "' has unrecognized type '" + field.type + "'");
        }
        if (i != 0) {
            fields += "\n";
        }
        fields += "    " + *mapped + " " + field.name + ";";
    }
    return fields;
}

// The composition member line (plus a blank separator line before the
// ordinary fields), or empty for a non-composed struct - matching §20's own
// generated-contract example, where a composed property's `composition`
// member appears first, followed by a blank line, then its base field.
std::string render_composition_member(const StructDecl& decl) {
    if (!decl.composition) {
        return "";
    }
    const auto mapped = map_composition_strategy(*decl.composition);
    if (!mapped) {
        throw std::invalid_argument("property '" + decl.name + "' has unrecognized composition strategy '" +
                                    *decl.composition + "'");
    }
    return "    static constexpr auto composition = " + *mapped + ";\n\n";
}

// The extra static_assert(atlas::Composable<...>) line for a composed
// struct, or empty otherwise - emitted alongside (never instead of) the
// ordinary PropertyContract/RequestContract/EventContract assertion, again
// matching §20's generated-contract example showing both asserts together.
std::string render_composable_assert(const StructDecl& decl) {
    if (!decl.composition) {
        return "";
    }
    return "\nstatic_assert(atlas::Composable<" + decl.name + ">);";
}

// The is_triggered member line (plus a blank separator line before the
// ordinary fields), or empty for a non-triggered struct - mirroring
// render_composition_member's shape for the analogous §20 concept.
std::string render_trigger_member(const StructDecl& decl) {
    if (!decl.trigger) {
        return "";
    }
    return "    static constexpr bool is_triggered = true;\n\n";
}

// The extra static_assert(atlas::Triggered<...>) line for a triggered
// struct, or empty otherwise - mirroring render_composable_assert.
std::string render_triggered_assert(const StructDecl& decl) {
    if (!decl.trigger) {
        return "";
    }
    return "\nstatic_assert(atlas::Triggered<" + decl.name + ">);";
}

std::string render_struct(const StructDecl& decl, std::string_view concept_name) {
    return render_template(templates::struct_decl,
                           {
                               {"STRUCT_NAME", decl.name},
                               {"COMPOSITION_MEMBER", render_composition_member(decl)},
                               {"TRIGGER_MEMBER", render_trigger_member(decl)},
                               {"FIELDS", render_fields(decl)},
                               {"CONCEPT_NAME", std::string(concept_name)},
                               {"COMPOSABLE_ASSERT", render_composable_assert(decl)},
                               {"TRIGGERED_ASSERT", render_triggered_assert(decl)},
                           });
}

// Collects the sorted, deduplicated set of #include lines a generated
// contract needs for the vocabulary types (EntityRef, ResourceId, ...) its
// fields actually use, across all three manifest blocks. std::set (rather
// than unordered_set) keeps output deterministic regardless of manifest
// field order - required for bit-reproducible generation (spec §4).
std::string collect_required_includes(const Manifest& manifest) {
    std::set<std::string> includes;
    auto collect_from_block = [&includes](const std::vector<StructDecl>& decls) {
        for (const auto& decl : decls) {
            for (const auto& field : decl.fields) {
                if (const auto include = required_include_for_type(field.type)) {
                    includes.insert(*include);
                }
            }
        }
    };
    collect_from_block(manifest.properties);
    collect_from_block(manifest.requests);
    collect_from_block(manifest.events);

    std::string result;
    for (const auto& include : includes) {
        result += "#include \"" + include + "\"\n";
    }
    return result;
}

// A view, not a reference member (cppcoreguidelines-avoid-const-or-ref-data-members) -
// Block instances are short-lived locals in generate_contract below.
struct Block {
    std::span<const StructDecl> decls;
    std::string_view concept_name;
};

} // namespace

std::string
generate_contract(const Manifest& manifest, std::string_view header_name, std::string_view source_name) {
    // Emitted in this fixed order (properties, then requests, then events)
    // regardless of how the manifest's own top-level keys were ordered in
    // YAML - matching §21's worked example, and keeping output reproducible
    // rather than dependent on YAML key iteration order.
    const std::array<Block, 3> blocks{{
        {manifest.properties, "PropertyContract"},
        {manifest.requests, "RequestContract"},
        {manifest.events, "EventContract"},
    }};

    std::string body;
    bool first = true;
    for (const auto& block : blocks) {
        for (const auto& decl : block.decls) {
            if (!first) {
                body += "\n";
            }
            body += render_struct(decl, block.concept_name);
            first = false;
        }
    }

    return render_template(templates::contract_file,
                           {
                               {"HEADER_NAME", std::string(header_name)},
                               {"SOURCE_NAME", std::string(source_name)},
                               {"CAPABILITY_NAME", manifest.capability_name},
                               {"EXTRA_INCLUDES", collect_required_includes(manifest)},
                               {"BODY", body},
                           });
}

} // namespace atlas::cgen
