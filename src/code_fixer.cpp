#include "code_fixer.h"
#include "code_analyzer.h"
#include "FixCoordinator.h"
#include "PatchManager.h"
#include "IFixEngine.h"
#include "IncludeFixer.h"
#include "SyntaxFixer.h"
#include "TypoFixer.h"
#include "TypeMismatchFixer.h"

CodeFixer::CodeFixer() {
    auto coordinator = std::make_unique<FixCoordinator>();
    coordinator->registerEngine(std::make_unique<IncludeFixer>());
    coordinator->registerEngine(std::make_unique<SyntaxFixer>());
    coordinator->registerEngine(std::make_unique<TypoFixer>());
    coordinator->registerEngine(std::make_unique<TypeMismatchFixer>());
    this->coordinator = std::move(coordinator);
}

CodeFixer::~CodeFixer() = default;

std::vector<FixSuggestion> CodeFixer::generateFixes(const std::string& file_path,
                                                    const std::vector<CompilerError>& errors) {
    std::vector<FixSuggestion> suggestions;
    CodeAnalyzer analyzer;
    analyzer.analyzeFile(file_path);
    auto lines = Utils::readFile(file_path);
    
    for (const auto& error : errors) {
        auto best = coordinator->getBestSuggestions(error, lines, analyzer, pattern_db);
        suggestions.insert(suggestions.end(), best.begin(), best.end());
    }
    return suggestions;
}

bool CodeFixer::applyFix(const std::string& file_path, const FixSuggestion& suggestion) {
    auto original_lines = Utils::readFile(file_path);
    if (original_lines.empty()) return false;

    auto new_lines = PatchManager::applySuggestions(original_lines, {suggestion});
    return Utils::writeFile(file_path, new_lines);
}

bool CodeFixer::applyAllFixes(const std::string& file_path,
                             const std::vector<FixSuggestion>& suggestions) {
    auto original_lines = Utils::readFile(file_path);
    if (original_lines.empty()) return false;

    std::vector<FixSuggestion> safe_fixes;
    for (const auto& suggestion : suggestions) {
        if (suggestion.is_safe) safe_fixes.push_back(suggestion);
    }

    if (safe_fixes.empty()) return true;

    Utils::backupFile(file_path);
    auto new_lines = PatchManager::applySuggestions(original_lines, safe_fixes);
    return Utils::writeFile(file_path, new_lines);
}
