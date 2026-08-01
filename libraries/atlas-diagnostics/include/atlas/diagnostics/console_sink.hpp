#pragma once

#include "atlas/diagnostics/sink.hpp"

#include <iostream>
#include <ostream>

namespace atlas::diagnostics {

// Writes each Record as one formatted line to a std::ostream - the
// "console/stdout sink" this library's issue asks for at minimum.
// Formats as "[SEVERITY] system: message" followed by " {key=value, ...}"
// only when details is non-empty, then a trailing newline.
//
// Takes the target stream by reference in its constructor (defaulting to
// std::cout) rather than hard-coding std::cout internally (CLAUDE.md,
// Architecture Principles: dependency injection) - this is what makes
// testing its exact output format straightforward (a test constructs one
// against a std::ostringstream) without capturing real stdout. An
// encapsulated class: its whole reason to exist is overriding Sink's
// virtual write(), not holding a plain value.
class ConsoleSink : public Sink {
public:
    explicit ConsoleSink(std::ostream& stream = std::cout) noexcept : stream_(&stream) {}

    void write(const Record& record) override;

private:
    // A pointer, not a reference member (cppcoreguidelines-avoid-const-or-ref-data-members) -
    // a non-owning, never-rebound back-reference to the caller-owned
    // stream, the same reasoning atlas::Context uses for its own Host*.
    std::ostream* stream_;
};

} // namespace atlas::diagnostics
