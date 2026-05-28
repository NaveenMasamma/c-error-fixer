enable_testing()

# Test: Missing include
add_test(
    NAME test_missing_include
    COMMAND c_error_fixer ${CMAKE_SOURCE_DIR}/tests/test_missing_include.c
)

# Test: Implicit function declaration  
add_test(
    NAME test_implicit_func
    COMMAND c_error_fixer ${CMAKE_SOURCE_DIR}/tests/test_implicit_func.c
)

# Test: Syntax error
add_test(
    NAME test_syntax_error
    COMMAND c_error_fixer ${CMAKE_SOURCE_DIR}/tests/test_syntax_error.c
)

# Test: Undefined reference
add_test(
    NAME test_undefined_reference
    COMMAND c_error_fixer ${CMAKE_SOURCE_DIR}/tests/test_undefined_reference.c
)
