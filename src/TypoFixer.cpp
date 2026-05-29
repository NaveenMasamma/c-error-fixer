#include "code_fixer.h"
#include "TypoFixer.h"
#include "utils.h"
#include <regex>

bool TypoFixer::canHandle(const std::string& error_code) const {
    return error_code == "syntax-keyword-typo";
}

std::vector<FixSuggestion> TypoFixer::generateSuggestions(const CompilerError& error, const std::vector<std::string>& lines, CodeAnalyzer& analyzer, ErrorPatternDB& db) {
    std::vector<FixSuggestion> suggestions;
    if (error.line_number < 1 || error.line_number > (int)lines.size()) return suggestions;

    std::string line = lines[error.line_number - 1];
    std::string replacement;
    std::string matched_typo;
    
    if (findBestKeywordMatch(line, replacement, matched_typo)) {
        FixSuggestion s;
        s.error = error;
        s.fix.fix_type = "syntax_fix";
        s.fix.fix_description = "Fix keyword typo to '" + replacement + "'";
        s.is_safe = true;
        s.confidence = 0.85f; // High confidence for single-character distance
        s.explanation = "It looks like there is a small typo in a C keyword. The compiler doesn't recognize the word as written.";
        s.reason = "C keywords like '" + replacement + "' must be spelled exactly and are case-sensitive. Swapping the typo for the correct keyword should fix the error.";
        
        CodeDelta delta;
        delta.line_number = error.line_number;
        delta.old_content = line;

        // Find the specific matched typo and replace it at its position
        size_t pos = line.find(matched_typo);
        if (pos != std::string::npos) {
            delta.new_content = line;
            delta.new_content.replace(pos, matched_typo.length(), replacement);
            s.deltas.push_back(delta);
            suggestions.push_back(s);
        }
    }
    return suggestions;
}

bool TypoFixer::findBestKeywordMatch(const std::string& line, std::string& best_match, std::string& matched_typo) {
    static const std::vector<std::string> keywords = {
        "int", "return", "float", "double", "char", "if", "else", "while", "for", "struct"
    };
    
    // Fallback: common handwritten typos map (check first to prefer explicit mistakes)
    static const std::vector<std::pair<std::string,std::string>> common = {
        {"retun", "return"},
        {"forr", "for"},
        {"whlie", "while"},
        {"sturct", "struct"},
        {"esle", "else"}
    };
    for (const auto& p : common) {
        size_t pos = line.find(p.first);
        if (pos != std::string::npos) {
            matched_typo = p.first;
            best_match = p.second;
            return true;
        }
    }

    std::regex token_regex(R"(([a-zA-Z_][a-zA-Z0-9_]*))");
    std::sregex_iterator it(line.begin(), line.end(), token_regex), end;
    for (; it != end; ++it) {
        std::string token = it->str();
        for (const auto& kw : keywords) {
            if (Utils::levenshteinDistance(token, kw) == 1) {
                matched_typo = token;
                best_match = kw;
                return true;
            }
        }
    }
    return false;
}