#ifndef TYPO_FIXER_H
#define TYPO_FIXER_H

#include "IFixEngine.h"
#include "utils.h"

class TypoFixer : public IFixEngine {
public:
    bool canHandle(const std::string& error_code) const override;
    std::vector<FixSuggestion> generateSuggestions(const CompilerError& error, 
                                                   const std::vector<std::string>& lines, 
                                                   CodeAnalyzer& analyzer, 
                                                   ErrorPatternDB& db) override;
private:
    bool findBestKeywordMatch(const std::string& line, std::string& best_match, std::string& matched_typo);
};

#endif