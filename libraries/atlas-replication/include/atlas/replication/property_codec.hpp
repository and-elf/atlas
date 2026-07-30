#pragma once

#include "atlas/reflection/field_visitor.hpp"
#include "atlas/serialization/byte_reader.hpp"
#include "atlas/serialization/byte_writer.hpp"

#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>

namespace atlas::replication {

// Generic, reflection-driven property field codec (issue #18): puts any
// `atlas::reflection::FieldVisitable` aggregate's direct data members on the
// wire in declaration order, via `atlas::reflection::for_each_field`/
// `field_types_t`, instead of a hand-written `write_health`/`read_health`-
// style function per property type. The replication *boundary* only needs
// to move bytes for a given property; it never needs a property-specific
// encoder written by that property's own capability.
//
// Scope: every direct field's type must be either one of the fixed-width
// primitives `atlas::serialization::ByteWriter`/`ByteReader` already
// support (int8/16/32/64, uint8/16/32/64, float, double), or itself a
// `FieldVisitable` aggregate (e.g. `EntityRef`, `ResourceId`, `PropertyId`) -
// recursed into exactly the same way a top-level property's own fields are
// visited (issue #21). A field of any other shape (a pointer, a container,
// a non-aggregate class) is a compile error - no overload of
// `detail::write_one_field`/`read_one_field` is viable for it - not a
// silently wrong encoding.

namespace detail {

inline void write_one_field(serialization::ByteWriter& writer, std::int8_t value) {
    writer.write_i8(value);
}
inline void write_one_field(serialization::ByteWriter& writer, std::int16_t value) {
    writer.write_i16(value);
}
inline void write_one_field(serialization::ByteWriter& writer, std::int32_t value) {
    writer.write_i32(value);
}
inline void write_one_field(serialization::ByteWriter& writer, std::int64_t value) {
    writer.write_i64(value);
}
inline void write_one_field(serialization::ByteWriter& writer, std::uint8_t value) {
    writer.write_u8(value);
}
inline void write_one_field(serialization::ByteWriter& writer, std::uint16_t value) {
    writer.write_u16(value);
}
inline void write_one_field(serialization::ByteWriter& writer, std::uint32_t value) {
    writer.write_u32(value);
}
inline void write_one_field(serialization::ByteWriter& writer, std::uint64_t value) {
    writer.write_u64(value);
}
inline void write_one_field(serialization::ByteWriter& writer, float value) {
    writer.write_f32(value);
}
inline void write_one_field(serialization::ByteWriter& writer, double value) {
    writer.write_f64(value);
}

// A Field with fields of its own (EntityRef, ResourceId, PropertyId, or any
// other FieldVisitable aggregate) recurses: visit its direct fields the
// same way write_property_fields visits T's. A primitive type can never
// satisfy FieldVisitable (aggregates and scalar types are disjoint in
// C++), so this overload and the ten non-template ones above never compete
// for the same Field - no ambiguity, and this is genuine recursion (a field
// that is itself a struct containing a struct works via this same overload
// calling itself one level deeper), not a single hard-coded extra level.
template <reflection::FieldVisitable Field>
void write_one_field(serialization::ByteWriter& writer, const Field& value) {
    reflection::for_each_field(
        value, [&writer](const auto& nested_field) { write_one_field(writer, nested_field); });
}

// A Field that is one of the fixed-width primitives ByteReader implements.
template <typename Field>
concept ReplicablePrimitive = std::same_as<Field, std::int8_t> || std::same_as<Field, std::int16_t> ||
                              std::same_as<Field, std::int32_t> || std::same_as<Field, std::int64_t> ||
                              std::same_as<Field, std::uint8_t> || std::same_as<Field, std::uint16_t> ||
                              std::same_as<Field, std::uint32_t> || std::same_as<Field, std::uint64_t> ||
                              std::same_as<Field, float> || std::same_as<Field, double>;

template <typename T, typename... Fields>
std::optional<T> read_fields_as(serialization::ByteReader& reader, std::tuple<Fields...>* /*tag*/);

template <ReplicablePrimitive Field> std::optional<Field> read_one_field(serialization::ByteReader& reader) {
    if constexpr (std::same_as<Field, std::int8_t>) {
        return reader.read_i8();
    } else if constexpr (std::same_as<Field, std::int16_t>) {
        return reader.read_i16();
    } else if constexpr (std::same_as<Field, std::int32_t>) {
        return reader.read_i32();
    } else if constexpr (std::same_as<Field, std::int64_t>) {
        return reader.read_i64();
    } else if constexpr (std::same_as<Field, std::uint8_t>) {
        return reader.read_u8();
    } else if constexpr (std::same_as<Field, std::uint16_t>) {
        return reader.read_u16();
    } else if constexpr (std::same_as<Field, std::uint32_t>) {
        return reader.read_u32();
    } else if constexpr (std::same_as<Field, std::uint64_t>) {
        return reader.read_u64();
    } else if constexpr (std::same_as<Field, float>) {
        return reader.read_f32();
    } else {
        return reader.read_f64();
    }
}

// The read-side counterpart to write_one_field's recursive overload: a
// struct-typed Field is read back by reading and reconstructing all of
// *its* fields, via the exact same read_fields_as machinery
// read_property_fields<T> itself uses - a nested struct is just another T,
// one level down.
template <reflection::FieldVisitable Field>
std::optional<Field> read_one_field(serialization::ByteReader& reader) {
    return read_fields_as<Field>(reader, static_cast<reflection::field_types_t<Field>*>(nullptr));
}

// Reads every Fields... in order into a tuple of optionals - braced-init-list
// element evaluation is sequenced left-to-right (unlike ordinary function
// call arguments), which is exactly the guarantee needed to read fields in
// declaration order. Reading continues even after an earlier field fails
// (ByteReader::read_* is safe to call again past exhaustion, always
// returning std::nullopt) rather than short-circuiting, since short-
// circuiting would need its own sequencing logic for no real benefit here.
template <typename... Fields>
std::optional<std::tuple<Fields...>> read_all_fields(serialization::ByteReader& reader) {
    std::tuple<std::optional<Fields>...> parts{read_one_field<Fields>(reader)...};
    const bool all_present = std::apply([](const auto&... opts) { return (opts.has_value() && ...); }, parts);
    if (!all_present) {
        return std::nullopt;
    }
    return std::apply([](auto&&... opts) { return std::tuple<Fields...>{*opts...}; }, parts);
}

// Reconstructs T by aggregate-initializing it from a tuple of already-read
// field values, in declaration order - the read-side counterpart to
// for_each_field's write-side visitation. The Fields... tag parameter (never
// dereferenced) exists purely to pattern-match reflection::field_types_t<T>
// (itself a std::tuple<...> type, not a value) into a template parameter
// pack.
template <typename T, typename... Fields>
std::optional<T> read_fields_as(serialization::ByteReader& reader, std::tuple<Fields...>* /*tag*/) {
    auto values = read_all_fields<Fields...>(reader);
    if (!values) {
        return std::nullopt;
    }
    return std::apply([](auto&&... args) { return T{std::forward<decltype(args)>(args)...}; }, *values);
}

} // namespace detail

// Writes every direct field of value, in declaration order, via for_each_field.
template <reflection::FieldVisitable T>
void write_property_fields(serialization::ByteWriter& writer, const T& value) {
    reflection::for_each_field(value,
                               [&writer](const auto& field) { detail::write_one_field(writer, field); });
}

// Reads a T back from writer_property_fields' own encoding. Fails explicitly
// (std::nullopt) if any field is truncated, mirroring every other codec in
// this library.
template <reflection::FieldVisitable T>
[[nodiscard]] std::optional<T> read_property_fields(serialization::ByteReader& reader) {
    using Fields = reflection::field_types_t<T>;
    return detail::read_fields_as<T>(reader, static_cast<Fields*>(nullptr));
}

} // namespace atlas::replication
