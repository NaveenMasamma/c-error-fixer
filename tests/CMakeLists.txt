find_package(GTest REQUIRED)
find_package(GMock QUIET)

if(TARGET GTest::gmock)
  set(GTEST_GMOCK_TARGET GTest::gmock)
elseif(TARGET GMock::gmock)
  add_library(GTest::gmock ALIAS GMock::gmock)
  set(GTEST_GMOCK_TARGET GTest::gmock)
endif()

add_executable(unit_tests
  test_IncludeFixer.cpp
  test_TypoFixer.cpp
  test_SyntaxFixer.cpp
  test_CompilerErrorParser.cpp
  test_MVP_Validation.cpp
  test_negative_cases.cpp
    ../src/utils.cpp
    ../src/compiler_error_parser.cpp
    ../src/error_pattern_db.cpp
    ../src/code_analyzer.cpp
    ../src/FixCoordinator.cpp
    ../src/PatchManager.cpp
    ../src/SyntaxFixer.cpp
    ../src/code_fixer.cpp
    ../src/IncludeFixer.cpp
    ../src/TypoFixer.cpp
    ../src/TypeMismatchFixer.cpp
)

target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(DEFINED GTEST_GMOCK_TARGET)
  target_link_libraries(unit_tests PRIVATE GTest::gtest ${GTEST_GMOCK_TARGET} GTest::gtest_main)
else()
  target_link_libraries(unit_tests PRIVATE GTest::gtest GTest::gtest_main)
endif()
enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
