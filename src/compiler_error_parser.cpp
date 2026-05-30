#include "compiler_error_parser.h"
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm> // For std::transform
#include <iostream>
#include <memory>

CompilerErrorParser::CompilerErrorParser() {
    // GCC error pattern: file.c:10:5: error: message
    gcc_error_pattern = std::regex(R"(([^:]+):(\d+):(\d+):\s*([a-z ]*error|warning):\s*(.+))");
    gcc_error_code_pattern = std::regex(R"(\[-Werror=([^\]]+)\]|\[-W([^\]]+)\])");
}

bool CompilerErrorParser::compileFile(const std::string& c_file_path, std::string& compiler_output) {
    // Compile with GCC and capture all output
    std::string command = "gcc -Wall -Wextra -fno-builtin " + c_file_path + " 2>&1";
    
    auto pipe = std::unique_ptr<FILE, decltype(&pclose)>(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        int err = errno;
        if (err == ENOENT) {
            compiler_output = "Compiler execution failed: gcc was not found.\nSuggested Action: Install GCC with 'sudo apt install gcc'.";
        } else if (err == EACCES) {
            compiler_output = "Compiler execution failed: permission denied while starting gcc.\nSuggested Action: Check compiler and file permissions.";
        } else {
            compiler_output = std::string("Compiler execution failed: ") + std::strerror(err) + ".\nSuggested Action: Verify that GCC is installed and accessible.";
        }
        return false;
    }
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        compiler_output += buffer;
    }
    
    return true;  // Return true even if compilation fails (we still got error messages)
}

std::vector<CompilerError> CompilerErrorParser::parseErrors(const std::string& compiler_output) {
    std::vector<CompilerError> errors;
    std::istringstream stream(compiler_output);
    std::string line;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        if (std::regex_search(line, match, gcc_error_pattern)) {
            CompilerError error;
            error.file_path = match[1].str();
            error.error_type = match[4].str();
            error.line_number = std::stoi(match[2].str());
            error.column = std::stoi(match[3].str());
            error.error_message = match[5].str();
            // populate the source line for better context
            error.full_line = getSourceLine(match[1].str(), error.line_number);
            error.error_code = extractErrorCode(error.error_message, error.full_line);
            
            errors.push_back(error);
        } else if (!errors.empty()) {
            std::regex note_pattern(R"(note:\s*(.+))");
            std::smatch note_match;
            if (std::regex_search(line, note_match, note_pattern)) {
                std::string note_text = note_match[1].str();
                errors.back().error_message += "\nnote: " + note_text;
                
                std::regex header_regex(R"(['"<]([a-zA-Z0-9_/\.]+\.h)['">])");
                std::smatch header_match;
                if (std::regex_search(note_text, header_match, header_regex)) {
                    errors.back().error_message += "\n[SUGGESTED_HEADER: " + header_match[1].str() + "]";
                }
            }
        }
    }
    
    return errors;
}

std::string CompilerErrorParser::getSourceLine(const std::string& file_path, int line_number) {
    std::ifstream file(file_path);
    if (!file.is_open()) return "";
    
    std::string line;
    int current_line = 1;
    
    while (std::getline(file, line)) {
        if (current_line == line_number) {
            return line;
        }
        current_line++;
    }
    
    return "";
}

// Include the new utility header
#include "utils.h"

