#include "code_fixer.h"
#include "code_analyzer.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>

CodeFixer::CodeFixer() {}

std::vector<FixSuggestion> CodeFixer::generateFixes(const std::string& file_path,
                                                    const std::vector<CompilerError>& errors) {
    std::vector<FixSuggestion> suggestions;
    CodeAnalyzer analyzer;
    analyzer.analyzeFile(file_path);
    
    auto lines = readFile(file_path);
    
    for (const auto& error : errors) {
        auto fixes = pattern_db.getSuggestions(error.error_code, error.error_message);
        
        for (const auto& fix : fixes) {
            FixSuggestion suggestion;
            suggestion.error = error;
            suggestion.fix = fix;
            
            // Get the actual line from file
            if (error.line_number > 0 && error.line_number <= lines.size()) {
                suggestion.before = lines[error.line_number - 1];
            } else {
                suggestion.before = error.full_line;
            }
            
            // Determine if fix is safe
            if (fix.fix_type == "include_header") {
                // Check if already included
                bool already_included = false;
                for (const auto& include : fix.suggested_includes) {
                    if (analyzer.isHeaderIncluded(include)) {
                        already_included = true;
                        break;
                    }
                }
                
                if (!already_included) {
                    suggestion.is_safe = true;
                    for (const auto& header : fix.suggested_includes) {
                        suggestion.after = "#include <" + header + ">\n" + suggestion.before;
                        break;
                    }
                    suggestions.push_back(suggestion);
                }
            } else if (fix.fix_type == "syntax_fix") {
                // Validate if syntax fix is safe
                if (validateSyntaxFix(suggestion.before, error.error_message)) {
                    suggestion.is_safe = true;
                    suggestion.after = suggestion.before;
                    
                    // Apply the appropriate syntax fix
                    if (needsSemicolon(suggestion.before)) {
                        // Only add semicolon if line doesn't already end with one
                        if (!suggestion.after.empty() && suggestion.after.back() != ';') {
                            suggestion.after += ";";
                        }
                    }
                    suggestions.push_back(suggestion);
                } else {
                    suggestion.is_safe = false;
                    suggestions.push_back(suggestion);
                }
            } else {
                suggestion.is_safe = false;
                suggestions.push_back(suggestion);
            }
        }
    }
    
    return suggestions;
}

bool CodeFixer::applyFix(const std::string& file_path, const FixSuggestion& suggestion) {
    if (suggestion.fix.fix_type == "include_header") {
        // Find the header from suggested includes
        for (const auto& header : suggestion.fix.suggested_includes) {
            return addIncludeHeader(file_path, header);
        }
    } else if (suggestion.fix.fix_type == "syntax_fix") {
        return fixSyntaxError(file_path, suggestion.error.line_number, 
                            suggestion.error.error_message);
    }
    return false;
}

bool CodeFixer::applyAllFixes(const std::string& file_path,
                             const std::vector<FixSuggestion>& suggestions) {
    backupFile(file_path);
    
    for (const auto& suggestion : suggestions) {
        if (suggestion.is_safe) {
            applyFix(file_path, suggestion);
        }
    }
    return true;
}

bool CodeFixer::backupFile(const std::string& file_path) {
    std::string backup_path = file_path + ".backup";
    std::ifstream src(file_path, std::ios::binary);
    std::ofstream dst(backup_path, std::ios::binary);
    
    if (src && dst) {
        dst << src.rdbuf();
        return true;
    }
    return false;
}

bool CodeFixer::addIncludeHeader(const std::string& file_path, const std::string& header) {
    auto lines = readFile(file_path);
    if (lines.empty()) return false;
    
    // Find the last include statement
    int insert_pos = 0;
    for (int i = 0; i < lines.size(); i++) {
        if (lines[i].find("#include") == 0) {
            insert_pos = i + 1;
        }
    }
    
    // Check if header is already included
    for (const auto& line : lines) {
        if (line.find("#include") != std::string::npos && 
            line.find(header) != std::string::npos) {
            return true;  // Already included
        }
    }
    
    // Insert new include
    std::string include_line = "#include <" + header + ">";
    lines.insert(lines.begin() + insert_pos, include_line);
    
    return writeFile(file_path, lines);
}

