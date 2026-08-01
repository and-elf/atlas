#pragma once

#include "atlas/diagnostics/record.hpp"

namespace atlas::diagnostics {

// Where a Record goes once logged - the pluggable half of this library's
// mechanism (see README). A small, single-method interface: a sink's only
// job is "given a Record, do something with it" - console output, a
// future file sink, a future in-memory sink for tests (this library ships
// only ConsoleSink; see README for why the others are deliberately not
// built yet).
//
// A polymorphic base rather than a std::function<void(const Record&)>
// (contrast atlas::scheduler::Job, which *is* a bare std::function): a
// sink typically owns state that persists across many write() calls (an
// output stream, eventually an open file handle), which reads more
// naturally as a named type a caller constructs once and registers, than
// as a capturing lambda reconstructed with awkward-to-share captured
// state. This is a plain runtime plugin-point, not a capability contract,
// so spec §5's "never a runtime interface table or virtual dispatch
// lookup" (which governs capability-to-capability contract satisfaction)
// does not apply to it.
class Sink {
public:
    Sink() = default;
    Sink(const Sink&) = default;
    Sink(Sink&&) = default;
    Sink& operator=(const Sink&) = default;
    Sink& operator=(Sink&&) = default;
    virtual ~Sink() = default;

    virtual void write(const Record& record) = 0;
};

} // namespace atlas::diagnostics
