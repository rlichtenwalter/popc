# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed

- Pin the `mixed-line-ending` pre-commit hook to `--fix=lf` so every commit normalises files to LF

## [1.0.1] - 2026-05-05

### Added
- Gitea Actions workflow that mirrors Gitea releases to GitHub, covering the release metadata Gitea's push mirror does not sync
  - Manual `workflow_dispatch` path with a `tag` input for mirroring an existing release
- Ignore `.env` and `.env.*` in `.gitignore` while still permitting a committed `.env.example` template

## [1.0.0] - 2026-05-04

### Added
- Doxygen-style API docstrings on every public class, method, and free function in `include/popc/`, plus the internal `detail/` namespace and CLI helpers
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
- Drop `-march=native` from the Release build of `popc-cli`, making the binary portable across x86-64 baselines
- LICENSE copyright year updated to 2020-2026
- C++ standard raised from C++14 to C++20 throughout the codebase
- k-means seeding replaced with a header-only bitpacked binary k-modes implementation, eliminating the mlpack and Armadillo system dependencies
- `--version` now reports the actual version from the `VERSION` file (was hardcoded as `0.2 (beta)`)
- Dataset parse errors throw `std::runtime_error` instead of calling `exit(2)`; caught at the CLI boundary and translated to exit code 2

### Fixed
- `-v info` no longer collides with `-v warning` (was matching `"1"` instead of `"2"`); `-v debug` now correctly maps to verbosity 3 (was matching `"2"`)
- Invalid `-m`/`-p` values now exit non-zero instead of printing a warning and continuing with garbage values
- Read loop no longer reassigns the dataset on every stream iteration
- Sanitize preset now runs ASan with `detect_leaks=1`; first-party-only build no longer needs to suppress leaks from uninstrumented system libraries
- Empty input (zero-instance dataset) no longer dereferences `end()` of an empty assignment vector; CLI exits cleanly with no output
- Reformat the test_*.cpp files with the project's pinned clang-format v22.1.2
- Modernize test code flagged by clang-tidy `modernize-*` checks: range-based `for` in `test_cluster.cpp`, `std::ranges::max_element` in `test_popc.cpp`
- Dataset stream parser now reports the correct line number on malformed input (previously always claimed "line 2")
- Bitpacked k-modes majority-vote test rewritten to avoid integer overflow for cluster sizes above ~2 billion on 32-bit `size_t`
- `dataset::operator<<` casts `std::vector<bool>` proxy reference values through `int` before stream insertion

## [0.5.0] - 2020-12-07

### Added
- Initial release with POPC binary feature clustering algorithm implementation
- Header-only library (`popc.hpp`, `cluster.hpp`, `dataset.hpp`)
- Command-line interface supporting file and stdin input, pre-computed k-means cluster seeding, configurable multiplier and power parameters, and verbosity control
