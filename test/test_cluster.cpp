#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include <popc/cluster.hpp>

TEST_CASE("cluster: default-constructed empty state", "[cluster]") {
  popc::cluster c{3};
  CHECK(c.empty());
  CHECK(c.num_instances() == 0);
  CHECK(c.attribute_count(0) == 0);
  CHECK(c.attribute_count(1) == 0);
  CHECK(c.attribute_count(2) == 0);
  CHECK(c.begin() == c.end());
  CHECK(c.cbegin() == c.cend());
}

TEST_CASE("cluster: add_instance grows membership", "[cluster]") {
  popc::cluster c{2};
  c.add_instance(7);
  CHECK_FALSE(c.empty());
  CHECK(c.num_instances() == 1);
  CHECK(*c.begin() == 7);

  c.add_instance(42);
  CHECK(c.num_instances() == 2);
  REQUIRE(std::distance(c.cbegin(), c.cend()) == 2);
}

TEST_CASE("cluster: remove_instance returns next iterator", "[cluster]") {
  popc::cluster c{1};
  c.add_instance(1);
  c.add_instance(2);
  c.add_instance(3);
  REQUIRE(c.num_instances() == 3);

  auto it = c.begin();
  ++it; // points at 2
  it = c.remove_instance(it);
  CHECK(c.num_instances() == 2);
  REQUIRE(it != c.end());
  CHECK(*it == 3);

  it = c.remove_instance(c.begin());
  CHECK(c.num_instances() == 1);
  CHECK(*it == 3);

  c.remove_instance(c.begin());
  CHECK(c.empty());
}

TEST_CASE("cluster: attribute counts increment and decrement", "[cluster]") {
  popc::cluster c{4};
  c.increment_attribute_count(0);
  c.increment_attribute_count(0);
  c.increment_attribute_count(0);
  c.increment_attribute_count(2);

  CHECK(c.attribute_count(0) == 3);
  CHECK(c.attribute_count(1) == 0);
  CHECK(c.attribute_count(2) == 1);
  CHECK(c.attribute_count(3) == 0);

  c.decrement_attribute_count(0);
  CHECK(c.attribute_count(0) == 2);

  c.decrement_attribute_count(0);
  c.decrement_attribute_count(0);
  CHECK(c.attribute_count(0) == 0);
}

TEST_CASE("cluster: zero-attribute cluster is well-defined", "[cluster]") {
  // A cluster with zero attributes should still allow membership tracking.
  // (Not particularly useful, but should not crash or misbehave.)
  popc::cluster c{0};
  CHECK(c.empty());
  c.add_instance(0);
  CHECK(c.num_instances() == 1);
  CHECK_FALSE(c.empty());
}

TEST_CASE("cluster: const iteration yields the inserted indices", "[cluster]") {
  popc::cluster c{1};
  c.add_instance(10);
  c.add_instance(20);
  c.add_instance(30);

  std::vector<std::size_t> seen;
  for (auto it = c.cbegin(); it != c.cend(); ++it) {
    seen.push_back(*it);
  }
  CHECK(seen == std::vector<std::size_t>{10, 20, 30});
}
