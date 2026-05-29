#ifndef IFIX_ENGINE_H
#define IFIX_ENGINE_H

#include <string>
#include <vector>
#include "compiler_error_parser.h"
#include "code_analyzer.h"
#include "error_pattern_db.h"

#include "FixSuggestion.h"

class IFixEngine {
public:
    virtual ~IFixEngine() = default;
    virtual bool canHandle(const std::string& error_code) const = 0;
    virtual std::vector<FixSuggestion> generateSuggestions(const CompilerError& error, const std::vector<std::string>& lines, CodeAnalyzer& analyzer, ErrorPatternDB& db) = 0;
};

#endif