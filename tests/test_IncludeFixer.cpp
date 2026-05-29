#include <gtest/gtest.h>
#include "IncludeFixer.h"
#include "code_analyzer.h"
#include "error_pattern_db.h"

TEST(IncludeFixerTest, HandlesImplicitDeclaration) {
    IncludeFixer fixer;
    EXPECT_TRUE(fixer.canHandle("implicit-function-declaration"));
    EXPECT_TRUE(fixer.canHandle("missing-include"));
}

TEST(IncludeFixerTest, SuggestsHeaderForPrintf) {
    IncludeFixer fixer;
    CompilerError err;
    err.error_code = "implicit-function-declaration";
    err.error_message = "implicit declaration of function 'printf'";
    err.line_number = 1;

    std::vector<std::string> lines = {"int main() { printf(\"hi\"); }"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db; // Uses default mapping for printf -> stdio.h

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);
    
    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].fix.suggested_includes[0], "stdio.h");
    EXPECT_GT(suggestions[0].confidence, 0.8f);
    EXPECT_EQ(suggestions[0].deltas[0].line_number, 1);
    EXPECT_NE(suggestions[0].deltas[0].new_content.find("#include <stdio.h>"), std::string::npos);
}

TEST(IncludeFixerTest, PreservesLicenseHeaders) {
    IncludeFixer fixer;
    CompilerError err;
    err.error_code = "implicit-function-declaration";
    err.error_message = "implicit declaration of function 'printf'";
    err.line_number = 5;

    std::vector<std::string> lines = {
        "/*",
        " * License Header",
        " */",
        "",
        "int main() { printf(\"hi\"); }"
    };
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);
    
    ASSERT_FALSE(suggestions.empty());
    // Insert at line 5 (start of code line)
    EXPECT_EQ(suggestions[0].deltas[0].line_number, 5);
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "#include <stdio.h>\nint main() { printf(\"hi\"); }");
}

TEST(IncludeFixerTest, InsertsAlongsideExistingIncludes) {
    IncludeFixer fixer;
    CompilerError err;
    err.error_code = "implicit-function-declaration";
    err.error_message = "implicit declaration of function 'printf'";
    err.line_number = 3;

    std::vector<std::string> lines = {
        "/* License Header */",
        "#include <stdlib.h>",
        "void main() { printf(\"hi\"); }"
    };
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);
    
    ASSERT_FALSE(suggestions.empty());
    // Prepends to the include block at line 2
    EXPECT_EQ(suggestions[0].deltas[0].line_number, 2);
    EXPECT_EQ(suggestions[0].deltas[0].new_content, "#include <stdio.h>\n#include <stdlib.h>");
}

TEST(IncludeFixerTest, DoesNotSuggestDuplicateHeader) {
    IncludeFixer fixer;
    CompilerError err;
    err.error_code = "implicit-function-declaration";
    err.error_message = "implicit declaration of function 'printf'";

    std::vector<std::string> lines = {"#include <stdio.h>", "void f() { printf(\"hi\"); }"};
    CodeAnalyzer analyzer;
}