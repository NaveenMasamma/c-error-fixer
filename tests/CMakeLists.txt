find_package(GTest REQUIRED)

add_executable(unit_tests
  test_IncludeFixer.cpp
  test_TypoFixer.cpp
  test_SyntaxFixer.cpp
  test_MVP_Validation.cpp
    ../src/utils.cpp
    ../src/error_pattern_db.cpp
    ../src/code_analyzer.cpp
    ../src/FixCoordinator.cpp
    ../src/PatchManager.cpp
    ../src/SyntaxFixer.cpp
    ../src/IncludeFixer.cpp
    ../src/TypoFixer.cpp
    ../src/TypeMismatchFixer.cpp
)

target_include_directories(unit_tests PRIVATE /usr/src/googletest/googlemock/include /usr/src/googletest/googletest/include)
target_link_libraries(unit_tests GTest::gtest GTest::gtest_main)
enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)