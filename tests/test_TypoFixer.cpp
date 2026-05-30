#include <gtest/gtest.h>
#include "TypoFixer.h"

TEST(TypoFixerTest, DetectsKeywordTypo) {
    TypoFixer fixer;
    CompilerError err;
    err.error_code = "syntax-keyword-typo";
    err.line_number = 1;

    std::vector<std::string> lines = {"retun 0;"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "return 0;");
    EXPECT_NEAR(suggestions[0].confidence, 0.85f, 0.01f);
}

TEST(TypoFixerTest, IgnoresUnknownTokens) {
    TypoFixer fixer;
    CompilerError err;
    err.error_code = "syntax-keyword-typo";
    err.line_number = 1;

    std::vector<std::string> lines = {"xyzzy = 5;"}; // Too far from any keyword
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);
    EXPECT_TRUE(suggestions.empty());
}

TEST(TypoFixerTest, CanHandleOnlyTypoCode) {
    TypoFixer fixer;
    EXPECT_TRUE(fixer.canHandle("syntax-keyword-typo"));
    EXPECT_FALSE(fixer.canHandle("syntax-error"));
}

TEST(TypoFixerTest, DetectsStdLibTypo) {
    TypoFixer fixer;
    CompilerError err;
    err.error_code = "syntax-keyword-typo";
    err.line_number = 1;

    std::vector<std::string> lines = {"pritf(\"Hello\");"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "printf(\"Hello\");");
}

TEST(TypoFixerTest, DetectsRetrnTypo) {
    TypoFixer fixer;
    CompilerError err;
    err.error_code = "syntax-keyword-typo";
    err.line_number = 1;

    std::vector<std::string> lines = {"retrn 0;"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "return 0;");
}