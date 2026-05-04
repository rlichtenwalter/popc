# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- CMake build system replacing custom Makefile (CMake 3.24+, presets for release/debug/sanitize)
- BSD 3-Clause license replacing MIT
- `.clang-format` and `.clang-tidy` configuration matching sibling library conventions
- `.pre-commit-config.yaml` with file hygiene and clang-format hooks
- Gitea Actions CI with build/test, quality, lint, and sanitize jobs
- CMake install targets with package config and pkg-config support
- CLI integration tests via CTest

### Changed
- Headers moved from `include/` to `include/popc/` subdirectory; consumers now use `#include <popc/popc.hpp>` etc.
- C++ standard raised from C++14 to C++20 throughout the codebase
- k-means seeding replaced with a header-only bitpacked binary k-modes implementation (`popc::detail::bitpacked_kmodes_seed`) using `std::popcount` for Hamming distance; eliminates the mlpack and Armadillo system dependencies
- `--version` now reports the actual version from the `VERSION` file (was hardcoded as `0.2 (beta)`)
- Dataset parse errors throw `std::runtime_error` instead of calling `exit(2)`; caught at the CLI boundary and translated to exit code 2

### Fixed
- `-v info` no longer collides with `-v warning` (was matching `"1"` instead of `"2"`); `-v debug` now correctly maps to verbosity 3 (was matching `"2"`)
- Invalid `-m`/`-p` values now exit non-zero instead of printing a warning and continuing with garbage values
- Read loop no longer reassigns the dataset on every stream iteration

## [0.5.0] - 2020-12-07

### Added
- Initial release with POPC binary feature clustering algorithm implementation
- Header-only library (`popc.hpp`, `cluster.hpp`, `dataset.hpp`)
- Command-line interface supporting file and stdin input, pre-computed k-means cluster seeding, configurable multiplier and power parameters, and verbosity control
