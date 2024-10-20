cmake_minimum_required(VERSION 3.10)
project(PCTree)
set(CMAKE_CXX_STANDARD 17)
set(PROJECT_VERSION 0.0.1)
include(GNUInstallDirs)

message("CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
message("CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_CXX_FLAGS is ${CMAKE_CXX_FLAGS}")
message("CMAKE_INTERPROCEDURAL_OPTIMIZATION is ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")

if (NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "No CMAKE_BUILD_TYPE set!")
endif ()


set(SOURCE_FILES
#        src/NodePCRotation.cpp
        src/PCNode.cpp
        src/PCTree_basic.cpp
        src/PCTree_construction.cpp
        src/PCTree_intersect.cpp
        src/PCTree_restriction.cpp
        src/PCTreeForest.cpp
)

function(make_pc_target TARGET)
    target_include_directories(${TARGET} PUBLIC "include" "libraries/bandit" "libraries/bigint/src")
    target_compile_options(${TARGET} PRIVATE "-w") # "-Werror=uninitialized" "-Wall"
endfunction()


add_executable(test ${SOURCE_FILES} test/pctree.cpp test/bandit_main.cpp)
make_pc_target(test)
add_executable(test-auto ${SOURCE_FILES} test/pctree-auto.cpp test/bandit_main.cpp libraries/bigint/src/bigint.cpp)
make_pc_target(test-auto)

# Installation
# see https://stackoverflow.com/a/49863065

add_library(${PROJECT_NAME} ${SOURCE_FILES})
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
