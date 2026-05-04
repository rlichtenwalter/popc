#ifndef POPC_DETAIL_BITPACKED_KMEANS_HPP
#define POPC_DETAIL_BITPACKED_KMEANS_HPP

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <span>
#include <vector>

#include "../dataset.hpp"

namespace popc::detail {

// Bitpacked binary dataset: each instance occupies words_per_instance() 64-bit
// words, contiguously stored. Padding bits in the trailing word are zero.
//
// Constructed from a popc::dataset (which uses std::vector<bool> internally).
// The bitpacked form lets distance kernels operate on whole words via popcount
// instead of one bit per branch, which is the entire point of using it for
// k-means seeding on binary data.
class bitpacked_dataset {
public:
  using word_type = std::uint64_t;
  static constexpr std::size_t bits_per_word = 64;

  explicit bitpacked_dataset(popc::dataset const &ds)
      : num_instances_{ds.num_instances()}, num_attributes_{ds.num_attributes()},
        words_per_instance_{(ds.num_attributes() + bits_per_word - 1) / bits_per_word},
        data_(num_instances_ * words_per_instance_, word_type{0}) {
    for (std::size_t i = 0; i < num_instances_; ++i) {
      std::size_t const base = i * words_per_instance_;
      std::size_t bit = 0;
      for (auto it = ds.cbegin(i); it != ds.cend(i); ++it) {
        if (*it) {
          data_[base + bit / bits_per_word] |= (word_type{1} << (bit % bits_per_word));
        }
        ++bit;
      }
    }
  }

  [[nodiscard]] std::size_t num_instances() const noexcept { return num_instances_; }
  [[nodiscard]] std::size_t num_attributes() const noexcept { return num_attributes_; }
  [[nodiscard]] std::size_t words_per_instance() const noexcept { return words_per_instance_; }

