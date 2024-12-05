cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 20)
project(rat LANGUAGES CXX)

# Link time optimization (LTO) // Maybe active by default?
#cmake_policy(SET CMP0069 NEW)
#set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

include(dependencies/boost/CMakeLists.txt)
include(dependencies/antlr/CMakeLists.txt)
include(dependencies/spdlog/CMakeLists.txt)
include(dependencies/googletests/CMakeLists.txt)

# RAT executable
file(GLOB_RECURSE rat_SRC "src/*.cpp")
add_compile_definitions("DEBUG=$<CONFIG:Debug>")
add_executable(${PROJECT_NAME}
        main.cpp
        ${rat_SRC}
        ${ANTLR_CatParser_CXX_OUTPUTS}
        ${ANTLR_LogicParser_CXX_OUTPUTS}
)
target_include_directories(${PROJECT_NAME} PUBLIC ${ANTLR_CatParser_OUTPUT_DIR}) # add generated parser files
target_include_directories(${PROJECT_NAME} PUBLIC ${ANTLR_LogicParser_OUTPUT_DIR}) # add generated parser files
target_include_directories(${PROJECT_NAME} PUBLIC ${ANTLR4_INCLUDE_DIRS}) # add antrl4cpp artifacts to project environment
target_link_libraries(${PROJECT_NAME} antlr4_static) # link with antlr runtime
target_link_libraries(${PROJECT_NAME} spdlog::spdlog_header_only)

# Tests executable
add_executable(tests
        Tests.cpp
        ${rat_SRC}
        ${ANTLR_CatParser_CXX_OUTPUTS}
        ${ANTLR_LogicParser_CXX_OUTPUTS}
)
target_include_directories(tests PUBLIC ${ANTLR_CatParser_OUTPUT_DIR}) # add generated parser files
target_include_directories(tests PUBLIC ${ANTLR_LogicParser_OUTPUT_DIR}) # add generated parser files
target_include_directories(tests PUBLIC ${ANTLR4_INCLUDE_DIRS}) # add antrl4cpp artifacts to project environment
target_link_libraries(tests antlr4_static) # link with antlr runtime
target_link_libraries(tests spdlog::spdlog_header_only)
target_link_libraries(tests gtest_main)
include(GoogleTest)
gtest_discover_tests(tests)
