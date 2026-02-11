cmake_minimum_required(VERSION 3.20)
project(GodSimulation VERSION 0.3.0 LANGUAGES CXX C)

# ─── C++ Standard ───
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Compiler Warnings ───
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    # Sanitisers in Debug mode
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
endif()

# ─── Fetch Dependencies ───
include(FetchContent)

# entt - ECS library
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.2
    GIT_SHALLOW    TRUE
)

# spdlog - Logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)

# Catch2 - Testing
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.4
    GIT_SHALLOW    TRUE
)

# PCG - Deterministic RNG
FetchContent_Declare(
    pcg_cpp
    GIT_REPOSITORY https://github.com/imneme/pcg-cpp.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

# LZ4 - Compression
FetchContent_Declare(
    lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG        v1.9.4
    GIT_SHALLOW    TRUE
)

# GLFW - Windowing (renderer)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)

# glm - Math library (header-only)
set(GLM_ENABLE_CXX_20 ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(entt spdlog Catch2 pcg_cpp glfw)

# glm - just use as include directory (avoids CMake target compatibility issues)
FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
    FetchContent_Populate(glm)
endif()
add_library(glm_headers INTERFACE)
target_include_directories(glm_headers INTERFACE ${glm_SOURCE_DIR})

# LZ4 needs special handling - it's not a CMake project at the top level
FetchContent_GetProperties(lz4)
if(NOT lz4_POPULATED)
    FetchContent_Populate(lz4)
endif()
add_library(lz4_lib STATIC ${lz4_SOURCE_DIR}/lib/lz4.c ${lz4_SOURCE_DIR}/lib/lz4hc.c)
target_include_directories(lz4_lib PUBLIC ${lz4_SOURCE_DIR}/lib)

# PCG include path
add_library(pcg INTERFACE)
target_include_directories(pcg INTERFACE ${pcg_cpp_SOURCE_DIR}/include)

# OpenGL
find_package(OpenGL REQUIRED)

# ─── God Simulation Library (core + layers, no renderer) ───
file(GLOB_RECURSE GODSIM_SOURCES
    src/core/*.cpp
    src/layers/*.cpp
    src/simulation/*.cpp
)
file(GLOB_RECURSE GODSIM_HEADERS
    src/core/*.h
    src/layers/*.h
    src/simulation/*.h
)

add_library(godsim_lib STATIC ${GODSIM_SOURCES} ${GODSIM_HEADERS})
target_include_directories(godsim_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(godsim_lib PUBLIC
    EnTT::EnTT
    spdlog::spdlog
    pcg
    lz4_lib
)

# ─── Main Executable (with renderer) ───
add_executable(godsim src/main.cpp)
target_include_directories(godsim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(godsim PRIVATE
    godsim_lib
    glfw
    glm_headers
    OpenGL::GL
)

# ─── Tests (no renderer dependency) ───
enable_testing()
file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)
add_executable(godsim_tests ${TEST_SOURCES})
target_link_libraries(godsim_tests PRIVATE
    godsim_lib
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(godsim_tests)
