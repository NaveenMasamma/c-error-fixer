#include "code_fixer.h"
#include "SyntaxFixer.h"
#include "utils.h"
#include <algorithm>
#include <regex>

bool SyntaxFixer::canHandle(const std::string& error_code) const {
    return error_code.find("syntax-") == 0 && error_code != "syntax-keyword-typo";
}

std::vector<FixSuggestion> SyntaxFixer::generateSuggestions(const CompilerError& error, 
                                                           const std::vector<std::string>& lines, 
                                                           CodeAnalyzer& analyzer, 
                                                           ErrorPatternDB& db) {
    (void)analyzer;
    (void)db;
    std::vector<FixSuggestion> suggestions;
        if (error.line_number < 1 || error.line_number > (int)lines.size()) return suggestions;

        std::string current_line = lines[error.line_number - 1];
        if (!isSafeToModify(current_line, error.error_code) && error.error_code != "syntax-expected-brace-eof") return suggestions;

        auto db_fixes = db.getSuggestions(error.error_code, error.error_message);
        for (const auto& fix : db_fixes) {
            FixSuggestion s;
            s.error = error;
            s.fix = fix;
            s.is_safe = true;
            s.confidence = 0.6f; // Default baseline for syntax fixes

            CodeDelta delta;
            delta.line_number = error.line_number;
            delta.old_content = current_line;

            if (error.error_code == "syntax-expected-semicolon") {
                std::string trimmed = Utils::trim(current_line);
                if (trimmed == "}" && error.line_number > 1) {
                    std::string prev_line = lines[error.line_number - 2];
                    if (Utils::trim(prev_line).back() != ';') {
                        delta.line_number = error.line_number - 1;
                        delta.old_content = prev_line;
                        delta.new_content = prev_line + ";";
                    } else {
                        continue;
                    }
                } else if (!trimmed.empty() && trimmed.back() != ';') {
                    delta.new_content = current_line + ";";
                } else {
                    continue;
                }
                
                s.confidence = 0.8f;
                s.explanation = "The compiler expected a semicolon (;) to terminate the statement on this line.";
                s.reason = "In C, statements are ended with a semicolon to help the compiler distinguish between separate instructions.";
            }
            else if (error.error_code == "syntax-expected-comma") {
                if (Utils::trim(current_line).back() != ',') {
                    delta.new_content = current_line + ",";
                } else {
                    continue;
                }
                s.confidence = 0.7f;
                s.explanation = "The compiler expected a comma (,) to separate items in a list or sequence.";
                s.reason = "Commas act as delimiters between function arguments, array elements, or variable declarations in C.";
            }
            else if (error.error_code == "syntax-expected-colon") {
                if (Utils::trim(current_line).back() != ':') {
                    delta.new_content = current_line + ":";
                } else {
                    continue;
                }
                s.confidence = 0.7f;
                s.explanation = "A colon (:) appears to be missing from this statement.";
                s.reason = "Certain C instructions, like 'case' labels in switch statements, require a colon to function correctly.";
            }
            else if (error.error_code == "syntax-unclosed-literal") {
                // Determine the quote character by inspecting the source line first
                std::string line = current_line;
                char quote = '"';
                size_t open_quote_pos = line.find_last_of("'\"");
                if (open_quote_pos != std::string::npos) {
                    quote = line[open_quote_pos];
                } else if (error.error_message.find("character") != std::string::npos) {
                    quote = '\'';
                }
                // Use a broader set of structural characters to find the actual end of the text
                size_t insert_pos = line.find_last_not_of(" \t\r\n);");
                if (insert_pos != std::string::npos) {
                    // Check if there's a structural character to insert before
                    size_t paren_pos = line.find(')', insert_pos);
                    size_t semi_pos = line.find(';', insert_pos);

                    if (paren_pos != std::string::npos) {
                        line.insert(paren_pos, std::string(1, quote));
                    } else if (semi_pos != std::string::npos) {
                        // Place closing quote after the semicolon so the literal includes it
                        line.insert(semi_pos + 1, std::string(1, quote));
                    } else {
                        line.insert(insert_pos + 1, std::string(1, quote));
                    }
                } else {
                    line += quote;
                }
                
                delta.new_content = line;
                s.confidence = 0.9f;
                s.explanation = "A text literal (a string or character) was started with a quote but never finished.";
                s.reason = "Quotes must always come in pairs. The compiler needs the second quote to know where the text ends.";
            }
            else if (error.error_code == "syntax-expected-paren") {
                std::string line = current_line;
                size_t comment_pos = line.find("//");
                std::string code_part = (comment_pos == std::string::npos) ? line : line.substr(0, comment_pos);
                std::string comment_part = (comment_pos == std::string::npos) ? "" : line.substr(comment_pos);

                bool fixed = false;
                // Case 1: expected ')' before ';'
                if (error.error_message.find("before ';'") != std::string::npos) {
                    size_t semi_pos = code_part.find_last_of(';');
                    if (semi_pos != std::string::npos) {
                        // Only add if not already preceded by ')'
                        if (semi_pos > 0 && code_part[semi_pos - 1] != ')') {
                            code_part.insert(semi_pos, ")");
                            fixed = true;
                        }
                    }
                }
                // Case 2: expected ')' before '}' or '{'
                if (!fixed && (error.error_message.find("before '}'") != std::string::npos || 
                               error.error_message.find("before '{'") != std::string::npos)) {
                    size_t brace_pos = code_part.find_last_of("{}");
                    if (brace_pos != std::string::npos) {
                        if (brace_pos > 0 && code_part[brace_pos - 1] != ')') {
                            // Trim trailing spaces before the brace
                            size_t before_brace = brace_pos;
                            while (before_brace > 0 && code_part[before_brace - 1] == ' ') {
                                before_brace--;
                            }
                            // Insert ')' after trimmed position and keep space + brace
                            std::string brace_char(1, code_part[brace_pos]);
                            code_part = code_part.substr(0, before_brace) + ") " + brace_char;
                            fixed = true;
                        }
                    }
                }
                // Case 3: Generic missing ')' - insert before end of code/semicolon
                if (!fixed) {
                    size_t last_content = code_part.find_last_not_of(" \t\r\n;");
                    if (last_content != std::string::npos && code_part[last_content] != ')') {
                        code_part.insert(last_content + 1, ")");
                        fixed = true;
                    }
                }

                if (fixed) {
                    delta.new_content = code_part + comment_part;
                    s.confidence = 0.75f;
                    s.explanation = "A closing parenthesis ')' is missing in this statement.";
                    s.reason = "The compiler detected an open '(' without a matching ')'. Parentheses must always be balanced to correctly group expressions.";
                }
            }
            else if (error.error_code == "syntax-expected-opening") {
                if (error.error_message.find("'('") != std::string::npos) {
                    std::regex control_regex(R"(^\s*(if|while|for)\b\s*)");
                    std::smatch match;
                    if (std::regex_search(current_line, match, control_regex)) {
                        std::string line = current_line;
                        line.insert(match[0].length(), "(");
                        delta.new_content = line;
                        s.confidence = 0.7f;
                        s.explanation = "An opening parenthesis '(' is missing.";
                        s.reason = "Control structures like 'if', 'while', and 'for' require their conditions to be enclosed in parentheses.";
                    }
                }
            }
            else if (error.error_code == "syntax-expected-brace-eof") {
                int open_braces = 0, close_braces = 0;
                bool in_string = false, in_char = false, in_multiline_comment = false;

                for (const auto& line : lines) {
                    for (size_t i = 0; i < line.size(); ++i) {
                        char c = line[i];
                        if (in_multiline_comment) {
                            if (c == '*' && i + 1 < line.size() && line[i+1] == '/') { in_multiline_comment = false; i++; }
                            continue;
                        }
                        if (in_string) {
                            // Corner Case: Handle escaped quotes and escaped backslashes
                            if (c == '\\' && i + 1 < line.size()) { 
                                i++; 
                                continue; 
                            }
                            if (c == '"') in_string = false;
                            continue;
                        }
                        if (in_char) {
                            if (c == '\\' && i + 1 < line.size()) { 
                                i++; 
                                continue; 
                            }
                            if (c == '\'') in_char = false;
                            continue;
                        }
                        if (c == '/' && i + 1 < line.size()) {
                            if (line[i+1] == '/') break;
                            if (line[i+1] == '*') { in_multiline_comment = true; i++; continue; }
                        }
                        if (c == '"') in_string = true;
                        else if (c == '\'') in_char = true;
                        else if (c == '{') open_braces++;
                        else if (c == '}') close_braces++;
                    }
                }

                if (open_braces > close_braces) {
                    int missing = open_braces - close_braces;
                    std::string braces_to_add = "";
                    for (int i = 0; i < missing; ++i) braces_to_add += "}";

                    delta.line_number = lines.size();
                    delta.old_content = lines.back();
                    delta.new_content = delta.old_content + "\n" + braces_to_add;

                    s.confidence = 0.85f;
                    s.explanation = "The compiler reached the end of the file while expecting a closing brace ('}'). This usually means a function or a block was never closed.";
                    s.reason = "Every opening brace '{' must have a matching closing brace '}'. Adding the missing brace(s) at the end closes the remaining scope.";
                }
            }
            else if (error.error_code == "syntax-malformed-preprocessor") {
                std::string line = current_line;
                bool changed = false;

                // 1. Handle misspelled or incorrectly capitalized directive (e.g., #includ, #INCLUDE)
                std::regex dir_regex(R"(^\s*#([iI][nN][cC][lL][uU][dD][a-zA-Z]*))");
                std::smatch match;
                if (std::regex_search(line, match, dir_regex)) {
                    std::string found_dir = match[1].str();
                    if (found_dir != "include") {
                        size_t dir_pos = line.find(found_dir);
                        line.replace(dir_pos, found_dir.length(), "include");
                        changed = true;
                        s.explanation = "The #include directive is misspelled or improperly capitalized.";
                        s.reason = "C preprocessor directives are case-sensitive and must be spelled exactly as '#include'.";
                    }
                }

                // 2. Handle missing or unclosed delimiters for the header path
                std::string trimmed_line = Utils::trim(line);
                if (trimmed_line.find("#include") == 0) {
                    if (trimmed_line.find('<') != std::string::npos && trimmed_line.find('>') == std::string::npos) {
                        line += ">";
                        changed = true;
                        s.explanation = "The #include directive is missing a closing angle bracket ('>').";
                        s.reason = "Header files specified with '<' must be terminated with a matching '>' character.";
                    } else if (trimmed_line.find('\"') != std::string::npos) {
                        size_t first_quote = trimmed_line.find('\"');
                        if (trimmed_line.find('\"', first_quote + 1) == std::string::npos) {
                            line += "\"";
                            changed = true;
                            s.explanation = "The #include directive is missing a closing double quote ('\"').";
                            s.reason = "Header files specified with '\"' must be terminated with a matching '\"' character.";
                        }
                    } else if (trimmed_line.find('<') == std::string::npos && trimmed_line.find('\"') == std::string::npos) {
                        // 3. Handle entirely missing delimiters (e.g., #include stdio.h)
                        std::regex no_delims_regex(R"(#include\s+([a-zA-Z0-9_\./]+))");
                        if (std::regex_search(trimmed_line, match, no_delims_regex)) {
                            std::string header = match[1].str();
                            size_t header_pos = line.find(header);
                            if (header_pos != std::string::npos) {
                                line.replace(header_pos, header.length(), "<" + header + ">");
                                changed = true;
                                s.explanation = "The header filename is missing surrounding brackets or quotes.";
                                s.reason = "In C, #include directives require filenames to be enclosed in < > or \" \".";
                            }
                        }
                    }
                }

                if (changed) {
                    delta.new_content = line;
                    s.confidence = 0.95f;
                }
            }

            if (!delta.new_content.empty()) {
                s.deltas.push_back(delta);
                suggestions.push_back(s);
            }
        }
        return suggestions;
}

bool SyntaxFixer::isSafeToModify(const std::string& line, const std::string& code) {
    if (code == "syntax-expected-brace-eof") return true;
    std::string trimmed = Utils::trim(line);
    if (trimmed.empty() || trimmed.find("//") == 0 || trimmed.find("/*") == 0) return false;
    if (trimmed.find("#") == 0 && code != "syntax-malformed-preprocessor") return false;
    return true;
}