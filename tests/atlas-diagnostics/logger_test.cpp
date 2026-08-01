#include "atlas/diagnostics/logger.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace {

using atlas::diagnostics::Logger;
using atlas::diagnostics::Record;
using atlas::diagnostics::Severity;
using atlas::diagnostics::Sink;

// A minimal capturing sink defined only in this test file, not shipped as
// part of the library - this is what proves Logger's dispatch mechanism
// without needing a production in-memory sink to exist yet (YAGNI; see
// the library README's sink-mechanism scoping decision).
class CapturingSink : public Sink {
public:
    void write(const Record& record) override { records.push_back(record); }

    std::vector<Record> records;
};

TEST(Logger, LogWithNoSinksRegisteredIsHarmless) {
    const Logger logger;

    EXPECT_NO_THROW(logger.log(Severity::Debug, "scheduler", "tick started"));
}

TEST(Logger, LogDispatchesToRegisteredSink) {
    Logger logger;
    CapturingSink sink;
    logger.add_sink(sink);

    logger.log(Severity::Error, "resource", "failed to resolve", {{.key = "id", .value = "42"}});

    ASSERT_EQ(sink.records.size(), 1U);
    EXPECT_EQ(sink.records[0].severity, Severity::Error);
    EXPECT_EQ(sink.records[0].system, "resource");
    EXPECT_EQ(sink.records[0].message, "failed to resolve");
    ASSERT_EQ(sink.records[0].details.size(), 1U);
    EXPECT_EQ(sink.records[0].details[0].key, "id");
    EXPECT_EQ(sink.records[0].details[0].value, "42");
}

TEST(Logger, LogDispatchesToEverySinkInRegistrationOrder) {
    Logger logger;
    CapturingSink first;
    CapturingSink second;
    logger.add_sink(first);
    logger.add_sink(second);

    logger.log(Severity::Info, "replication", "state sent");

    ASSERT_EQ(first.records.size(), 1U);
    ASSERT_EQ(second.records.size(), 1U);
    EXPECT_EQ(first.records[0].message, "state sent");
    EXPECT_EQ(second.records[0].message, "state sent");
}

TEST(Logger, LogRecordOverloadForwardsRecordUnchanged) {
    Logger logger;
    CapturingSink sink;
    logger.add_sink(sink);

    const Record record{.severity = Severity::Critical, .system = "networking", .message = "disconnected"};
    logger.log(record);

    ASSERT_EQ(sink.records.size(), 1U);
    EXPECT_EQ(sink.records[0], record);
}

} // namespace
