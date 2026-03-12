cmake_minimum_required(VERSION 3.14)

project(cellml-text-2
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "CellML Text 2.0 — A human-friendly text format for CellML models"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --- libCellML ---------------------------------------------------------
set(COVERAGE OFF CACHE BOOL "Disable libCellML coverage" FORCE)
set(LLVM_COVERAGE OFF CACHE BOOL "Disable libCellML LLVM coverage" FORCE)
set(MEMCHECK OFF CACHE BOOL "Disable libCellML memcheck" FORCE)
set(UNIT_TESTS OFF CACHE BOOL "Disable libCellML tests" FORCE)
FetchContent_Declare(
    libcellml
    GIT_REPOSITORY https://github.com/cellml/libcellml.git
    GIT_TAG        387274c5da5be35a7eefbf167405ef20424308e4
)
FetchContent_MakeAvailable(libcellml)

# --- pugixml (for MathML parsing) --------------------------------------
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.15
)
FetchContent_MakeAvailable(pugixml)

# --- Library ------------------------------------------------------------
add_subdirectory(src)

# --- Tests (optional) ---------------------------------------------------
option(CELLML_TEXT_BUILD_TESTS "Build CellML Text 2.0 tests" ON)
if(CELLML_TEXT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
