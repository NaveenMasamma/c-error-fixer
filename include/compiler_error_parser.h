#ifndef COMPILER_ERROR_PARSER_H
#define COMPILER_ERROR_PARSER_H

#include <string>
#include <vector>
#include <regex>
#include "utils.h" // Include the new utility header

struct CompilerError {
    std::string file_path;
    int line_number;
    int column;
    std::string error_type;      // e.g., "error", "warning"
    std::string error_code;      // e.g., "implicit-function-declaration"
    std::string error_message;
    std::string full_line;       // The actual code line with error
};

class CompilerErrorParser {
public:
    CompilerErrorParser();
    
    // Compile C file and capture errors
    bool compileFile(const std::string& c_file_path, std::string& compiler_output);
    
    // Parse compiler output and extract errors
    std::vector<CompilerError> parseErrors(const std::string& compiler_output);
    
    // Get the line of code from source file
    std::string getSourceLine(const std::string& file_path, int line_number);
    
private:
    std::regex gcc_error_pattern;
    std::regex gcc_error_code_pattern;
    
    // Try to extract error code from gcc error message and source context
    std::string extractErrorCode(const std::string& message, const std::string& source_line);
};

#endif // COMPILER_ERROR_PARSER_H
