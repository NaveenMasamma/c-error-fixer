#include "code_fixer.h"
#include "code_analyzer.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <regex>

CodeFixer::CodeFixer() {}

std::vector<FixSuggestion> CodeFixer::generateFixes(const std::string& file_path,
                                                    const std::vector<CompilerError>& errors) {
    std::vector<FixSuggestion> suggestions;
    CodeAnalyzer analyzer;
    analyzer.analyzeFile(file_path);
    
    auto lines = readFile(file_path);
    
    for (const auto& error : errors) {
        // Special handling for implicit function declarations: try to map common functions to headers
        std::vector<ErrorFix> fixes;
        if (error.error_code == "implicit-function-declaration" || error.error_code == "undeclared-identifier") {
            // try to identify function name from the message
            std::string msg = error.error_message;
            // naive extraction: look for an identifier-like token
            std::smatch m;
            std::regex ident("([a-zA-Z_][a-zA-Z0-9_]*)");
            std::string func;
            if (std::regex_search(msg, m, ident)) {
                func = m[1].str();
            }
            std::string header;
            if (!func.empty()) header = pattern_db.lookupHeaderForFunction(func);

            if (!header.empty()) {
                ErrorFix hfix;
                hfix.fix_type = "include_header";
                hfix.fix_description = "Add missing #include directive for " + func;
                hfix.error_pattern = error.error_code;
                hfix.suggested_includes = {header};
                fixes.push_back(hfix);
            }

            // also include any database suggestions
            auto dbfixes = pattern_db.getSuggestions(error.error_code, error.error_message);
            fixes.insert(fixes.end(), dbfixes.begin(), dbfixes.end());
        } else {
            fixes = pattern_db.getSuggestions(error.error_code, error.error_message);
        }

        for (const auto& fix : fixes) {
            FixSuggestion suggestion;
            suggestion.error = error;
            suggestion.fix = fix;

            // Get the actual line from file (if available)
            if (error.line_number > 0 && error.line_number <= static_cast<int>(lines.size())) {
                suggestion.before = lines[error.line_number - 1];
            } else {
                suggestion.before = error.full_line;
            }

            // Handle include_header fixes
            if (fix.fix_type == "include_header") {
                for (const auto& include : fix.suggested_includes) {
                    if (!analyzer.isHeaderIncluded(include)) {
                        suggestion.is_safe = true; // adding an include is generally safe
                        suggestion.after = "#include <" + include + ">\n" + suggestion.before;
                        suggestions.push_back(suggestion);
                        break;
                    }
                }
            } else if (fix.fix_type == "syntax_fix") {
                if (validateSyntaxFix(suggestion.before, error.error_message)) {
                    suggestion.is_safe = true;
                    suggestion.after = suggestion.before;
                    std::string lower_msg = Utils::toLower(error.error_message);
                    if (error.error_code == "syntax-expected-semicolon" || lower_msg.find("semicolon") != std::string::npos || needsSemicolon(suggestion.before)) {
                        if (suggestion.after.empty() || suggestion.after.back() != ';') suggestion.after += ";";
                    } else if (error.error_code == "syntax-expected-comma" || lower_msg.find("comma") != std::string::npos) {
                        if (suggestion.after.empty() || suggestion.after.back() != ',') suggestion.after += ",";
                    } else if (error.error_code == "syntax-expected-colon" || lower_msg.find("colon") != std::string::npos) {
                        if (suggestion.after.empty() || suggestion.after.back() != ':') suggestion.after += ":";
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
    } else if (suggestion.fix.fix_type == "syntax_fix" && suggestion.fix.error_pattern == "syntax-expected-colon") {
        return fixMissingColon(file_path, suggestion.error.line_number,
                               suggestion.error.error_message);
    } else if (suggestion.fix.fix_type == "syntax_fix") {
        return fixSyntaxError(file_path, suggestion.error.line_number,
                             suggestion.error.error_message,
                             suggestion.error.column);
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
    size_t insert_pos = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
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
    if (line_number > static_cast<int>(lines.size()) || line_number < 1) return false;
    
    std::string& line = lines[line_number - 1];
    if (!line.empty() && line.back() != ';') {
        size_t insert_pos = line.find("//");
        if (insert_pos == std::string::npos) {
            insert_pos = line.find("/*");
        }
        if (insert_pos != std::string::npos) {
            size_t trim_pos = insert_pos;
            while (trim_pos > 0 && (line[trim_pos - 1] == ' ' || line[trim_pos - 1] == '\t')) {
                trim_pos--;
            }
            line.insert(trim_pos, ";");
        } else {
            line += ";";
        }
    }
    
    return writeFile(file_path, lines);
}

bool CodeFixer::declareMissingFunction(const std::string& file_path,
                                      const std::string& function_name) {
    // This is more complex and would require parsing the error message
    // to extract function name and parameters
    (void)file_path;
    (void)function_name;
    return false;
}

bool CodeFixer::fixMissingToken(const std::string& file_path, int line_number,
                                char token, int column, const std::string& error_message) {
    auto lines = readFile(file_path);
    if (line_number < 1 || line_number > static_cast<int>(lines.size())) return false;
    (void)error_message;

    std::string& current_line = lines[line_number - 1];
    std::string target_line = current_line;
    bool changed = false;

    auto insertBeforeCommentOrEnd = [&](const std::string& replacement) {
        size_t insert_pos = current_line.find("//");
        if (insert_pos == std::string::npos) {
            insert_pos = current_line.find("/*");
        }
        if (insert_pos != std::string::npos) {
            size_t trim_pos = insert_pos;
            while (trim_pos > 0 && (current_line[trim_pos - 1] == ' ' || current_line[trim_pos - 1] == '\t')) {
                trim_pos--;
            }
            target_line.insert(trim_pos, replacement);
        } else {
            target_line += replacement;
        }
        changed = true;
    };

    if (token == ';') {
        std::string prev_line = line_number > 1 ? readFile(file_path)[line_number - 2] : std::string();
        if (needsSemicolon(current_line, prev_line)) {
            insertBeforeCommentOrEnd(";");
        }
    } else if (token == ',') {
        auto close_pos = target_line.find_last_of(")}");
        if (close_pos != std::string::npos && close_pos > 0 && target_line[close_pos - 1] != ',') {
            target_line.insert(close_pos, ",");
            changed = true;
        } else if (!target_line.empty() && target_line.back() != ',' && target_line.back() != ';') {
            target_line += ",";
            changed = true;
        }
    } else if (token == ')' || token == ']' || token == '}') {
        if (!hasUnmatchedOpen(current_line, token == ')' ? '(' : token == ']' ? '[' : '{', token)) {
            // nothing to fix
        } else {
            insertBeforeCommentOrEnd(std::string(1, token));
        }
    } else if (token == '(' || token == '[' || token == '{') {
        char close_char = token == '(' ? ')' : token == '[' ? ']' : '}';
        if (hasUnmatchedClose(current_line, token, close_char)) {
            size_t insert_pos = current_line.size();
            if (column > 0 && static_cast<size_t>(column - 1) <= current_line.size()) {
                insert_pos = static_cast<size_t>(column - 1);
            }
            target_line.insert(insert_pos, std::string(1, token));
            changed = true;
        }
    } else if (token == '"' || token == '\'') {
        if (isQuotedLiteralUnclosed(current_line, token)) {
            insertBeforeCommentOrEnd(std::string(1, token));
        }
    }

    if (!changed) {
        return false;
    }

    current_line = target_line;
    return writeFile(file_path, lines);
}

bool CodeFixer::hasUnmatchedOpen(const std::string& line, char open_char, char close_char) {
    int balance = 0;
    for (char c : line) {
        if (c == open_char) balance++;
        if (c == close_char) balance--;
    }
    return balance > 0;
}

bool CodeFixer::hasUnmatchedClose(const std::string& line, char open_char, char close_char) {
    int balance = 0;
    for (char c : line) {
        if (c == open_char) balance++;
        if (c == close_char) balance--;
    }
    return balance < 0;
}

bool CodeFixer::isQuotedLiteralUnclosed(const std::string& line, char quote_char) {
    std::string code_line = line;
    size_t comment_pos = code_line.find("//");
    if (comment_pos != std::string::npos) {
        code_line = code_line.substr(0, comment_pos);
    }
    comment_pos = code_line.find("/*");
    if (comment_pos != std::string::npos) {
        code_line = code_line.substr(0, comment_pos);
    }

    int count = 0;
    bool escaped = false;
    for (char c : code_line) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == quote_char) {
            count++;
        }
    }

    return (count % 2) != 0;
}

bool CodeFixer::suggestKeywordReplacement(const std::string& line, std::string& replacement) {
    static const std::vector<std::string> c_keywords = { // C keywords for typo checking
        "auto", "break", "case", "char", "const", "continue", "default", "do", "double",
        "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int",
        "long", "register", "restrict", "return", "short", "signed", "sizeof", "static",
        "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"
    };

    std::regex token_regex(R"(([a-zA-Z_][a-zA-Z0-9_]*))");
    std::sregex_iterator it(line.begin(), line.end(), token_regex);
    std::sregex_iterator end;
    int best_distance = 2;
    std::string best_keyword;

    for (; it != end; ++it) {
        std::string token = it->str();
        std::string lower_token = token;
        std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower);
        for (const auto& keyword : c_keywords) { // Compare with C keywords
            int dist = Utils::levenshteinDistance(lower_token, keyword);
            if (dist < best_distance) {
                best_distance = dist;
                best_keyword = keyword;
            }
        }
    }

    if (best_distance == 1 && !best_keyword.empty()) {
        replacement = best_keyword;
        return true;
    }
    return false;
}

bool CodeFixer::fixKeywordTypo(const std::string& file_path, int line_number,
                               const std::string& error_message) {
    auto lines = readFile(file_path);
    if (line_number < 1 || line_number > static_cast<int>(lines.size())) return false;
    (void)error_message;

    std::string& line = lines[line_number - 1];
    std::string replacement;
    if (!suggestKeywordReplacement(line, replacement)) {
        return false;
    }

    std::regex token_regex(R"((\b[a-zA-Z_][a-zA-Z0-9_]*\b))");
    std::sregex_iterator it(line.begin(), line.end(), token_regex);
    std::sregex_iterator end;
    for (; it != end; ++it) {
        std::string token = it->str();
        std::string lower_token = token;
        std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower);
        if (Utils::levenshteinDistance(lower_token, replacement) == 1) {
            line.replace(it->position(), it->length(), replacement);
            return writeFile(file_path, lines);
        }
    }

    return false;
}

bool CodeFixer::fixUnclosedLiteral(const std::string& file_path, int line_number,
                                   const std::string& error_message) {
    auto lines = readFile(file_path);
    if (line_number < 1 || line_number > static_cast<int>(lines.size())) return false;

    std::string& line = lines[line_number - 1];
    char quote_char = '"';
    std::string lower_msg = error_message;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);
    if (lower_msg.find("character") != std::string::npos) {
        quote_char = '\'';
    }

    if (!isQuotedLiteralUnclosed(line, quote_char)) {
        return false;
    }

    size_t comment_pos = line.find("//");
    if (comment_pos == std::string::npos) {
        comment_pos = line.find("/*");
    }

    if (comment_pos != std::string::npos) {
        line.insert(comment_pos, 1, quote_char);
    } else {
        line.push_back(quote_char);
    }

    return writeFile(file_path, lines);
}

