add_executable(cellml_text_tests tests.cpp)

target_link_libraries(cellml_text_tests PRIVATE cellml_text)

add_test(NAME cellml_text_tests COMMAND cellml_text_tests)