bool CodeFixer::addMissingSemicolon(const std::string& file_path, int line_number) {
    auto lines = readFile(file_path);
    if (line_number > lines.size() || line_number < 1) return false;
    
    std::string& line = lines[line_number - 1];
    if (line.back() != ';') {
        line += ";";
    }
    
    return writeFile(file_path, lines);
}

bool CodeFixer::declareMissingFunction(const std::string& file_path,
                                      const std::string& function_name) {
    // This is more complex and would require parsing the error message
    // to extract function name and parameters
    return false;
}

bool CodeFixer::fixSyntaxError(const std::string& file_path, int line_number,
                              const std::string& error_message) {
    auto lines = readFile(file_path);
    if (line_number > lines.size() || line_number < 1) return false;
    
    // Check if the error is on the current line
    std::string& current_line = lines[line_number - 1];
    
    // For "expected ';'" errors, check the previous line first
    if (error_message.find("expected") != std::string::npos && 
        (error_message.find(";") != std::string::npos || error_message.find("semicolon") != std::string::npos)) {
        
        // Check previous line (more likely to have missing semicolon)
        if (line_number > 1) {
            std::string& prev_line = lines[line_number - 2];
            
            if (needsSemicolon(prev_line)) {
                // Find where to insert semicolon (before any comment)
                size_t comment_pos = prev_line.find("//");
                if (comment_pos == std::string::npos) {
                    comment_pos = prev_line.find("/*");
                }
                
                if (comment_pos != std::string::npos) {
                    // Trim trailing whitespace before the comment
                    size_t trim_pos = comment_pos;
                    while (trim_pos > 0 && (prev_line[trim_pos - 1] == ' ' || prev_line[trim_pos - 1] == '\t')) {
                        trim_pos--;
                    }
                    // Remove trailing spaces
                    prev_line.erase(trim_pos, comment_pos - trim_pos);
                    // Insert semicolon at trimmed position
                    prev_line.insert(trim_pos, ";  ");
                } else {
                    // No comment, just append
                    if (!prev_line.empty() && prev_line.back() != ';') {
                        prev_line += ";";
                    }
                }
                return writeFile(file_path, lines);
            }
        }
        
        // Check current line if previous didn't work
        if (needsSemicolon(current_line)) {
            size_t comment_pos = current_line.find("//");
            if (comment_pos == std::string::npos) {
                comment_pos = current_line.find("/*");
            }
            
            if (comment_pos != std::string::npos) {
                size_t trim_pos = comment_pos;
                while (trim_pos > 0 && (current_line[trim_pos - 1] == ' ' || current_line[trim_pos - 1] == '\t')) {
                    trim_pos--;
                }
                current_line.erase(trim_pos, comment_pos - trim_pos);
                current_line.insert(trim_pos, ";  ");
            } else {
                if (!current_line.empty() && current_line.back() != ';') {
                    current_line += ";";
                }
            }
            return writeFile(file_path, lines);
        }
    }
    
    // General case: check current line
    if (needsSemicolon(current_line)) {
        size_t comment_pos = current_line.find("//");
        if (comment_pos == std::string::npos) {
            comment_pos = current_line.find("/*");
        }
        
        if (comment_pos != std::string::npos) {
            size_t trim_pos = comment_pos;
            while (trim_pos > 0 && (current_line[trim_pos - 1] == ' ' || current_line[trim_pos - 1] == '\t')) {
                trim_pos--;
            }
            current_line.erase(trim_pos, comment_pos - trim_pos);
            current_line.insert(trim_pos, ";  ");
        } else {
            if (!current_line.empty() && current_line.back() != ';') {
                current_line += ";";
            }
        }
        return writeFile(file_path, lines);
    }
    
    return false;
}

