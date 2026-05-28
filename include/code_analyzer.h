#ifndef CODE_ANALYZER_H
#define CODE_ANALYZER_H

#include <string>
#include <vector>
#include <map>

class CodeAnalyzer {
public:
    CodeAnalyzer();
    
    // Parse C file to extract information
    void analyzeFile(const std::string& file_path);
    
    // Get all included headers
    std::vector<std::string> getIncludes() const;
    
    // Get all declared functions
    std::vector<std::string> getDeclaredFunctions() const;
    
    // Check if header is already included
    bool isHeaderIncluded(const std::string& header) const;
    
    // Get standard C headers
    static std::vector<std::string> getStandardHeaders();
    
private:
    std::vector<std::string> includes;
    std::vector<std::string> functions;
    std::map<std::string, int> function_definitions;
};

#endif // CODE_ANALYZER_H
