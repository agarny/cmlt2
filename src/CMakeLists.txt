set(SOURCES
    api.cpp
    lexer.cpp
    parser.cpp
    serializer.cpp
    units.cpp
    mathml.cpp
)

set(HEADERS
    api.h
    token.h
    ast.h
    lexer.h
    parser.h
    serializer.h
    units.h
    mathml.h
)

add_library(cellml_text ${SOURCES})

target_include_directories(cellml_text
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cellml_text
    PUBLIC  cellml
    PRIVATE pugixml::pugixml
)
