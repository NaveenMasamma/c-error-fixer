#include "error_pattern_db.h"

ErrorPatternDB::ErrorPatternDB() {
    initializePatterns();
}

void ErrorPatternDB::initializePatterns() {
    addCommonErrors();
}

void ErrorPatternDB::addCommonErrors() {
    // Missing includes
    {
        ErrorFix fix;
        fix.error_pattern = "missing-include";
        fix.fix_description = "Add missing #include directive";
        fix.fix_type = "include_header";
        fix.suggested_includes = {"stdio.h", "stdlib.h", "string.h", "math.h", "time.h"};
        error_database["missing-include"].push_back(fix);
    }
    
    // Implicit function declaration
    {
        ErrorFix fix;
        fix.error_pattern = "implicit-function-declaration";
        fix.fix_description = "Add function declaration or include appropriate header";
        fix.fix_type = "declare_function";
        fix.suggested_includes = {"stdio.h", "stdlib.h", "string.h", "math.h"};
        error_database["implicit-function-declaration"].push_back(fix);
    }
    
    // Undefined reference
    {
        ErrorFix fix;
        fix.error_pattern = "undefined-reference";
        fix.fix_description = "Add function implementation or link against required library";
        fix.fix_type = "link_library";
        fix.suggested_includes = {};
        error_database["undefined-reference"].push_back(fix);
    }
    
    // Undeclared identifier
    {
        ErrorFix fix;
        fix.error_pattern = "undeclared-identifier";
        fix.fix_description = "Declare the variable before use";
        fix.fix_type = "declare_variable";
        fix.suggested_includes = {};
        error_database["undeclared-identifier"].push_back(fix);
    }
    
    // Syntax errors
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-error";
        fix.fix_description = "Fix syntax error (missing semicolon, bracket, etc.)";
        fix.fix_type = "syntax_fix";
        fix.suggested_includes = {};
        error_database["syntax-error"].push_back(fix);
    }
}

std::vector<ErrorFix> ErrorPatternDB::getSuggestions(const std::string& error_code,
                                                     const std::string& error_message) {
    if (error_database.find(error_code) != error_database.end()) {
        return error_database[error_code];
    }
    return {};
}

bool ErrorPatternDB::hasPatternFor(const std::string& error_code) const {
    return error_database.find(error_code) != error_database.end();
}
