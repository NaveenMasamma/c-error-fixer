#ifndef CODE_FIXER_H
#define CODE_FIXER_H

#include <string>
#include <vector>
#include <memory>
#include "compiler_error_parser.h"
#include "error_pattern_db.h"
#include "IFixEngine.h"
#include "utils.h" // Include the new utility header

#include "FixSuggestion.h"

class FixCoordinator;

class CodeFixer {
public:
    CodeFixer();
    ~CodeFixer();
    
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
    std::unique_ptr<FixCoordinator> coordinator;
};

#endif // CODE_FIXER_H
