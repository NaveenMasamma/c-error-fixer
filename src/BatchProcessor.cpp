#include "BatchProcessor.h"
#include "PatchManager.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <sstream>

BatchProcessor::BatchProcessor() {}

void BatchProcessor::process(const std::vector<std::string>& target_paths, const BatchConfig& config) {
    auto files = Utils::collectSourceFiles(target_paths);
    current_config = config;
    
    std::cout << "Found " << files.size() << " files to process.\n\n";

    for (const auto& file : files) {
        std::cout << "[Processing] " << file << "... ";
        if (processSingleFile(file, config)) {
            std::cout << (config.dry_run ? Utils::Color::YELLOW + "Analyzed.\n" + Utils::Color::RESET 
                                 : Utils::Color::GREEN + "Done.\n" + Utils::Color::RESET);
        } else {
            std::cout << Utils::Color::RED + "Failed.\n" + Utils::Color::RESET;
        }
    }
}

bool BatchProcessor::processSingleFile(const std::string& file_path, const BatchConfig& config) {
    FileStats& fs = stats[file_path];
    fs.processed = true;
    auto startTime = std::chrono::steady_clock::now();
    int pass = 0;
    const int MAX_PASSES = 5;
    bool file_changed = true;

    while (file_changed && pass < MAX_PASSES) {
        if (config.timeout_seconds > 0) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count() >= config.timeout_seconds) {
                fs.error_log = "Processing timed out after " + std::to_string(pass) + " passes.";
                return false;
            }
        }

        file_changed = false;
        std::string output;
        if (!parser.compileFile(file_path, output)) {
            std::cerr << Utils::Color::RED << "[Error] " << output << Utils::Color::RESET << std::endl;
            fs.error_log = output;
            return false;
        }

        if (config.verbose_mode) {
            std::string text_to_show = output;
            if (config.show_errors_only) {
                std::string filtered;
                std::istringstream iss(output);
                std::string line_text;
                while (std::getline(iss, line_text)) {
                    if (line_text.find("error:") != std::string::npos || 
                        line_text.find("warning:") != std::string::npos ||
                        line_text.find("fatal error:") != std::string::npos) {
                        filtered += line_text + "\n";
                    }
                }
                text_to_show = filtered;
            }

            if (!config.log_file_path.empty()) {
                std::ofstream log(config.log_file_path, std::ios::app);
                if (log.is_open()) {
                    log << "[File: " << file_path << "][Pass " << pass + 1 << "]\n" 
                        << text_to_show << "\n" << std::string(40, '-') << "\n";
                }
            } else {
                std::cout << "\n" << Utils::Color::BLUE << "[Pass " << pass + 1 << " Compiler Output]" << Utils::Color::RESET << "\n"
                          << text_to_show << "\n";
            }
        }

        auto errors = parser.parseErrors(output);
        if (pass == 0) stats[file_path].errors_found = errors.size();
        
        if (errors.empty()) break;

        auto lines = Utils::readFile(file_path);
        if (lines.empty()) {
            stats[file_path].error_log = "Read failure: Source file could not be read.";
            return false;
        }

        // Safety check: skip files that appear to be minified
        for (const auto& line : lines) {
            if (line.length() > config.max_line_length) {
                stats[file_path].error_log = "Skipped: Line exceeds max length.";
                return false;
            }
        }

        auto suggestions = fixer.generateFixes(file_path, errors);
        if (suggestions.empty() && !errors.empty()) {
            std::cout << "\n" << Utils::Color::YELLOW << "Compiler Error:" << Utils::Color::RESET << "\n";
            for (const auto& error : errors) {
                std::cout << error.error_message << "\n";
            }
            std::cout << Utils::Color::YELLOW << "Status:" << Utils::Color::RESET << "\n"
                      << "No automatic fix is currently available." << "\n";
            std::cout << Utils::Color::YELLOW << "Reason:" << Utils::Color::RESET << "\n"
                      << "This error type is not yet supported by the tool." << "\n";
            std::cout << Utils::Color::YELLOW << "Suggestion:" << Utils::Color::RESET << "\n"
                      << "Review the compiler output manually." << "\n";
            break;
        }

        if (config.dry_run) {
            std::cout << "\n" << Utils::Color::YELLOW << Utils::Color::BOLD 
                      << "[Dry Run] Found potential fixes for " << file_path << ":" << Utils::Color::RESET << "\n";
            for (const auto& s : suggestions) {
                PatchManager::printDetailedSuggestion(s);
                fs.errors_fixed++;
            }
            return true; // No looping in dry run
        }

        if (config.interactive_mode) {
            for (const auto& s : suggestions) {
                PatchManager::printDetailedSuggestion(s);
                std::cout << "Apply this fix? (y/n): ";
                char response;
                std::cin >> response;
                if (response == 'y' || response == 'Y') {
                    if (fixer.applyFix(file_path, s)) {
                        fs.errors_fixed++;
                        file_changed = true;
                        std::cout << "Fix applied successfully.\n";
                    } else {
                        std::cout << "Failed to apply fix.\n";
                    }
                }
            }
        } else {
            std::vector<FixSuggestion> safe_fixes;
            for (const auto& s : suggestions) {
                if (s.is_safe) safe_fixes.push_back(s);
            }

            if (!safe_fixes.empty()) {
                if (fixer.applyAllFixes(file_path, safe_fixes)) {
                    fs.errors_fixed += safe_fixes.size();
                    file_changed = true;
                }
            }

            // Show unsafe suggestions even if we don't apply them
            for (const auto& s : suggestions) {
                if (!s.is_safe) {
                    PatchManager::printDetailedSuggestion(s);
                }
            }
        }
        pass++;

        if (file_changed && pass < MAX_PASSES) {
            std::cout << "[Pass " << pass << "] Fixes applied, re-analyzing for cascading errors...\n";
        }
    }

    return true;
}

void BatchProcessor::printSummary() const {
    std::cout << "\n" << Utils::Color::CYAN << "==========================================\n";
    std::cout << Utils::Color::BOLD << "          BATCH PROCESSING REPORT " 
              << (current_config.dry_run ? Utils::Color::YELLOW + "(DRY RUN)" : "") << "\n";
    std::cout << Utils::Color::CYAN << "==========================================\n" << Utils::Color::RESET;
    
    std::cout << std::left << std::setw(40) << "File" << std::setw(10) << "Found" << std::setw(10) << "Fixed" << "\n";
    std::cout << std::string(60, '-') << "\n";

    int total_found = 0;
    int total_fixed = 0;

    for (const auto& [file, fs] : stats) {
        std::cout << std::left << std::setw(40) << file 
                  << std::setw(10) << fs.errors_found 
                  << std::setw(10) << fs.errors_fixed << "\n";
        total_found += fs.errors_found;
        total_fixed += fs.errors_fixed;
    }

    std::cout << std::string(60, '-') << "\n";
    std::cout << Utils::Color::BOLD << "TOTAL: " << stats.size() << " files | " 
              << total_found << " errors found | " 
              << (total_fixed > 0 ? Utils::Color::GREEN : "") << total_fixed 
              << (current_config.dry_run ? " potential fixes" : " errors fixed") << Utils::Color::RESET << "\n";
}