// Helper function for keyword typo detection, now using Utils::levenshteinDistance
static std::string findKeywordTypo(const std::string& source_line) {
    static const std::vector<std::string> c_keywords = {
        "auto", "break", "case", "char", "const", "continue", "default", "do", "double",
        "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int",
        "long", "register", "restrict", "return", "short", "signed", "sizeof", "static",
        "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while",
        "printf", "scanf", "fprintf", "sprintf", "malloc", "calloc", "realloc", "free",
        "strlen", "strcpy", "strcmp", "fopen", "fclose", "exit", "isupper", "tolower"
    };

    static const std::vector<std::pair<std::string,std::string>> common_typos = {
        {"retun", "return"}, {"forr", "for"}, {"whlie", "while"}, {"sturct", "struct"}, {"esle", "else"},
        {"pritf", "printf"}, {"streln", "strlen"}, {"mallc", "malloc"}, {"scnaf", "scanf"}, {"retrn", "return"}
    };

    std::regex token_regex(R"(([a-zA-Z_][a-zA-Z0-9_]*))");
    std::sregex_iterator it(source_line.begin(), source_line.end(), token_regex);
    std::sregex_iterator end;
    std::string best_match;
    int best_distance = 2; // Max distance for a "typo"

    for (; it != end; ++it) {
        std::string token = it->str();
        for (const auto& p : common_typos) {
            if (token == p.first) return p.second;
        }
        for (const auto& keyword : c_keywords) {
            if (token == keyword) continue; // Skip if it's an exact keyword
            int dist = Utils::levenshteinDistance(token, keyword);
            if (dist < best_distance) {
                best_distance = dist;
                best_match = keyword;
            }
        }
    }
    return best_match;
}

std::string CompilerErrorParser::extractErrorCode(const std::string& message,
                                                 const std::string& source_line) {
    std::string trimmed_line = Utils::trim(source_line);
    std::string lower_message = Utils::toLower(message);

    // GCC can report a missing function-call parenthesis as -Wint-conversion
    // because the bare function name is treated as a pointer value.
    std::regex missing_call_paren(R"(\b[a-zA-Z_][a-zA-Z0-9_]*\s+["'])");
    if ((lower_message.find("makes integer from pointer") != std::string::npos ||
         lower_message.find("before string constant") != std::string::npos) &&
        std::regex_search(trimmed_line, missing_call_paren)) {
        return "syntax-expected-opening";
    }

    // 1. Check for compiler-specific warning/error codes (e.g., [-Werror=...])
    std::smatch match;
    if (std::regex_search(message, match, gcc_error_code_pattern)) {
        return match[1].matched ? match[1].str() : match[2].str();
    }

    // 2. Iterate through predefined error patterns by priority
    // The patterns are ordered from most specific to least specific.
    const auto& patterns = Utils::getErrorCodePatterns();
    for (const auto& pattern : patterns) {
        std::smatch pattern_match;
        if (std::regex_search(lower_message, pattern_match, pattern.regex_pattern)) {

            // Intercept implicit function and undeclared identifiers to check for typos
            if (pattern.error_code == "implicit-function-declaration" || 
                pattern.error_code == "undeclared-identifier") {
                std::string keyword_suggestion = findKeywordTypo(trimmed_line);
                if (!keyword_suggestion.empty()) {
                    return "syntax-keyword-typo";
                }
            }

            // Special handling for malformed preprocessor: check source line too
            if (pattern.error_code == "syntax-malformed-preprocessor") {
                // Ensure the source line actually starts with a malformed include-like directive
            std::string lower_line = Utils::toLower(trimmed_line);
            if (lower_line.rfind("#includ", 0) == 0) {
                    return pattern.error_code;
                }
                continue; // If source line doesn't match, continue to next pattern
            }
            
            // For other patterns, the message match is sufficient
            return pattern.error_code;
        }
    }

    // 3. Check for keyword typos if a generic "expected" error or syntax error is present
    // This is done after general regex patterns to ensure more specific syntax errors are caught first.
    if (lower_message.find("expected") != std::string::npos ||
        lower_message.find("syntax error") != std::string::npos ||
        lower_message.find("parse error") != std::string::npos ||
        lower_message.find("unknown type name") != std::string::npos ||
        lower_message.find("undeclared identifier") != std::string::npos ||
        lower_message.find("implicit declaration") != std::string::npos) {
        std::string keyword_suggestion = findKeywordTypo(trimmed_line);
        if (!keyword_suggestion.empty()) {
            return "syntax-keyword-typo";
        }
    }

    // Fallback if no specific pattern matched
    return "unknown-error";
}
