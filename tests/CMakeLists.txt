add_executable(cellml_text_tests tests.cpp)

target_link_libraries(cellml_text_tests PRIVATE cellml_text)

target_compile_definitions(cellml_text_tests PRIVATE
    TEST_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models"
)

add_test(NAME cellml_text_tests COMMAND cellml_text_tests)
