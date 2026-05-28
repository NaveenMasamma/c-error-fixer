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

# Test: Unterminated string literal fix suggestion
add_test(
    NAME test_unclosed_literal_fix
    COMMAND ${CMAKE_SOURCE_DIR}/tests/run_unclosed_literal_fix_test.sh ${CMAKE_BINARY_DIR}/c_error_fixer
)

# Test: Undefined reference
add_test(
    NAME test_undefined_reference
    COMMAND c_error_fixer ${CMAKE_SOURCE_DIR}/tests/test_undefined_reference.c
)
