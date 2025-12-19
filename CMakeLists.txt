cmake_minimum_required(VERSION 3.10)
project(VenomMemory VERSION 1.0.0 LANGUAGES C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs" ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(VENOM_SOURCES
    src/venom_shm.c
    src/venom_sync.c
    src/venom_channel.c
)

# Library target
add_library(venom_memory ${VENOM_SOURCES})
target_include_directories(venom_memory PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link required libraries
target_link_libraries(venom_memory PRIVATE rt pthread)

# Also build static library
add_library(venom_memory_static STATIC ${VENOM_SOURCES})
target_include_directories(venom_memory_static PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(venom_memory_static PRIVATE rt pthread)
set_target_properties(venom_memory_static PROPERTIES OUTPUT_NAME venom_memory)

# Examples
if(BUILD_EXAMPLES)
    add_executable(example_daemon examples/example_daemon.c)
    target_link_libraries(example_daemon venom_memory)

    add_executable(example_shell examples/example_shell.c)
    target_link_libraries(example_shell venom_memory)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_shm tests/test_shm.c)
    target_link_libraries(test_shm venom_memory)
    add_test(NAME test_shm COMMAND test_shm)

    add_executable(test_channel tests/test_channel.c)
    target_link_libraries(test_channel venom_memory)
    add_test(NAME test_channel COMMAND test_channel)

    add_executable(benchmark tests/benchmark.c)
    target_link_libraries(benchmark venom_memory)
endif()

# Installation
install(TARGETS venom_memory venom_memory_static
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

# pkg-config file
configure_file(
    ${CMAKE_SOURCE_DIR}/venom_memory.pc.in
    ${CMAKE_BINARY_DIR}/venom_memory.pc
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/venom_memory.pc DESTINATION lib/pkgconfig)

# Print configuration
message(STATUS "VenomMemory v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
message(STATUS "Build tests: ${BUILD_TESTS}")
