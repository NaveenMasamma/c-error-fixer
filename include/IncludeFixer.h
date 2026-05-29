#ifndef INCLUDE_FIXER_H
#define INCLUDE_FIXER_H

#include "IFixEngine.h"
#include "utils.h"

class IncludeFixer : public IFixEngine {
public:
    bool canHandle(const std::string& error_code) const override;
    std::vector<FixSuggestion> generateSuggestions(const CompilerError& error, 
                                                   const std::vector<std::string>& lines, 
                                                   CodeAnalyzer& analyzer, 
                                                   ErrorPatternDB& db) override;
};

#endif