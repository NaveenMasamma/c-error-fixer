#include "PatchManager.h"
#include "utils.h"
#include <sstream>
#include <iostream>
#include <map>

std::string PatchManager::generateUnifiedDiff(const std::string& filename, const std::vector<FixSuggestion>& suggestions) {
    std::stringstream ss;
    ss << "--- " << filename << "\n";
    ss << "+++ " << filename << " (fixed)\n";

    for (const auto& suggestion : suggestions) {
        for (const auto& delta : suggestion.deltas) {
            ss << "@@ -" << delta.line_number << " + " << delta.line_number << " @@\n";
            if (!delta.old_content.empty()) {
                ss << "-" << delta.old_content << "\n";
            }
            if (!delta.new_content.empty()) {
                // Handle multi-line insertions (common in IncludeFixer)
                std::stringstream content(delta.new_content);
                std::string segment;
                while(std::getline(content, segment)) {
                    ss << "+" << segment << "\n";
                }
            }
        }
    }
    return ss.str();
}

std::vector<std::string> PatchManager::applySuggestions(const std::vector<std::string>& original_lines, const std::vector<FixSuggestion>& suggestions) {
    std::vector<std::string> modified = original_lines;
    
    // Group suggestions by line to handle multiple modifications cumulatively
    std::map<int, std::vector<CodeDelta>> line_deltas;
    for (const auto& suggestion : suggestions) {
        for (const auto& delta : suggestion.deltas) {
            line_deltas[delta.line_number].push_back(delta);
        }
    }

    for (auto const& [line_num, deltas] : line_deltas) {
        int idx = line_num - 1;
        if (idx < 0 || idx >= (int)modified.size()) continue;

        std::string original_line = original_lines[idx];
        for (const auto& delta : deltas) {
            if (delta.old_content.empty()) {
                modified[idx] = delta.new_content;
            } else {
                // Corner Case: If multiple similar sub-strings exist, 
                // the current 'find' might be ambiguous. 
                // We prioritize full-line matching or the first significant occurrence.
                std::string target = modified[idx];
                size_t pos = target.find(delta.old_content);
                
                // If exact match is found, apply the replacement
                if (pos != std::string::npos) {
                    modified[idx].replace(pos, delta.old_content.length(), delta.new_content);
                } 
                // If exact match fails, check if the engine provided a full-line delta 
                // that was invalidated by a previous fix on this line.
                else if (delta.old_content == original_line) {
                    // Merging Logic Corner Case: Avoid double-application if two 
                    // engines suggested the same character addition.
                    if (delta.new_content.size() > delta.old_content.size()) {
                        if (delta.new_content.find(delta.old_content) == 0) {
                            // It was an append (e.g., adding a semicolon, quote, or brace)
                            std::string addition = delta.new_content.substr(delta.old_content.size());
                            
                            // Clean up addition to avoid duplicates like ';;'
                            addition = Utils::trim(addition);
                            if (addition.empty()) continue;

                            if (modified[idx].find(addition) == std::string::npos) {
                                modified[idx] += addition;
                            }
                        } else if (size_t suffix_pos = delta.new_content.find(delta.old_content); 
                                   suffix_pos != std::string::npos && suffix_pos > 0) {
                            // It was a prepend (e.g., adding a header or parenthesis)
                            std::string prefix = delta.new_content.substr(0, suffix_pos);
                            if (modified[idx].find(prefix) == std::string::npos) {
                                modified[idx] = prefix + modified[idx];
                            }
                        }
                    }
                }
            }
        }
    }
    return modified;
}

void PatchManager::printDetailedSuggestion(const FixSuggestion& suggestion) {
    std::cout << "\n" << Utils::Color::CYAN << "==================================================\n";
    std::cout << Utils::Color::BOLD << "Fix Suggestion Details\n";
    std::cout << Utils::Color::CYAN << "==================================================\n" << Utils::Color::RESET;

    std::cout << "\n" << Utils::Color::YELLOW << "Compiler Error:" << Utils::Color::RESET << "\n" << suggestion.error.error_message << "\n";

    std::cout << "\n" << Utils::Color::YELLOW << "Location:" << Utils::Color::RESET << "\n" << suggestion.error.file_path << ":" << suggestion.error.line_number << "\n";

    std::cout << "\n" << Utils::Color::YELLOW << "What it means:" << Utils::Color::RESET << "\n" 
              << (suggestion.explanation.empty() ? "No detailed explanation available." : suggestion.explanation) << "\n";

    std::cout << "\n" << Utils::Color::YELLOW << "Recommended Fix:" << Utils::Color::RESET << "\n" << suggestion.fix.fix_description << "\n";

    std::cout << "\n" << Utils::Color::YELLOW << "Reason:" << Utils::Color::RESET << "\n" 
              << (suggestion.reason.empty() ? "No specific reason provided." : suggestion.reason) << "\n";

    int confPct = static_cast<int>(suggestion.confidence * 100);
    std::string safetyColor = Utils::Color::RED;
    std::string safetyText = "Experimental";
    
    if (suggestion.confidence >= 0.9f) { safetyColor = Utils::Color::GREEN; safetyText = "Very Safe"; }
    else if (suggestion.confidence >= 0.7f) { safetyColor = Utils::Color::YELLOW; safetyText = "Safe"; }

    std::cout << "\n" << Utils::Color::YELLOW << "Confidence:" << Utils::Color::RESET << "\n" 
              << safetyColor << confPct << "% (" << safetyText << ")" << Utils::Color::RESET << "\n";

    std::cout << "\n" << Utils::Color::BOLD << "Patch Preview:" << Utils::Color::RESET << "\n";
    for (const auto& delta : suggestion.deltas) {
        if (!delta.old_content.empty()) {
            std::stringstream oldStream(delta.old_content);
            std::string line;
            while (std::getline(oldStream, line)) {
                std::cout << Utils::Color::RED << "- " << line << Utils::Color::RESET << "\n";
            }
        }
        if (!delta.new_content.empty()) {
            std::stringstream newStream(delta.new_content);
            std::string line;
            while (std::getline(newStream, line)) {
                std::cout << Utils::Color::GREEN << "+ " << line << Utils::Color::RESET << "\n";
            }
        }
    }
    std::cout << Utils::Color::CYAN << "==================================================\n" << Utils::Color::RESET;
}