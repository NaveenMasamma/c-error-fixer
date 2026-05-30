#include "code_fixer.h"
#include "code_analyzer.h"
#include "FixCoordinator.h"
#include "PatchManager.h"
#include "IFixEngine.h"
#include "IncludeFixer.h"
#include "SyntaxFixer.h"
#include "TypoFixer.h"
#include "TypeMismatchFixer.h"

bool CodeFixer::backup_enabled = true;

void CodeFixer::setBackupEnabled(bool enabled) {
    backup_enabled = enabled;
}

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
        for (const auto& candidate : best) {
            bool duplicate = false;
            for (const auto& existing : suggestions) {
                if (existing.fix.fix_type != candidate.fix.fix_type ||
                    existing.deltas.size() != candidate.deltas.size()) {
                    continue;
                }

                duplicate = true;
                for (size_t i = 0; i < existing.deltas.size(); ++i) {
                    if (existing.deltas[i].line_number != candidate.deltas[i].line_number ||
                        existing.deltas[i].old_content != candidate.deltas[i].old_content ||
                        existing.deltas[i].new_content != candidate.deltas[i].new_content) {
                        duplicate = false;
                        break;
                    }
                }

                if (duplicate) break;
            }

            if (!duplicate) suggestions.push_back(candidate);
        }
    }
    return suggestions;
}

bool CodeFixer::applyFix(const std::string& file_path, const FixSuggestion& suggestion) {
    auto original_lines = Utils::readFile(file_path);
    if (original_lines.empty()) return false;

    auto new_lines = PatchManager::applySuggestions(original_lines, {suggestion});
    if (new_lines == original_lines) return false;

    backupFile(file_path);
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

    if (safe_fixes.empty()) return false;

    auto new_lines = PatchManager::applySuggestions(original_lines, safe_fixes);
    if (new_lines == original_lines) return false;

    backupFile(file_path);
    return Utils::writeFile(file_path, new_lines);
}

bool CodeFixer::backupFile(const std::string& file_path) {
    if (!backup_enabled) return true;
    if (backed_up_files.find(file_path) != backed_up_files.end()) return true;
    
    bool result = Utils::backupFile(file_path);
    if (result) backed_up_files.insert(file_path);
    return result;
}
