#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <algorithm>
#include <regex>

namespace Utils {
    namespace Color {
        inline const std::string RESET  = "\033[0m";
        inline const std::string BOLD   = "\033[1m";
        inline const std::string RED    = "\033[31m";
        inline const std::string GREEN  = "\033[32m";
        inline const std::string YELLOW = "\033[33m";
        inline const std::string BLUE   = "\033[34m";
        inline const std::string CYAN   = "\033[36m";
    }

    // Calculates the Levenshtein distance between two strings.
    int levenshteinDistance(const std::string& s1, const std::string& s2);

    // Converts a string to lowercase.
    std::string toLower(std::string s);

    // File I/O Helpers for Fix Engines
    std::vector<std::string> readFile(const std::string& file_path);
    bool writeFile(const std::string& file_path, const std::vector<std::string>& lines);
    bool backupFile(const std::string& file_path);
    std::string trim(const std::string& s);

    // Collects all .c files from a list of paths (files or directories)
    std::vector<std::string> collectSourceFiles(const std::vector<std::string>& paths);

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