#include <bit>
#include <cstdint>
#include <set>
#include <sstream>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <popc/dataset.hpp>
#include <popc/detail/bitpacked_kmeans.hpp>

using popc::detail::bitpacked_dataset;
using popc::detail::bitpacked_kmodes_seed;
using popc::detail::hamming_distance;

// ============================================================
// bitpacked_dataset
// ============================================================

TEST_CASE("bitpacked_dataset: dimensions reflect the source dataset", "[bitpacked_dataset]") {
  // 70 attributes => spans 2 uint64 words (1 word holds 64 bits).
  std::ostringstream header;
  header << "f0";
  for (int i = 1; i < 70; ++i) {
    header << "\tf" << i;
  }
  std::ostringstream row;
  for (int i = 0; i < 70; ++i) {
    if (i > 0) {
      row << '\t';
    }
    row << '0';
  }
  std::istringstream stream{header.str() + "\n" + row.str() + "\n" + row.str() + "\n"};
  popc::dataset ds{stream};
  bitpacked_dataset bp{ds};

  CHECK(bp.num_instances() == 2);
  CHECK(bp.num_attributes() == 70);
  CHECK(bp.words_per_instance() == 2);
}

TEST_CASE("bitpacked_dataset: bits round-trip from the source", "[bitpacked_dataset]") {
  std::istringstream stream{"a\tb\tc\td\te\n1\t0\t1\t1\t0\n0\t1\t0\t0\t1\n"};
  popc::dataset ds{stream};
  bitpacked_dataset bp{ds};

  REQUIRE(bp.num_instances() == 2);
  REQUIRE(bp.num_attributes() == 5);
  REQUIRE(bp.words_per_instance() == 1);

  auto inst0 = bp.instance(0);
  // Bits 0, 2, 3 set => 0b01101 = 13
  CHECK(inst0[0] == 0b01101ULL);

  auto inst1 = bp.instance(1);
  // Bits 1, 4 set => 0b10010 = 18
  CHECK(inst1[0] == 0b10010ULL);
}

TEST_CASE("bitpacked_dataset: trailing padding bits are zero", "[bitpacked_dataset]") {
  // 3 attributes, all set. The trailing 61 bits of the uint64 must be zero.
  std::istringstream stream{"a\tb\tc\n1\t1\t1\n"};
  popc::dataset ds{stream};
  bitpacked_dataset bp{ds};

  auto inst = bp.instance(0);
  CHECK(inst[0] == 0b111ULL);
  CHECK(std::popcount(inst[0]) == 3);
}

// ============================================================
// hamming_distance
// ============================================================

TEST_CASE("hamming_distance: identical vectors have distance 0", "[hamming]") {
  std::vector<std::uint64_t> a{0xDEADBEEFCAFEBABEULL, 0x0123456789ABCDEFULL};
  CHECK(hamming_distance(a, a) == 0);
}

TEST_CASE("hamming_distance: complement has distance equal to bit count", "[hamming]") {
  std::vector<std::uint64_t> a{0x0ULL, 0x0ULL};
  std::vector<std::uint64_t> b{~0x0ULL, ~0x0ULL};
  CHECK(hamming_distance(a, b) == 128);
}

TEST_CASE("hamming_distance: single-bit difference", "[hamming]") {
  std::vector<std::uint64_t> a{0x1ULL};
  std::vector<std::uint64_t> b{0x3ULL}; // differs in bit 1
  CHECK(hamming_distance(a, b) == 1);
}

TEST_CASE("hamming_distance: multiple-word difference", "[hamming]") {
  std::vector<std::uint64_t> a{0xFFULL, 0x00ULL};
  std::vector<std::uint64_t> b{0x00ULL, 0xFFULL};
  CHECK(hamming_distance(a, b) == 16); // 8 differences in each word
}

// ============================================================
// bitpacked_kmodes_seed
// ============================================================

TEST_CASE("bitpacked_kmodes_seed: returns one assignment per instance", "[kmodes]") {
  std::istringstream stream{"a\tb\n1\t0\n0\t1\n1\t1\n0\t0\n"};
  popc::dataset ds{stream};
  bitpacked_dataset bp{ds};

  auto labels = bitpacked_kmodes_seed(bp, /*k=*/2, /*max_iterations=*/16,
                                      /*seed=*/42ULL);
  REQUIRE(labels.size() == ds.num_instances());
  for (auto const a : labels) {
    CHECK(a < 2);
  }
}

