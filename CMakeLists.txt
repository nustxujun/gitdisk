cmake_minimum_required(VERSION 3.0.0)


project(gitdisk)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")

set(BUILD_SHARED_LIBS CACHE BOOL "set libgit2 to dynamic lib")



AUX_SOURCE_DIRECTORY(. sourcefiles)
source_group(src FILES ${sourcefiles})
include_directories(deps/asio)

# libgit2
set(LIBGIT2 "deps/libgit2")
include_directories(${LIBGIT2}/include)
add_subdirectory(${LIBGIT2})

#asio
set(ASIO "deps/asio")
include_directories(${ASIO}/include)
add_definitions(-DASIO_STANDALONE)



add_executable(gitdisk ${sourcefiles})

add_dependencies(gitdisk git2)
target_link_libraries(gitdisk debug git2)
target_link_libraries(gitdisk optimized git2)