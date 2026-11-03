# Standalone project that compiles a consumer against an *installed* popc.
#
# Deliberately not add_subdirectory()'d from the main build: it must resolve
# popc through find_package() and the installed popcConfig.cmake, never
# through the in-tree target. Configure it against a staged prefix:
#
#   cmake --install build --prefix "$PWD/stage"
#   cmake -S test/install-test -B build/install-test \
#         -DCMAKE_PREFIX_PATH="$PWD/stage"
#   cmake --build build/install-test
#   ./build/install-test/popc_install_consumer
#
# The `cmake-min` CI job runs exactly this sequence under the oldest CMake the
# project supports.

cmake_minimum_required(VERSION 3.24)

project(popc_install_test LANGUAGES CXX)

find_package(popc REQUIRED)

add_executable(popc_install_consumer consumer.cpp)
target_link_libraries(popc_install_consumer PRIVATE popc::popc)
target_compile_options(popc_install_consumer PRIVATE -Wall -Wextra -Werror -pedantic)
