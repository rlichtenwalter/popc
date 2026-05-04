include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.13.0
)
FetchContent_MakeAvailable(Catch2)

# Mark Catch2's headers as SYSTEM so third-party warnings (notably
# Clang's -Wdouble-promotion firing inside catch_matchers_impl.hpp's
# float-vs-double comparison helpers) do not break our builds under
# -Werror. CMake 3.25 added a SYSTEM keyword to FetchContent_Declare
# that would do this declaratively; the project floor is still 3.24,
# so we move INTERFACE_INCLUDE_DIRECTORIES into
# INTERFACE_SYSTEM_INCLUDE_DIRECTORIES manually post-MakeAvailable.
get_target_property(_catch2_includes Catch2 INTERFACE_INCLUDE_DIRECTORIES)
if(_catch2_includes)
    set_target_properties(Catch2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
    target_include_directories(Catch2 SYSTEM INTERFACE ${_catch2_includes})
endif()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# --- Unit tests ---
# One test executable per public header, matching kdtree's layout.
set(POPC_TEST_TARGETS
    test_cluster
    test_dataset
    test_popc
    test_bitpacked_kmeans
)

foreach(tgt IN LISTS POPC_TEST_TARGETS)
    add_executable(${tgt} ${tgt}.cpp)
    target_link_libraries(${tgt} PRIVATE popc::popc Catch2::Catch2WithMain)
    target_compile_options(${tgt} PRIVATE ${POPC_WARNING_FLAGS} ${POPC_SANITIZE_FLAGS})
    target_link_options(${tgt}    PRIVATE ${POPC_SANITIZE_FLAGS})
    catch_discover_tests(${tgt})
endforeach()

# --- CLI integration / E2E tests ---

set(POPC_CLI $<TARGET_FILE:popc-cli>)
set(TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Help / version
add_test(NAME cli_version COMMAND ${POPC_CLI} --version)
set_tests_properties(cli_version PROPERTIES
    PASS_REGULAR_EXPRESSION "POPC C\\+\\+ by Ryan N. Lichtenwalter v[0-9]+\\.[0-9]+\\.[0-9]+")

add_test(NAME cli_version_short COMMAND ${POPC_CLI} -V)
set_tests_properties(cli_version_short PROPERTIES
    PASS_REGULAR_EXPRESSION "v[0-9]+\\.[0-9]+\\.[0-9]+")

add_test(NAME cli_help COMMAND ${POPC_CLI} --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")

add_test(NAME cli_help_short COMMAND ${POPC_CLI} -h)
set_tests_properties(cli_help_short PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")

# File and stdin input paths
add_test(NAME cli_file_input
    COMMAND ${POPC_CLI} -c "${TEST_DATA_DIR}/clusters.list" "${TEST_DATA_DIR}/data.tsv")
set_tests_properties(cli_file_input PROPERTIES PASS_REGULAR_EXPRESSION "^[0-9]")

add_test(NAME cli_stdin_input
    COMMAND sh -c "cat '${TEST_DATA_DIR}/data.tsv' | ${POPC_CLI} -c '${TEST_DATA_DIR}/clusters.list'")
set_tests_properties(cli_stdin_input PROPERTIES PASS_REGULAR_EXPRESSION "^[0-9]")

# Custom delimiter
add_test(NAME cli_delimiter_explicit_tab
    COMMAND ${POPC_CLI} -t "\\t" -c "${TEST_DATA_DIR}/clusters.list" "${TEST_DATA_DIR}/data.tsv")
set_tests_properties(cli_delimiter_explicit_tab PROPERTIES PASS_REGULAR_EXPRESSION "^[0-9]")

# Custom multiplier and power
add_test(NAME cli_custom_multiplier
    COMMAND ${POPC_CLI} -m 500.0 -c "${TEST_DATA_DIR}/clusters.list" "${TEST_DATA_DIR}/data.tsv")
set_tests_properties(cli_custom_multiplier PROPERTIES PASS_REGULAR_EXPRESSION "^[0-9]")

add_test(NAME cli_custom_power
    COMMAND ${POPC_CLI} -p 5.0 -c "${TEST_DATA_DIR}/clusters.list" "${TEST_DATA_DIR}/data.tsv")
set_tests_properties(cli_custom_power PROPERTIES PASS_REGULAR_EXPRESSION "^[0-9]")

# Verbosity levels
foreach(level 0 1 2 3 quiet warning info debug)
    add_test(NAME cli_verbosity_${level}
        COMMAND ${POPC_CLI} -v ${level} -c "${TEST_DATA_DIR}/clusters.list" "${TEST_DATA_DIR}/data.tsv")
endforeach()

# Error paths
add_test(NAME cli_missing_file COMMAND ${POPC_CLI} nonexistent.tsv)
set_tests_properties(cli_missing_file PROPERTIES WILL_FAIL true)

add_test(NAME cli_missing_clusters_file
    COMMAND ${POPC_CLI} -c nonexistent.list "${TEST_DATA_DIR}/data.tsv")
set_tests_properties(cli_missing_clusters_file PROPERTIES WILL_FAIL true)

add_test(NAME cli_too_many_args COMMAND ${POPC_CLI} file1.tsv file2.tsv)
set_tests_properties(cli_too_many_args PROPERTIES WILL_FAIL true)

add_test(NAME cli_bad_delimiter_multichar COMMAND ${POPC_CLI} -t "ab")
set_tests_properties(cli_bad_delimiter_multichar PROPERTIES WILL_FAIL true)

add_test(NAME cli_bad_multiplier COMMAND ${POPC_CLI} -m "abc" "${TEST_DATA_DIR}/data.tsv")
set_tests_properties(cli_bad_multiplier PROPERTIES WILL_FAIL true)

add_test(NAME cli_bad_power COMMAND ${POPC_CLI} -p "xyz" "${TEST_DATA_DIR}/data.tsv")
set_tests_properties(cli_bad_power PROPERTIES WILL_FAIL true)

add_test(NAME cli_bad_verbosity COMMAND ${POPC_CLI} -v 99)
set_tests_properties(cli_bad_verbosity PROPERTIES WILL_FAIL true)

add_test(NAME cli_invalid_option COMMAND ${POPC_CLI} --not-a-flag)
set_tests_properties(cli_invalid_option PROPERTIES WILL_FAIL true)

# Reject inputs whose values aren't 0/1 (parser invariant)
add_test(NAME cli_bad_data_value
    COMMAND sh -c "printf 'a\\tb\\n2\\t0\\n' | ${POPC_CLI}")
set_tests_properties(cli_bad_data_value PROPERTIES WILL_FAIL true)

add_test(NAME cli_inconsistent_columns
    COMMAND sh -c "printf 'a\\tb\\tc\\n1\\t0\\n0\\t1\\t1\\n' | ${POPC_CLI}")
set_tests_properties(cli_inconsistent_columns PROPERTIES WILL_FAIL true)

# Empty input (no header, no rows) must not crash; exit cleanly with no output.
add_test(NAME cli_empty_input
    COMMAND sh -c "printf '' | ${POPC_CLI} -v quiet")

# --- Regression: deterministic output for known input ---
#
# Uses --clusters to bypass the random-seeded k-modes seeder, so the popc
# refinement runs against a fixed initial partition. The expected output is
# captured from a run of the verified-correct implementation; any future
# refactor that changes the algorithm's behavior on this input will fail
# this test.

add_test(NAME regression_data_tsv
    COMMAND sh -c "${POPC_CLI} -v quiet -c '${TEST_DATA_DIR}/clusters.list' '${TEST_DATA_DIR}/data.tsv' | diff - '${TEST_DATA_DIR}/expected_output.list'")
