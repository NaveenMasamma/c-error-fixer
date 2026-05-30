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

TEST(IncludeFixerTest, SuggestsCtypeHeaderForIsUpper) {
    IncludeFixer fixer;
    CompilerError err;
    err.error_code = "implicit-function-declaration";
    err.line_number = 3;
    err.error_message = "implicit declaration of function 'isupper'";
    err.full_line = "    int is_upper = isupper(c);";

    std::vector<std::string> lines = {
        "int main() {",
        "    char c = 'A';",
        "    int is_upper = isupper(c);",
        "    return 0;",
        "}"
    };
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions.size(), 1); // Should only suggest the exact match, not generics
    bool found_ctype = false;
    for (const auto& s : suggestions) {
        for (const auto& delta : s.deltas) {
            if (delta.new_content.find("#include <ctype.h>") != std::string::npos) {
                found_ctype = true;
            }
        }
    }
    EXPECT_TRUE(found_ctype);
}

void runCompilerSuggestionTest(const std::string& func, const std::string& header) {
    IncludeFixer fixer;
    CompilerError err;
    err.error_code = "implicit-function-declaration";
    err.line_number = 2;
    err.error_message = "implicit declaration of function '" + func + "'\nnote: '" + func + "' is defined in header '<" + header + ">'; did you forget to '#include <" + header + ">'?\n[SUGGESTED_HEADER: " + header + "]";
    err.full_line = "    " + func + "();";

    std::vector<std::string> lines = {
        "int main() {",
        "    " + func + "();",
        "}"
    };
    CodeAnalyzer analyzer;
    ErrorPatternDB db; // No explicit mapping loaded, purely testing compiler notes

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);

    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions[0].fix.suggested_includes[0], header);
    EXPECT_FALSE(suggestions[0].deltas.empty());
    EXPECT_NE(suggestions[0].deltas[0].new_content.find("#include <" + header + ">"), std::string::npos);
}

TEST(IncludeFixerTest, UsesCompilerSuggestionRegression) {
    runCompilerSuggestionTest("assert", "assert.h");
    runCompilerSuggestionTest("printf", "stdio.h");
    runCompilerSuggestionTest("malloc", "stdlib.h");
    runCompilerSuggestionTest("free", "stdlib.h");
    runCompilerSuggestionTest("memcpy", "string.h");
    runCompilerSuggestionTest("isupper", "ctype.h");
    runCompilerSuggestionTest("sqrt", "math.h");
}

TEST(IncludeFixerTest, DoesNotInsertRandomHeaders) {
    IncludeFixer fixer;
    CompilerError err;
    err.error_code = "implicit-function-declaration";
    err.error_message = "implicit declaration of function 'unknown_func'";
    err.full_line = "    unknown_func();";
    err.line_number = 2;

    std::vector<std::string> lines = {"int main() {", "    unknown_func();", "}"};
    CodeAnalyzer analyzer;
    ErrorPatternDB db;

    auto suggestions = fixer.generateSuggestions(err, lines, analyzer, db);
    
    ASSERT_FALSE(suggestions.empty());
    EXPECT_TRUE(suggestions[0].deltas.empty()); // No deltas -> suggestion only, no random headers
    EXPECT_EQ(suggestions[0].confidence, 0.0f);
}