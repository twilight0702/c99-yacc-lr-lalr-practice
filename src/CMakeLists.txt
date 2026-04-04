add_library(yacc_core
    yacc/model/grammar.cpp
    yacc/parser/yacc_parser.cpp
    yacc/preprocess/grammar_preprocessor.cpp
    yacc/report/report_exporter.cpp
)

target_include_directories(yacc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(yacc_parse_tool
    tools/yacc_parse_main.cpp
)

target_link_libraries(yacc_parse_tool PRIVATE yacc_core)
