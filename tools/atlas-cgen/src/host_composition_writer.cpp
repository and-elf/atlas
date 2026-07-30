#include "atlas/cgen/host_composition_writer.hpp"

#include "atlas/cgen/template_engine.hpp"

#include <cctype>
#include <set>

#include "host_composition_file_template.hpp"

namespace atlas::cgen {

namespace {

// "MovementSpeed" -> "movement_speed", "CastSpeed" -> "cast_speed" - every
// manifest struct name this generator round handles is plain PascalCase
// (no consecutive-capital abbreviations to disambiguate), so "insert an
// underscore before every capital that isn't the first character" is exact,
// not a heuristic.
std::string to_snake_case(std::string_view pascal_case) {
    std::string result;
    result.reserve(pascal_case.size() + 4);
    for (const char c : pascal_case) {
        if (static_cast<bool>(std::isupper(static_cast<unsigned char>(c))) && !result.empty()) {
            result += '_';
        }
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

std::string property_store_member_name(const std::string& capability_name, const StructDecl& property) {
    return capability_name + "_" + to_snake_case(property.name) + "_store";
}

std::string render_members(const HostComposition& composition) {
    std::string members;
    bool first = true;
    for (const auto& capability : composition.ordered_capabilities) {
        for (const auto& property : capability.properties) {
            if (!first) {
                members += "\n";
            }
            members += "    runtime::PropertyStore<" + capability.capability_name + "::" + property.name +
                       "> " + property_store_member_name(capability.capability_name, property) + ";";
            first = false;
        }
    }
    return members;
}

std::string render_registrations(const HostComposition& composition) {
    std::string registrations;
    bool first = true;
    for (const auto& capability : composition.ordered_capabilities) {
        for (const auto& property : capability.properties) {
            if (!first) {
                registrations += "\n";
            }
            registrations += "    ctx.register_property_store(host." +
                             property_store_member_name(capability.capability_name, property) + ");";
            first = false;
        }
    }
    return registrations;
}

// One #include per composed capability that actually declares at least one
// property - a capability with none (e.g. a pure event vocabulary like
// interruption) contributes nothing to this header and needs no include.
// References the capability's own *generated* contract header
// (<name>.capability.hpp, atlas_generate_capability_contract's own output
// naming) directly, rather than a hand-written wrapper header
// (<name>/<name>.hpp) - the generated contract is guaranteed to exist
// wherever contracts are generated at all; a hand-written wrapper is only
// this demo's own convention (spec §11's project layout), not something
// atlas-cgen itself can assume for every consuming project. std::set keeps
// the result deterministic (spec §4) regardless of any future change to
// iteration order, though ordered_capabilities is already deterministic
// itself.
std::string collect_capability_includes(const HostComposition& composition) {
    std::set<std::string> includes;
    for (const auto& capability : composition.ordered_capabilities) {
        if (!capability.properties.empty()) {
            includes.insert(capability.capability_name + ".capability.hpp");
        }
    }

    std::string result;
    for (const auto& include : includes) {
        result += "#include \"" + include + "\"\n";
    }
    return result;
}

} // namespace

std::string generate_host_composition(const HostComposition& composition,
                                      std::string_view header_name,
                                      std::string_view source_name) {
    return render_template(templates::host_composition_file,
                           {
                               {"HEADER_NAME", std::string(header_name)},
                               {"SOURCE_NAME", std::string(source_name)},
                               {"HOST_NAME", composition.host_name},
                               {"EXTRA_INCLUDES", collect_capability_includes(composition)},
                               {"MEMBERS", render_members(composition)},
                               {"REGISTRATIONS", render_registrations(composition)},
                           });
}

} // namespace atlas::cgen
