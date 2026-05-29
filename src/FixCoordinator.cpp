#include "FixCoordinator.h"

std::vector<FixSuggestion> FixCoordinator::getBestSuggestions(const CompilerError& error, const std::vector<std::string>& lines, CodeAnalyzer& analyzer, ErrorPatternDB& db) {
    std::vector<FixSuggestion> all_suggestions;

    for (auto& engine : engines) {
        if (engine->canHandle(error.error_code)) {
            auto engine_results = engine->generateSuggestions(error, lines, analyzer, db);
            all_suggestions.insert(all_suggestions.end(), engine_results.begin(), engine_results.end());
        }
    }

    // Rank suggestions by confidence score descending
    std::sort(all_suggestions.begin(), all_suggestions.end(), 
        [](const FixSuggestion& a, const FixSuggestion& b) {
            return a.confidence > b.confidence;
        }
    );

    return all_suggestions;
}