bool CodeFixer::validateSyntaxFix(const std::string& line, const std::string& error_msg) {
    // Check if the line is safe to modify (not a comment, not multi-line issue, etc.)
    if (line.empty()) return false;
    
    // Trim whitespace
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    
    // Don't modify comment lines
    if (trimmed.find("//") == 0 || trimmed.find("/*") == 0) {
        return false;
    }
    
    // Check if line looks like a statement that needs semicolon
    if (error_msg.find("expected") != std::string::npos) {
        return isSafeToModifyLine(line);
    }
    
    return false;
}

bool CodeFixer::needsSemicolon(const std::string& line) {
    if (line.empty()) return false;
    
    // Remove comments first
    std::string working_line = line;
    size_t comment_pos = working_line.find("//");
    if (comment_pos != std::string::npos) {
        working_line = working_line.substr(0, comment_pos);
    }
    
    comment_pos = working_line.find("/*");
    if (comment_pos != std::string::npos) {
        working_line = working_line.substr(0, comment_pos);
    }
    
    // Trim whitespace
    std::string trimmed = working_line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    if (trimmed.empty()) return false;
    
    // Already has semicolon
    if (!trimmed.empty() && trimmed.back() == ';') {
        return false;
    }
    
    // Don't add semicolon to opening/closing braces
    if (trimmed == "{" || trimmed == "}") {
        return false;
    }
    
    // Don't add semicolon to control flow statements (they handle their own syntax)
    if (trimmed.find("if ") == 0 || trimmed.find("while ") == 0 || 
        trimmed.find("for ") == 0 || trimmed.find("switch ") == 0 ||
        trimmed.find("else") == 0 || trimmed.find("do ") == 0) {
        return false;
    }
    
    // Check if line looks like a statement that should have semicolon
    // Variable declarations with initialization
    if (trimmed.find("int ") != std::string::npos || 
        trimmed.find("float ") != std::string::npos ||
        trimmed.find("double ") != std::string::npos ||
        trimmed.find("char ") != std::string::npos ||
        trimmed.find("void ") != std::string::npos ||
        trimmed.find("bool ") != std::string::npos) {
        return true;
    }
    
    // Assignment statement
    if (trimmed.find("=") != std::string::npos && trimmed.find("==") == std::string::npos) {
        return true;
    }
    
    // Function call
    if (trimmed.find("(") != std::string::npos && trimmed.find(")") != std::string::npos) {
        return true;
    }
    
    // Return statement
    if (trimmed.find("return ") == 0) {
        return true;
    }
    
    return false;
}

bool CodeFixer::isSafeToModifyLine(const std::string& line) {
    if (line.empty()) return false;
    
    // Trim whitespace
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    // Don't modify:
    // - Comments
    if (trimmed.find("//") == 0 || trimmed.find("/*") == 0) {
        return false;
    }
    
    // - Preprocessor directives
    if (trimmed.find("#") == 0) {
        return false;
    }
    
    // - Control flow statements (already handled by compiler)
    if (trimmed.find("if") == 0 || trimmed.find("while") == 0 || 
        trimmed.find("for") == 0 || trimmed.find("switch") == 0) {
        return false;
    }
    
    // - Opening/closing braces
    if (trimmed == "{" || trimmed == "}") {
        return false;
    }
    
    return true;
}

std::vector<std::string> CodeFixer::readFile(const std::string& file_path) {
    std::vector<std::string> lines;
    std::ifstream file(file_path);
    std::string line;
    
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

bool CodeFixer::writeFile(const std::string& file_path, const std::vector<std::string>& lines) {
    std::ofstream file(file_path);
    if (!file.is_open()) return false;
    
    for (const auto& line : lines) {
        file << line << "\n";
    }
    
    return true;
}
