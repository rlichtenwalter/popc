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

// Change in J = sum_i sum_k p^P(cl(f_i)=k) when sample `instance_num` is added
// to or removed from `cluster`. Only attributes that are positive in the
// instance contribute, since attributes that are zero in the instance leave
// the cluster's per-feature count unchanged for that move.
//
// p(cl(f_i)=k) = (counts * Cm + 1) / (counts_all * Cm + N), where
//   counts     = c(s_j(f_i)=1, cl(s_j)=k)   — cluster.attribute_count(i)
//   counts_all = c(f_i=1)                   — ds.positive_count(i)
//   Cm         = multiplier (paper default 1000)
//   N          = num_clusters
//   P          = power (paper default 10, must be > 1)
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

// Powered Outer Probabilistic Clustering refinement (Taraba 2017).
//
// Given a seeded list of clusters, iteratively reshuffles each instance into
// the destination cluster that maximally increases J, accepting the move only
// when J strictly improves. Empty clusters are removed during iteration so
// that N (used in the probability denominator) reflects the current partition.
// Terminates when an entire pass over all instances produces no moves.
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

#endif
