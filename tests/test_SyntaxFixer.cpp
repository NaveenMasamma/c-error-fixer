#include <gtest/gtest.h>
#include "SyntaxFixer.h"

TEST(SyntaxFixerTest, FixesMissingSemicolon) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-semicolon";
    err.line_number = 1;
    err.error_message = "expected ';'";

    std::vector<std::string> lines = {"int x = 5"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "int x = 5;");
    EXPECT_EQ(suggestions[0].confidence, 0.8f);
}

TEST(SyntaxFixerTest, SafetyCheckComments) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-semicolon";
    err.line_number = 1;

    std::vector<std::string> lines = {"// Just a comment"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);
    EXPECT_TRUE(suggestions.empty()); // Should not modify comments
}

TEST(SyntaxFixerTest, HandlesUnclosedLiteral) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-unclosed-literal";
    err.error_message = "missing terminating \" character";
    // ... verify s.deltas[0].new_content adds the quote ...
}

TEST(SyntaxFixerTest, FixesMissingClosingBraceAtEOF) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-brace-eof";
    err.line_number = 3;
    err.error_message = "expected '}' at end of input";

    std::vector<std::string> lines = {
        "int main()",
        "{",
        "    printf(\"Hello\");"
    };
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].line_number, 3);
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "    printf(\"Hello\");\n}");
}

TEST(SyntaxFixerTest, FixesMultipleMissingClosingBracesAtEOF) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-brace-eof";
    err.line_number = 3;
    err.error_message = "expected declaration or statement at end of input";

    std::vector<std::string> lines = {"int main() {", "    if (1) {", "        return 0;"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "        return 0;\n}}");
}

TEST(SyntaxFixerTest, FixesMissingClosingParenBeforeSemicolon) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-paren";
    err.line_number = 1;
    err.error_message = "expected ')' before ';'";

    std::vector<std::string> lines = {"printf(\"Hello\";"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "printf(\"Hello\");");
    EXPECT_EQ(suggestions[0].confidence, 0.75f);
}

TEST(SyntaxFixerTest, FixesMissingOpeningParenInControlStructure) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-opening";
    err.line_number = 1;
    err.error_message = "expected '(' before 'x'";

    std::vector<std::string> lines = {"if x == 5)"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "if (x == 5)");
}

TEST(SyntaxFixerTest, FixesMalformedIncludeTypo) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-malformed-preprocessor";
    err.line_number = 1;
    err.error_message = "invalid preprocessing directive #includ";

    std::vector<std::string> lines = {"#includ <stdio.h>"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "#include <stdio.h>");
    EXPECT_EQ(suggestions[0].confidence, 0.95f);
    EXPECT_FALSE(suggestions[0].explanation.empty());
}

TEST(SyntaxFixerTest, FixesMalformedIncludeMissingDelimiters) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-malformed-preprocessor";
    err.line_number = 1;
    err.error_message = "#include expects \"FILENAME\" or <FILENAME>";

    std::vector<std::string> lines = {"#include stdio.h"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "#include <stdio.h>");
    EXPECT_EQ(suggestions[0].confidence, 0.95f);
}

TEST(SyntaxFixerTest, FixesMalformedIncludeUnclosedBracket) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-malformed-preprocessor";
    err.line_number = 1;
    err.error_message = "missing terminating > character";

    std::vector<std::string> lines = {"#include <stdio.h"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "#include <stdio.h>");
    EXPECT_EQ(suggestions[0].confidence, 0.95f);
}