cmake_minimum_required(VERSION 3.10)
project(PCTree)
set(CMAKE_CXX_STANDARD 17) # only needed for evaluation, remaining code is C++11
set(PROJECT_VERSION 0.0.1)
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG -march=native -mtune=native")
include(GNUInstallDirs)

message("CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
message("CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_CXX_FLAGS is ${CMAKE_CXX_FLAGS}")

if (NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "No CMAKE_BUILD_TYPE set!")
endif ()

## Utils libraries

#set(OGDF_DIR /home/user/OGDF/build-default)
find_package(OGDF REQUIRED)
get_target_property(OGDF_IMPORTED_CONFIGURATIONS OGDF IMPORTED_CONFIGURATIONS)
message("OGDF_IMPORTED_CONFIGURATIONS is ${OGDF_IMPORTED_CONFIGURATIONS}")

add_library(bigInt libraries/bigInt/bigint.cpp)

if (LIKWID_PERFMON)
    #set(likwid_DIR /home/user/likwid/install/share/likwid)
    if (TARGET likwid-library)
        message("LIKWID already loaded")
    else()
        message("Including LIKWID")
        find_package(likwid REQUIRED)
    endif()
endif ()

## Other Implementations as libraries

set(HSU_PC_DIR libraries/hsuPC)
add_library(hsuPC ${HSU_PC_DIR}/master.cpp)
target_link_libraries(hsuPC OGDF bigInt)
target_include_directories(hsuPC PRIVATE "libraries" "${HSU_PC_DIR}/include")


set(COMPARE_LIBRARIES hsuPC)
if (OTHER_LIBS)
    message("Including other libraries")
    add_compile_definitions(OTHER_LIBS)

    set(GREG_PQ_DIR ${CMAKE_CURRENT_LIST_DIR}/libraries/gregablePQ)
    if (EXISTS ${GREG_PQ_DIR}/pqnode.cc AND EXISTS ${GREG_PQ_DIR}/pqtree.cc)
        add_library(gregPQ ${GREG_PQ_DIR}/pqnode.cc ${GREG_PQ_DIR}/pqtree.cc)
        target_include_directories(gregPQ PRIVATE "libraries")
        list(APPEND COMPARE_LIBRARIES gregPQ)
        add_compile_definitions(ENABLE_GREGABLE TRUE)
    endif ()

    set(CREISLE_PQ_DIR ${CMAKE_CURRENT_LIST_DIR}/libraries/creislePQ)
    if (EXISTS ${CREISLE_PQ_DIR}/Leaf.cpp AND EXISTS ${CREISLE_PQ_DIR}/Node.cpp AND EXISTS ${CREISLE_PQ_DIR}/PQnode.cpp AND EXISTS ${CREISLE_PQ_DIR}/PQTree.cpp)
        add_library(creislePQ ${CREISLE_PQ_DIR}/Leaf.cpp ${CREISLE_PQ_DIR}/Node.cpp ${CREISLE_PQ_DIR}/PQnode.cpp
                ${CREISLE_PQ_DIR}/PQTree.cpp)
        target_include_directories(creislePQ PRIVATE "libraries")
        list(APPEND COMPARE_LIBRARIES creislePQ)
        add_compile_definitions(ENABLE_REISLE TRUE)
    endif ()

    set(BIVOC_PQ_DIR ${CMAKE_CURRENT_LIST_DIR}/libraries/bivocPQ)
    if (EXISTS ${BIVOC_PQ_DIR}/pqnode.h AND EXISTS ${BIVOC_PQ_DIR}/pqtree.h)
        add_library(bivocPQ ${BIVOC_PQ_DIR}/pqnode.h ${BIVOC_PQ_DIR}/pqtree.h)
        set_target_properties(bivocPQ PROPERTIES LINKER_LANGUAGE CXX)
        target_include_directories(bivocPQ PRIVATE "libraries")
        list(APPEND COMPARE_LIBRARIES bivocPQ)
        add_compile_definitions(ENABLE_BIVOC TRUE)
    endif ()

    set(GRAPHSET_PQ_DIR ${CMAKE_CURRENT_LIST_DIR}/libraries/graphSetPQ)
    if (EXISTS ${GRAPHSET_PQ_DIR}/LeafNode.cpp AND EXISTS ${GRAPHSET_PQ_DIR}/Node.cpp AND EXISTS ${GRAPHSET_PQ_DIR}/Pnode.cpp AND EXISTS ${GRAPHSET_PQ_DIR}/PQtree.cpp AND EXISTS ${GRAPHSET_PQ_DIR}/Qnode.cpp)
        add_library(graphsetPQ ${GRAPHSET_PQ_DIR}/LeafNode.cpp ${GRAPHSET_PQ_DIR}/Node.cpp ${GRAPHSET_PQ_DIR}/Pnode.cpp
                ${GRAPHSET_PQ_DIR}/PQtree.cpp ${GRAPHSET_PQ_DIR}/Qnode.cpp)
        target_include_directories(graphsetPQ PRIVATE "libraries")
        list(APPEND COMPARE_LIBRARIES graphsetPQ)
        add_compile_definitions(ENABLE_GRAPHSET TRUE)
    endif ()

    set(CPP_ZANETTI_DIR ${CMAKE_CURRENT_LIST_DIR}/libraries/zanettiPQR)
    if (EXISTS ${CPP_ZANETTI_DIR}/PQRTree.cpp)
        add_library(cppZanetti ${CPP_ZANETTI_DIR}/PQRTree.cpp)
        target_link_libraries(cppZanetti OGDF bigInt)
        target_include_directories(cppZanetti PRIVATE "libraries")
        list(APPEND COMPARE_LIBRARIES cppZanetti)
        add_compile_definitions(ENABLE_CPPZANETTI TRUE)
        set(ENABLE_CPPZANETTI TRUE)
    endif ()
endif ()

## Our implementation

set(SOURCE_FILES
        src/master.cpp
        #src/NodePCRotation.cpp
        #src/PCNode.cpp
        #src/PCTree_basic.cpp
        #src/PCTree_construction.cpp
        #src/PCTree_intersect.cpp
        #src/PCTree_restriction.cpp
        )

function(make_pc_target TARGET)
    target_include_directories(${TARGET} PUBLIC "include")
    target_include_directories(${TARGET} PRIVATE "libraries" "libraries/bandit")
    target_link_libraries(${TARGET} OGDF bigInt)
    target_compile_options(${TARGET} PRIVATE "-w") # "-Werror=uninitialized" "-Wall"
endfunction()


add_executable(construction ${SOURCE_FILES} src/test/construction.cpp)
make_pc_target(construction)
#target_include_directories(construction PRIVATE ${OGDF_INCLUDE_DIRS})
#target_include_directories(construction PRIVATE ${OGDF_DIR}/../)

add_executable(restriction ${SOURCE_FILES} src/test/restriction.cpp)
make_pc_target(restriction)

add_executable(test ${SOURCE_FILES} src/test/test.cpp)
make_pc_target(test)

add_executable(intersection ${SOURCE_FILES} src/test/intersection.cpp)
target_include_directories(intersection PUBLIC "include")
target_include_directories(intersection PRIVATE "libraries" "libraries/bandit")
target_link_libraries(intersection OGDF bigInt hsuPC)

add_executable(make_restrictions_planarity ${SOURCE_FILES} src/exec/make_restrictions_planarity.cpp)
make_pc_target(make_restrictions_planarity)
target_link_libraries(make_restrictions_planarity ${COMPARE_LIBRARIES} stdc++fs)

add_executable(make_restrictions_tree ${SOURCE_FILES} src/exec/make_restrictions_tree.cpp)
make_pc_target(make_restrictions_tree)

add_executable(make_restrictions_matrix ${SOURCE_FILES} src/exec/make_restrictions_matrix.cpp)
make_pc_target(make_restrictions_matrix)
target_link_libraries(make_restrictions_matrix ${COMPARE_LIBRARIES} stdc++fs)

add_executable(make_graphs ${SOURCE_FILES} src/exec/make_graphs.cpp)
make_pc_target(make_graphs)

add_executable(test_planarity ${SOURCE_FILES} src/exec/test_planarity.cpp)
make_pc_target(test_planarity)
target_link_libraries(test_planarity hsuPC)

add_executable(test_planarity_performance ${SOURCE_FILES} src/exec/test_planarity_performance.cpp)
make_pc_target(test_planarity_performance)
target_link_libraries(test_planarity_performance hsuPC)

if (OTHER_LIBS AND ENABLE_CPPZANETTI)
    target_link_libraries(test_planarity cppZanetti)
    target_link_libraries(test_planarity_performance cppZanetti)
endif()

add_executable(test_restrictions ${SOURCE_FILES} src/exec/test_restrictions.cpp)
make_pc_target(test_restrictions)
target_link_libraries(test_restrictions ${COMPARE_LIBRARIES})

if (LIKWID_PERFMON)
    target_include_directories(test_planarity PRIVATE ${LIKWID_INCLUDE_DIR})
    target_compile_definitions(test_planarity PUBLIC LIKWID_PERFMON)
    target_link_libraries(test_planarity likwid-library)

    target_include_directories(test_planarity_performance PRIVATE ${LIKWID_INCLUDE_DIR})
    target_compile_definitions(test_planarity_performance PUBLIC LIKWID_PERFMON)
    target_link_libraries(test_planarity_performance likwid-library)

    target_include_directories(test_restrictions PRIVATE ${LIKWID_INCLUDE_DIR})
    target_compile_definitions(test_restrictions PUBLIC LIKWID_PERFMON)
    target_link_libraries(test_restrictions likwid-library)
endif ()

add_library(${PROJECT_NAME} ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1)
target_include_directories(${PROJECT_NAME} PRIVATE ${OGDF_INCLUDE_DIRS} "libraries")
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libraries>
        $<INSTALL_INTERFACE:include>)
#target_link_libraries(${TARGET} INTERFACE OGDF bigInt)

install(TARGETS ${PROJECT_NAME} EXPORT PCTreeConfig
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

install(EXPORT PCTreeConfig DESTINATION cmake)

export(TARGETS ${PROJECT_NAME} bigInt FILE PCTreeConfig.cmake)

# see https://stackoverflow.com/a/49863065
