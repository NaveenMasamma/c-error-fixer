cmake_minimum_required(VERSION 3.10)
project(CErrorFixer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/compiler_error_parser.cpp
    src/error_pattern_db.cpp
    src/code_fixer.cpp
    src/code_analyzer.cpp
)

# Create executable
add_executable(c_error_fixer ${SOURCES})

# Enable testing
enable_testing()
add_subdirectory(tests)
