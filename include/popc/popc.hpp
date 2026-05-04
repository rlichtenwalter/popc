#ifndef POPC_POPC_HPP
#define POPC_POPC_HPP

#include <cmath>
#include <cstddef>
#include <limits>
#include <list>
#include <vector>

#include "cluster.hpp"
#include "dataset.hpp"

namespace popc {

/**
 * @brief Compute the change in the POPC objective J for a single move.
 *
 * Evaluates `delta = sum_i [ p_new^P - p_old^P ]` where the sum runs over
 * the attributes that are positive in instance `instance_num`. Attributes
 * that are zero in the instance leave the cluster's per-attribute count
 * unchanged for the candidate move and contribute zero to the delta, so
 * they are skipped.
 *
 * The per-attribute probability is
 * `p(cluster|f_i) = (counts * Cm + 1) / (counts_all * Cm + N)`, where:
 *   - `counts`     is `cluster.attribute_count(i)` (cluster-local count
 *                   of instances with attribute `i` set);
 *   - `counts_all` is `ds.positive_count(i)` (dataset-wide count of
 *                   instances with attribute `i` set);
 *   - `Cm`         is the multiplier hyperparameter (paper default 1000);
 *   - `N`          is the current number of clusters.
 *
 * The exponent `P` is the power hyperparameter (paper default 10, must
 * be greater than 1 for the objective to be sensitive to outer-mass
 * concentration rather than uniform).
 *
 * @tparam fptype       Floating-point type for the probability arithmetic.
 *                      `double` is the default and the recommended choice;
 *                      `float` produces visibly different rankings for
 *                      borderline moves on large datasets.
 * @param ds            Dataset providing `positive_count` and the row
 *                      iterators for `instance_num`.
 * @param cluster       Source or destination cluster whose `attribute_count`
 *                      is read for the current `counts` term.
 * @param instance_num  Zero-based index of the instance being moved.
 * @param num_clusters  Current number of clusters in the partition (the `N`
 *                      term in the denominator).
 * @param multiplier    The `Cm` hyperparameter.
 * @param power         The `P` hyperparameter.
 * @param added         `true` if `cluster` is the destination (instance is
 *                      being added; `counts` increments by 1); `false` if
 *                      `cluster` is the source (instance is being removed;
 *                      `counts` decrements by 1).
 *
 * @return Change in the cluster's contribution to J for the move.
 */
template <typename fptype = double>
[[nodiscard]] fptype compute_delta(popc::dataset const &ds, popc::cluster const &cluster,
                                   std::size_t instance_num, std::size_t num_clusters,
                                   fptype multiplier, fptype power, bool added) {
  fptype delta = 0;
  std::size_t attribute_num = 0;
  for (auto it = ds.cbegin(instance_num); it != ds.cend(instance_num); ++it) {
    if (*it) {
      auto const counts = static_cast<fptype>(cluster.attribute_count(attribute_num));
      auto const counts_all = static_cast<fptype>(ds.positive_count(attribute_num));
      fptype const denom = counts_all * multiplier + static_cast<fptype>(num_clusters);
      fptype const old_p = (counts * multiplier + 1) / denom;
      fptype const new_count = added ? counts + 1 : counts - 1;
      fptype const new_p = (new_count * multiplier + 1) / denom;
      delta -= std::pow(old_p, power);
      delta += std::pow(new_p, power);
    }
    ++attribute_num;
  }
  return delta;
}

/**
 * @brief Powered Outer Probabilistic Clustering refinement (Taraba 2017).
 *
 * Iteratively reshuffles each instance into the cluster that produces the
 * largest strict increase in the objective J. A move is accepted only when
 * the combined source-removal and destination-addition delta is strictly
 * positive (steepest-ascent local search). Empty clusters are removed
 * during iteration so that the `N` term in the probability denominator
 * always reflects the current partition size. The loop terminates when
 * an entire pass over every instance produces no moves.
 *
 * The seeded `clusters` argument is consumed in place: the function
 * mutates it as moves are accepted, and the resulting partition lives
 * in this list at return time. The returned label vector is a flat
 * `instance_num -> cluster_index` map produced by walking the final
 * cluster list.
 *
 * @tparam fptype     Floating-point type for the probability arithmetic.
 *                    See compute_delta() for selection guidance.
 * @param ds          Source dataset; provides instance values and the
 *                    dataset-wide positive counts.
 * @param clusters    Seeded partition. Each cluster's `attribute_counts`
 *                    must already match its members. Mutated in place.
 * @param multiplier  The `Cm` hyperparameter (defaults to the paper's 1000).
 * @param power       The `P` hyperparameter (defaults to the paper's 10).
 *
 * @return Vector of size `ds.num_instances()` mapping each instance index
 *         to its final cluster index in `[0, clusters.size())`. Cluster
 *         indices are assigned in the order in which the clusters appear
 *         in `clusters` at return time.
 */
template <typename fptype = double>
[[nodiscard]] std::vector<std::size_t>
popc(popc::dataset const &ds, std::list<popc::cluster> &clusters,
     fptype multiplier = static_cast<fptype>(1000), fptype power = static_cast<fptype>(10)) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto cluster_it = clusters.begin(); cluster_it != clusters.end();) {
      auto &src = *cluster_it;
      for (auto instance_it = src.begin(); instance_it != src.end();) {
        auto const instance_num = *instance_it;
        auto largest_gain = -std::numeric_limits<fptype>::infinity();
        popc::cluster *dest = nullptr;
        auto const delta_base =
            compute_delta(ds, src, instance_num, clusters.size(), multiplier, power,
                          /*added=*/false);
        for (auto &candidate : clusters) {
          if (&src == &candidate) {
            continue;
          }
          auto const delta = delta_base + compute_delta(ds, candidate, instance_num,
                                                        clusters.size(), multiplier, power,
                                                        /*added=*/true);
          if (delta > largest_gain) {
            largest_gain = delta;
            dest = &candidate;
          }
        }
        if (largest_gain > 0 && dest != nullptr) {
          changed = true;
          instance_it = src.remove_instance(instance_it);
          dest->add_instance(instance_num);
          std::size_t attribute_num = 0;
          for (auto val_it = ds.cbegin(instance_num); val_it != ds.cend(instance_num); ++val_it) {
            if (*val_it) {
              src.decrement_attribute_count(attribute_num);
              dest->increment_attribute_count(attribute_num);
            }
            ++attribute_num;
          }
        } else {
          ++instance_it;
        }
      }
      if (src.empty()) {
        cluster_it = clusters.erase(cluster_it);
      } else {
        ++cluster_it;
      }
    }
  }
  std::vector<std::size_t> labels(ds.num_instances());
  std::size_t cluster_index = 0;
  for (auto const &c : clusters) {
    for (auto it = c.cbegin(); it != c.cend(); ++it) {
      labels[*it] = cluster_index;
    }
    ++cluster_index;
  }
  return labels;
}

} // namespace popc

#endif // POPC_POPC_HPP
