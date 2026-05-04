cmake_minimum_required(VERSION 3.24)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" POPC_VERSION_STRING)
string(STRIP "${POPC_VERSION_STRING}" POPC_VERSION_STRING)

project(popc
    VERSION ${POPC_VERSION_STRING}
    LANGUAGES CXX
    DESCRIPTION "Efficient C++ implementation of the POPC binary feature clustering algorithm"
)

# Always emit compile_commands.json so editor LSPs (clangd) and
# clang-tidy in CI can find compilation database entries without the
# caller having to remember -DCMAKE_EXPORT_COMPILE_COMMANDS=ON.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# --- Header-only library ---
add_library(popc INTERFACE)
add_library(popc::popc ALIAS popc)

target_compile_features(popc INTERFACE cxx_std_20)

target_include_directories(popc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(popc INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/popc/popc.hpp
        include/popc/cluster.hpp
        include/popc/dataset.hpp
        include/popc/detail/bitpacked_kmeans.hpp
)

# --- Warning flags ---
# Shared baseline applied to every target that consumes the library
# (CLI, tests). Centralized so that a flag added here lands in every
# consumer build at once.
#
# Beyond -Wall -Wextra -pedantic, the expanded set catches:
#   -Wconversion           lossy implicit numeric conversions
#   -Wsign-conversion      signed<->unsigned implicit conversions
#   -Wshadow               variables shadowing outer-scope names
#   -Wnull-dereference     statically-provable null derefs
#   -Wdouble-promotion     implicit float->double
#   -Wimplicit-fallthrough switch case missing [[fallthrough]] attr
# GCC adds two more that Clang does not implement:
#   -Wlogical-op           `a || a` / `a && a` typos
#   -Wduplicated-cond      `if(a) ... else if(a)` typos
set(POPC_WARNING_FLAGS
    -Wall -Wextra -Werror -pedantic -Wno-unused-local-typedefs
    -Wconversion -Wsign-conversion -Wshadow -Wnull-dereference
    -Wdouble-promotion -Wimplicit-fallthrough
    $<$<CXX_COMPILER_ID:GNU>:-Wlogical-op>
    $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond>
)

# Clang's -flto produces LLVM bitcode that the default GNU `ld` cannot
# read ("file format not recognized"). Route Clang link steps through
# `lld`, which speaks bitcode natively. CI installs `lld` alongside
# `clang`. GCC builds keep their default linker.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_link_options(-fuse-ld=lld)
endif()

# --- Sanitizers ---
# When -DPOPC_SANITIZE=ON, the CLI tool and tests are compiled and
# linked with AddressSanitizer + UndefinedBehaviorSanitizer. Guarded
# on Debug builds so Release artifacts are unaffected.
option(POPC_SANITIZE "Enable AddressSanitizer + UndefinedBehaviorSanitizer in Debug builds" OFF)
set(POPC_SANITIZE_FLAGS
    $<$<BOOL:${POPC_SANITIZE}>:$<$<CONFIG:Debug>:-fsanitize=address,undefined>>
    $<$<BOOL:${POPC_SANITIZE}>:$<$<CONFIG:Debug>:-fno-sanitize-recover=all>>
    $<$<BOOL:${POPC_SANITIZE}>:$<$<CONFIG:Debug>:-fno-omit-frame-pointer>>
)

# --- CLI tool ---
add_executable(popc-cli src/popc.cpp)
target_link_libraries(popc-cli PRIVATE popc::popc)
target_compile_definitions(popc-cli PRIVATE POPC_VERSION="${PROJECT_VERSION}")
set_target_properties(popc-cli PROPERTIES OUTPUT_NAME popc)

target_compile_options(popc-cli PRIVATE
    ${POPC_WARNING_FLAGS}
    ${POPC_SANITIZE_FLAGS}
    $<$<CONFIG:Release>:-O3 -fomit-frame-pointer -DNDEBUG>
    $<$<CONFIG:Debug>:-Og -g -fno-omit-frame-pointer>
)
target_link_options(popc-cli PRIVATE ${POPC_SANITIZE_FLAGS})
# Enable LTO via the CMake property rather than a raw -flto flag.
# CMake handles the gcc-vs-clang toolchain difference automatically.
set_target_properties(popc-cli PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
)

# --- Tests ---
option(POPC_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
if(POPC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# --- Install ---
install(TARGETS popc
    EXPORT popcTargets
    FILE_SET HEADERS
)

install(TARGETS popc-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT popcTargets
    FILE popcTargets.cmake
    NAMESPACE popc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/popc
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/popcConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/popcConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/popc
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/popcConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/popcConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/popcConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/popc
)

configure_file(cmake/popc.pc.in "${CMAKE_CURRENT_BINARY_DIR}/popc.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/popc.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
