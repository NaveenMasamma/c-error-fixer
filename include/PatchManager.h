#ifndef PATCH_MANAGER_H
#define PATCH_MANAGER_H

#include <string>
#include <vector>
#include "code_fixer.h"

class PatchManager {
public:
    // Generates a unified diff string for preview
    static std::string generateUnifiedDiff(const std::string& filename, const std::vector<FixSuggestion>& suggestions);
    
    // Applies suggestions to a buffer of strings
    static std::vector<std::string> applySuggestions(const std::vector<std::string>& original_lines, const std::vector<FixSuggestion>& suggestions);

    // Prints a detailed, beginner-friendly report of a single fix suggestion
    static void printDetailedSuggestion(const FixSuggestion& suggestion);
};

#endif