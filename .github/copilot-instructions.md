# C Compilation Error Fixer Tool

## Project Overview
This tool automatically detects, analyzes, and fixes C compilation errors. It captures compiler output, matches patterns against a knowledge base, suggests fixes, and applies them with user confirmation.

## Key Features
- Automatic compiler error capture from gcc/clang
- Pattern-based error recognition
- Intelligent fix suggestions
- Interactive mode with user confirmation
- Re-compilation verification
- Fix history logging

## Build & Run
```bash
cd build
cmake ..
make
./c_error_fixer <source_file.c>
```

## Project Structure
- `src/` - Implementation files
- `include/` - Header files
- `tests/` - Test cases and sample C files with errors
- `build/` - CMake build directory
