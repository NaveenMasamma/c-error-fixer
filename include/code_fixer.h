#ifndef CODE_FIXER_H
#define CODE_FIXER_H

#include <string>
#include <vector>
#include "compiler_error_parser.h"
#include "error_pattern_db.h"
#include "utils.h" // Include the new utility header

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
    bool fixSyntaxError(const std::string& file_path, int line_number, 
                       const std::string& error_message, int column = 0);
    bool fixMissingComma(const std::string& file_path, int line_number,
                        const std::string& error_message);
    bool fixMissingToken(const std::string& file_path, int line_number,
                        char token, int column = 0, const std::string& error_message = "");
    bool fixMissingColon(const std::string& file_path, int line_number, const std::string& error_message);
    bool fixMalformedPreprocessor(const std::string& file_path, int line_number,
                                  const std::string& error_message);
    bool fixKeywordTypo(const std::string& file_path, int line_number,
                        const std::string& error_message);
    bool fixUnclosedLiteral(const std::string& file_path, int line_number,
                            const std::string& error_message);
    bool hasUnmatchedOpen(const std::string& line, char open_char, char close_char);
    bool hasUnmatchedClose(const std::string& line, char open_char, char close_char);
    bool suggestKeywordReplacement(const std::string& line, std::string& replacement);
    bool isQuotedLiteralUnclosed(const std::string& line, char quote_char);
    
    // Validation helpers
    bool validateSyntaxFix(const std::string& line, const std::string& error_msg);
    bool needsSemicolon(const std::string& line, const std::string& previous_line = "");
    bool isSafeToModifyLine(const std::string& line, const std::string& error_msg = "");
    
    // File manipulation helpers
    std::vector<std::string> readFile(const std::string& file_path);
    bool writeFile(const std::string& file_path, const std::vector<std::string>& lines);
};

#endif // CODE_FIXER_H
