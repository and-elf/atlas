#include "atlas/refl/reflection_writer.hpp"

#include "atlas/refl/template_engine.hpp"

#include <array>
#include <span>
#include <stdexcept>

namespace atlas::refl {

namespace {

// Deliberately inline string_view constants, not files embedded at build
// time the way tools/atlas-cgen/templates/*.tmpl are (via
// cmake/scripts/EmbedTextFile.cmake): that script hard-codes the
// `atlas::cgen::templates` namespace it writes into, and this round's scope
// is kept strictly inside tools/atlas-refl/ - reusing it verbatim would
// place these templates' generated constants under the wrong tool's
// namespace. render_template's own generic {{PLACEHOLDER}} substitution
// engine (template_engine.hpp) is still exactly atlas-cgen's, so the
// "template text plus C++ code assembling repeated/conditional content"
// split (this project's Declarative Boundary principle, CLAUDE.md / spec
// §14) is structurally identical either way - only the delivery mechanism
// for the two small template strings below differs.
constexpr std::string_view reflection_file_template = R"TEMPLATE(// GENERATED — {{HEADER_NAME}}
// Source: {{SOURCE_NAME}} — do not hand-edit.

#pragma once

#include "atlas/refl/field_metadata.hpp"

#include <array>
#include <string_view>

namespace atlas::refl::{{CAPABILITY_NAME}} {

{{BODY}}
}  // namespace atlas::refl::{{CAPABILITY_NAME}}
)TEMPLATE";

// The extra space between the two opening/closing braces below (`{ {` /
// `} }`, not `{{`/`}}`) is deliberate on two counts: it avoids colliding
// with this file's own "{{PLACEHOLDER}}" token syntax, and - the actual
// reason it's needed at all - std::array<T, N> is an aggregate wrapping a
// single raw-array member, so initializing it from N already-braced
// sub-initializers (one per FieldMetadata element, e.g. `{"current", ...}`)
// needs one extra brace level for that member, exactly like the
// well-known `std::array<std::pair<int,int>, 2> p{ {1, 2}, {3, 4} };`
// idiom - omitting it left brace elision consuming only the first
// element's braced-init-list for the whole (one-member) top-level
// aggregate, which GCC correctly rejected as "too many initializers".
constexpr std::string_view struct_metadata_template =
    R"TEMPLATE(inline constexpr std::array<atlas::refl::FieldMetadata, {{FIELD_COUNT}}> k{{STRUCT_NAME}}Fields{ {
{{FIELDS}}
} };
{{COMPOSITION_CONSTANT}})TEMPLATE";

std::string render_fields(const StructDecl& decl) {
    std::string fields;
    for (const auto& field : decl.fields) {
        const auto mapped = map_field_type(field.type);
        if (!mapped) {
            throw std::invalid_argument("field '" + decl.name + "." + field.name +
                                        "' has unrecognized type '" + field.type + "'");
        }
        fields += "    {\"" + field.name + "\", \"" + *mapped + "\"},\n";
    }
    // Drop the trailing newline after the last entry - the struct_metadata
    // template supplies its own before the closing "};".
    if (!fields.empty()) {
        fields.pop_back();
    }
    return fields;
}

// The composition constant line, or empty for a non-composed struct -
// mirroring tools/atlas-cgen/src/contract_writer.cpp's
// render_composition_member, but as a standalone descriptive constant
// (this generator emits metadata, not the contract member itself).
std::string render_composition_constant(const StructDecl& decl) {
    if (!decl.composition) {
        return "";
    }
    const auto mapped = map_composition_strategy(*decl.composition);
    if (!mapped) {
        throw std::invalid_argument("property '" + decl.name + "' has unrecognized composition strategy '" +
                                    *decl.composition + "'");
    }
    return "inline constexpr std::string_view k" + decl.name + "Composition = \"" + *mapped + "\";\n";
}

std::string render_struct(const StructDecl& decl) {
    return render_template(struct_metadata_template,
                           {
                               {"STRUCT_NAME", decl.name},
                               {"FIELD_COUNT", std::to_string(decl.fields.size())},
                               {"FIELDS", render_fields(decl)},
                               {"COMPOSITION_CONSTANT", render_composition_constant(decl)},
                           });
}

} // namespace

std::string generate_reflection_metadata(const Manifest& manifest,
                                         std::string_view header_name,
                                         std::string_view source_name) {
    // Emitted in this fixed order (properties, then requests, then events),
    // matching tools/atlas-cgen/src/contract_writer.cpp's generate_contract
    // exactly - so the two generators' output stays predictably ordered
    // relative to each other, and output is reproducible rather than
    // dependent on YAML key iteration order (spec §4, determinism).
    const std::array<std::span<const StructDecl>, 3> blocks{{
        manifest.properties,
        manifest.requests,
        manifest.events,
    }};

    std::string body;
    bool first = true;
    for (const auto& block : blocks) {
        for (const auto& decl : block) {
            if (!first) {
                body += "\n";
            }
            body += render_struct(decl);
            first = false;
        }
    }

    return render_template(reflection_file_template,
                           {
                               {"HEADER_NAME", std::string(header_name)},
                               {"SOURCE_NAME", std::string(source_name)},
                               {"CAPABILITY_NAME", manifest.capability_name},
                               {"BODY", body},
                           });
}

} // namespace atlas::refl
