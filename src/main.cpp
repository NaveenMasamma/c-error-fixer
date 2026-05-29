#include <iostream>
#include <string>
#include <vector>
#include "BatchProcessor.h"

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [-i|--interactive] [-d|--dry-run] [-v|--verbose] [--log <file>] [--show-errors-only] [--max-line-length <len>] [--timeout <sec>] <file1.c> [file2.c] [dir/]\n";
    std::cout << "Example: " << program_name << " -i -v src/ main.c\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::cout << "C Compilation Error Fixer Tool\n";
    std::cout << "==============================\n\n";

    std::vector<std::string> inputs;
    BatchConfig config;

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
