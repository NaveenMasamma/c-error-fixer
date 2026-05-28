#ifndef ERROR_PATTERN_DB_H
#define ERROR_PATTERN_DB_H

#include <string>
#include <vector>
#include <map>

struct ErrorFix {
    std::string error_pattern;
    std::string fix_description;
    std::string fix_type;        // e.g., "include_header", "add_semicolon", "declare_function"
    std::vector<std::string> suggested_includes;
    std::string code_modification;  // Template for fix
};

class ErrorPatternDB {
public:
    ErrorPatternDB();
    
    // Get fix suggestions for an error code
    std::vector<ErrorFix> getSuggestions(const std::string& error_code, 
                                        const std::string& error_message);
    
    // Check if error is in database
    bool hasPatternFor(const std::string& error_code) const;
    
private:
    std::map<std::string, std::vector<ErrorFix>> error_database;
    
    // Initialize error patterns
    void initializePatterns();
    
    // Add common C errors
    void addCommonErrors();
};

#endif // ERROR_PATTERN_DB_H
