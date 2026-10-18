cmake_minimum_required(VERSION 3.20)
project(mini_runtime LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Feature switches (shared with the epoll-server pipeline) ---------------
option(WERROR          "Treat warnings as errors"            OFF)
option(ENABLE_COVERAGE "Instrument for gcov/lcov coverage"   OFF)
option(ENABLE_ASAN     "Enable AddressSanitizer"             OFF)
option(ENABLE_UBSAN    "Enable UndefinedBehaviorSanitizer"   OFF)
option(ENABLE_TSAN     "Enable ThreadSanitizer"              OFF)
option(ENABLE_FUZZING  "Build libFuzzer targets (clang)"     OFF)

include(CTest)

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
    $<$<BOOL:${WERROR}>:-Werror>
)

add_library(project_sanitizers INTERFACE)
if(ENABLE_ASAN)
    target_compile_options(project_sanitizers INTERFACE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(project_sanitizers INTERFACE -fsanitize=address)
endif()
if(ENABLE_UBSAN)
    target_compile_options(project_sanitizers INTERFACE -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(project_sanitizers INTERFACE -fsanitize=undefined)
endif()
if(ENABLE_TSAN)
    target_compile_options(project_sanitizers INTERFACE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(project_sanitizers INTERFACE -fsanitize=thread)
endif()

if(ENABLE_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# --- Core library -----------------------------------------------------------
add_library(ctr_core STATIC
    src/cli.cpp
    src/config.cpp
    src/logger.cpp
    src/cgroup.cpp
    src/namespaces.cpp
    src/rootfs.cpp
    src/capabilities.cpp
    src/container.cpp
)
target_include_directories(ctr_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ctr_core PUBLIC project_warnings project_sanitizers)

# --- Application ------------------------------------------------------------
add_executable(mini-ctr src/main.cpp)
target_link_libraries(mini-ctr PRIVATE ctr_core)

# --- Tests ------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Fuzzing ----------------------------------------------------------------
if(ENABLE_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ENABLE_FUZZING requires a clang compiler")
    endif()
    add_subdirectory(fuzz)
endif()
