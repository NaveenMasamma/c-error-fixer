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

TEST(SyntaxFixerTest, FixesMissingClosingBraceMidFile) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-brace";
    err.line_number = 3;
    err.error_message = "expected '}' before 'else'";

    std::vector<std::string> lines = {
        "if (x) {",
        "    return 1;",
        "else {"
    };
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].line_number, 2);
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "    return 1;\n}");
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

TEST(SyntaxFixerTest, FixesMissingOpeningParenInFunctionCall) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-opening";
    err.line_number = 1;
    err.error_message = "makes integer from pointer without a cast";

    std::vector<std::string> lines = {"    int result = printf \"Hello\";"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "    int result = printf(\"Hello\");");
    EXPECT_EQ(suggestions[0].confidence, 0.9f);
}

TEST(SyntaxFixerTest, FixesMissingOpeningBracketForArray) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-opening";
    err.line_number = 1;
    err.error_message = "expected '[' before numeric constant";

    std::vector<std::string> lines = {"int arr 5];"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "int arr[5];");
}

TEST(SyntaxFixerTest, FixesMissingOpeningBracketForArrayWithVariable) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-opening";
    err.line_number = 1;
    err.error_message = "expected '[' before 'i'";

    std::vector<std::string> lines = {"int val = arr i];"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "int val = arr[i];");
}

TEST(SyntaxFixerTest, FixesMissingClosingBracketForArray) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-brace";
    err.line_number = 1;
    err.error_message = "expected ']' before ';'";

    std::vector<std::string> lines = {"int arr[5 ;"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "int arr[5];");
}

TEST(SyntaxFixerTest, FixesMissingClosingBracketForArrayWithAssignment) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-brace";
    err.line_number = 1;
    err.error_message = "expected ']' before '='";

    std::vector<std::string> lines = {"int arr[5  = {1, 2, 3};"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "int arr[5]  = {1, 2, 3};");
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

TEST(SyntaxFixerTest, IgnoresFalsePositiveComma) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-comma";
    err.line_number = 1;
    // Compilers sometimes confuse semicolons for commas in nested closures
    err.error_message = "expected ',' or ';' before '}' token"; 

    // This line is already fully terminated. Appending a comma here is a false positive.
    std::vector<std::string> lines = {"int arr[] = {1, 2, 3};"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    // Ensure it doesn't blindly append a comma (e.g., creating 'int arr[] = {1, 2, 3};,')
    if (!suggestions.empty() && !suggestions[0].deltas.empty()) {
        EXPECT_NE(suggestions[0].deltas[0].new_content.back(), ',');
    }
}

TEST(SyntaxFixerTest, FixesMissingColonForCaseLabel) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-colon";
    err.line_number = 1;
    err.error_message = "expected ':' before 'break'";

    std::vector<std::string> lines = {"    case 1 break;"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "    case 1: break;");
}

TEST(SyntaxFixerTest, FixesMissingColonForPublicLabel) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-colon";
    err.line_number = 1;
    err.error_message = "expected ':' before 'int'";

    std::vector<std::string> lines = {"public int x;"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "public: int x;");
}

TEST(SyntaxFixerTest, FixesMissingMiddleBracketForMultiDimArray) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-brace";
    err.line_number = 1;
    err.error_message = "expected ']' before '['";

    std::vector<std::string> lines = {"int arr[5 [5];"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "int arr[5] [5];");
}

TEST(SyntaxFixerTest, FixesMissingSemicolonAfterArrayInit) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-semicolon";
    err.line_number = 2;
    err.error_message = "expected ',' or ';' before 'printf'";

    std::vector<std::string> lines = {
        "    int arr[5] = {1, 2, 3, 4, 5}",
        "    printf(\"%d\\n\", arr[0]);"
    };
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].line_number, 1);
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "    int arr[5] = {1, 2, 3, 4, 5};");
}

TEST(SyntaxFixerTest, FixesMissingSemicolonAfterStructBrace) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-semicolon";
    err.line_number = 3;
    err.error_message = "no semicolon at end of struct or union";

    std::vector<std::string> lines = {
        "struct Point {",
        "    int x;",
        "}"
    };
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].line_number, 3);
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "};");
}

TEST(SyntaxFixerTest, HandlesAmbiguousDeclarationsWithoutFixing) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-ambiguous";
    err.line_number = 2;
    err.error_message = "expected '=', ',', ';', 'asm' or '__attribute__' before '{' token";

    std::vector<std::string> lines = {
        "void print_number(int n)",
        "    printf(\"%d\\n\", n);",
        "}"
    };
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].confidence, 0.3f);
    EXPECT_FALSE(suggestions[0].is_safe);
    EXPECT_TRUE(suggestions[0].deltas.empty());
    EXPECT_NE(suggestions[0].explanation.find("Missing opening brace"), std::string::npos);
    EXPECT_NE(suggestions[0].explanation.find("Line 1"), std::string::npos);
}

TEST(SyntaxFixerTest, HandlesAmbiguousEOF) {
    SyntaxFixer fixer;
    CompilerError err;
    err.error_code = "syntax-expected-brace-eof";
    err.line_number = 10;
    err.error_message = "expected declaration or statement at end of input";

    std::vector<std::string> lines = { "int main() {}" };
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].confidence, 0.4f);
    EXPECT_FALSE(suggestions[0].is_safe);
    EXPECT_TRUE(suggestions[0].deltas.empty());
    EXPECT_NE(suggestions[0].explanation.find("Missing closing brace"), std::string::npos);
}
