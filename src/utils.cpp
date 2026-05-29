#include "utils.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace Utils {

    int levenshteinDistance(const std::string& s1, const std::string& s2) {
        const int len1 = s1.length();
        const int len2 = s2.length();
        
        // Optimize space to O(min(N, M))
        if (len1 < len2) return levenshteinDistance(s2, s1);

        std::vector<int> prev(len2 + 1);
        std::vector<int> curr(len2 + 1);

        for (int j = 0; j <= len2; ++j) prev[j] = j;

        for (int i = 1; i <= len1; ++i) {
            curr[0] = i;
            for (int j = 1; j <= len2; ++j) {
                int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
                curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
            }
            prev = curr;
        }
        return prev[len2];
    }

    std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }

    std::vector<std::string> readFile(const std::string& file_path) {
        namespace fs = std::filesystem;
        std::vector<std::string> lines;
        std::error_code ec;

        if (!fs::exists(file_path, ec)) {
            std::cerr << Color::RED << "[Error] Missing File: " << file_path << " - No such file." << Color::RESET << std::endl;
            return lines;
        }

        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << Color::RED << "[Error] Permission Denied: " << file_path << " - Cannot open for reading." << Color::RESET << std::endl;
            return lines;
        }

        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    bool writeFile(const std::string& file_path, const std::vector<std::string>& lines) {
        namespace fs = std::filesystem;
        std::error_code ec;

        if (fs::exists(file_path, ec)) {
            auto status = fs::status(file_path, ec);
            if ((status.permissions() & fs::perms::owner_write) == fs::perms::none) {
                std::cerr << Color::RED << "[Error] Permission Denied: " << file_path << " - File is read-only." << Color::RESET << std::endl;
                return false;
            }
        }

        std::ofstream file(file_path);
        if (!file.is_open()) {
            std::cerr << Color::RED << "[Error] Write Failed: " << file_path << " - Unable to create or open file." << Color::RESET << std::endl;
            return false;
        }

        for (const auto& line : lines) {
            file << line << "\n";
        }
        return true;
    }

    bool backupFile(const std::string& file_path) {
        namespace fs = std::filesystem;
        std::string backup_path = file_path + ".backup";
        std::error_code ec;

        if (fs::exists(backup_path, ec) && !ec) {
            auto now = std::time(nullptr);
            std::tm tm;
#if defined(_WIN32)
            localtime_s(&tm, &now);
#else
            localtime_r(&now, &tm);
#endif
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
            backup_path = file_path + ".backup." + oss.str();
            std::cout << Color::YELLOW << "Existing backup found. Creating timestamped backup: "
                      << backup_path << Color::RESET << std::endl;
        }

        if (!fs::copy_file(file_path, backup_path, fs::copy_options::overwrite_existing, ec)) {
            std::cerr << Color::RED << "[Error] Backup Failed: " << file_path << " - " << ec.message() << Color::RESET << std::endl;
            return false;
        }

        std::cout << Color::GREEN << "Backup created: " << backup_path << Color::RESET << std::endl;
        return true;
    }

    std::string trim(const std::string& s) {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (std::string::npos == first) return s;
        size_t last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, (last - first + 1));
    }

    std::vector<std::string> collectSourceFiles(const std::vector<std::string>& paths) {
        std::vector<std::string> files;
        namespace fs = std::filesystem;

        for (const auto& path : paths) {
            std::error_code ec;
            if (!fs::exists(path, ec)) continue;

            if (fs::is_directory(path, ec)) {
                // Added skip_permission_denied and follow_symlinks safety
                for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".c") {
                        files.push_back(entry.path().string());
                    }
                }
            } else if (std::filesystem::is_regular_file(path)) {
                files.push_back(path);
            }
        }
        // Remove duplicates and sort
        std::sort(files.begin(), files.end());
        files.erase(std::unique(files.begin(), files.end()), files.end());
        return files;
    }

    // Define the error code patterns here, ordered by priority (more specific first)
    const std::vector<ErrorCodePattern>& getErrorCodePatterns() {
        static const std::vector<ErrorCodePattern> patterns = {
            // Missing closing brace at EOF
            {std::regex(R"(expected [\}'‘“’”]+ at end of input|expected declaration or statement at end of input)"), "syntax-expected-brace-eof", 68},

            // Type mismatch patterns
            {std::regex(R"(incompatible [a-zA-Z ]+ types|incompatible types when assigning)"), "incompatible-pointer-types", 88},
            {std::regex(R"(makes (?:pointer|integer) from (?:integer|pointer) without a cast)"), "int-conversion", 87},

            // Unclosed literals (very specific message)
            {std::regex(R"(missing terminating [‘'“"’”]|unterminated (?:string|character) literal)"), "syntax-unclosed-literal", 95},

            // Malformed preprocessor directives (specific keywords)
            {std::regex(R"(#includ[e]?\s*<[^>]*$|#includ[e]?\s*\"[^\"]*$)"), "syntax-malformed-preprocessor", 90}, // Unclosed include
            {std::regex(R"(#includ[e]?\s*([a-zA-Z_][a-zA-Z0-9_]*))"), "syntax-malformed-preprocessor", 90}, 

            // Implicit function declaration
            {std::regex(R"(implicit declaration of function [‘'“"']([a-zA-Z_][a-zA-Z0-9_]*)[’'”"'])"), "implicit-function-declaration", 85},

            // Undefined reference
            {std::regex(R"(undefined reference to `([a-zA-Z_][a-zA-Z0-9_]*)')"), "undefined-reference", 80},

            // Missing includes (file not found)
            {std::regex(R"(no such file or directory: '([^']+)'|'([^']+)' file not found)"), "missing-include", 75},

            // Undeclared identifier
            {std::regex(R"([‘'“"]([a-zA-Z_][a-zA-Z0-9_]*)[’'”"] undeclared|use of undeclared identifier [‘'“"]([a-zA-Z_][a-zA-Z0-9_]*)[’'”"]|has no member named [‘'“"]([a-zA-Z_][a-zA-Z0-9_]*)[’'”"])"), "undeclared-identifier", 70},

            // Expected specific tokens (more specific than generic syntax error)
            {std::regex(R"(expected [;‘“’”']+ (?:before|after))"), "syntax-expected-semicolon", 65},
            {std::regex(R"(expected [,‘“’”']+ (?:before|after))"), "syntax-expected-comma", 65},
            {std::regex(R"(expected [\)'‘“’”]+ (?:before|after)|missing [\)'‘“’”]+)"), "syntax-expected-paren", 65},
            {std::regex(R"(expected [\]'‘“’”]+ (?:before|after)|missing [\]'‘“’”]+)"), "syntax-expected-brace", 65},
            {std::regex(R"(expected [\}'‘“’”]+ (?:before|after)|missing [\}'‘“’”]+)"), "syntax-expected-brace", 65},
            {std::regex(R"(expected [\('‘“’”]+ (?:before|after))"), "syntax-expected-opening", 65},
            {std::regex(R"(expected [\['‘“’”]+ (?:before|after))"), "syntax-expected-opening", 65},
            {std::regex(R"(expected [\{'‘“’”]+ (?:before|after))"), "syntax-expected-opening", 65},
            {std::regex(R"(expected [:'‘“’”]+ (?:before|after))"), "syntax-expected-colon", 65},

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