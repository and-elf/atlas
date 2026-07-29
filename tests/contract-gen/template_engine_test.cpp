#include "atlas/contract_gen/template_engine.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace atlas::contract_gen {
namespace {

TEST(RenderTemplate, SubstitutesASinglePlaceholder) {
    EXPECT_EQ(render_template("Hello, {{NAME}}!", {{"NAME", "world"}}), "Hello, world!");
}

TEST(RenderTemplate, SubstitutesMultipleDistinctPlaceholders) {
    EXPECT_EQ(render_template("{{GREETING}}, {{NAME}}!", {{"GREETING", "Hi"}, {"NAME", "atlas"}}),
              "Hi, atlas!");
}

TEST(RenderTemplate, SubstitutesTheSamePlaceholderEveryTimeItAppears) {
    EXPECT_EQ(render_template("{{X}}-{{X}}-{{X}}", {{"X", "a"}}), "a-a-a");
}

TEST(RenderTemplate, TemplateWithNoPlaceholdersIsReturnedUnchanged) {
    EXPECT_EQ(render_template("plain text, no placeholders here", {}), "plain text, no placeholders here");
}

TEST(RenderTemplate, EmptyTemplateProducesEmptyOutput) {
    EXPECT_EQ(render_template("", {}), "");
}

TEST(RenderTemplate, UnusedValuesInTheMapAreHarmless) {
    EXPECT_EQ(render_template("{{X}}", {{"X", "used"}, {"Y", "never referenced"}}), "used");
}

TEST(RenderTemplate, RejectsAnUnresolvedPlaceholder) {
    try {
        (void)render_template("Hello, {{MISSING}}!", {});
        FAIL() << "expected render_template to throw";
    } catch (const std::invalid_argument& e) {
        EXPECT_NE(std::string_view(e.what()).find("MISSING"), std::string_view::npos);
    }
}

TEST(RenderTemplate, RejectsAnUnterminatedPlaceholder) {
    EXPECT_THROW((void)render_template("Hello, {{NAME", {{"NAME", "world"}}), std::invalid_argument);
}

} // namespace
} // namespace atlas::contract_gen
