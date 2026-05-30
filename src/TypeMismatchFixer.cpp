#include "code_fixer.h"
#include "TypeMismatchFixer.h"
#include "utils.h"
#include <regex>

bool TypeMismatchFixer::canHandle(const std::string& error_code) const {
    return error_code == "incompatible-pointer-types" || 
           error_code == "int-conversion";
}

std::vector<FixSuggestion> TypeMismatchFixer::generateSuggestions(const CompilerError& error, 
                                                                 const std::vector<std::string>& lines, 
                                                                 CodeAnalyzer& analyzer, 
                                                                 ErrorPatternDB& db) {
    (void)analyzer;
    (void)db;
    std::vector<FixSuggestion> suggestions;
    if (error.line_number < 1 || error.line_number > (int)lines.size()) return suggestions;

    std::string line = lines[error.line_number - 1];
    std::string msg = Utils::toLower(error.error_message);

    std::regex missing_call_paren(R"(\b[a-zA-Z_][a-zA-Z0-9_]*\s+["'])");
    if (std::regex_search(line, missing_call_paren)) {
        return suggestions;
    }
    
    // Basic identification of the token at the error column
    int col = error.column - 1;
    if (col < 0 || col >= (int)line.length()) return suggestions;

    // Find the identifier boundaries
    size_t start = line.find_last_of(" \t(,=+-*/<>!&|[]{}", col);
    if (start == std::string::npos) start = 0; else start++;
    size_t end = line.find_first_of(" \t),;=+-*/<>!&|[]{}()\"'", col);
    if (end == std::string::npos) end = line.length();
    
    if (end <= start) return suggestions;

    std::string var = line.substr(start, end - start);
    if (var.empty()) return suggestions;

    bool is_function_call = false;
    size_t next_char = line.find_first_not_of(" \t", end);
    if (next_char != std::string::npos && line[next_char] == '(') {
        is_function_call = true;
    }

    FixSuggestion s;
    s.error = error;
    s.fix.fix_type = "type_fix";
    s.is_safe = false; // Type fixes usually require manual review
    s.confidence = 0.7f;

    CodeDelta delta;
    delta.line_number = error.line_number;
    delta.old_content = line;

    // Logic: pointer expected vs integer expected
    if (msg.find("pointer from integer") != std::string::npos || 
        (msg.find("int *") != std::string::npos && msg.find("'int'") != std::string::npos)) {
        
        if (is_function_call) return suggestions;
        if (std::isdigit(var[0]) || var[0] == '\'') return suggestions;

        delta.new_content = line;
        delta.new_content.replace(start, var.length(), "&" + var);
        s.fix.fix_description = "Pass address of '" + var + "'";
        s.explanation = "The compiler expected a memory address (pointer) but found a regular value.";
        s.reason = "Using '&' retrieves the address of the variable, matching the expected pointer type.";
        s.deltas.push_back(delta);
        suggestions.push_back(s);
    } 
    else if (msg.find("integer from pointer") != std::string::npos || 
             (msg.find("'int'") != std::string::npos && msg.find("int *") != std::string::npos)) {
        
        if (std::isdigit(var[0]) || var[0] == '\'') return suggestions;

        delta.new_content = line;
        delta.new_content.replace(start, var.length(), "*" + var);
        s.fix.fix_description = "Dereference pointer '" + var + "'";
        s.explanation = "The compiler expected a regular value but found a pointer (address).";
        s.reason = "Using '*' accesses the actual value stored at the memory address.";
        s.deltas.push_back(delta);
        suggestions.push_back(s);
    }

    return suggestions;
}
