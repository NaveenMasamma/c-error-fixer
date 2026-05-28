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
            error.error_code = extractErrorCode(error.error_message);
            
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

std::string CompilerErrorParser::extractErrorCode(const std::string& message) {
    // Extract error code from message like "undefined reference to `printf'"
    // or "'stdio.h' file not found"
    // Common patterns: implicit-function-declaration, undefined-reference, etc.
    
    if (message.find("implicit") != std::string::npos && 
        message.find("function") != std::string::npos) {
        return "implicit-function-declaration";
    }
    
    if (message.find("undefined reference") != std::string::npos) {
        return "undefined-reference";
    }
    
    if (message.find("file not found") != std::string::npos) {
        return "missing-include";
    }
    
    if (message.find("undeclared") != std::string::npos) {
        return "undeclared-identifier";
    }
    
    if (message.find("expected") != std::string::npos) {
        return "syntax-error";
    }
    
    return "unknown-error";
}
