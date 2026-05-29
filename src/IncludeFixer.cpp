#include "code_fixer.h"
#include "IncludeFixer.h"
#include "utils.h"
#include <regex>

bool IncludeFixer::canHandle(const std::string& error_code) const {
    return error_code == "implicit-function-declaration" || 
           error_code == "undeclared-identifier" || 
           error_code == "missing-include";
}

std::vector<FixSuggestion> IncludeFixer::generateSuggestions(const CompilerError& error, const std::vector<std::string>& lines, CodeAnalyzer& analyzer, ErrorPatternDB& db) {
        std::vector<FixSuggestion> suggestions;
        std::string func;
        std::smatch m;
        // Prefer function names inside single quotes: "function 'name'"
        std::regex func_re("function '([a-zA-Z_][a-zA-Z0-9_]*)'");
        std::regex quoted_re("'([a-zA-Z_][a-zA-Z0-9_]*)'");

        if (std::regex_search(error.error_message, m, func_re)) {
            func = m[1].str();
        } else if (std::regex_search(error.error_message, m, quoted_re)) {
            func = m[1].str();
        }

        std::string header = db.lookupHeaderForFunction(func);
        auto dbfixes = db.getSuggestions(error.error_code, error.error_message);

        auto createSuggestion = [&](const std::string& h) {
            if (!analyzer.isHeaderIncluded(h)) {
                FixSuggestion s;
                s.error = error;
                s.fix.fix_type = "include_header";
                s.fix.fix_description = "Add #include <" + h + ">";
                s.fix.suggested_includes = {h};
                s.is_safe = true;
                s.confidence = 0.95f; // Very Safe for standard headers
                s.explanation = "The compiler found a reference to '" + func + "' but couldn't find its definition. This usually happens when a standard library header is missing.";
                s.reason = "The function '" + func + "' is a standard C function defined in the <" + h + "> header file. Including it gives the compiler the necessary blueprints.";

                CodeDelta delta;
                
                // Refactored Logic: Find the first include line or the first code line to preserve license headers
                int first_include = -1;
                int first_code = -1;
                int last_comment = -1;
                bool in_block_comment = false;

                for (int i = 0; i < (int)lines.size(); ++i) {
                    std::string trimmed = Utils::trim(lines[i]);
                    if (trimmed.empty()) continue;

                    if (!in_block_comment && trimmed.find("/*") == 0) {
                        in_block_comment = true;
                        last_comment = i;
                        if (trimmed.find("*/") != std::string::npos) in_block_comment = false;
                        continue;
                    }
                    if (in_block_comment) {
                        last_comment = i;
                        if (trimmed.find("*/") != std::string::npos) in_block_comment = false;
                        continue;
                    }
                    if (trimmed.find("//") == 0) {
                        last_comment = i;
                        continue;
                    }

                    if (trimmed.find("#include") == 0) {
                        if (first_include == -1) first_include = i;
                    } else {
                        if (first_code == -1) first_code = i;
                    }
                }

                int target_idx = 0;
                bool prepend = true;

                if (first_include != -1) {
                    target_idx = first_include;
                } else if (first_code != -1) {
                    target_idx = first_code;
                } else if (last_comment != -1) {
                    target_idx = last_comment;
                    prepend = false;
                }

                delta.line_number = target_idx + 1;
                if (target_idx < (int)lines.size()) {
                    delta.old_content = lines[target_idx];
                    if (prepend) {
                        delta.new_content = "#include <" + h + ">\n" + delta.old_content;
                    } else {
                        delta.new_content = delta.old_content + "\n#include <" + h + ">";
                    }
                } else {
                    delta.old_content = "";
                    delta.new_content = "#include <" + h + ">\n";
                }
                
                s.deltas.push_back(delta);
                suggestions.push_back(s);
            }
        };

        if (!header.empty()) createSuggestion(header);
        for (const auto& dbf : dbfixes) {
            for (const auto& h : dbf.suggested_includes) {
                if (h != header) {
                    createSuggestion(h);
                }
            }
        }

        return suggestions;
    }