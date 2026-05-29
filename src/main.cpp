#include <iostream>
#include <string>
#include <vector>
#include "BatchProcessor.h"

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <file1.c> [file2.c] [dir/]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help             Show this help message and exit\n";
    std::cout << "  -i, --interactive      Prompt before applying each suggested fix\n";
    std::cout << "  -d, --dry-run          Analyze code and show fixes without applying them\n";
    std::cout << "  -v, --verbose          Show compiler output and detailed processing info\n";
    std::cout << "  --show-errors-only     Show only compiler errors in verbose output\n";
    std::cout << "  --log <file>           Write compiler output and logs to the given file\n";
    std::cout << "  --max-line-length <n>  Skip files with lines longer than <n> characters\n";
    std::cout << "  --timeout <sec>        Stop processing after <sec> seconds per file\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " main.c\n";
    std::cout << "  " << program_name << " --dry-run main.c\n";
    std::cout << "  " << program_name << " --verbose src/\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::vector<std::string> inputs;
    BatchConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-i" || arg == "--interactive") {
            config.interactive_mode = true;
        } else if (arg == "-d" || arg == "--dry-run") {
            config.dry_run = true;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose_mode = true;
        } else if (arg == "--show-errors-only") {
            config.show_errors_only = true;
        } else if (arg == "--log" && i + 1 < argc) {
            config.log_file_path = argv[++i];
        } else if (arg == "--timeout" && i + 1 < argc) {
            config.timeout_seconds = std::stoi(argv[++i]);
        } else if (arg == "--max-line-length" && i + 1 < argc) {
            config.max_line_length = std::stoul(argv[++i]);
        } else {
            inputs.push_back(arg);
        }
    }

    std::cout << "C Compilation Error Fixer Tool\n";
    std::cout << "==============================\n\n";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i" || arg == "--interactive") {
            config.interactive_mode = true;
        } else if (arg == "-d" || arg == "--dry-run") {
            config.dry_run = true;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose_mode = true;
        } else if (arg == "--show-errors-only") {
            config.show_errors_only = true;
        } else if (arg == "--log" && i + 1 < argc) {
            config.log_file_path = argv[++i];
        } else if (arg == "--timeout" && i + 1 < argc) {
            config.timeout_seconds = std::stoi(argv[++i]);
        } else if (arg == "--max-line-length" && i + 1 < argc) {
            config.max_line_length = std::stoul(argv[++i]);
        } else {
            inputs.push_back(arg);
        }
    }

    BatchProcessor processor;
    processor.process(inputs, config);
    processor.printSummary();

    return 0;
}
