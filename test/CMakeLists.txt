set(POPC_CLI $<TARGET_FILE:popc-cli>)
set(TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# --- CLI integration tests ---

add_test(NAME cli_version COMMAND ${POPC_CLI} --version)

add_test(NAME cli_help COMMAND ${POPC_CLI} --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")

add_test(NAME cli_help_short COMMAND ${POPC_CLI} -h)
set_tests_properties(cli_help_short PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")

add_test(NAME cli_file_input
    COMMAND ${POPC_CLI} -c "${TEST_DATA_DIR}/clusters.list" "${TEST_DATA_DIR}/data.tsv")
set_tests_properties(cli_file_input PROPERTIES PASS_REGULAR_EXPRESSION "[0-9]+")

add_test(NAME cli_stdin_input
    COMMAND sh -c "cat '${TEST_DATA_DIR}/data.tsv' | ${POPC_CLI} -c '${TEST_DATA_DIR}/clusters.list'")
set_tests_properties(cli_stdin_input PROPERTIES PASS_REGULAR_EXPRESSION "[0-9]+")

add_test(NAME cli_missing_file COMMAND ${POPC_CLI} nonexistent.tsv)
set_tests_properties(cli_missing_file PROPERTIES WILL_FAIL true)

add_test(NAME cli_too_many_args COMMAND ${POPC_CLI} file1.tsv file2.tsv)
set_tests_properties(cli_too_many_args PROPERTIES WILL_FAIL true)

add_test(NAME cli_quiet
    COMMAND ${POPC_CLI} -v quiet -c "${TEST_DATA_DIR}/clusters.list" "${TEST_DATA_DIR}/data.tsv")
set_tests_properties(cli_quiet PROPERTIES PASS_REGULAR_EXPRESSION "[0-9]+")

add_test(NAME cli_verbosity_0
    COMMAND ${POPC_CLI} -v 0 -c "${TEST_DATA_DIR}/clusters.list" "${TEST_DATA_DIR}/data.tsv")
add_test(NAME cli_verbosity_warning
    COMMAND ${POPC_CLI} -v warning -c "${TEST_DATA_DIR}/clusters.list" "${TEST_DATA_DIR}/data.tsv")
