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
    
    for (const auto& error : errors) {
        auto fixes = pattern_db.getSuggestions(error.error_code, error.error_message);
        
        for (const auto& fix : fixes) {
            FixSuggestion suggestion;
            suggestion.error = error;
            suggestion.fix = fix;
            suggestion.before = error.full_line;
            suggestion.is_safe = (fix.fix_type == "include_header");
            
            // For include headers, check if already included
            if (fix.fix_type == "include_header") {
                for (const auto& include : fix.suggested_includes) {
                    if (!analyzer.isHeaderIncluded(include)) {
                        suggestion.after = "#include <" + include + ">\n" + suggestion.before;
                        suggestions.push_back(suggestion);
                        break;
                    }
                }
            } else {
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
