cmake_minimum_required(VERSION 3.28.1)

project(Eshamell VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-stdlib=libc++ ${CMAKE_CXX_FLAGS} -O0 -g")
find_package(Boost 1.84 REQUIRED)

# set(SRCS )
# set(HDRS eshamel.hpp )
# add_library(eshamell STATIC ${SRCS} ${HDRS})

# set_target_properties(eshamell PROPERTIES LINKER_LANGUAGE CXX)
# target_include_directories(eshamell PUBLIC ${Boost_INCLUDE_DIRS})
# target_link_libraries(eshamell ${Boost_LIBRARY_DIRS})

set(TESTS_SRCS main.cpp)
set(TESTS_HDRS)
add_executable(eshamell_tests ${TESTS_SRCS} ${TESTS_HDRS} )
target_include_directories(eshamell_tests PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(eshamell_tests ${Boost_LIBRARY_DIRS} ctre )

add_subdirectory(compile-time-regular-expressions)
