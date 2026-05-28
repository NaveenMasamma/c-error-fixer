#include "error_pattern_db.h"
#include <fstream>

ErrorPatternDB::ErrorPatternDB() {
    initializePatterns();
    // attempt to load external function->header mappings
    loadFunctionHeaderMap();
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
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-expected-opening";
        fix.fix_description = "Insert missing opening token";
        fix.fix_type = "syntax_fix";
        fix.suggested_includes = {};
        error_database["syntax-expected-opening"].push_back(fix);
    }
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-keyword-typo";
        fix.fix_description = "Fix keyword typo";
        fix.fix_type = "syntax_fix";
        fix.suggested_includes = {};
        error_database["syntax-keyword-typo"].push_back(fix);
    }
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-unclosed-literal";
        fix.fix_description = "Close unterminated string or character literal";
        fix.fix_type = "syntax_fix";
        fix.suggested_includes = {};
        error_database["syntax-unclosed-literal"].push_back(fix);
    }
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-malformed-preprocessor";
        fix.fix_description = "Fix malformed preprocessor directive";
        fix.fix_type = "syntax_fix";
        fix.suggested_includes = {};
        error_database["syntax-malformed-preprocessor"].push_back(fix);
    }
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-expected-semicolon";
        fix.fix_description = "Insert missing semicolon";
        fix.fix_type = "syntax_fix";
        fix.suggested_includes = {};
        error_database["syntax-expected-semicolon"].push_back(fix);
    }
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-expected-comma";
        fix.fix_description = "Insert missing comma";
        fix.fix_type = "syntax_fix";
        fix.suggested_includes = {};
        error_database["syntax-expected-comma"].push_back(fix);
    }
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-expected-paren";
        fix.fix_description = "Insert missing parenthesis";
        fix.fix_type = "syntax_fix";
        fix.suggested_includes = {};
        error_database["syntax-expected-paren"].push_back(fix);
    }
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-expected-brace";
        fix.fix_description = "Insert missing closing brace or bracket";
        fix.fix_type = "syntax_fix";
        fix.suggested_includes = {};
        error_database["syntax-expected-brace"].push_back(fix);
    }
    {
        ErrorFix fix;
        fix.error_pattern = "syntax-expected-colon";
        fix.fix_description = "Insert missing colon";
        fix.fix_type = "syntax_fix";
        error_database["syntax-expected-colon"].push_back(fix);
    }
}

void ErrorPatternDB::loadFunctionHeaderMap(const std::string& path) {
    function_header_map.clear();
    std::ifstream in(path);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        // skip comments and blank lines
        if (line.empty()) continue;
        if (line.size() > 0 && line[0] == '#') continue;

        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string func = line.substr(0, pos);
        std::string header = line.substr(pos + 1);
        // trim
        auto trim = [](std::string &s){
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            if (a == std::string::npos) { s = ""; return; }
            s = s.substr(a, b - a + 1);
        };
        trim(func);
        trim(header);
        if (!func.empty() && !header.empty()) {
            function_header_map[func] = header;
        }
    }
}

std::string ErrorPatternDB::lookupHeaderForFunction(const std::string& function_name) const {
    auto it = function_header_map.find(function_name);
    if (it != function_header_map.end()) return it->second;
    return std::string();
}

std::vector<ErrorFix> ErrorPatternDB::getSuggestions(const std::string& error_code,
                                                     const std::string& error_message) {
    (void)error_message;
    if (error_database.find(error_code) != error_database.end()) {
        return error_database[error_code];
    }
    return {};
}

bool ErrorPatternDB::hasPatternFor(const std::string& error_code) const {
    return error_database.find(error_code) != error_database.end();
}
