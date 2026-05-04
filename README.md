# popc

Efficient C++ implementation of the POPC binary feature clustering algorithm.

## Building

Requires CMake 3.24+, a C++20 compiler, and the mlpack and Armadillo libraries.

```bash
# Release build
cmake --preset=release
cmake --build --preset=release

# Debug build
cmake --preset=debug
cmake --build --preset=debug

# Debug build with AddressSanitizer + UndefinedBehaviorSanitizer
cmake --preset=sanitize
cmake --build --preset=sanitize
```

## Testing

```bash
ctest --preset=release
ctest --preset=debug
ctest --preset=sanitize
```

## Usage

```
Usage: popc [OPTION]... [FILE]

Generate POPC cluster assignments from input. Input may be taken from standard
input or from FILE. Input is a tab-separated binary (0/1) matrix preceded by a
header line naming the columns. Output is one integer cluster assignment per line.

  -t, --delimiter=CHAR      field separator (default: TAB)
  -c, --clusters=CFILE      pre-computed k-means cluster assignments (one per line)
  -m, --multiplier=MULT     multiplying constant C_m (default: 1000.0)
  -p, --power=POW           power constant P (default: 10.0)
  -v, --verbosity=VALUE     0/quiet, 1/warning, 2/info, 3/debug (default: 1)
  -h, --help                display this help and exit
  -V, --version             output version information and exit
```

## Installing

```bash
cmake --install build/release
```

## Development

```bash
# Run quality checks
uvx --with pre-commit==4.5.1 pre-commit run --all-files

# Lint
clang-tidy -p build/release src/*.cpp test/*.cpp
```
