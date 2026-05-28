cmake_minimum_required(VERSION 3.10)
project(C-Error-Fixer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include the directory containing our header files
include_directories(include)

# Define the executable and its source files
add_executable(c_error_fixer
    src/main.cpp
    src/compiler_error_parser.cpp
    src/code_fixer.cpp
    src/error_pattern_db.cpp
    src/code_analyzer.cpp
    src/utils.cpp
)

# Enable warnings
if(MSVC)
    target_compile_options(c_error_fixer PRIVATE /W4)
else()
    target_compile_options(c_error_fixer PRIVATE -Wall -Wextra)
endif()