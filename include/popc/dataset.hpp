#ifndef POPC_DATASET_HPP
#define POPC_DATASET_HPP

#include <cstddef>
#include <iosfwd>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace popc {

// Binary feature dataset: row-major flat std::vector<bool> storage with named
// attribute columns. Used as the input to popc::popc() and as the substrate
// the bitpacked seeder rebuilds into uint64 chunks for popcount kernels.
class dataset {
  friend std::ostream &operator<<(std::ostream &os, dataset const &ds);

public:
  using value_type = bool;

private:
  using storage_type = std::vector<value_type>;

public:
  using size_type = std::size_t;
  using const_iterator = storage_type::const_iterator;

  dataset() = default;
  explicit dataset(std::istream &is, char delimiter = '\t');
  dataset(storage_type data, size_type num_instances, size_type num_attributes,
          std::vector<std::string> names = {});

  [[nodiscard]] size_type num_instances() const noexcept { return num_instances_; }
  [[nodiscard]] size_type num_attributes() const noexcept { return names_.size(); }
  [[nodiscard]] value_type operator()(size_type instance_num,
                                      size_type attribute_num) const noexcept {
    return data_[instance_num * num_attributes() + attribute_num];
  }
  [[nodiscard]] std::string const &attribute_name(size_type attribute_num) const noexcept {
    return names_[attribute_num];
  }
  [[nodiscard]] size_type positive_count(size_type attribute_num) const noexcept {
    return positive_counts_[attribute_num];
  }
  [[nodiscard]] const_iterator cbegin(size_type instance_num) const noexcept {
    return data_.cbegin() + static_cast<std::ptrdiff_t>(instance_num * num_attributes());
  }
  [[nodiscard]] const_iterator cend(size_type instance_num) const noexcept {
    return data_.cbegin() + static_cast<std::ptrdiff_t>((instance_num + 1) * num_attributes());
  }

private:
  std::vector<std::string> names_;
  storage_type data_;
  size_type num_instances_{};
  std::vector<size_type> positive_counts_;
};

inline dataset::dataset(std::istream &is, char delimiter) {
  // Header line: delimiter-separated names, terminated by newline.
  std::string name;
  while (!is.eof()) {
    char const c = static_cast<char>(is.get());
    if (c == delimiter) {
      names_.emplace_back(std::move(name));
      name.clear();
    } else if (c == '\n') {
      names_.emplace_back(std::move(name));
      break;
    } else {
      name.push_back(c);
    }
    is.peek();
  }

  // Body: one binary value per column, delimiter-separated, newline-terminated.
  data_.reserve(256 * num_attributes());
  positive_counts_.resize(num_attributes());
  size_type instance_num = 0;
  size_type attribute_num = 0;
  while (!is.eof()) {
    if (attribute_num == 0) {
      ++num_instances_;
    }
    char c = static_cast<char>(is.get());
    if (c == '0') {
      data_.push_back(false);
    } else if (c == '1') {
      data_.push_back(true);
      ++positive_counts_[attribute_num];
    } else if (c == delimiter) {
      throw std::runtime_error{"unexpected delimiter at line " + std::to_string(instance_num + 2)};
    } else if (c == '\n') {
      throw std::runtime_error{"newline after delimiter at line " +
                               std::to_string(instance_num + 2)};
    } else {
      throw std::runtime_error{"invalid character for attribute value at line " +
                               std::to_string(instance_num + 2) + " column " +
                               std::to_string(attribute_num + 1) + " (must be 0 or 1)"};
    }
    c = static_cast<char>(is.get());
    if (c == delimiter) {
      ++attribute_num;
    } else if (c == '\n') {
      if (attribute_num + 1 == num_attributes()) {
        attribute_num = 0;
      } else {
        throw std::runtime_error{"inconsistent column count at line " +
                                 std::to_string(instance_num + 2)};
      }
    } else {
      throw std::runtime_error{std::string{"invalid character '"} + c + "' at line " +
                               std::to_string(instance_num)};
    }
    is.peek();
  }
  data_.shrink_to_fit();
}

inline dataset::dataset(storage_type data, size_type num_instances, size_type num_attributes,
                        std::vector<std::string> names)
    : names_{std::move(names)}, data_{std::move(data)}, num_instances_{num_instances},
      positive_counts_(num_attributes, 0) {
  if (num_instances * num_attributes != data_.size()) {
    throw std::logic_error{"data size must equal num_instances * num_attributes"};
  }
  if (names_.empty()) {
    for (size_type i = 0; i < num_attributes; ++i) {
      names_.emplace_back("attr" + std::to_string(i + 1));
    }
  } else if (num_attributes != names_.size()) {
    throw std::logic_error{"names size must equal num_attributes or be zero"};
  }
  for (size_type i = 0; i < num_instances; ++i) {
    for (size_type j = 0; j < num_attributes; ++j) {
      if ((*this)(i, j)) {
        ++positive_counts_[j];
      }
    }
  }
}

inline std::ostream &operator<<(std::ostream &os, dataset const &ds) {
  dataset::size_type attribute_num = 0;
  for (auto const &name : ds.names_) {
    os << name;
    ++attribute_num;
    if (attribute_num == ds.num_attributes()) {
      os << '\n';
      attribute_num = 0;
    } else {
      os << '\t';
    }
  }
  for (auto const val : ds.data_) {
    os << val;
    ++attribute_num;
    if (attribute_num == ds.num_attributes()) {
      os << '\n';
      attribute_num = 0;
    } else {
      os << '\t';
    }
  }
  return os;
}

} // namespace popc

#endif
