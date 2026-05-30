# C Compilation Error Fixer Tool

A practical C++ command-line utility that detects common C compilation errors and suggests safe fixes. This tool helps C developers fix compiler issues faster while preserving the original source in backups.

## Problem Statement

C developers often spend time decoding compiler diagnostics and applying repetitive fixes manually. This tool analyzes compiler output and proposes source changes for frequent errors, reducing time spent on syntax, include, and type issues.

## Features

- Compiles C source files and captures compiler diagnostics
- Parses common GCC-style error output
- Suggests fixes for missing includes, implicit declarations, syntax faults, and type mismatches
- Interactive mode asks before applying each suggested fix
- Creates timestamped backups before modifying source files
- Re-compiles files after applying fixes to verify success

## Supported Error Types

- Missing includes (`#include` directives)
- Implicit function declarations
- Undefined references
- Undeclared identifiers
- Syntax errors such as missing semicolons or mismatched brackets
- Common type mismatches

## Building

### Prerequisites
- GCC or Clang compiler
- CMake 3.10 or higher
- C++17 support

### Build Instructions

```bash
cd /mnt/c/Users/navee/Naveen
rm -rf build
mkdir -p build
cd build
cmake ..
make
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

## Usage

### Basic Usage

```bash
cd build
./c_error_fixer path/to/file.c
```

### Help and Options

```bash
./build/c_error_fixer --help
```

Common options:

- `-h, --help` — Show help and exit
- `-i, --interactive` — Prompt before applying each suggested fix
- `-d, --dry-run` — Analyze and show fixes without modifying files
- `-v, --verbose` — Show compiler output and detailed processing info
- `--show-errors-only` — Show only compiler errors in verbose output
- `--log <file>` — Write compiler output and logs to the given file
- `--max-line-length <n>` — Skip files with lines longer than `<n>` characters
- `--timeout <sec>` — Stop processing after `<sec>` seconds per file
- `--no-backup` — Disable creating timestamped backups before modifications

## Example

```bash
# Analyze a source file and apply fixes interactively
./build/c_error_fixer --interactive program.c

# Inspect suggested fixes without modifying the file
./build/c_error_fixer --dry-run program.c

# Run with verbose compiler diagnostics
./build/c_error_fixer --verbose program.c

# Run without creating timestamped backups
./build/c_error_fixer --no-backup program.c
```

## Project Structure

```
C-Error-Fixer/
├── include/                    # Header files
├── src/                        # Implementation files
├── tests/                      # Test files and validation code
├── build/                      # Local build directory (ignored)
├── CMakeLists.txt              # Build configuration
├── CHANGELOG.md                # Release history
├── LICENSE                     # Open source license
└── README.md                   # This file
```

## Limitations

- Supports GCC-compatible compiler output only
- Fix suggestions are heuristic and may not cover every case
- Complex semantic issues may still require manual intervention
- Designed for smaller C source files and incremental error correction

## Testing

Run the unit test suite from the build directory:

```bash
cd build
ctest --output-on-failure
```

## License

This project is released under the MIT License. See `LICENSE` for details.
