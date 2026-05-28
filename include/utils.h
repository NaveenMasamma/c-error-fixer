#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <algorithm>
#include <regex>

namespace Utils {
    // Calculates the Levenshtein distance between two strings.
    int levenshteinDistance(const std::string& s1, const std::string& s2);

    // Trims leading and trailing whitespace from a string.
    std::string trimWhitespace(std::string value);

    // Converts a string to lowercase.
    std::string toLower(std::string s);

    // Structure to define an error pattern for extraction
    struct ErrorCodePattern {
        std::regex regex_pattern;
        std::string error_code;
        int priority; // Higher value means higher priority (checked first)
    };

    // Global list of error code patterns
    const std::vector<ErrorCodePattern>& getErrorCodePatterns();
} // namespace Utils

#endif // UTILS_H