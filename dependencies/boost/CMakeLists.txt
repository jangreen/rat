# Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
message(STATUS "Boost version: ${Boost_VERSION}")