#include "atlas/cgen/manifest.hpp"

#include <stdexcept>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace atlas::cgen {

namespace {

// is_properties_block is true only for the properties block: composition
// and trigger are both property-only concepts (spec §20), so a
// "composition"/"trigger" key encountered while parsing requests/events is
// left to fall through to ordinary field parsing (and rejected there as an
// unrecognized field type) rather than silently special-cased somewhere it
// doesn't apply.
std::vector<StructDecl> parse_struct_block(const YAML::Node& block, bool is_properties_block) {
    std::vector<StructDecl> result;
    if (!block.IsDefined() || block.IsNull()) {
        return result;
    }
    if (!block.IsMap()) {
        throw std::invalid_argument("expected a mapping of struct name to fields");
    }

    for (const auto& struct_entry : block) {
        StructDecl decl;
        decl.name = struct_entry.first.as<std::string>();

        const YAML::Node& fields_node = struct_entry.second;
        if (!fields_node.IsMap()) {
            throw std::invalid_argument("struct '" + decl.name + "' must map field names to types");
        }

        for (const auto& field_entry : fields_node) {
            const auto key = field_entry.first.as<std::string>();

            if (is_properties_block && key == "composition") {
                const auto strategy = field_entry.second.as<std::string>();
                if (!map_composition_strategy(strategy)) {
                    throw std::invalid_argument("property '" + decl.name +
                                                "' has unrecognized composition strategy '" + strategy + "'");
                }
                decl.composition = strategy;
                continue;
            }

            if (is_properties_block && key == "trigger") {
                decl.trigger = field_entry.second.as<bool>();
                continue;
            }

            Field field;
            field.name = key;
            field.type = field_entry.second.as<std::string>();

            if (!map_field_type(field.type)) {
                throw std::invalid_argument("field '" + decl.name + "." + field.name +
                                            "' has unrecognized type '" + field.type + "'");
            }
            decl.fields.push_back(std::move(field));
        }

        result.push_back(std::move(decl));
    }

    return result;
}

} // namespace

std::optional<std::string> map_field_type(const std::string& yaml_type) {
    // A small, closed set of manifest type tokens this generator round maps
    // to C++. Anything outside it is rejected with a clear error rather than
    // guessed at - widening this table is a deliberate, reviewed decision,
    // not something the generator infers.
    static const std::unordered_map<std::string, std::string> type_map{
        {"int8", "std::int8_t"},
        {"int16", "std::int16_t"},
        {"int32", "std::int32_t"},
        {"int64", "std::int64_t"},
        {"uint8", "std::uint8_t"},
        {"uint16", "std::uint16_t"},
        {"uint32", "std::uint32_t"},
        {"uint64", "std::uint64_t"},
        {"float", "float"},
        {"double", "double"},
        {"bool", "bool"},
        {"EntityRef", "atlas::EntityRef"},
        {"ResourceId", "atlas::ResourceId"},
        {"SessionId", "atlas::SessionId"},
    };

    const auto it = type_map.find(yaml_type);
    if (it == type_map.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::string> required_include_for_type(const std::string& yaml_type) {
    // Only vocabulary types need a dedicated #include - primitives are
    // covered by the contract file template's own unconditional includes.
    static const std::unordered_map<std::string, std::string> include_map{
        {"EntityRef", "atlas/entity/entity_ref.hpp"},
        {"ResourceId", "atlas/resource/resource_id.hpp"},
        {"SessionId", "atlas/session/session_id.hpp"},
    };

    const auto it = include_map.find(yaml_type);
    if (it == include_map.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::string> map_composition_strategy(const std::string& yaml_composition) {
    // §20's Composition Strategies table, exhaustively - see this function's
    // header comment for why every strategy is mapped even though only
    // Additive has a working evaluator so far.
    static const std::unordered_map<std::string, std::string> strategy_map{
        {"Additive", "atlas::Composition::Additive"},
        {"Multiplicative", "atlas::Composition::Multiplicative"},
        {"Override", "atlas::Composition::Override"},
        {"PriorityOverride", "atlas::Composition::PriorityOverride"},
        {"SetUnion", "atlas::Composition::SetUnion"},
        {"OrderedComposition", "atlas::Composition::OrderedComposition"},
        {"WeightedComposition", "atlas::Composition::WeightedComposition"},
    };

    const auto it = strategy_map.find(yaml_composition);
    if (it == strategy_map.end()) {
        return std::nullopt;
    }
    return it->second;
}

Manifest parse_manifest(std::string_view yaml_text) {
    try {
        const YAML::Node root = YAML::Load(std::string(yaml_text));

        if (!root.IsMap()) {
            throw std::invalid_argument("manifest root must be a mapping");
        }

        const YAML::Node capability_node = root["capability"];
        if (!capability_node.IsDefined() || !capability_node.IsMap()) {
            throw std::invalid_argument("manifest is missing a 'capability:' block");
        }

        const YAML::Node name_node = capability_node["name"];
        if (!name_node.IsDefined() || !name_node.IsScalar()) {
            throw std::invalid_argument("manifest's 'capability:' block is missing 'name'");
        }

        Manifest manifest;
        manifest.capability_name = name_node.as<std::string>();

        const YAML::Node depends_on_node = root["depends_on"];
        if (depends_on_node.IsDefined() && depends_on_node.IsSequence()) {
            for (const auto& dependency : depends_on_node) {
                manifest.depends_on.push_back(dependency.as<std::string>());
            }
        }

        const YAML::Node consumes_node = root["consumes"];
        if (consumes_node.IsDefined() && consumes_node.IsSequence()) {
            for (const auto& property : consumes_node) {
                manifest.consumes.push_back(property.as<std::string>());
            }
        }

        manifest.properties = parse_struct_block(root["properties"], /*is_properties_block=*/true);
        manifest.requests = parse_struct_block(root["requests"], /*is_properties_block=*/false);
        manifest.events = parse_struct_block(root["events"], /*is_properties_block=*/false);

        return manifest;
    } catch (const YAML::Exception& e) {
        throw std::invalid_argument(std::string("YAML error: ") + e.what());
    }
}

} // namespace atlas::cgen
