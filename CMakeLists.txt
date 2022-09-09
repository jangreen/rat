cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake) # add ./cmake folder to cmake module path
project(CatInfer)

# generate parser files out of g4 files, output: ${ANTLR_CatParser_CXX_OUTPUTS}
set(ANTLR_EXECUTABLE /usr/local/lib/antlr-4.10.1-complete.jar)
find_package(ANTLR REQUIRED)
antlr_target(CatParser Cat.g4 VISITOR)
include_directories(${ANTLR_CatParser_OUTPUT_DIR})

# build and add runtime for antlr4, variable antlr4_static
add_definitions(-DANTLR4CPP_STATIC) # required if linking to static library
set(ANTLR4_WITH_STATIC_CRT OFF) # using /MD flag for antlr4_runtime (for Visual C++ compilers only)
include(ExternalAntlr4Cpp) # Add external build for antlrcpp runtime
include_directories(${ANTLR4_INCLUDE_DIRS}) # add antrl4cpp artifacts to project environment

#TODO: include all src folder
add_executable(${PROJECT_NAME} main.cpp CatInferVisitor.cpp Relation.cpp Constraint.cpp ${ANTLR_CatParser_CXX_OUTPUTS})
target_link_libraries(${PROJECT_NAME} antlr4_static)