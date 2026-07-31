#pragma once

#include <string_view>

namespace atlas::refl {

// Plain aggregate (Rule of Zero - no invariant to protect) describing one
// field's identity for generic reflection-metadata display: the field's
// declared name and the C++ spelling of its declared type, both recovered
// from a capability manifest at generation time by atlas-refl.
//
// This is the missing half of what libraries/atlas-reflection's runtime
// primitives (field_count()/for_each_field()/field_types_t<T>) can recover
// on their own in standard C++23: no compile-time mechanism in C++23 names
// a data member (that's C++26 P2996 - see that library's README, "Scoping
// decision: field visitation and type inspection, not field names"), so
// field *names* have to come from somewhere that already knows them - the
// manifest a capability author wrote, not the compiled struct itself. A
// generated array of these, one per property/request/event struct (see
// reflection_writer.hpp), is what closes that gap without hand-rolled or
// compiler-specific reflection.
struct FieldMetadata {
    std::string_view name;
    std::string_view type_name;
};

} // namespace atlas::refl
