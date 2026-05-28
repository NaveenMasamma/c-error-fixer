#include <iostream>
#include <string>
#include <vector>
#include "compiler_error_parser.h"
#include "code_fixer.h"

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <source_file.c>\n";
    std::cout << "Example: " << program_name << " program.c\n";
}

void printErrors(const std::vector<CompilerError>& errors) {
    if (errors.empty()) {
        std::cout << "\n✓ No compilation errors found!\n";
        return;
    }
    
    std::cout << "\n=== Found " << errors.size() << " compilation error(s) ===\n\n";
    
    for (size_t i = 0; i < errors.size(); ++i) {
        const auto& err = errors[i];
        std::cout << (i + 1) << ". Line " << err.line_number 
                  << ", Column " << err.column << "\n";
        std::cout << "   Type: " << err.error_code << "\n";
        std::cout << "   Message: " << err.error_message << "\n\n";
    }
}

void printFixSuggestions(const std::vector<FixSuggestion>& suggestions) {
    if (suggestions.empty()) {
        std::cout << "No automatic fixes available.\n";
        return;
    }
    
    std::cout << "\n=== Fix Suggestions ===\n\n";
    
    for (size_t i = 0; i < suggestions.size(); ++i) {
        const auto& suggestion = suggestions[i];
        std::cout << (i + 1) << ". Line " << suggestion.error.line_number << "\n";
        std::cout << "   Problem: " << suggestion.error.error_message << "\n";
        std::cout << "   Fix: " << suggestion.fix.fix_description << "\n";
        std::cout << "   Type: " << suggestion.fix.fix_type << "\n";
        std::cout << "   Safe to auto-apply: " << (suggestion.is_safe ? "Yes" : "No") << "\n\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string c_file = argv[1];
    
    std::cout << "C Compilation Error Fixer Tool\n";
    std::cout << "==============================\n\n";
    std::cout << "Analyzing file: " << c_file << "\n";
    
    // Step 1: Compile and capture errors
    CompilerErrorParser parser;
    std::string compiler_output;
    
    std::cout << "Compiling...\n";
    parser.compileFile(c_file, compiler_output);
    
    // Step 2: Parse errors
    auto errors = parser.parseErrors(compiler_output);
    printErrors(errors);
    
    if (errors.empty()) {
        return 0;
    }
    
    // Step 3: Generate fix suggestions
    CodeFixer fixer;
    auto suggestions = fixer.generateFixes(c_file, errors);
    printFixSuggestions(suggestions);
    
    // Step 4: Ask user to apply fixes
    if (!suggestions.empty()) {
        std::cout << "Apply suggested fixes? (y/n): ";
        char response;
        std::cin >> response;
        
        if (response == 'y' || response == 'Y') {
            std::cout << "Applying fixes...\n";
            fixer.applyAllFixes(c_file, suggestions);
            
            // Recompile to verify
            std::cout << "Re-compiling to verify fixes...\n";
            compiler_output.clear();
            parser.compileFile(c_file, compiler_output);
            auto new_errors = parser.parseErrors(compiler_output);
            
            std::cout << "\n=== After fixes ===\n";
            printErrors(new_errors);
            
            if (new_errors.size() < errors.size()) {
                std::cout << "✓ Fixed " << (errors.size() - new_errors.size()) 
                         << " error(s)!\n";
            }
        }
    }
    
    return 0;
}
