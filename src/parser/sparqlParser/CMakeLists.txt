add_library(sparqlParser
        generated/SparqlAutomaticBaseListener.cpp
        generated/SparqlAutomaticLexer.cpp
        generated/SparqlAutomaticListener.cpp
        generated/SparqlAutomaticParser.cpp
        generated/SparqlAutomaticVisitor.cpp)
qlever_target_link_libraries(sparqlParser antlr4_static)
target_precompile_headers(sparqlParser PRIVATE "${antlr_SOURCE_DIR}/runtime/Cpp/runtime/src/antlr4-runtime.h")
# Silence warnings in files that are auto-generated by ANTLR.
# TODO<joka921> Submit a pull request to ANTLR to fix those warnings.
target_compile_options(sparqlParser PRIVATE -Wno-logical-op-parentheses -Wno-parentheses)