  [[nodiscard]] std::span<word_type const> instance(std::size_t i) const noexcept {
    return {data_.data() + i * words_per_instance_, words_per_instance_};
  }

private:
  std::size_t num_instances_;
  std::size_t num_attributes_;
  std::size_t words_per_instance_;
  std::vector<word_type> data_;
};

// Hamming distance between two bitpacked vectors of the same word length.
// Compiles to one popcount per 64 features on x86-64 (POPCNT) and ARMv8 (CNT).
[[nodiscard]] inline std::size_t
hamming_distance(std::span<bitpacked_dataset::word_type const> a,
                 std::span<bitpacked_dataset::word_type const> b) noexcept {
  std::size_t dist = 0;
  for (std::size_t w = 0; w < a.size(); ++w) {
    dist += static_cast<std::size_t>(std::popcount(a[w] ^ b[w]));
  }
  return dist;
}

// Bitpacked binary k-modes clustering for POPC seeding.
//
// k-modes (a.k.a. binary k-means with majority-vote centroids) is chosen over
// probabilistic-centroid k-means because:
//   1. Centroids stay binary, so the assignment kernel is one popcount per word
//      rather than F floating-point ops per distance.
//   2. POPC immediately re-partitions whatever seed we hand it; the partition's
//      exact identity matters less than its arrival cost.
//
// Algorithm:
//   1. Random initial assignment (each instance to a uniformly-chosen cluster).
//      Matches the C# reference and avoids k-means++'s O(n*k) pre-pass, which
//      with POPC's k = n/2 default would be quadratic in n by itself.
//   2. Lloyd's iteration:
//      a. For each cluster, recompute the centroid: bit j is 1 iff a strict
//         majority (count > size/2) of cluster members have bit j set.
//      b. Reassign each instance to its nearest centroid by Hamming distance.
//      c. Stop when no instance changes assignment, or after max_iterations.
//
// With k = n/2 (POPC's default seed count), each Lloyd's iteration is O(n^2 *
// words_per_instance) — quadratic in n by construction of the algorithm, not
// of this implementation. Bitpacking gives a ~64x constant-factor speedup over
// per-bit kernels.
//
// Returns a vector of length data.num_instances() with values in [0, k).
// Empty clusters are tolerated; downstream POPC removes them as instances
// drain.
[[nodiscard]] inline std::vector<std::size_t> bitpacked_kmodes_seed(bitpacked_dataset const &data,
                                                                    std::size_t k,
                                                                    std::size_t max_iterations,
                                                                    std::uint64_t seed) {
  std::size_t const n = data.num_instances();
  std::size_t const f = data.num_attributes();
  std::size_t const wpi = data.words_per_instance();
  using word_type = bitpacked_dataset::word_type;

  if (n == 0 || k == 0) {
    return {};
  }
  if (k > n) {
    k = n;
  }

  std::mt19937_64 rng{seed};

  // Step 1: random initial assignment.
  std::vector<std::size_t> assignments(n);
  {
    std::uniform_int_distribution<std::size_t> dist{0, k - 1};
    for (std::size_t i = 0; i < n; ++i) {
      assignments[i] = dist(rng);
    }
  }

  // Reusable scratch buffers.
  std::vector<word_type> centroids(k * wpi, word_type{0});
  std::vector<std::size_t> bit_counts(f);
  std::vector<std::size_t> cluster_sizes(k);
  std::vector<std::vector<std::size_t>> members(k);

  for (std::size_t iter = 0; iter < max_iterations; ++iter) {
    // Bucket instances by cluster.
    for (auto &m : members) {
      m.clear();
    }
    for (std::size_t i = 0; i < n; ++i) {
      members[assignments[i]].push_back(i);
    }

    // Step 2a: recompute centroids by majority vote.
    for (std::size_t c = 0; c < k; ++c) {
      std::size_t const size = members[c].size();
      cluster_sizes[c] = size;
      std::size_t const cent_off = c * wpi;
      std::fill_n(centroids.begin() + static_cast<std::ptrdiff_t>(cent_off), wpi, word_type{0});
      if (size == 0) {
        continue;
      }
      std::ranges::fill(bit_counts, std::size_t{0});
      for (std::size_t i : members[c]) {
        auto const inst = data.instance(i);
        for (std::size_t w = 0; w < wpi; ++w) {
          word_type word = inst[w];
          std::size_t const base = w * bitpacked_dataset::bits_per_word;
          // Padding bits in the trailing word are guaranteed zero by
          // bitpacked_dataset's constructor, so no set bit can have
          // base + b >= f.
          while (word != 0U) {
            auto const b = static_cast<std::size_t>(std::countr_zero(word));
            ++bit_counts[base + b];
            word &= word - 1;
          }
        }
      }
      // Strict majority: 2 * count > size, integer-safe.
      for (std::size_t j = 0; j < f; ++j) {
        if (2 * bit_counts[j] > size) {
          centroids[cent_off + j / bitpacked_dataset::bits_per_word] |=
              (word_type{1} << (j % bitpacked_dataset::bits_per_word));
        }
      }
    }

    // Step 2b: reassign instances to nearest centroid.
    bool changed = false;
    for (std::size_t i = 0; i < n; ++i) {
      auto const inst = data.instance(i);
      std::size_t best_c = assignments[i];
      std::size_t best_d = std::numeric_limits<std::size_t>::max();
      for (std::size_t c = 0; c < k; ++c) {
        // Skip centroids of empty clusters: they are all-zero, which
        // would attract every all-zero-ish instance and starve other
        // clusters. Leaving an instance assigned to its current cluster
        // when no non-empty cluster is closer keeps the partition
        // moving rather than collapsing.
        if (cluster_sizes[c] == 0) {
          continue;
        }
        std::span<word_type const> cent{centroids.data() + c * wpi, wpi};
        std::size_t const d = hamming_distance(inst, cent);
        if (d < best_d) {
          best_d = d;
          best_c = c;
        }
      }
      if (assignments[i] != best_c) {
        assignments[i] = best_c;
        changed = true;
      }
    }

    if (!changed) {
      break;
    }
  }

  return assignments;
}

// Convenience overload: takes a popc::dataset and bitpacks it inline.
[[nodiscard]] inline std::vector<std::size_t>
bitpacked_kmodes_seed(popc::dataset const &ds, std::size_t k, std::size_t max_iterations = 32,
                      std::uint64_t seed = std::random_device{}()) {
  bitpacked_dataset const data{ds};
  return bitpacked_kmodes_seed(data, k, max_iterations, seed);
}

} // namespace popc::detail

#endif // POPC_DETAIL_BITPACKED_KMEANS_HPP
