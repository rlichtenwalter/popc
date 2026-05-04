#ifndef POPC_CLUSTER_HPP
#define POPC_CLUSTER_HPP

#include <cstddef>
#include <list>
#include <vector>

namespace popc {

/**
 * @brief A set of dataset instance indices with a per-attribute positive count.
 *
 * Stores the indices of the instances assigned to this cluster, together with
 * a count, for each attribute, of how many of those instances have the
 * attribute set. Maintaining the counts incrementally on every move is the
 * central optimization in Taraba's POPC paper: without it, every candidate
 * move would have to re-scan the cluster's instances to re-derive its
 * per-attribute counts.
 *
 * `cluster` is value-typed and intended to be held in a `std::list` by
 * popc::popc(). `std::list` is the chosen container so that erasures during
 * iteration (cluster removal when emptied, instance moves) do not invalidate
 * iterators to other clusters and instances.
 */
class cluster {
public:
  using size_type = std::size_t;

private:
  using storage_type = std::list<size_type>;

public:
  using iterator = storage_type::iterator;
  using const_iterator = storage_type::const_iterator;

  cluster() = delete;

  /**
   * @brief Construct an empty cluster sized for a fixed attribute count.
   *
   * The cluster begins with no member instances and a per-attribute count
   * vector of length `num_attributes`, all zeros.
   *
   * @param num_attributes Number of attributes in the source dataset; sets
   *                       the length of the internal attribute-count vector.
   */
  explicit cluster(size_type num_attributes) : attribute_counts_(num_attributes, 0) {}

  /** @brief Return `true` when the cluster has no member instances. */
  [[nodiscard]] bool empty() const noexcept { return members_.empty(); }

  /** @brief Return the number of instances currently assigned to the cluster. */
  [[nodiscard]] size_type num_instances() const noexcept { return members_.size(); }

  /**
   * @brief Append an instance index to the cluster's member list.
   *
   * Does not update the per-attribute counts; the caller is responsible
   * for invoking increment_attribute_count() for each attribute the
   * inserted instance has set. popc::popc() does this in lockstep
   * during the move loop.
   *
   * @param instance_num Zero-based index of the instance to add.
   */
  void add_instance(size_type instance_num) { members_.push_back(instance_num); }

  /**
   * @brief Remove an instance from the cluster's member list.
   *
   * Does not update the per-attribute counts; the caller must mirror this
   * removal with the appropriate decrement_attribute_count() calls.
   *
   * @param it Iterator to the member to remove.
   * @return Iterator to the element following the removed one.
   */
  iterator remove_instance(iterator it) { return members_.erase(it); }

  /**
   * @brief Increment the cluster's positive-count for one attribute.
   *
   * @param attribute_num Zero-based index of the attribute whose count to
   *                      increment. Must be in `[0, num_attributes)`.
   */
  void increment_attribute_count(size_type attribute_num) noexcept {
    ++attribute_counts_[attribute_num];
  }

  /**
   * @brief Decrement the cluster's positive-count for one attribute.
   *
   * @param attribute_num Zero-based index of the attribute whose count to
   *                      decrement. Must be in `[0, num_attributes)` and
   *                      the current count must be non-zero.
   */
  void decrement_attribute_count(size_type attribute_num) noexcept {
    --attribute_counts_[attribute_num];
  }

  /**
   * @brief Return the cluster's positive-count for one attribute.
   *
   * @param attribute_num Zero-based attribute index in `[0, num_attributes)`.
   * @return Number of cluster members that have the attribute set.
   */
  [[nodiscard]] size_type attribute_count(size_type attribute_num) const noexcept {
    return attribute_counts_[attribute_num];
  }

  /** @brief Return an iterator to the first member instance index. */
  [[nodiscard]] iterator begin() noexcept { return members_.begin(); }

  /** @brief Return an iterator past the last member instance index. */
  [[nodiscard]] iterator end() noexcept { return members_.end(); }

  /** @brief Return a const iterator to the first member instance index. */
  [[nodiscard]] const_iterator begin() const noexcept { return members_.begin(); }

  /** @brief Return a const iterator past the last member instance index. */
  [[nodiscard]] const_iterator end() const noexcept { return members_.end(); }

  /** @brief Return a const iterator to the first member instance index. */
  [[nodiscard]] const_iterator cbegin() const noexcept { return members_.cbegin(); }

  /** @brief Return a const iterator past the last member instance index. */
  [[nodiscard]] const_iterator cend() const noexcept { return members_.cend(); }

private:
  storage_type members_;
  std::vector<size_type> attribute_counts_;
};

} // namespace popc

#endif // POPC_CLUSTER_HPP
