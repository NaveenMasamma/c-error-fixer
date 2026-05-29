#ifndef BATCH_PROCESSOR_H
#define BATCH_PROCESSOR_H

#include <string>
#include <vector>
#include <map>
#include "code_fixer.h"

struct FileStats {
    int errors_found = 0;
    int errors_fixed = 0;
    bool processed = false;
    std::string error_log;
};

struct BatchConfig {
    bool interactive_mode = false;
    bool dry_run = false;
    bool verbose_mode = false;
    bool show_errors_only = false;
    std::string log_file_path;
    size_t max_line_length = 500;
    int timeout_seconds = 0;
};

class BatchProcessor {
public:
    BatchProcessor();
    void process(const std::vector<std::string>& target_paths, const BatchConfig& config);
    void printSummary() const;

private:
    bool processSingleFile(const std::string& file_path, const BatchConfig& config);
    
    std::map<std::string, FileStats> stats;
    CodeFixer fixer;
    CompilerErrorParser parser;
    BatchConfig current_config;
};

#endif