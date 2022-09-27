cmake_minimum_required(VERSION 3.1)

project(MP3Buster)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/mpeg/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/mpeg/id3v2)

# Add source
file(GLOB_RECURSE MP3BUSTER_SOURCE
                              ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
                              ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
                              )

add_executable(MP3Buster ${MP3BUSTER_SOURCE})

target_link_libraries(MP3Buster asound)
