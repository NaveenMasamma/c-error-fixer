cmake_minimum_required(VERSION 3.10)
project(C-Error-Fixer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the executable and its source files
add_executable(c_error_fixer
    src/main.cpp
    src/compiler_error_parser.cpp
    src/code_fixer.cpp
    src/error_pattern_db.cpp
    src/code_analyzer.cpp
    src/utils.cpp
    src/BatchProcessor.cpp
    src/FixCoordinator.cpp
    src/PatchManager.cpp
    src/IncludeFixer.cpp
    src/SyntaxFixer.cpp
    src/TypoFixer.cpp
    src/TypeMismatchFixer.cpp
)

target_include_directories(c_error_fixer PRIVATE include)

# Enable warnings
if(MSVC)
    target_compile_options(c_error_fixer PRIVATE /W4)
else()
    target_compile_options(c_error_fixer PRIVATE -Wall -Wextra)
endif()

# Enable testing and include tests directory
enable_testing()
add_subdirectory(tests)