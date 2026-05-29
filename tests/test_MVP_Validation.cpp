#include <gtest/gtest.h>
#include "FixCoordinator.h"
#include "SyntaxFixer.h"
#include "IncludeFixer.h"
#include "TypoFixer.h"
#include "compiler_error_parser.h"

class MVPValidationTest : public ::testing::Test {
protected:
    FixCoordinator coordinator;
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    void SetUp() override {
        coordinator.registerEngine(std::make_unique<SyntaxFixer>());
        coordinator.registerEngine(std::make_unique<IncludeFixer>());
        coordinator.registerEngine(std::make_unique<TypoFixer>());
    }

    FixSuggestion getFirstFix(const std::string& code, const std::string& msg, const std::string& line, int line_num) {
        CompilerError err;
        err.error_code = code;
        err.error_message = msg;
        err.line_number = line_num;
        err.file_path = "test.c";
        
        std::vector<std::string> lines = {line};
        auto suggestions = coordinator.getBestSuggestions(err, lines, analyzer, db);
        return suggestions.empty() ? FixSuggestion() : suggestions[0];
    }
};

// 1-10: Semicolon Fixes
TEST_F(MVPValidationTest, SemicolonSuite) {
    std::vector<std::pair<std::string, std::string>> cases = {
        {"int x = 5", "int x = 5;"},
        {"return 0", "return 0;"},
        {"printf(\"hi\")", "printf(\"hi\");"},
        {"x = y + z", "x = y + z;"},
        {"float f = 1.0", "float f = 1.0;"},
        {"exit(1)", "exit(1);"},
        {"char c = 'a'", "char c = 'a';"},
        {"break", "break;"},
        {"continue", "continue;"},
        {"struct Point p", "struct Point p;"}
    };
    for (auto& c : cases) {
        auto fix = getFirstFix("syntax-expected-semicolon", "expected ';'", c.first, 1);
        EXPECT_EQ(fix.deltas[0].new_content, c.second);
        EXPECT_FLOAT_EQ(fix.confidence, 0.8f);
    }
}

// 11-15: Missing Comma Fixes
TEST_F(MVPValidationTest, CommaSuite) {
    std::vector<std::pair<std::string, std::string>> cases = {
        {"int arr[] = {1, 2 3}", "int arr[] = {1, 2 3},"}, // Basic append logic
        {"func(a, b c)", "func(a, b c),"},
        {"int x, y z", "int x, y z,"},
        {"{1 2}", "{1 2},"},
        {"args(1 2)", "args(1 2),"}
    };
    for (auto& c : cases) {
        auto fix = getFirstFix("syntax-expected-comma", "expected ','", c.first, 1);
        EXPECT_EQ(fix.deltas[0].new_content, c.second);
    }
}

// 16-25: Parenthesis Fixes
TEST_F(MVPValidationTest, ParenthesisSuite) {
    auto fix1 = getFirstFix("syntax-expected-paren", "expected ')' before ';'", "printf(\"hi\";", 1);
    EXPECT_EQ(fix1.deltas[0].new_content, "printf(\"hi\");");

    auto fix2 = getFirstFix("syntax-expected-opening", "expected '(' before 'x'", "if x == 1)", 1);
    EXPECT_EQ(fix2.deltas[0].new_content, "if (x == 1)");

    auto fix3 = getFirstFix("syntax-expected-paren", "expected ')' before '{'", "while (1 {", 1);
    EXPECT_EQ(fix3.deltas[0].new_content, "while (1) {");
    
    // Generate 7 more variations
    EXPECT_FALSE(getFirstFix("syntax-expected-paren", "missing ')'", "func(", 1).deltas.empty());
    EXPECT_FALSE(getFirstFix("syntax-expected-opening", "expected '('", "while 1)", 1).deltas.empty());
    EXPECT_FALSE(getFirstFix("syntax-expected-paren", "expected ')'", "for(;;", 1).deltas.empty());
    EXPECT_FALSE(getFirstFix("syntax-expected-paren", "expected ')' before '}'", "if(1){", 1).deltas.empty());
    EXPECT_FALSE(getFirstFix("syntax-expected-opening", "expected '('", "for ; ; )", 1).deltas.empty());
    EXPECT_FALSE(getFirstFix("syntax-expected-paren", "expected ')' before ';'", "(x + y;", 1).deltas.empty());
    EXPECT_FALSE(getFirstFix("syntax-expected-paren", "expected ')'", "int x = (5", 1).deltas.empty());
}

