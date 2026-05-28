#include "code_analyzer.h"
#include <fstream>
#include <sstream>
#include <regex>

CodeAnalyzer::CodeAnalyzer() {}

void CodeAnalyzer::analyzeFile(const std::string& file_path) {
    std::ifstream file(file_path);
    std::string line;
    std::regex include_pattern(R"(#include\s*[<\"]([^>\"]+)[>\"])");
    std::regex function_pattern(R"(^\w+\s+\*?(\w+)\s*\()");
    
    while (std::getline(file, line)) {
        std::smatch match;
        
        // Extract includes
        if (std::regex_search(line, match, include_pattern)) {
            includes.push_back(match[1].str());
        }
        
        // Extract function definitions (simplified)
        if (std::regex_search(line, match, function_pattern)) {
            functions.push_back(match[1].str());
            function_definitions[match[1].str()]++;
        }
    }
}

std::vector<std::string> CodeAnalyzer::getIncludes() const {
    return includes;
}

std::vector<std::string> CodeAnalyzer::getDeclaredFunctions() const {
    return functions;
}

bool CodeAnalyzer::isHeaderIncluded(const std::string& header) const {
    for (const auto& inc : includes) {
        if (inc.find(header) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> CodeAnalyzer::getStandardHeaders() {
    return {
        "stdio.h", "stdlib.h", "string.h", "math.h", "time.h",
        "ctype.h", "errno.h", "float.h", "limits.h", "locale.h",
        "setjmp.h", "signal.h", "stdarg.h", "stddef.h", "assert.h",
        "stdint.h", "stdbool.h", "inttypes.h", "iso646.h"
    };
}
