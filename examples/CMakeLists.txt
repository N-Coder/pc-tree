cmake_minimum_required(VERSION 3.8)
project(PCTreeExample CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(PCTree REQUIRED)

add_executable(example pctree.cpp)
target_include_directories(example PRIVATE ${PCTree_INCLUDE_DIRS})
target_link_libraries(example PCTree)
