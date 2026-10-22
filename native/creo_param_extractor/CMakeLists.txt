cmake_minimum_required(VERSION 3.16)
project(creo_param_extractor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(creo_param_extractor STATIC
  src/creo_param_extractor.cpp
)
target_include_directories(creo_param_extractor
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(test_extractor tests/test_extractor.cpp)
target_link_libraries(test_extractor PRIVATE creo_param_extractor)

add_executable(dump_props src/dump_props.cpp)
target_link_libraries(dump_props PRIVATE creo_param_extractor)

enable_testing()
add_test(NAME extractor_sample COMMAND test_extractor
  ${CMAKE_SOURCE_DIR}/../../testdata/detailstest.prt.2)
