// Standalone consumer compiled against an *installed* popc tree only.
//
// The rest of the test suite consumes the headers through the library's
// BUILD_INTERFACE, i.e. straight out of the source tree, so it stays green
// even when the FILE_SET HEADERS list omits a header and the installation is
// therefore uncompilable. This translation unit is built by a separate CMake
// project that locates popc via find_package(), with no path into the
// source tree at all. It includes every public header.

#include <iostream>
#include <sstream>

#include <popc/cluster.hpp>
#include <popc/dataset.hpp>
#include <popc/detail/bitpacked_kmeans.hpp>
#include <popc/popc.hpp>

int main() {
  std::istringstream input{"a\tb\n1\t0\n0\t1\n1\t1\n"};
  popc::dataset const data{input};

  if (data.num_instances() != 3 || data.num_attributes() != 2 || data.positive_count(0) != 2) {
    std::cerr << "unexpected dataset shape\n";
    return 1;
  }

  std::cout << "installed-tree consumer parsed " << data.num_instances() << " instances\n";
  return 0;
}
