#ifndef FIX_COORDINATOR_H
#define FIX_COORDINATOR_H

#include <vector>
#include <memory>
#include <algorithm>
#include "IFixEngine.h"
#include "code_fixer.h"

class FixCoordinator {
public:
    FixCoordinator() = default;

    // Register an engine (Dependency Injection)
    void registerEngine(std::unique_ptr<IFixEngine> engine) {
        engines.push_back(std::move(engine));
    }

    std::vector<FixSuggestion> getBestSuggestions(const CompilerError& error, const std::vector<std::string>& lines, CodeAnalyzer& analyzer, ErrorPatternDB& db);

private:
    std::vector<std::unique_ptr<IFixEngine>> engines;
};

#endif