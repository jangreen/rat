cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 20)
project(CatInfer LANGUAGES CXX)

# Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
message(STATUS "Boost version: ${Boost_VERSION}")

# ANTLR
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake) # add ./cmake folder to cmake module path
set(ANTLR_EXECUTABLE /usr/local/lib/antlr-4.10.1-complete.jar)
find_package(ANTLR REQUIRED) # require antlr commands
antlr_target(CatParser Cat.g4 VISITOR) # generate parser files out of g4 files -> output: ${ANTLR_CatParser_CXX_OUTPUTS}
antlr_target(LogicParser Logic.g4 VISITOR) # generate parser for logic
add_definitions(-DANTLR4CPP_STATIC) # required if linking to static library
set(ANTLR4_WITH_STATIC_CRT OFF) # using /MD flag for antlr4_runtime (for Visual C++ compilers only)
include(ExternalAntlr4Cpp) # build and add runtime for antlr4 -> variable: antlr4_static

# project executable
file(GLOB CatInfer_SRC "src/*.cpp")
add_executable(${PROJECT_NAME} src/main.cpp ${CatInfer_SRC} ${ANTLR_CatParser_CXX_OUTPUTS} ${ANTLR_LogicParser_CXX_OUTPUTS})
target_include_directories(${PROJECT_NAME} PUBLIC ${ANTLR_CatParser_OUTPUT_DIR}) # add generated parser files
target_include_directories(${PROJECT_NAME} PUBLIC ${ANTLR_LogicParser_OUTPUT_DIR}) # add generated parser files
target_include_directories(${PROJECT_NAME} PUBLIC ${ANTLR4_INCLUDE_DIRS}) # add antrl4cpp artifacts to project environment
target_link_libraries(${PROJECT_NAME} antlr4_static) # link with antlr runtime
