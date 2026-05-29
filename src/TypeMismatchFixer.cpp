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
    std::vector<FixSuggestion> suggestions;
    if (error.line_number < 1 || error.line_number > (int)lines.size()) return suggestions;

    std::string line = lines[error.line_number - 1];
    std::string msg = Utils::toLower(error.error_message);
    
    // Basic identification of the token at the error column
    int col = error.column - 1;
    if (col < 0 || col >= (int)line.length()) return suggestions;

    // Find the identifier boundaries
    size_t start = line.find_last_of(" \t(,=+-*/", col);
    if (start == std::string::npos) start = 0; else start++;
    size_t end = line.find_first_of(" \t),;=+-*/", col);
    if (end == std::string::npos) end = line.length();
    
    std::string var = line.substr(start, end - start);
    if (var.empty()) return suggestions;

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