#include "utils.h"

namespace Utils {

    int levenshteinDistance(const std::string& s1, const std::string& s2) {
        const int len1 = s1.length();
        const int len2 = s2.length();
        std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));

        for (int i = 0; i <= len1; ++i) dp[i][0] = i;
        for (int j = 0; j <= len2; ++j) dp[0][j] = j;

        for (int i = 1; i <= len1; ++i) {
            for (int j = 1; j <= len2; ++j) {
                int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
                dp[i][j] = std::min({dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost});
            }
        }
        return dp[len1][len2];
    }

    std::string trimWhitespace(std::string value) {
        const char* whitespace = " \t\r\n";
        size_t start = value.find_first_not_of(whitespace);
        size_t end = value.find_last_not_of(whitespace);
        if (start == std::string::npos || end == std::string::npos) return "";
        return value.substr(start, end - start + 1);
    }

    std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }

    // Define the error code patterns here, ordered by priority (more specific first)
    const std::vector<ErrorCodePattern>& getErrorCodePatterns() {
        static const std::vector<ErrorCodePattern> patterns = {
            // Unclosed literals (very specific message)
            {std::regex(R"(missing terminating (['"])|unterminated (?:string|character) literal)"), "syntax-unclosed-literal", 95},

            // Malformed preprocessor directives (specific keywords)
            {std::regex(R"(#includ[e]?\s*<[^>]*$|#includ[e]?\s*\"[^\"]*$)"), "syntax-malformed-preprocessor", 90}, // Unclosed include
            {std::regex(R"(#includ[e]?\s*([a-zA-Z_][a-zA-Z0-9_]*))"), "syntax-malformed-preprocessor", 90}, // Typo in include directive

            // Implicit function declaration
            {std::regex(R"(implicit declaration of function '([a-zA-Z_][a-zA-Z0-9_]*)')"), "implicit-function-declaration", 85},

            // Undefined reference
            {std::regex(R"(undefined reference to `([a-zA-Z_][a-zA-Z0-9_]*)')"), "undefined-reference", 80},

            // Missing includes (file not found)
            {std::regex(R"(no such file or directory: '([^']+)'|'([^']+)' file not found)"), "missing-include", 75},

            // Undeclared identifier
            {std::regex(R"('([a-zA-Z_][a-zA-Z0-9_]*)' undeclared|use of undeclared identifier '([a-zA-Z_][a-zA-Z0-9_]*)'|has no member named '([a-zA-Z_][a-zA-Z0-9_]*)')"), "undeclared-identifier", 70},

            // Expected specific tokens (more specific than generic syntax error)
            {std::regex(R"(expected ';' (?:before|after))"), "syntax-expected-semicolon", 65},
            {std::regex(R"(expected ',' (?:before|after))"), "syntax-expected-comma", 65},
            {std::regex(R"(expected '\)' (?:before|after)|missing '\)')"), "syntax-expected-paren", 65},
            {std::regex(R"(expected '\]' (?:before|after)|missing '\]')"), "syntax-expected-brace", 65},
            {std::regex(R"(expected '\}' (?:before|after)|missing '\}')"), "syntax-expected-brace", 65},
            {std::regex(R"(expected '\(' (?:before|after))"), "syntax-expected-opening", 65},
            {std::regex(R"(expected '\[' (?:before|after))"), "syntax-expected-opening", 65},
            {std::regex(R"(expected '\{' (?:before|after))"), "syntax-expected-opening", 65},
            {std::regex(R"(expected ':' (?:before|after))"), "syntax-expected-colon", 65}, // Added colon

            // Generic "expected" syntax errors
            {std::regex(R"(expected [^ ]+)"), "syntax-error", 50},

            // Catch-all for other syntax errors
            {std::regex(R"(syntax error|parse error)"), "syntax-error", 40},

            // Fallback for anything else
            {std::regex(R"(.+)"), "unknown-error", 0} // Lowest priority, matches anything
        };
        return patterns;
    }

} // namespace Utils