TEST_CASE("bitpacked_kmodes_seed: deterministic with fixed seed", "[kmodes]") {
  std::istringstream stream{
      "a\tb\tc\td\n1\t1\t0\t0\n1\t1\t0\t0\n0\t0\t1\t1\n0\t0\t1\t1\n1\t0\t1\t0\n"};
  popc::dataset ds{stream};
  bitpacked_dataset bp{ds};

  auto a = bitpacked_kmodes_seed(bp, 3, 16, 1234ULL);
  auto b = bitpacked_kmodes_seed(bp, 3, 16, 1234ULL);
  CHECK(a == b);
}

TEST_CASE("bitpacked_kmodes_seed: different seeds may yield different partitions", "[kmodes]") {
  // Not a strict guarantee — for some inputs every seed converges to the
  // same partition — but with 5 distinct points and k=3, we expect at
  // least one of these seeds to disagree with seed 1.
  std::istringstream stream{
      "a\tb\tc\td\n1\t0\t0\t0\n0\t1\t0\t0\n0\t0\t1\t0\n0\t0\t0\t1\n1\t1\t0\t0\n"};
  popc::dataset ds{stream};
  bitpacked_dataset bp{ds};

  auto seed1 = bitpacked_kmodes_seed(bp, 3, 16, 1ULL);
  bool any_different = false;
  for (std::uint64_t s = 2; s <= 32; ++s) {
    auto seedn = bitpacked_kmodes_seed(bp, 3, 16, s);
    if (seedn != seed1) {
      any_different = true;
      break;
    }
  }
  CHECK(any_different);
}

TEST_CASE("bitpacked_kmodes_seed: pulls identical instances together", "[kmodes]") {
  // Three groups of identical binary patterns. The seeder should converge
  // such that each group's members share a label.
  std::istringstream stream{"a\tb\tc\td\n"
                            "1\t1\t0\t0\n"
                            "1\t1\t0\t0\n"
                            "1\t1\t0\t0\n"
                            "0\t0\t1\t1\n"
                            "0\t0\t1\t1\n"
                            "0\t0\t1\t1\n"
                            "1\t0\t1\t0\n"
                            "1\t0\t1\t0\n"
                            "1\t0\t1\t0\n"};
  popc::dataset ds{stream};
  bitpacked_dataset bp{ds};

  auto labels = bitpacked_kmodes_seed(bp, 3, 64, 7ULL);
  REQUIRE(labels.size() == 9);

  // Within each triple, all three instances should land in the same cluster.
  CHECK(labels[0] == labels[1]);
  CHECK(labels[1] == labels[2]);
  CHECK(labels[3] == labels[4]);
  CHECK(labels[4] == labels[5]);
  CHECK(labels[6] == labels[7]);
  CHECK(labels[7] == labels[8]);
}

TEST_CASE("bitpacked_kmodes_seed: handles k = 1 trivially", "[kmodes]") {
  std::istringstream stream{"a\tb\n1\t0\n0\t1\n1\t1\n"};
  popc::dataset ds{stream};
  bitpacked_dataset bp{ds};

  auto labels = bitpacked_kmodes_seed(bp, 1, 16, 0ULL);
  REQUIRE(labels.size() == 3);
  CHECK(labels[0] == 0);
  CHECK(labels[1] == 0);
  CHECK(labels[2] == 0);
}

TEST_CASE("bitpacked_kmodes_seed: k larger than n is clamped to n", "[kmodes]") {
  std::istringstream stream{"a\n1\n0\n"};
  popc::dataset ds{stream};
  bitpacked_dataset bp{ds};

  auto labels = bitpacked_kmodes_seed(bp, /*k=*/100, 16, 0ULL);
  REQUIRE(labels.size() == 2);
  // With k clamped to 2 and only 2 instances, both labels should fit in [0,2)
  for (auto const a : labels) {
    CHECK(a < 2);
  }
}

TEST_CASE("bitpacked_kmodes_seed: empty dataset returns empty assignment", "[kmodes]") {
  popc::dataset ds;
  bitpacked_dataset bp{ds};
  auto labels = bitpacked_kmodes_seed(bp, 4, 16, 0ULL);
  CHECK(labels.empty());
}
