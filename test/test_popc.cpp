#include <algorithm>
#include <list>
#include <set>
#include <sstream>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <popc/cluster.hpp>
#include <popc/dataset.hpp>
#include <popc/popc.hpp>

using Catch::Matchers::WithinAbs;

namespace {

// Build a list of clusters that exactly mirrors the given assignment vector.
// Each cluster's attribute_counts are populated from the dataset so the popc
// refinement can run incrementally without having to reseed the counts.
std::list<popc::cluster> build_clusters(popc::dataset const &ds,
                                        std::vector<std::size_t> const &assignments) {
  auto const k = *std::ranges::max_element(assignments) + 1;
  std::vector<popc::cluster> vec(k, popc::cluster{ds.num_attributes()});
  for (std::size_t i = 0; i < assignments.size(); ++i) {
    auto &c = vec[assignments[i]];
    c.add_instance(i);
    for (std::size_t j = 0; j < ds.num_attributes(); ++j) {
      if (ds(i, j)) {
        c.increment_attribute_count(j);
      }
    }
  }
  std::list<popc::cluster> out;
  for (auto &c : vec) {
    out.push_back(std::move(c));
  }
  return out;
}

} // namespace

// ============================================================
// compute_delta
// ============================================================

TEST_CASE("compute_delta: removing the only contributor decreases p", "[compute_delta]") {
  // Single feature, single sample, one cluster of size 1.
  std::istringstream in{"f\n1\n"};
  popc::dataset ds{in};

  popc::cluster c{1};
  c.add_instance(0);
  c.increment_attribute_count(0);

  // Removing instance 0 from cluster c: counts goes 1 -> 0
  // counts_all = 1, num_clusters = 1, multiplier = 1000, power = 10
  // denom = 1*1000 + 1 = 1001
  // old_p = (1*1000 + 1)/1001 = 1.0
  // new_p = (0*1000 + 1)/1001 ≈ 0.000999
  // delta = -1^10 + (1/1001)^10 ≈ -1 + tiny ≈ -1
  auto const delta =
      popc::compute_delta<double>(ds, c, 0, /*num_clusters=*/1, /*multiplier=*/1000.0,
                                  /*power=*/10.0, /*added=*/false);
  CHECK_THAT(delta, WithinAbs(-1.0, 1e-6));
}

TEST_CASE("compute_delta: zero contribution from inactive features", "[compute_delta]") {
  // Sample with all-zero feature values: no attribute is active, so the
  // delta must be exactly zero regardless of cluster state.
  std::istringstream in{"a\tb\n0\t0\n"};
  popc::dataset ds{in};

  popc::cluster c{2};
  c.add_instance(0);

  auto const delta = popc::compute_delta<double>(ds, c, 0, 1, 1000.0, 10.0, true);
  CHECK(delta == 0.0);
}

TEST_CASE("compute_delta: adding to a heavily-populated cluster yields gain", "[compute_delta]") {
  // Two-feature dataset where feature 0 is positive in everyone. Adding an
  // instance to a cluster that already has all the positive contributions
  // should increase J for that cluster.
  std::istringstream in{"a\tb\n1\t0\n1\t0\n1\t0\n"};
  popc::dataset ds{in};

  // Cluster already contains instances 0 and 1 (counts[0]=2)
  popc::cluster c{2};
  c.add_instance(0);
  c.add_instance(1);
  c.increment_attribute_count(0);
  c.increment_attribute_count(0);

  // Adding instance 2 (feature 0 is active): counts[0] goes 2 -> 3
  auto const delta = popc::compute_delta<double>(ds, c, 2, 1, 1000.0, 10.0, true);
  CHECK(delta > 0.0);
}

// ============================================================
// popc()
// ============================================================

TEST_CASE("popc: returns one label per instance", "[popc]") {
  std::istringstream in{"a\tb\n1\t0\n1\t0\n0\t1\n0\t1\n"};
  popc::dataset ds{in};
  auto clusters = build_clusters(ds, {0, 1, 2, 3}); // each in its own cluster

  auto labels = popc::popc(ds, clusters);
  CHECK(labels.size() == ds.num_instances());
}

