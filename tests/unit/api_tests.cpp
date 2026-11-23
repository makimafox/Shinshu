#include "shinshu/cli.hpp"
#include "shinshu/format.hpp"
#include "shinshu/version.hpp"

#include <gtest/gtest.h>

#include <string_view>

TEST(Version, IsAvailable) {
    EXPECT_FALSE(shinshu::version().empty());
}

TEST(FormatContract, HeaderDefaultsAreExplicit) {
    const shinshu::FileHeader header;
    EXPECT_EQ(header.format_version, 1);
    EXPECT_EQ(header.original_size, 0);
    EXPECT_EQ(header.flags, 0);
}

TEST(CliArguments, ReportsVersionAction) {
    constexpr char const* argv[] = {"shinshu", "--version"};
    const auto parsed = shinshu::cli::parse_arguments(2, argv);
    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.action, shinshu::cli::Action::version);
}

TEST(CliArguments, RejectsMalformedCompressionArguments) {
    constexpr char const* argv[] = {"shinshu", "compress", "input.bin"};
    const auto parsed = shinshu::cli::parse_arguments(3, argv);
    EXPECT_FALSE(parsed.valid);
    EXPECT_FALSE(parsed.error.empty());
}

TEST(CliArguments, RejectsNullOptionValue) {
    constexpr char const* argv[] = {"shinshu", "compress", "input.bin", "-o", nullptr};
    const auto parsed = shinshu::cli::parse_arguments(5, argv);
    EXPECT_FALSE(parsed.valid);
}
