#pragma once

#include <map>
#include <string>
#include <string_view>

namespace atlas::refl {

// Deliberately not a templating language: no loops, no conditionals - just
// "{{NAME}}" token substitution. Repeated or conditional content (one
// fields-array per property/request/event, an optional composition
// constant) is assembled by ordinary C++ code and handed in as a single
// already-built value for one placeholder, mirroring this project's own
// Declarative Boundary principle (CLAUDE.md / spec §14) and matching
// tools/atlas-cgen/include/atlas/cgen/template_engine.hpp's own engine
// exactly (this file is that same small, generic utility, duplicated for
// atlas-refl's own dependency position - see this tool's README).
//
// Every "{{NAME}}" in template_text must have a matching entry in values, or
// this throws std::invalid_argument naming the unresolved (or unterminated)
// placeholder rather than emitting a file with a literal "{{NAME}}" left in
// it. An entry in values that's never referenced by the template is not an
// error.
[[nodiscard]] std::string render_template(std::string_view template_text,
                                          const std::map<std::string, std::string>& values);

} // namespace atlas::refl
