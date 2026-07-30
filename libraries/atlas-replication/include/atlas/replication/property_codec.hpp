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
// Scope: every direct field's type must be one of the fixed-width
// primitives `atlas::serialization::ByteWriter`/`ByteReader` already
// support (int8/16/32/64, uint8/16/32/64, float, double) - covers
// `health::Health` (two int32 fields) fully. A field typed EntityRef,
// ResourceId, or another struct is not yet supported (a future increment,
// once a real property needs one) - calling with such a field type is a
// compile error (see detail::read_one_field's deleted primary template),
// not a silently wrong encoding.

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

// Deleted primary template: a Field type with no matching explicit
// specialization below is a compile error at the call site
// (read_all_fields's pack expansion), not a link error from an
// instantiated-but-undefined function.
template <typename Field> std::optional<Field> read_one_field(serialization::ByteReader& reader) = delete;

template <> inline std::optional<std::int8_t> read_one_field<std::int8_t>(serialization::ByteReader& reader) {
    return reader.read_i8();
}
template <>
inline std::optional<std::int16_t> read_one_field<std::int16_t>(serialization::ByteReader& reader) {
    return reader.read_i16();
}
template <>
inline std::optional<std::int32_t> read_one_field<std::int32_t>(serialization::ByteReader& reader) {
    return reader.read_i32();
}
template <>
inline std::optional<std::int64_t> read_one_field<std::int64_t>(serialization::ByteReader& reader) {
    return reader.read_i64();
}
template <>
inline std::optional<std::uint8_t> read_one_field<std::uint8_t>(serialization::ByteReader& reader) {
    return reader.read_u8();
}
template <>
inline std::optional<std::uint16_t> read_one_field<std::uint16_t>(serialization::ByteReader& reader) {
    return reader.read_u16();
}
template <>
inline std::optional<std::uint32_t> read_one_field<std::uint32_t>(serialization::ByteReader& reader) {
    return reader.read_u32();
}
template <>
inline std::optional<std::uint64_t> read_one_field<std::uint64_t>(serialization::ByteReader& reader) {
    return reader.read_u64();
}
template <> inline std::optional<float> read_one_field<float>(serialization::ByteReader& reader) {
    return reader.read_f32();
}
template <> inline std::optional<double> read_one_field<double>(serialization::ByteReader& reader) {
    return reader.read_f64();
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
