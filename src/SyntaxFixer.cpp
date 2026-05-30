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

    if (error.error_code == "syntax-ambiguous" || error.error_code == "syntax-ambiguous-eof") {
        FixSuggestion s;
        s.error = error;
        s.fix.fix_type = "syntax_fix";
        s.is_safe = false;
        
        if (error.error_code == "syntax-ambiguous-eof") {
            s.fix.fix_description = "Review suspicious location";
            s.explanation = "Likely Cause:\nMissing closing brace '}'\n\nSuspicious Location:\nNear end of file";
            s.reason = "The compiler reached the end of the file unexpectedly. This usually happens when a block or function is missing a closing brace.";
            s.confidence = 0.4f;
            suggestions.push_back(s);
            return suggestions;
        }

        int decl_line = -1;
        std::string likely_cause;
        std::string example;
        std::string code_snippet;

        for (int i = error.line_number - 1; i >= 0 && i >= error.line_number - 3; --i) {
            std::string line = lines[i];
            std::string trimmed = Utils::trim(line);
            if (trimmed.empty()) continue;
            
            if (trimmed.find("(") != std::string::npos && trimmed.find(")") != std::string::npos && 
                trimmed.find(";") == std::string::npos && trimmed.find("{") == std::string::npos) {
                decl_line = i + 1;
                code_snippet = trimmed;
                likely_cause = "Missing opening brace '{' after function declaration.";
                
                std::string next_line = (i + 1 < (int)lines.size()) ? Utils::trim(lines[i + 1]) : "";
                example = trimmed + "\n{\n" + next_line + "\n}";
                break;
            }
        }

        if (decl_line != -1) {
            s.fix.fix_description = "Review suspicious location";
            s.explanation = "Likely Cause:\n" + likely_cause + "\n\nSuspicious Location:\nLine " + std::to_string(decl_line) + "\n\nCode:\n" + code_snippet + "\n\nSuggested Example:\n" + example;
            s.reason = "A function declaration must be followed by a block enclosed in braces { ... }.";
            s.confidence = 0.3f;
        } else {
            s.fix.fix_description = "Manual review required";
            s.explanation = "The compiler encountered an unexpected token. This is often caused by a missing opening brace '{', a missing semicolon ';', or a malformed declaration.";
            s.reason = "Because this diagnostic is ambiguous and has multiple interpretations, no automatic fix can be safely applied. Please inspect the code manually.";
            s.confidence = 0.0f;
        }
        
        suggestions.push_back(s);
        return suggestions;
    }

    std::string current_line = lines[error.line_number - 1];
    if (!isSafeToModify(current_line, error.error_code) && error.error_code != "syntax-expected-brace-eof" && error.error_code != "syntax-expected-brace") return suggestions;

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
                auto hasTerminator = [](const std::string& text) {
                    if (text.empty()) return false;
                    char last = text.back();
                    // A closing brace '}' is no longer considered a strict statement terminator,
                    // as struct, enum, and array declarations ending in '}' still require a semicolon.
                    return last == ';' || last == '{' || last == ':' || last == ',';
                };

                if (trimmed == "}" && error.line_number > 1) {
                    std::string prev_line = lines[error.line_number - 2];
                    if (!hasTerminator(Utils::trim(prev_line))) {
                        delta.line_number = error.line_number - 1;
                        delta.old_content = prev_line;
                        delta.new_content = prev_line + ";";
                    } else {
                        delta.new_content = current_line + ";";
                    }
                } else if (error.line_number > 1 && hasTerminator(trimmed)) {
                    std::string prev_line = lines[error.line_number - 2];
                    if (!hasTerminator(Utils::trim(prev_line))) {
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
                if (delta.line_number != error.line_number) {
                    s.explanation = "The compiler reached this line while the previous statement was missing a semicolon (;).";
                } else {
                    s.explanation = "The compiler expected a semicolon (;) to terminate the statement on this line.";
                }
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
                std::string trimmed = Utils::trim(current_line);
                std::regex case_regex(R"(^\s*(case\s+[a-zA-Z0-9_'\.\-\+]+|default|public|private|protected)(?:\s+(.*))?$)");
                std::smatch match;
                if (std::regex_match(current_line, match, case_regex)) {
                    std::string label = match[1].str();
                    std::string rest = match[2].str();
                    if (rest.empty()) {
                        delta.new_content = current_line + ":";
                    } else {
                        size_t pos = current_line.find(label) + label.length();
                        std::string line = current_line;
                        line.insert(pos, ":");
                        delta.new_content = line;
                    }
                } else if (!trimmed.empty() && trimmed.back() != ':') {
                    delta.new_content = current_line + ":";
                } else {
                    continue;
                }
                s.confidence = 0.8f;
                s.explanation = "A colon (:) appears to be missing from this statement.";
                s.reason = "Certain C/C++ keywords like 'case', 'default', 'public', or 'private' require a trailing colon.";
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
                size_t comment_pos = current_line.find("//");
                std::string code_part = (comment_pos == std::string::npos) ? current_line : current_line.substr(0, comment_pos);
                std::string comment_part = (comment_pos == std::string::npos) ? "" : current_line.substr(comment_pos);

                std::regex call_regex(R"(^(\s*(?:.*=\s*)?)([a-zA-Z_][a-zA-Z0-9_]*)\s+(.+?)\s*$)");
                std::smatch call_match;
                if (std::regex_match(code_part, call_match, call_regex)) {
                    std::string prefix = call_match[1].str();
                    std::string func = call_match[2].str();
                    std::string args = Utils::trim(call_match[3].str());
                    bool had_semicolon = !args.empty() && args.back() == ';';
                    if (had_semicolon) {
                        args.pop_back();
                        args = Utils::trim(args);
                    }

                    if (!args.empty() && (args.front() == '"' || args.front() == '\'')) {
                        delta.new_content = prefix + func + "(" + args + ")" + (had_semicolon ? ";" : "") + comment_part;
                        s.confidence = 0.9f;
                        s.explanation = "A function call is missing the opening parenthesis after the function name.";
                        s.reason = "Function arguments in C must be wrapped in parentheses, like function_name(argument).";
                    }
                }

                if (delta.new_content.empty() && error.error_message.find("'('") != std::string::npos) {
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
                else if (delta.new_content.empty() && error.error_message.find("'['") != std::string::npos) {
                    std::regex array_regex(R"(([a-zA-Z_][a-zA-Z0-9_]*)\s+([0-9a-zA-Z_]+)\s*\])");
                    std::smatch match;
                    if (std::regex_search(current_line, match, array_regex)) {
                        std::string line = current_line;
                        std::string replaced = match[1].str() + "[" + match[2].str() + "]";
                        line.replace(match.position(0), match.length(0), replaced);
                        
                        delta.new_content = line;
                        s.confidence = 0.8f;
                        s.explanation = "An opening bracket '[' is missing for this array declaration or access.";
                        s.reason = "Arrays in C are accessed and declared using paired square brackets '[ ]'.";
                    }
                }
            }
            else if (error.error_code == "syntax-expected-brace") {
                if (error.error_message.find("']'") != std::string::npos) {
                    size_t comment_pos = current_line.find("//");
                    std::string code_part = (comment_pos == std::string::npos) ? current_line : current_line.substr(0, comment_pos);
                    std::string comment_part = (comment_pos == std::string::npos) ? "" : current_line.substr(comment_pos);

                    bool fixed = false;
                    int open_count = 0;
                    size_t unclosed_pos = std::string::npos;
                    for (size_t i = 0; i < code_part.length(); ++i) {
                        if (code_part[i] == '[') {
                            if (open_count == 0) unclosed_pos = i;
                            open_count++;
                        } else if (code_part[i] == ']') {
                            open_count--;
                            if (open_count == 0) unclosed_pos = std::string::npos;
                        }
                    }

                    if (open_count > 0 && unclosed_pos != std::string::npos) {
                        size_t insert_pos = code_part.find_first_of(";,=[", unclosed_pos + 1);
                        if (insert_pos != std::string::npos) {
                            size_t real_insert = insert_pos;
                            while (real_insert > unclosed_pos + 1 && std::isspace(code_part[real_insert - 1])) {
                                real_insert--;
                            }
                            code_part.insert(real_insert, "]");
                            fixed = true;
                        } else {
                            size_t last_content = code_part.find_last_not_of(" \t\r\n");
                            if (last_content != std::string::npos) {
                                code_part.insert(last_content + 1, "]");
                                fixed = true;
                            }
                        }
                    }

                    if (fixed) {
                        delta.new_content = code_part + comment_part;
                        s.confidence = 0.8f;
                        s.explanation = "A closing bracket ']' is missing for this array declaration or access.";
                        s.reason = "Arrays in C are accessed and declared using paired square brackets '[ ]'.";
                    }
                }
                else if (error.line_number > 1) {
                    std::string prev_line = lines[error.line_number - 2];
                    delta.line_number = error.line_number - 1;
                    delta.old_content = prev_line;
                    delta.new_content = prev_line + "\n}";
                    
                    s.confidence = 0.7f;
                    s.explanation = "A closing brace '}' appears to be missing to close a preceding block.";
                    s.reason = "Every opening brace '{' requires a matching closing brace '}' to properly delimit functions, loops, and conditions.";
                } else {
                    delta.new_content = "}\n" + current_line;
                    s.confidence = 0.6f;
                    s.explanation = "A closing brace '}' appears to be missing.";
                    s.reason = "Code blocks require matching braces.";
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
                } else {
                    s.fix.fix_type = "suggestion_only";
                    s.is_safe = false;
                    s.confidence = 0.4f;
                    s.explanation = "Likely Cause:\nMissing closing brace '}'\n\nSuspicious Location:\nNear end of file\n\nRelevant Source Snippet:\n" + Utils::trim(lines.back()) + "\n\nSuggested Example:\nint main() {\n    ...\n}";
                    s.reason = "The compiler reached the end of the file unexpectedly. This usually happens when a block or function is missing a closing brace.";
                    delta.new_content = ""; // Clear delta for Suggestion Only
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
    if (code == "syntax-expected-brace-eof" || code == "syntax-expected-brace") return true;
    std::string trimmed = Utils::trim(line);
    if (trimmed.empty() || trimmed.find("//") == 0 || trimmed.find("/*") == 0) return false;
    if (trimmed.find("#") == 0 && code != "syntax-malformed-preprocessor") return false;
    return true;
}
