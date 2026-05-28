#include "compiler_error_parser.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>

CompilerErrorParser::CompilerErrorParser() {
    // GCC error pattern: file.c:10:5: error: message
    gcc_error_pattern = std::regex(R"(([^:]+):(\d+):(\d+):\s*(error|warning):\s*(.+))");
}

bool CompilerErrorParser::compileFile(const std::string& c_file_path, std::string& compiler_output) {
    // Compile with GCC and capture all output
    std::string command = "gcc -Wall -Wextra -fno-builtin " + c_file_path + " 2>&1";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return false;
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        compiler_output += buffer;
    }
    
    int status = pclose(pipe);
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
            error.error_type = match[4].str();
            error.line_number = std::stoi(match[2].str());
            error.column = std::stoi(match[3].str());
            error.error_message = match[5].str();
            // populate the source line for better context
            error.full_line = getSourceLine(match[1].str(), error.line_number);
            error.error_code = extractErrorCode(error.error_message, error.full_line);
            
            errors.push_back(error);
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

static int levenshteinDistance(const std::string& a, const std::string& b) {
    std::vector<std::vector<int>> dp(a.size() + 1, std::vector<int>(b.size() + 1));
    for (int i = 0; i <= static_cast<int>(a.size()); ++i) dp[i][0] = i;
    for (int j = 0; j <= static_cast<int>(b.size()); ++j) dp[0][j] = j;
    for (int i = 1; i <= static_cast<int>(a.size()); ++i) {
        for (int j = 1; j <= static_cast<int>(b.size()); ++j) {
            int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            dp[i][j] = std::min({dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost});
        }
    }
    return dp[a.size()][b.size()];
}

static std::string findKeywordTypo(const std::string& source_line) {
    static const std::vector<std::string> c_keywords = {
        "auto", "break", "case", "char", "const", "continue", "default", "do", "double",
        "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int",
        "long", "register", "restrict", "return", "short", "signed", "sizeof", "static",
        "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"
    };

    std::regex token_regex(R"(([a-zA-Z_][a-zA-Z0-9_]*))");
    std::sregex_iterator it(source_line.begin(), source_line.end(), token_regex);
    std::sregex_iterator end;
    std::string best_match;
    int best_distance = 2;

    for (; it != end; ++it) {
        std::string token = it->str();
        for (const auto& keyword : c_keywords) {
            if (token == keyword) break;
            int dist = levenshteinDistance(token, keyword);
            if (dist < best_distance) {
                best_distance = dist;
                best_match = keyword;
            }
        }
    }

    return best_match;
}

static std::string trimWhitespace(std::string value) {
    const char* whitespace = " \t\r\n";
    size_t start = value.find_first_not_of(whitespace);
    size_t end = value.find_last_not_of(whitespace);
    if (start == std::string::npos || end == std::string::npos) return "";
    return value.substr(start, end - start + 1);
}

std::string CompilerErrorParser::extractErrorCode(const std::string& message,
                                                 const std::string& source_line) {
    // Extract error code from message like "undefined reference to `printf'"
    // or "'stdio.h' file not found". This also handles keyword typos,
    // malformed preprocessing directives, and unterminated literals.

    std::regex code_regex(R"(\[-Werror=([^\]]+)\]|\[-W([^\]]+)\])");
    std::smatch match;
    if (std::regex_search(message, match, code_regex)) {
        return match[1].matched ? match[1].str() : match[2].str();
    }

    std::string trimmed_line = trimWhitespace(source_line);
    std::string lower_message = message;
    std::transform(lower_message.begin(), lower_message.end(), lower_message.begin(), ::tolower);

    if (lower_message.find("implicit declaration of function") != std::string::npos) {
        return "implicit-function-declaration";
    }
    if (lower_message.find("undefined reference") != std::string::npos) {
        return "undefined-reference";
    }
    if (lower_message.find("no such file or directory") != std::string::npos || 
        lower_message.find("file not found") != std::string::npos) {
        return "missing-include";
    }
    if (lower_message.find("undeclared") != std::string::npos || 
        lower_message.find("has no member named") != std::string::npos) {
        return "undeclared-identifier";
    }
    if (lower_message.find("missing terminating character") != std::string::npos ||
        lower_message.find("unterminated") != std::string::npos) {
        return "syntax-unclosed-literal";
    }

    if (trimmed_line.rfind("#includ", 0) == 0 ||
        (trimmed_line.rfind("#include", 0) == 0 &&
         trimmed_line.find("<") == std::string::npos &&
         trimmed_line.find("\"") == std::string::npos)) {
        return "syntax-malformed-preprocessor";
    }

    if (lower_message.find("expected '(' before") != std::string::npos ||
        lower_message.find("expected '[' before") != std::string::npos ||
        lower_message.find("expected '{' before") != std::string::npos) {
        return "syntax-expected-opening";
    }

    std::string keyword_suggestion = findKeywordTypo(trimmed_line);
    if (!keyword_suggestion.empty() && lower_message.find("expected") != std::string::npos) {
        return "syntax-keyword-typo";
    }

    if (lower_message.find("expected ';'") != std::string::npos ||
        lower_message.find("expected ';' before") != std::string::npos ||
        lower_message.find("expected ':'") != std::string::npos) {
        return "syntax-expected-semicolon";
    }
    if (lower_message.find("expected ','") != std::string::npos ||
        lower_message.find("expected ',' before") != std::string::npos) {
        return "syntax-expected-comma";
    }
    if (lower_message.find("expected ')'") != std::string::npos ||
        lower_message.find("missing )") != std::string::npos) {
        return "syntax-expected-paren";
    }
    if (lower_message.find("expected '}'") != std::string::npos ||
        lower_message.find("expected ']'") != std::string::npos) {
        return "syntax-expected-brace";
    }
    if (lower_message.find("expected") != std::string::npos) {
        return "syntax-error";
    }
    
    return "unknown-error";
}
