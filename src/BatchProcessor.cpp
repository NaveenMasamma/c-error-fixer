#include "BatchProcessor.h"
#include "PatchManager.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cstdlib>
#include <tuple>

static std::unordered_map<std::string, int> g_remaining_errors;
static std::unordered_map<std::string, int> g_remaining_warnings;

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
    int current_strict_errors = 0;
    int current_warnings = 0;
    std::vector<CompilerError> last_errors;

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
        last_errors = errors;
        current_strict_errors = 0;
        current_warnings = 0;
        for (const auto& e : errors) {
            if (e.error_type.find("warning") != std::string::npos) current_warnings++;
            else current_strict_errors++;
        }
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
            if (current_strict_errors > 0) {
                std::cout << "\n" << Utils::Color::YELLOW << "Compiler Diagnostics:" << Utils::Color::RESET << "\n";
                for (const auto& error : errors) {
                    std::cout << error.error_message << "\n";
                }
                std::cout << Utils::Color::YELLOW << "Status:" << Utils::Color::RESET << "\n"
                          << "No automatic fix is currently available." << "\n";
                std::cout << Utils::Color::YELLOW << "Reason:" << Utils::Color::RESET << "\n"
                          << "This error type is not yet supported by the tool." << "\n";
                std::cout << Utils::Color::YELLOW << "Suggestion:" << Utils::Color::RESET << "\n"
                          << "Review the compiler output manually." << "\n";
            }
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
                if (s.deltas.empty()) {
                    std::cout << "This diagnostic is ambiguous.\n"
                              << "No automatic fix is available.\n"
                              << "Please review the suggested location manually.\n";
                    continue;
                }
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
                if (s.is_safe && !s.deltas.empty()) safe_fixes.push_back(s);
            }

            if (!safe_fixes.empty()) {
                if (fixer.applyAllFixes(file_path, safe_fixes)) {
                    fs.errors_fixed += safe_fixes.size();
                    file_changed = true;
                }
            }

            // Show unsafe suggestions even if we don't apply them
            for (const auto& s : suggestions) {
                if (!s.is_safe || s.deltas.empty()) {
                    PatchManager::printDetailedSuggestion(s);
                    if (s.deltas.empty()) {
                        std::cout << "This diagnostic is ambiguous.\n"
                                  << "No automatic fix is available.\n"
                                  << "Please review the suggested location manually.\n";
                    }
                }
            }
        }
        pass++;

        if (file_changed && pass < MAX_PASSES) {
            std::cout << "[Pass " << pass << "] Recompiling after applying fixes...\n";
        }
    }

    if (current_strict_errors == 0) {
        std::cout << "\n" << Utils::Color::GREEN << "Compilation successful." << Utils::Color::RESET << "\n";
        if (current_warnings > 0) {
            std::cout << "\n" << Utils::Color::YELLOW << "Warnings:" << Utils::Color::RESET << "\n";
            for (const auto& error : last_errors) {
                if (error.error_type.find("warning") != std::string::npos) {
                    std::cout << "* " << error.error_message << "\n";
                }
            }
        }
    }

    g_remaining_errors[file_path] = current_strict_errors;
    g_remaining_warnings[file_path] = current_warnings;
    return true;
}

void BatchProcessor::printSummary() const {
    std::cout << "\n" << Utils::Color::CYAN << "==========================================\n";
    std::cout << Utils::Color::BOLD << "          BATCH PROCESSING REPORT " 
              << (current_config.dry_run ? Utils::Color::YELLOW + "(DRY RUN)" : "") << "\n";
    std::cout << Utils::Color::CYAN << "==========================================\n" << Utils::Color::RESET;
    
    std::cout << std::left << std::setw(40) << "File" << std::setw(15) << "Errors Fixed" << std::setw(20) << "Errors Remaining" << std::setw(10) << "Warnings" << "\n";
    std::cout << std::string(85, '-') << "\n";

    int total_fixed = 0;
    int total_remaining_errors = 0;
    int total_warnings = 0;

    for (const auto& [file, fs] : stats) {
        int rem = g_remaining_errors[file];
        int warns = g_remaining_warnings[file];
        std::cout << std::left << std::setw(40) << file 
                  << std::setw(15) << fs.errors_fixed 
                  << std::setw(20) << rem 
                  << std::setw(10) << warns << "\n";
        total_fixed += fs.errors_fixed;
        total_remaining_errors += rem;
        total_warnings += warns;
    }

    std::cout << std::string(85, '-') << "\n";
    std::cout << Utils::Color::BOLD << "TOTAL:\n" 
              << "Errors Fixed: " << total_fixed << "\n"
              << "Errors Remaining: " << total_remaining_errors << "\n"
              << "Warnings: " << total_warnings << Utils::Color::RESET << "\n";

    if (std::getenv("C_ERROR_FIXER_VALIDATE")) {
        int total_files = stats.size();
        int auto_fix_pass = 0;
        int suggestion_pass = 0;
        int failed = 0;

        std::unordered_map<std::string, bool> expects_autofix;
        std::ifstream csv("validation_results.csv");
        if (!csv.is_open()) csv.open("validation/validation_results.csv");
        if (csv.is_open()) {
            std::string csv_line;
            while (std::getline(csv, csv_line)) {
                for (const auto& [file, fs] : stats) {
                    std::string basename = file.substr(file.find_last_of("/\\") + 1);
                    if (csv_line.find(basename) != std::string::npos) {
                        if (csv_line.find(",Yes,") != std::string::npos) expects_autofix[file] = true;
                        else if (csv_line.find(",No,") != std::string::npos) expects_autofix[file] = false;
                    }
                }
            }
        }

        std::vector<std::tuple<std::string, int, int, std::string>> report_rows;
        for (const auto& [file, fs] : stats) {
            int rem = g_remaining_errors[file];
            bool expected = true;
            if (expects_autofix.find(file) != expects_autofix.end()) {
                expected = expects_autofix[file];
            }

            std::string status;
            if (rem == 0) {
                status = "✅ AUTO_FIX_PASS";
                auto_fix_pass++;
            } else if (!expected) {
                status = "✅ SUGGESTION_PASS";
                suggestion_pass++;
            } else {
                status = "❌ FAIL";
                failed++;
            }
            report_rows.push_back({file, fs.errors_fixed, rem, status});
        }
        int passed = auto_fix_pass + suggestion_pass;
        double success_rate = total_files > 0 ? (passed * 100.0 / total_files) : 0.0;

        std::ofstream out("validation_report.md");
        if (out.is_open()) {
            out << "# Validation Report\n\n";
            out << "## Summary\n\n";
            out << "- **Total Files:** " << total_files << "\n";
            out << "- **Auto-Fix Pass:** " << auto_fix_pass << "\n";
            out << "- **Suggestion Pass:** " << suggestion_pass << "\n";
            out << "- **Failed:** " << failed << "\n";
            out << "- **Errors Fixed:** " << total_fixed << "\n";
            out << "- **Unsupported Errors:** " << total_remaining_errors << "\n";
            out << "- **Success Rate:** " << std::fixed << std::setprecision(1) << success_rate << "%\n\n";

            out << "## Per-File Results\n\n";
            out << "| File | Errors Fixed | Remaining Errors | Status |\n";
            out << "|------|--------------|------------------|--------|\n";
            
            for (const auto& row : report_rows) {
                out << "| `" << std::get<0>(row) << "` | " 
                    << std::get<1>(row) << " | " << std::get<2>(row) << " | " << std::get<3>(row) << " |\n";
            }
            out.close();
            std::cout << "\nValidation report generated: validation_report.md\n";
        }
    }
}