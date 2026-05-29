#ifndef SYNTAX_FIXER_H
#define SYNTAX_FIXER_H

#include "IFixEngine.h"
#include "utils.h"

class SyntaxFixer : public IFixEngine {
public:
    bool canHandle(const std::string& error_code) const override;
    std::vector<FixSuggestion> generateSuggestions(const CompilerError& error, 
                                                   const std::vector<std::string>& lines, 
                                                   CodeAnalyzer& analyzer, 
                                                   ErrorPatternDB& db) override;
private:
    bool isSafeToModify(const std::string& line, const std::string& code);
};

#endif