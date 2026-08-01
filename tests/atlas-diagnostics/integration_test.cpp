#include "atlas/diagnostics/console_sink.hpp"
#include "atlas/diagnostics/logger.hpp"

#include <gtest/gtest.h>
#include <sstream>

namespace {

using atlas::diagnostics::ConsoleSink;
using atlas::diagnostics::Logger;
using atlas::diagnostics::Severity;

// Proves the whole mechanism end to end - a Logger fanning a Record out to
// a real ConsoleSink - the way a runtime system reporting onto the uniform
// failure channel (spec §6) would actually compose the two pieces this
// library provides, rather than exercising Logger and ConsoleSink only in
// isolation from each other.
TEST(DiagnosticsIntegration, LoggerFansOutToConsoleSink) {
    std::ostringstream stream;
    ConsoleSink sink(stream);
    Logger logger;
    logger.add_sink(sink);

    logger.log(Severity::Error, "resource", "failed to resolve", {{.key = "resource_id", .value = "7"}});

    EXPECT_EQ(stream.str(), "[ERROR] resource: failed to resolve {resource_id=7}\n");
}

TEST(DiagnosticsIntegration, LoggerFansOutToMultipleConsoleSinks) {
    std::ostringstream first_stream;
    std::ostringstream second_stream;
    ConsoleSink first_sink(first_stream);
    ConsoleSink second_sink(second_stream);
    Logger logger;
    logger.add_sink(first_sink);
    logger.add_sink(second_sink);

    logger.log(Severity::Warning, "request", "rejected");

    EXPECT_EQ(first_stream.str(), "[WARNING] request: rejected\n");
    EXPECT_EQ(second_stream.str(), "[WARNING] request: rejected\n");
}

} // namespace