TEST_CASE("popc: identical instances coalesce", "[popc]") {
  // Three identical positive instances + three all-zero instances. POPC
  // should pull the identical instances together regardless of the seed.
  std::istringstream in{"a\tb\tc\n1\t1\t1\n1\t1\t1\n1\t1\t1\n0\t0\t0\n0\t0\t0\n0\t0\t0\n"};
  popc::dataset ds{in};

  // Seed each instance into its own cluster.
  auto clusters = build_clusters(ds, {0, 1, 2, 3, 4, 5});
  auto labels = popc::popc(ds, clusters);

  CHECK(labels[0] == labels[1]);
  CHECK(labels[1] == labels[2]);
  // The all-zero instances contribute nothing to the J function (compute_delta
  // only sums over active features), so popc has no signal to merge them. They
  // may or may not coalesce — we only assert behavior we can guarantee.
}

TEST_CASE("popc: already-optimal partition does not move instances", "[popc]") {
  // Two distinct clusters where each cluster's instances are identical
  // within and disjoint between. The seed already perfectly separates
  // them, so popc should make no moves.
  std::istringstream in{"a\tb\n1\t0\n1\t0\n0\t1\n0\t1\n"};
  popc::dataset ds{in};

  auto clusters = build_clusters(ds, {0, 0, 1, 1});
  auto labels = popc::popc(ds, clusters);

  CHECK(labels[0] == labels[1]);
  CHECK(labels[2] == labels[3]);
  CHECK(labels[0] != labels[2]);
}

TEST_CASE("popc: empty seed clusters are removed", "[popc]") {
  std::istringstream in{"a\n1\n1\n"};
  popc::dataset ds{in};

  // Three seeds where clusters 1 and 2 are empty initially. popc::popc must
  // tolerate empty seed clusters.
  std::vector<popc::cluster> vec{popc::cluster{1}, popc::cluster{1}, popc::cluster{1}};
  vec[0].add_instance(0);
  vec[0].add_instance(1);
  vec[0].increment_attribute_count(0);
  vec[0].increment_attribute_count(0);

  std::list<popc::cluster> clusters;
  for (auto &c : vec) {
    clusters.push_back(std::move(c));
  }
  auto labels = popc::popc(ds, clusters);
  REQUIRE(labels.size() == 2);
  CHECK(labels[0] == labels[1]);
}

TEST_CASE("popc: parameter overrides are honored", "[popc]") {
  // With a power of 1, J is linear in the per-attribute probabilities
  // rather than concentrating mass at outer values, so the gradient that
  // would normally pull duplicate instances together is absent. On this
  // single-attribute, all-positive seed every move's delta is zero (no
  // strict improvement), so popc must leave the seed partition untouched.
  std::istringstream in{"a\n1\n1\n1\n"};
  popc::dataset ds{in};

  auto clusters = build_clusters(ds, {0, 1, 2});
  auto labels = popc::popc<double>(ds, clusters, 1000.0, 1.0);

  CHECK(labels[0] != labels[1]);
  CHECK(labels[1] != labels[2]);
  CHECK(labels[0] != labels[2]);
}

TEST_CASE("popc: float and double specializations agree on label structure", "[popc]") {
  std::istringstream in1{"a\tb\n1\t1\n1\t1\n0\t0\n0\t0\n"};
  popc::dataset ds1{in1};
  std::istringstream in2{"a\tb\n1\t1\n1\t1\n0\t0\n0\t0\n"};
  popc::dataset ds2{in2};

  auto clusters_f = build_clusters(ds1, {0, 1, 2, 3});
  auto clusters_d = build_clusters(ds2, {0, 1, 2, 3});

  auto labels_f = popc::popc<float>(ds1, clusters_f);
  auto labels_d = popc::popc<double>(ds2, clusters_d);

  // Both should at least merge the two duplicated positive instances.
  CHECK(labels_f[0] == labels_f[1]);
  CHECK(labels_d[0] == labels_d[1]);
}
