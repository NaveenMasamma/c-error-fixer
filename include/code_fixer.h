#ifndef CODE_FIXER_H
#define CODE_FIXER_H

#include <string>
#include <vector>
#include "compiler_error_parser.h"
#include "error_pattern_db.h"

struct FixSuggestion {
    CompilerError error;
    ErrorFix fix;
    std::string before;          // Code before fix
    std::string after;           // Code after fix
    bool is_safe;               // Whether fix can be auto-applied
};

class CodeFixer {
public:
    CodeFixer();
    
    // Generate fix suggestions for errors
    std::vector<FixSuggestion> generateFixes(const std::string& file_path,
                                            const std::vector<CompilerError>& errors);
    
    // Apply a specific fix to the file
    bool applyFix(const std::string& file_path, const FixSuggestion& suggestion);
    
    // Apply all fixes
    bool applyAllFixes(const std::string& file_path, 
                      const std::vector<FixSuggestion>& suggestions);
    
    // Backup original file
    bool backupFile(const std::string& file_path);
    
private:
    ErrorPatternDB pattern_db;
    
    // Specific fix implementations
    bool addIncludeHeader(const std::string& file_path, const std::string& header);
    bool addMissingSemicolon(const std::string& file_path, int line_number);
    bool declareMissingFunction(const std::string& file_path, 
                               const std::string& function_name);
    
    // File manipulation helpers
    std::vector<std::string> readFile(const std::string& file_path);
    bool writeFile(const std::string& file_path, const std::vector<std::string>& lines);
};

#endif // CODE_FIXER_H
