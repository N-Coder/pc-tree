cmake_minimum_required(VERSION 3.8)
project(PCTree)
set(CMAKE_CXX_STANDARD 17)
set(PROJECT_VERSION 1.0.0)

message("CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
message("CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_CXX_FLAGS is ${CMAKE_CXX_FLAGS}")
message("CMAKE_INTERPROCEDURAL_OPTIMIZATION is ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")

if (NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "No CMAKE_BUILD_TYPE set!")
endif ()


set(SOURCE_FILES
        src/PCNode.cpp
        src/PCTree_basic.cpp
        src/PCTree_construction.cpp
        src/PCTree_intersect.cpp
        src/PCTree_restriction.cpp
        src/PCTreeForest.cpp
)

add_executable(test ${SOURCE_FILES} test/pctree.cpp test/bandit_main.cpp)
target_include_directories(test PUBLIC "include" "libraries/bandit")
add_executable(test-auto ${SOURCE_FILES} test/pctree-auto.cpp test/bandit_main.cpp libraries/bigint/src/bigint.cpp)
target_include_directories(test-auto PUBLIC "include" "libraries/bandit" "libraries/bigint/src")

# Installation
# see https://stackoverflow.com/a/49863065
include(GNUInstallDirs)
add_library(${PROJECT_NAME} ${SOURCE_FILES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${CMAKE_CXX_STANDARD})
set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1)
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

install(TARGETS ${PROJECT_NAME} EXPORT PCTreeConfig
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

install(EXPORT PCTreeConfig DESTINATION cmake)

export(TARGETS ${PROJECT_NAME} FILE PCTreeConfig.cmake)
