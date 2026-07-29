#include "atlas/contract_gen/contract_writer.hpp"

#include "atlas/contract_gen/template_engine.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <stdexcept>

#include "contract_file_template.hpp"
#include "struct_decl_template.hpp"

namespace atlas::contract_gen {

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

bool any_field_uses_entity_ref(const Manifest& manifest) {
    auto block_uses_entity_ref = [](const std::vector<StructDecl>& decls) {
        return std::ranges::any_of(decls, [](const StructDecl& decl) {
            return std::ranges::any_of(decl.fields,
                                       [](const Field& field) { return field.type == "EntityRef"; });
        });
    };
    return block_uses_entity_ref(manifest.properties) || block_uses_entity_ref(manifest.requests) ||
           block_uses_entity_ref(manifest.events);
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

    const std::string entity_include =
        any_field_uses_entity_ref(manifest) ? "#include \"atlas/entity/entity_ref.hpp\"\n" : "";

    return render_template(templates::contract_file,
                           {
                               {"HEADER_NAME", std::string(header_name)},
                               {"SOURCE_NAME", std::string(source_name)},
                               {"CAPABILITY_NAME", manifest.capability_name},
                               {"ENTITY_INCLUDE", entity_include},
                               {"BODY", body},
                           });
}

} // namespace atlas::contract_gen