bool CodeFixer::fixMalformedPreprocessor(const std::string& file_path, int line_number,
                                         const std::string& error_message) {
    auto lines = readFile(file_path);
    if (line_number < 1 || line_number > static_cast<int>(lines.size())) return false;
    (void)error_message;

    std::string& line = lines[line_number - 1];
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));

    if (trimmed.rfind("#includ", 0) == 0 && trimmed.find("#include") != 0) {
        size_t pos = line.find("#includ");
        if (pos != std::string::npos) {
            line.replace(pos, 7, "#include");
        }
    }

    if (trimmed.rfind("#include", 0) == 0) {
        size_t pos = line.find("#include");
        size_t start = line.find_first_not_of(" \t", pos + 8);
        if (start != std::string::npos) {
            char opening = line[start];
            if (opening == '<') {
                if (line.find('>', start + 1) == std::string::npos) {
                    line.push_back('>');
                }
            } else if (opening == '"') {
                if (line.find('"', start + 1) == std::string::npos) {
                    line.push_back('"');
                }
            }
        }
    }

    return writeFile(file_path, lines);
}

bool CodeFixer::fixSyntaxError(const std::string& file_path, int line_number,
                              const std::string& error_message, int column) {
    std::string lower_msg = error_message;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);
    
    if (lower_msg.find("syntax-keyword-typo") != std::string::npos ||
        (lower_msg.find("expected") != std::string::npos && lower_msg.find("identifier") != std::string::npos)) {
        if (fixKeywordTypo(file_path, line_number, error_message)) {
            return true;
        }
    }

    if (lower_msg.find("syntax-unclosed-literal") != std::string::npos ||
        lower_msg.find("missing terminating character") != std::string::npos ||
        lower_msg.find("unterminated") != std::string::npos) {
        return fixUnclosedLiteral(file_path, line_number, error_message);
    }

    if (lower_msg.find("syntax-malformed-preprocessor") != std::string::npos ||
        lower_msg.find("#includ") != std::string::npos ||
        lower_msg.find("#include") != std::string::npos) {
        return fixMalformedPreprocessor(file_path, line_number, error_message);
    }

    if (lower_msg.find("expected '(' before") != std::string::npos ||
        lower_msg.find("expected '[' before") != std::string::npos ||
        lower_msg.find("expected '{' before") != std::string::npos ||
        lower_msg.find("syntax-expected-opening") != std::string::npos) {
        if (lower_msg.find("expected '(' before") != std::string::npos) {
            return fixMissingToken(file_path, line_number, '(', column, error_message);
        }
        if (lower_msg.find("expected '[' before") != std::string::npos) {
            return fixMissingToken(file_path, line_number, '[', column, error_message);
        }
        if (lower_msg.find("expected '{' before") != std::string::npos) {
            return fixMissingToken(file_path, line_number, '{', column, error_message);
        }
    }

    if (lower_msg.find("expected ';'") != std::string::npos ||
        lower_msg.find("expected ';' before") != std::string::npos ||
        lower_msg.find("semicolon") != std::string::npos) {
        auto lines = readFile(file_path);
        if (line_number > 1 && needsSemicolon(lines[line_number - 2], line_number > 2 ? lines[line_number - 3] : "")) {
            return fixMissingToken(file_path, line_number - 1, ';', 0, error_message);
        }
        return fixMissingToken(file_path, line_number, ';', 0, error_message);
    }

    if (lower_msg.find("expected ','") != std::string::npos ||
        lower_msg.find("expected ',' before") != std::string::npos) {
        return fixMissingToken(file_path, line_number, ',', 0, error_message);
    }
    if (lower_msg.find("expected ':'") != std::string::npos ||
        lower_msg.find("expected ':' before") != std::string::npos) {
        return fixMissingToken(file_path, line_number, ':', 0, error_message);
    }

    if (lower_msg.find("expected ')'") != std::string::npos ||
        lower_msg.find("missing )") != std::string::npos) {
        return fixMissingToken(file_path, line_number, ')', 0, error_message);
    }

    if (lower_msg.find("expected '}'") != std::string::npos ||
        lower_msg.find("expected ']'" ) != std::string::npos) {
        char token = lower_msg.find("expected '}'") != std::string::npos ? '}' : ']';
        return fixMissingToken(file_path, line_number, token, 0, error_message);
    }

    auto lines = readFile(file_path);
    if (line_number >= 1 && line_number <= static_cast<int>(lines.size()) && needsSemicolon(lines[line_number - 1], line_number > 1 ? lines[line_number - 2] : "")) {
        return fixMissingToken(file_path, line_number, ';', 0, error_message);
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
    if (error_msg.find("expected") != std::string::npos ||
        error_msg.find("missing terminating character") != std::string::npos ||
        error_msg.find("unterminated") != std::string::npos ||
        error_msg.find("#includ") != std::string::npos ||
        error_msg.find("#include") != std::string::npos) {
        return isSafeToModifyLine(line, error_msg);
    }
    
    return false;
}

