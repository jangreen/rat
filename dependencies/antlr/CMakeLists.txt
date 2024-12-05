# ANTLR
message(${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/antlr/cmake) # add ./cmake folder to cmake module path
set(ANTLR_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/antlr/antlr-4.10.1-complete.jar)
find_package(ANTLR REQUIRED) # require antlr commands
antlr_target(CatParser Cat.g4 VISITOR) # generate parser files out of g4 files -> output: ${ANTLR_CatParser_CXX_OUTPUTS}
antlr_target(LogicParser Logic.g4 VISITOR) # generate parser for logic
add_definitions(-DANTLR4CPP_STATIC) # required if linking to static library
set(ANTLR4_WITH_STATIC_CRT OFF) # using /MD flag for antlr4_runtime (for Visual C++ compilers only)
include(ExternalAntlr4Cpp) # build and add runtime for antlr4 -> variable: antlr4_static
