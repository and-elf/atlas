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

std::string render_struct(const StructDecl& decl, std::string_view concept_name) {
    return render_template(templates::struct_decl,
                           {
                               {"STRUCT_NAME", decl.name},
                               {"FIELDS", render_fields(decl)},
                               {"CONCEPT_NAME", std::string(concept_name)},
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