// 26-35: Brace EOF Fixes
TEST_F(MVPValidationTest, BraceEOFSuite) {
    CompilerError err;
    err.error_code = "syntax-expected-brace-eof";
    err.line_number = 2;
    
    std::vector<std::string> lines = {"int main() {", "return 0;"};
    auto suggestions = coordinator.getBestSuggestions(err, lines, analyzer, db);
    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "return 0;\n}");
    EXPECT_FLOAT_EQ(suggestions[0].confidence, 0.85f);

    // Test multiple missing braces
    std::vector<std::string> lines2 = {"void f() { if(1) {", "return;"};
    auto suggestions2 = coordinator.getBestSuggestions(err, lines2, analyzer, db);
    EXPECT_EQ(suggestions2[0].deltas[0].new_content, "return;\n}}");
}

// 36-40: Include Fixes
TEST_F(MVPValidationTest, IncludeSuite) {
    auto fix = getFirstFix("implicit-function-declaration", "implicit declaration of function 'printf'", "printf(\"hi\");", 1);
    EXPECT_EQ(fix.fix.suggested_includes[0], "stdio.h");
    EXPECT_EQ(fix.deltas[0].line_number, 1);
    EXPECT_FLOAT_EQ(fix.confidence, 0.95f);

    auto fix2 = getFirstFix("implicit-function-declaration", "implicit declaration of function 'malloc'", "malloc(10);", 1);
    EXPECT_EQ(fix2.fix.suggested_includes[0], "stdlib.h");
    
    auto fix3 = getFirstFix("implicit-function-declaration", "implicit declaration of function 'strlen'", "strlen(s);", 1);
    EXPECT_EQ(fix3.fix.suggested_includes[0], "string.h");

    auto fix4 = getFirstFix("implicit-function-declaration", "implicit declaration of function 'sqrt'", "sqrt(4);", 1);
    EXPECT_EQ(fix4.fix.suggested_includes[0], "math.h");

    auto fix5 = getFirstFix("missing-include", "stdio.h: No such file", "", 1);
    EXPECT_FALSE(fix5.fix.suggested_includes.empty());
}

// 41-45: Typo Fixes
TEST_F(MVPValidationTest, TypoSuite) {
    std::vector<std::pair<std::string, std::string>> cases = {
        {"retun 0;", "return 0;"},
        {"if(1) { break; } esle { }", "if(1) { break; } else { }"},
        {"forr(;;)", "for(;;)"},
        {"whlie(1)", "while(1)"},
        {"sturct Point", "struct Point"}
    };
    for (auto& c : cases) {
        auto fix = getFirstFix("syntax-keyword-typo", "typo", c.first, 1);
        EXPECT_EQ(fix.deltas[0].new_content, c.second);
        EXPECT_FLOAT_EQ(fix.confidence, 0.85f);
    }
}

// 46-50: Malformed Preprocessor & Literals
TEST_F(MVPValidationTest, PreprocessorLiteralSuite) {
    // Malformed Include Typo
    auto fix1 = getFirstFix("syntax-malformed-preprocessor", "invalid directive", "#includ <stdio.h>", 1);
    EXPECT_EQ(fix1.deltas[0].new_content, "#include <stdio.h>");

    // Missing delimiter
    auto fix2 = getFirstFix("syntax-malformed-preprocessor", "expects <FILENAME>", "#include stdio.h", 1);
    EXPECT_EQ(fix2.deltas[0].new_content, "#include <stdio.h>");

    // Unclosed bracket
    auto fix3 = getFirstFix("syntax-malformed-preprocessor", "missing >", "#include <stdio.h", 1);
    EXPECT_EQ(fix3.deltas[0].new_content, "#include <stdio.h>");

    // Unclosed string literal
    auto fix4 = getFirstFix("syntax-unclosed-literal", "missing \"", "char* s = \"hello;", 1);
    EXPECT_EQ(fix4.deltas[0].new_content, "char* s = \"hello;\"");

    // Unclosed char literal
    auto fix5 = getFirstFix("syntax-unclosed-literal", "missing '", "char c = 'a;", 1);
    EXPECT_EQ(fix5.deltas[0].new_content, "char c = 'a;'");
}

// 51-55: Advanced Preprocessor (Capitalization)
TEST_F(MVPValidationTest, AdvancedPreprocessorSuite) {
    auto fix = getFirstFix("syntax-malformed-preprocessor", "invalid directive", "#INCLUDE <stdio.h>", 1);
    EXPECT_EQ(fix.deltas[0].new_content, "#include <stdio.h>");
    
    auto fix2 = getFirstFix("syntax-malformed-preprocessor", "invalid directive", "  #InClUdE <math.h>", 1);
    EXPECT_EQ(fix2.deltas[0].new_content, "  #include <math.h>");
}