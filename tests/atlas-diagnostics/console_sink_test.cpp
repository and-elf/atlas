#include "atlas/diagnostics/console_sink.hpp"

#include <gtest/gtest.h>
#include <sstream>

namespace {

using atlas::diagnostics::ConsoleSink;
using atlas::diagnostics::Record;
using atlas::diagnostics::Severity;

TEST(ConsoleSink, WritesSeveritySystemAndMessage) {
    std::ostringstream stream;
    ConsoleSink sink(stream);

    sink.write(
        Record{.severity = Severity::Warning, .system = "request", .message = "rejected: invalid target"});

    EXPECT_EQ(stream.str(), "[WARNING] request: rejected: invalid target\n");
}

TEST(ConsoleSink, WritesDetailsWhenPresent) {
    std::ostringstream stream;
    ConsoleSink sink(stream);

    sink.write(
        Record{.severity = Severity::Error,
               .system = "resource",
               .message = "failed to resolve",
               .details = {{.key = "resource_id", .value = "42"}, {.key = "reason", .value = "not_found"}}});

    EXPECT_EQ(stream.str(), "[ERROR] resource: failed to resolve {resource_id=42, reason=not_found}\n");
}

TEST(ConsoleSink, OmitsBracesWhenNoDetailsPresent) {
    std::ostringstream stream;
    ConsoleSink sink(stream);

    sink.write(Record{.severity = Severity::Info, .system = "scheduler", .message = "tick started"});

    EXPECT_EQ(stream.str(), "[INFO] scheduler: tick started\n");
}

TEST(ConsoleSink, DefaultConstructionTargetingStdoutDoesNotThrow) {
    EXPECT_NO_THROW(ConsoleSink{});
}

} // namespace
