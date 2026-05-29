#ifndef FIX_SUGGESTION_H
#define FIX_SUGGESTION_H

#include <string>
#include <vector>
#include "compiler_error_parser.h"
#include "error_pattern_db.h"

struct CodeDelta {
    int line_number;             // 1-indexed
    std::string old_content;     // The line as it was
    std::string new_content;     // The line as it should be (can contain \n)
};

struct FixSuggestion {
    CompilerError error;
    ErrorFix fix;
    std::vector<CodeDelta> deltas;
    bool is_safe;               // Whether fix can be auto-applied
    float confidence;           // Score from 0.0 to 1.0
    std::string explanation;    // Beginner-friendly explanation of the error
    std::string reason;         // Logic behind why the fix is effective
};

#endif // FIX_SUGGESTION_H
