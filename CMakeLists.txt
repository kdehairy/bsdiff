cmake_minimum_required(VERSION 3.1)
project(libbsdiff)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_ENDIAN_H -Wall -std=c++11")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/a")

set(SOURCE_FILES bsdiff.h bsdiff.cpp bspatch.h bspatch.cpp sais/sais.h sais/sais.c)
add_library(bsdiff SHARED ${SOURCE_FILES})
target_link_libraries(bsdiff bz2)

set(TEST_SOURCE_FILES test.cpp test_diff.cpp test_patch.cpp)
add_executable(bstest ${TEST_SOURCE_FILES})
target_link_libraries(bstest bsdiff)