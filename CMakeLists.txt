cmake_minimum_required(VERSION 3.0.0)
project(tpool VERSION 0.1.0)

include(CTest)

if (BUILD_TESTING)
add_subdirectory(tests)
endif()