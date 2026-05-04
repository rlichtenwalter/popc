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

## [0.5.0] - 2020-12-07

### Added
- Initial release with POPC binary feature clustering algorithm implementation
- Header-only library (`popc.hpp`, `cluster.hpp`, `dataset.hpp`)
- Command-line interface supporting file and stdin input, pre-computed k-means cluster seeding, configurable multiplier and power parameters, and verbosity control
