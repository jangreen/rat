# Google Tests
include(FetchContent)
FetchContent_Declare(googletest
        GIT_REPOSITORY https://www.github.com/google/googletest.git
        GIT_TAG release-1.12.0
)
FetchContent_MakeAvailable(googletest)
include_directories(${googletest_SOURCE_DIR}/googletest/include)
