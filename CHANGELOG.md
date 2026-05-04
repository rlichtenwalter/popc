# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Doxygen-style API docstrings on every public class, method, and free function in `include/popc/`, matching the convention used by the sibling C++ libraries (vcp, mRMR). The `popc::detail::bitpacked_kmodes_seed` family in the internal `detail/` namespace is also documented. The CLI-internal helpers in `src/popc.cpp` (verbosity_level, message_type, log_message, parse_double, parse_verbosity, run, main) also carry Doxygen briefs.
- CMake build system replacing custom Makefile (CMake 3.24+, presets for release/debug/sanitize)
- BSD 3-Clause license replacing MIT
- `.clang-format` and `.clang-tidy` configuration matching sibling library conventions
- `.pre-commit-config.yaml` with file hygiene and clang-format hooks
- Gitea Actions CI with build/test, quality, lint, and sanitize jobs
- CMake install targets with package config and pkg-config support
- CLI integration tests via CTest
- Full Catch2-based unit test suite (one test file per public header) covering cluster, dataset, popc, and the bitpacked k-modes seeder; auto-discovered via `catch_discover_tests`
- Regression test that pins popc's output on `test/data.tsv + test/clusters.list` against a captured baseline so future refactors cannot silently drift
- Expanded CLI/E2E test coverage: custom delimiter, custom multiplier/power, all eight verbosity values, missing-cluster-file path, multi-character delimiter rejection, malformed numeric arguments, malformed input data
- README expanded with algorithm summary, attribution to Peter Taraba, citations to the WCECS 2017 paper and the Springer chapter, link to the C# reference, and an explicit list of enhancements over the reference implementation

### Changed
- `popc::cluster` exposes `const_iterator begin() const` / `end() const` overloads so range-based `for` works on a const cluster
- Headers moved from `include/` to `include/popc/` subdirectory; consumers now use `#include <popc/popc.hpp>` etc.
- LICENSE copyright year updated to 2020-2026
- C++ standard raised from C++14 to C++20 throughout the codebase
- k-means seeding replaced with a header-only bitpacked binary k-modes implementation (`popc::detail::bitpacked_kmodes_seed`) using `std::popcount` for Hamming distance; eliminates the mlpack and Armadillo system dependencies
- `--version` now reports the actual version from the `VERSION` file (was hardcoded as `0.2 (beta)`)
- Dataset parse errors throw `std::runtime_error` instead of calling `exit(2)`; caught at the CLI boundary and translated to exit code 2

### Fixed
- `-v info` no longer collides with `-v warning` (was matching `"1"` instead of `"2"`); `-v debug` now correctly maps to verbosity 3 (was matching `"2"`)
- Invalid `-m`/`-p` values now exit non-zero instead of printing a warning and continuing with garbage values
- Read loop no longer reassigns the dataset on every stream iteration
- Sanitize preset now runs ASan with `detect_leaks=1`; first-party-only build no longer needs to suppress leaks from uninstrumented system libraries
- Empty input (zero-instance dataset) no longer dereferences `end()` of an empty assignment vector; CLI exits cleanly with no output
- Reformat the test_*.cpp files with the project's pinned clang-format v22.1.2; they predated the pre-commit hook install and had drifted slightly from the canonical formatting, which was caught by the CI quality job's `pre-commit run --all-files` step on the first pipeline execution
- Replace the C-style index loop in `test_cluster.cpp` with a range-based `for` over `std::as_const(c)`, and `std::max_element(begin, end)` with `std::ranges::max_element(range)` in `test_popc.cpp` — both flagged by the CI lint job's clang-tidy `modernize-*` checks (which run only against source TUs, not headers, so they were not caught by the pre-commit hooks)

## [0.5.0] - 2020-12-07

### Added
- Initial release with POPC binary feature clustering algorithm implementation
- Header-only library (`popc.hpp`, `cluster.hpp`, `dataset.hpp`)
- Command-line interface supporting file and stdin input, pre-computed k-means cluster seeding, configurable multiplier and power parameters, and verbosity control
