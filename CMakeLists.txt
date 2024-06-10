cmake_minimum_required(VERSION 3.10)
project(PCTree)
set(CMAKE_CXX_STANDARD 11)
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
        src/master.cpp
        src/NodePCRotation.cpp
        src/PCNode.cpp
        src/PCTree_basic.cpp
        src/PCTree_construction.cpp
        src/PCTree_intersect.cpp
        src/PCTree_restriction.cpp
)

function(make_pc_target TARGET)
    target_include_directories(${TARGET} PUBLIC "include")
    target_compile_options(${TARGET} PRIVATE "-w") # "-Werror=uninitialized" "-Wall"
endfunction()


add_executable(construction ${SOURCE_FILES} src/test/construction.cpp)
make_pc_target(construction)

add_executable(restriction ${SOURCE_FILES} src/test/restriction.cpp)
make_pc_target(restriction)

add_executable(test ${SOURCE_FILES} src/test/test.cpp)
make_pc_target(test)

add_executable(intersection ${SOURCE_FILES} src/test/intersection.cpp)
make_pc_target(intersection)

add_executable(make_restrictions_planarity ${SOURCE_FILES} src/exec/make_restrictions_planarity.cpp)
make_pc_target(make_restrictions_planarity)

add_executable(make_restrictions_tree ${SOURCE_FILES} src/exec/make_restrictions_tree.cpp)
make_pc_target(make_restrictions_tree)

add_executable(make_restrictions_matrix ${SOURCE_FILES} src/exec/make_restrictions_matrix.cpp)
make_pc_target(make_restrictions_matrix)

add_executable(make_graphs ${SOURCE_FILES} src/exec/make_graphs.cpp)
make_pc_target(make_graphs)

add_executable(test_planarity ${SOURCE_FILES} src/exec/test_planarity.cpp)
make_pc_target(test_planarity)

add_executable(test_planarity_performance ${SOURCE_FILES} src/exec/test_planarity_performance.cpp)
make_pc_target(test_planarity_performance)

add_executable(test_restrictions ${SOURCE_FILES} src/exec/test_restrictions.cpp)
make_pc_target(test_restrictions)

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
