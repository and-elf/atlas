#include "atlas/docgen/markdown_writer.hpp"

#include <array>
#include <span>

namespace atlas::docgen {

namespace {

using atlas::cgen::Manifest;
using atlas::cgen::StructDecl;

// A comma-separated list of names, or "_(none)_" for an empty list -
// shared by the Depends on/Consumes lines, which have identical rendering
// rules (spec §13's depends_on/consumes are both plain name lists).
std::string render_name_list(const std::vector<std::string>& names) {
    if (names.empty()) {
        return "_(none)_";
    }
    std::string result;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0) {
            result += ", ";
        }
        result += names[i];
    }
    return result;
}

// The optional composition/trigger annotation lines for one struct, or
// empty for a plain (non-composed, non-triggered) one - mirroring
// atlas::cgen::contract_writer's render_composition_member/render_trigger_member,
// which emit analogous C++ members ahead of a struct's ordinary fields.
// Composed and triggered are independent (spec §20): a property may have
// either, both, or neither, so each line is rendered on its own.
std::string render_annotations(const StructDecl& decl) {
    std::string annotations;
    if (decl.composition) {
        annotations += "_Composition strategy: " + *decl.composition + "_\n\n";
    }
    if (decl.trigger) {
        annotations += "_Triggered: value is meaningful only on the tick it was written_\n\n";
    }
    return annotations;
}

// One Markdown table of a struct's ordinary fields, name/type in manifest
// declaration order - see markdown_writer.hpp for why the manifest's own
// type token (e.g. "int32") is shown rather than its mapped C++ spelling.
std::string render_fields_table(const StructDecl& decl) {
    std::string table = "| Field | Type |\n| --- | --- |\n";
    for (const auto& field : decl.fields) {
        table += "| " + field.name + " | " + field.type + " |\n";
    }
    return table;
}

std::string render_struct_section(const StructDecl& decl) {
    return "### " + decl.name + "\n\n" + render_annotations(decl) + render_fields_table(decl) + "\n";
}

// A view, not a reference member (cppcoreguidelines-avoid-const-or-ref-data-members) -
// Block instances are short-lived locals in generate_markdown_doc below,
// mirroring atlas::cgen::contract_writer's own identically-shaped Block
// helper.
struct Block {
    std::span<const StructDecl> decls;
    std::string_view heading;
};

// One "## <Heading>" section (Properties/Requests/Events) followed by one
// "### <Name>" subsection per struct, or empty entirely if the block has no
// entries - an empty manifest (or one that only declares, say, properties)
// must not emit a heading for a section with nothing under it.
std::string render_block(const Block& block) {
    if (block.decls.empty()) {
        return "";
    }
    std::string section = "## " + std::string(block.heading) + "\n\n";
    for (const auto& decl : block.decls) {
        section += render_struct_section(decl);
    }
    return section;
}

} // namespace

std::string
generate_markdown_doc(const Manifest& manifest, std::string_view header_name, std::string_view source_name) {
    std::string doc;
    doc += "<!-- GENERATED — " + std::string(header_name) + ". Source: " + std::string(source_name) +
           " — do not hand-edit. -->\n\n";
    doc += "# " + manifest.capability_name + "\n\n";
    doc += "**Depends on:** " + render_name_list(manifest.depends_on) + "\n\n";
    doc += "**Consumes:** " + render_name_list(manifest.consumes) + "\n\n";

    // Fixed order (properties, then requests, then events) regardless of
    // the manifest's own top-level YAML key order - matching
    // atlas::cgen::generate_contract's identical reasoning: reproducible
    // output shouldn't depend on YAML key iteration order (spec §4).
    const std::array<Block, 3> blocks{{
        {manifest.properties, "Properties"},
        {manifest.requests, "Requests"},
        {manifest.events, "Events"},
    }};

    for (const auto& block : blocks) {
        doc += render_block(block);
    }

    return doc;
}

} // namespace atlas::docgen
