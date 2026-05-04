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

/**
 * @brief Bitpacked binary view over a popc::dataset.
 *
 * Each instance occupies `words_per_instance()` 64-bit words, contiguously
 * stored. Padding bits in the trailing word are zero — a guarantee used by
 * the centroid-recomputation loop in bitpacked_kmodes_seed() to skip
 * out-of-range bookkeeping.
 *
 * Constructed from a `popc::dataset` (whose internal storage is
 * `std::vector<bool>`). The bitpacked form lets distance kernels operate on
 * whole 64-bit words via popcount instead of one bit per branch — the
 * entire point of using it for k-means seeding on binary data.
 */
class bitpacked_dataset {
public:
  using word_type = std::uint64_t;
  static constexpr std::size_t bits_per_word = 64;

  /**
   * @brief Pack a popc::dataset into 64-bit-word storage.
   *
   * Reads bits from the source dataset row-by-row and lays them out little-end
   * within each word. Padding bits in the trailing word of each instance
   * (when `num_attributes()` is not a multiple of 64) are zero-initialized.
   *
   * @param ds Source binary dataset to pack.
   */
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

  /** @brief Return the number of packed instances. */
  [[nodiscard]] std::size_t num_instances() const noexcept { return num_instances_; }

  /** @brief Return the number of attributes per instance. */
  [[nodiscard]] std::size_t num_attributes() const noexcept { return num_attributes_; }

  /** @brief Return the number of 64-bit words used to store one instance. */
  [[nodiscard]] std::size_t words_per_instance() const noexcept { return words_per_instance_; }

  /**
   * @brief Return a read-only span over one packed instance.
   *
   * @param i Zero-based instance index in `[0, num_instances())`.
   * @return Span of `words_per_instance()` words.
   */
  [[nodiscard]] std::span<word_type const> instance(std::size_t i) const noexcept {
    return {data_.data() + i * words_per_instance_, words_per_instance_};
  }

private:
  std::size_t num_instances_;
  std::size_t num_attributes_;
  std::size_t words_per_instance_;
  std::vector<word_type> data_;
};

/**
 * @brief Hamming distance between two bitpacked vectors of equal word length.
 *
 * Compiles to one popcount instruction per 64 features on x86-64 (POPCNT)
 * and ARMv8 (CNT). The two spans must have the same length; the function
 * does not check.
 *
 * @param a First operand.
 * @param b Second operand. Must have the same size as `a`.
 * @return Number of bit positions where `a` and `b` differ.
 */
[[nodiscard]] inline std::size_t
hamming_distance(std::span<bitpacked_dataset::word_type const> a,
                 std::span<bitpacked_dataset::word_type const> b) noexcept {
  std::size_t dist = 0;
  for (std::size_t w = 0; w < a.size(); ++w) {
    dist += static_cast<std::size_t>(std::popcount(a[w] ^ b[w]));
  }
  return dist;
}

/**
 * @brief Bitpacked binary k-modes clustering for POPC seeding.
 *
 * k-modes (binary k-means with majority-vote centroids) is chosen over
 * probabilistic-centroid k-means because:
 *   1. Centroids stay binary, so the assignment kernel is one popcount per
 *      word rather than F floating-point operations per distance.
 *   2. POPC immediately re-partitions whatever seed it receives; the seed
 *      partition's exact identity matters less than its arrival cost.
 *
 * Algorithm:
 *   1. Random initial assignment (each instance to a uniformly-chosen
 *      cluster). Matches the C# reference and avoids k-means++'s O(n*k)
 *      pre-pass, which with POPC's `k = n/2` default would be quadratic
 *      in n by itself.
 *   2. Lloyd's iteration:
 *      a. For each cluster, recompute the centroid: bit j is 1 iff a strict
 *         majority (count > size/2) of cluster members have bit j set.
 *      b. Reassign each instance to its nearest centroid by Hamming distance.
 *      c. Stop when no instance changes assignment, or after `max_iterations`.
 *
 * With `k = n/2`, each Lloyd's iteration is `O(n^2 * words_per_instance)` —
 * quadratic in n by construction of the algorithm, not by limitation of
 * this implementation. Bitpacking yields a roughly 64x constant-factor
 * speedup over per-bit kernels.
 *
 * Empty clusters are tolerated and may emerge from Lloyd's iteration;
 * downstream POPC removes them as instances drain.
 *
 * @param data           Pre-packed binary dataset.
 * @param k              Requested number of clusters. Clamped to
 *                       `data.num_instances()` if larger.
 * @param max_iterations Hard cap on Lloyd's iterations.
 * @param seed           Seed for the `std::mt19937_64` used for the random
 *                       initial assignment. Same seed yields the same
 *                       partition for the same input.
 *
 * @return Vector of length `data.num_instances()` mapping each instance
 *         to its final cluster index in `[0, k)`. Empty when `data` has
 *         no instances or `k == 0`.
 */
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
      // Strict majority: count > size - count is equivalent to 2*count > size
      // but avoids the intermediate multiplication that would overflow on a
      // 32-bit size_t with cluster sizes above ~2 billion.
      for (std::size_t j = 0; j < f; ++j) {
        if (bit_counts[j] > size - bit_counts[j]) {
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

/**
 * @brief Convenience overload: pack a popc::dataset and seed in one call.
 *
 * Equivalent to constructing a `bitpacked_dataset` from `ds` and calling
 * the primary `bitpacked_kmodes_seed` overload. Use the primary overload
 * when you intend to call the seeder more than once on the same data, so
 * the bitpacking work is paid only once.
 *
 * @param ds             Source binary dataset.
 * @param k              Requested number of clusters.
 * @param max_iterations Hard cap on Lloyd's iterations. Defaults to 32.
 * @param seed           PRNG seed. Defaults to a value drawn from
 *                       `std::random_device`, so calls are non-reproducible
 *                       unless an explicit seed is supplied.
 *
 * @return Cluster-assignment vector; see the primary overload.
 */
[[nodiscard]] inline std::vector<std::size_t>
bitpacked_kmodes_seed(popc::dataset const &ds, std::size_t k, std::size_t max_iterations = 32,
                      std::uint64_t seed = std::random_device{}()) {
  bitpacked_dataset const data{ds};
  return bitpacked_kmodes_seed(data, k, max_iterations, seed);
}

} // namespace popc::detail

#endif // POPC_DETAIL_BITPACKED_KMEANS_HPP
