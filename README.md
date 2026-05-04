# popc

A C++20 implementation of **Powered Outer Probabilistic Clustering** (POPC),
a clustering algorithm for binary feature data introduced by Peter Taraba in
[Powered Outer Probabilistic Clustering][popc-paper] (WCECS 2017, best paper award).
The algorithm starts from many candidate clusters and iteratively merges
them by maximizing a powered-probability objective, converging on the optimal
number of clusters without requiring it to be specified in advance.

## Algorithm

For binary feature data with samples `s_j` and features `f_i`, define a
discounted probability that feature `f_i` belongs to cluster `k`:

```
p(cl(f_i) = k) = (c(s_j(f_i)=1, cl(s_j)=k) · C_m + 1) / (c(f_i=1) · C_m + N)
```

where `c` is the count function, `N` is the current number of clusters, and
`C_m` is a multiplying constant (default 1000). The evaluation function
raises each probability to a power `P` (default 10) before summing:

```
J = Σ_i Σ_k p^P(cl(f_i) = k) ≤ F
```

The algorithm seeds an initial partition with `N = samples / 2` clusters,
then iteratively reshuffles each sample into the destination cluster that
maximally increases `J`. Empty clusters are dropped during iteration so `N`
contracts naturally to the data's intrinsic cluster count.

## Enhancements over the reference

This implementation is functionally faithful to the [C# reference][csharp-ref]
that accompanies the paper, with several engineering improvements:

- **Parameterized C_m and P** — exposed as `--multiplier` and `--power` CLI
  flags. The reference hardcodes 1000 and 10 throughout.
- **Bitpacked binary k-modes seeding** — seed clusters are built with a
  header-only k-modes implementation that packs each sample into `uint64_t`
  chunks and uses `std::popcount` for Hamming distance. This is roughly a
  64× constant-factor speedup over per-bit kernels and avoids any system
  dependency on mlpack, Armadillo, or BLAS/LAPACK.
- **Header-only library** — `cluster`, `dataset`, and `popc` are pure
  C++20 templates, embeddable in any project via `find_package(popc)` or
  `add_subdirectory`.
- **Templated floating-point type** — `popc::popc<float>` and
  `popc::popc<double>` are both available; the reference is hardcoded to
  `double`.
- **Stream-based I/O** — input is read from any `std::istream`, so files,
  stdin, named pipes, and process substitution all work uniformly.
- **CI under sanitizers** — every push runs the test suite under
  AddressSanitizer + UndefinedBehaviorSanitizer with `-fno-sanitize-recover=all`.

## Building

Requires CMake 3.24+ and a C++20 compiler (GCC 12+ or Clang 16+). No system
libraries needed.

```bash
cmake --preset=release
cmake --build --preset=release

# Debug build
cmake --preset=debug
cmake --build --preset=debug

# Debug + AddressSanitizer + UndefinedBehaviorSanitizer
cmake --preset=sanitize
cmake --build --preset=sanitize
```

## Testing

```bash
ctest --preset=release
ctest --preset=sanitize
```

## Usage

```
Usage: popc [OPTION]... [FILE]

Generate POPC cluster assignments from input. Input is read either from
[FILE] or stdin: tabular binary (0 or 1) values separated by tab (or CHAR)
preceded by a single-line header naming the columns. Output is one integer
cluster assignment per line.

  -t, --delimiter=CHAR      field separator (default: TAB)
  -c, --clusters=CFILE      pre-computed cluster assignments (one per line)
  -m, --multiplier=MULT     multiplying constant C_m (default: 1000.0)
  -p, --power=POW           power constant P (default: 10.0)
  -v, --verbosity=VALUE     0/quiet, 1/warning, 2/info, 3/debug (default: 1)
  -h, --help                display this help and exit
  -V, --version             output version information and exit
```

Example:

```bash
popc data.tsv > assignments.txt           # bitpacked k-modes seed
popc -c kmeans.list data.tsv              # seed from external partition
popc -m 500 -p 5 data.tsv                 # custom hyperparameters
cat data.tsv | popc                       # stdin
```

## Programmatic use

```cpp
#include <popc/cluster.hpp>
#include <popc/dataset.hpp>
#include <popc/popc.hpp>
#include <popc/detail/bitpacked_kmeans.hpp>

#include <fstream>
#include <list>

std::ifstream in{"data.tsv"};
popc::dataset data{in};

auto seed = popc::detail::bitpacked_kmodes_seed(data, data.num_instances() / 2);

std::list<popc::cluster> clusters;
// ... build clusters from seed assignments ...

auto labels = popc::popc(data, clusters);
```

## Installing

```bash
cmake --install build/release --prefix /your/prefix
```

Installs the `popc` binary, the public headers, a CMake package config (so
downstream projects can `find_package(popc)` and link `popc::popc`), and a
pkg-config file.

## References

- Peter Taraba, *Powered Outer Probabilistic Clustering*, Proceedings of the
  World Congress on Engineering and Computer Science 2017 Vol I (WCECS 2017),
  October 25-27, 2017, San Francisco, USA. ISBN 978-988-14047-5-6.
  [PDF][popc-paper]
- Peter Taraba, *Clustering for Binary Featured Datasets*, in: Transactions
  on Engineering Technologies, Springer Singapore, 2019,
  [doi:10.1007/978-981-13-2191-7_10][popc-springer] (extended chapter version).
- Reference C# implementation: [pepe78/POPC-examples][csharp-ref] (mirror).

## License

BSD 3-Clause. See [LICENSE](LICENSE).

The POPC algorithm itself is the work of Peter Taraba; this repository is an
independent C++ implementation by Ryan N. Lichtenwalter.

[popc-paper]: https://www.iaeng.org/publication/WCECS2017/WCECS2017_pp394-398.pdf
[popc-springer]: https://doi.org/10.1007/978-981-13-2191-7_10
[csharp-ref]: https://github.com/nagornuiai/POPC-examples
