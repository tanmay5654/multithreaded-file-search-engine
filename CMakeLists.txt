cmake_minimum_required(VERSION 3.16)
project(file_search_engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(search_engine
    src/search_engine.cpp
)
target_include_directories(search_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(search_engine PUBLIC Threads::Threads)
target_compile_options(search_engine PRIVATE -Wall -Wextra -Wpedantic)

add_executable(file_search src/main.cpp)
target_link_libraries(file_search PRIVATE search_engine)

include(CTest)
if(BUILD_TESTING)
    add_executable(search_engine_tests tests/search_engine_tests.cpp)
    target_link_libraries(search_engine_tests PRIVATE search_engine)
    add_test(NAME search_engine_tests COMMAND search_engine_tests)
endif()