bool CodeFixer::needsSemicolon(const std::string& line, const std::string& previous_line) {
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
    
    // Don't add semicolon to opening braces. 
    // Closing braces usually don't need them, unless it's a struct/enum definition.
    if (trimmed == "{") {
        return false;
    }

    if (trimmed.back() == '}') {
        std::string lower_trimmed = Utils::toLower(trimmed);
        std::string lower_prev = Utils::toLower(previous_line);

        if (lower_trimmed.find("struct") != std::string::npos ||
            lower_trimmed.find("union") != std::string::npos ||
            lower_trimmed.find("enum") != std::string::npos ||
            lower_prev.find("struct") != std::string::npos ||
            lower_prev.find("union") != std::string::npos ||
            lower_prev.find("enum") != std::string::npos) {
            return true;
        }
    }
    
    // Don't add semicolon to control flow statements (they handle their own syntax)
    if (trimmed.find("if ") == 0 || trimmed.find("while ") == 0 || 
        trimmed.find("for ") == 0 || trimmed.find("switch ") == 0 ||
        trimmed.find("else") == 0 || trimmed.find("do ") == 0) {
        return false;
    }
    
    // Check if line looks like a statement that should have semicolon (declarations)
    static const std::vector<std::string> types = {
        "int", "float", "double", "char", "void", "bool", "uint", "size_t", "unsigned", "long", "short"
    };
    
    bool is_declaration = false;
    for (const auto& type : types) {
        if (trimmed.find(type) == 0 || trimmed.find(" " + type + " ") != std::string::npos || 
            trimmed.find(type + "*") != std::string::npos) {
            is_declaration = true;
            break;
        }
    }
    if (is_declaration) return true;
    
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

bool CodeFixer::isSafeToModifyLine(const std::string& line, const std::string& error_msg) {
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
        std::string lower_msg = error_msg;
        std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);
        if (lower_msg.find("syntax-malformed-preprocessor") != std::string::npos ||
            lower_msg.find("#include") != std::string::npos ||
            lower_msg.find("#includ") != std::string::npos) {
            return true;
        }
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

bool CodeFixer::fixMissingColon(const std::string& file_path, int line_number, const std::string& error_message) {
    return fixMissingToken(file_path, line_number, ':', 0, error_message);
}

bool CodeFixer::fixMissingComma(const std::string& file_path, int line_number, const std::string& error_message) {
    return fixMissingToken(file_path, line_number, ',', 0, error_message);
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
