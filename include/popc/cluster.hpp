#ifndef POPC_CLUSTER_HPP
#define POPC_CLUSTER_HPP

#include <cstddef>
#include <list>
#include <vector>

namespace popc {

// A cluster is a set of dataset instance indices, plus a per-attribute count
// of how many of those instances have the attribute set. The paper notes that
// maintaining these counts incrementally is the central optimization: without
// it, each move would have to recount all attributes from scratch.
class cluster {
public:
  using size_type = std::size_t;

private:
  using storage_type = std::list<size_type>;

public:
  using iterator = storage_type::iterator;
  using const_iterator = storage_type::const_iterator;

  cluster() = delete;
  explicit cluster(size_type num_attributes) : attribute_counts_(num_attributes, 0) {}

  [[nodiscard]] bool empty() const noexcept { return members_.empty(); }
  [[nodiscard]] size_type num_instances() const noexcept { return members_.size(); }

  void add_instance(size_type instance_num) { members_.push_back(instance_num); }
  iterator remove_instance(iterator it) { return members_.erase(it); }

  void increment_attribute_count(size_type attribute_num) noexcept {
    ++attribute_counts_[attribute_num];
  }
  void decrement_attribute_count(size_type attribute_num) noexcept {
    --attribute_counts_[attribute_num];
  }
  [[nodiscard]] size_type attribute_count(size_type attribute_num) const noexcept {
    return attribute_counts_[attribute_num];
  }

  [[nodiscard]] iterator begin() noexcept { return members_.begin(); }
  [[nodiscard]] iterator end() noexcept { return members_.end(); }
  [[nodiscard]] const_iterator cbegin() const noexcept { return members_.cbegin(); }
  [[nodiscard]] const_iterator cend() const noexcept { return members_.cend(); }

private:
  storage_type members_;
  std::vector<size_type> attribute_counts_;
};

} // namespace popc

#endif
