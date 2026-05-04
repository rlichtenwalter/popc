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

/**
 * @brief Binary feature dataset with named attribute columns.
 *
 * Stores `num_instances * num_attributes` boolean values in row-major order
 * inside a flat `std::vector<bool>`, plus the attribute names parsed from
 * the input header and a per-attribute positive-count cache. The positive
 * counts are precomputed at construction so popc::compute_delta() can use
 * them as the `c(f_i = 1)` term of the probability formula without a full
 * scan on every call.
 *
 * Used as the input to popc::popc() and as the source the bitpacked seeder
 * (popc::detail::bitpacked_kmodes_seed) repacks into 64-bit chunks for its
 * popcount distance kernel.
 */
class dataset {
  friend std::ostream &operator<<(std::ostream &os, dataset const &ds);

public:
  using value_type = bool;

private:
  using storage_type = std::vector<value_type>;

public:
  using size_type = std::size_t;
  using const_iterator = storage_type::const_iterator;

  /** @brief Construct an empty dataset with no instances or attributes. */
  dataset() = default;

  /**
   * @brief Parse a dataset from a delimited text stream.
   *
   * The first line is read as a delimited header of attribute names.
   * Subsequent lines contain one binary value (`0` or `1`) per column,
   * separated by `delimiter` and terminated by `'\n'`. The parser throws
   * `std::runtime_error` on any malformed line — invalid characters,
   * unexpected delimiters, inconsistent column counts.
   *
   * @param is        Input stream positioned at the start of the header.
   * @param delimiter Column separator. Defaults to TAB (`'\t'`).
   *
   * @throws std::runtime_error if the input is not a valid header followed
   *         by zero or more binary rows.
   */
  explicit dataset(std::istream &is, char delimiter = '\t');

  /**
   * @brief Construct directly from a pre-built storage vector.
   *
   * Intended for language bindings and unit tests that already have the
   * data in memory and do not need to round-trip through the text parser.
   * If `names` is empty, generic names of the form `"attr1"`, `"attr2"`,
   * ... are generated. The positive-count cache is computed eagerly.
   *
   * @param data           Row-major storage of `num_instances * num_attributes` bools.
   * @param num_instances  Row count of the dataset.
   * @param num_attributes Column count of the dataset.
   * @param names          Optional attribute names; must be empty or have
   *                       size equal to `num_attributes`.
   *
   * @throws std::logic_error if `data.size() != num_instances * num_attributes`,
   *         or if `names` is non-empty and `names.size() != num_attributes`.
   */
  dataset(storage_type data, size_type num_instances, size_type num_attributes,
          std::vector<std::string> names = {});

  /** @brief Return the number of instances (rows) in the dataset. */
  [[nodiscard]] size_type num_instances() const noexcept { return num_instances_; }

  /** @brief Return the number of attributes (columns) in the dataset. */
  [[nodiscard]] size_type num_attributes() const noexcept { return names_.size(); }

  /**
   * @brief Read the value at a given instance and attribute.
   *
   * @param instance_num  Zero-based row index in `[0, num_instances())`.
   * @param attribute_num Zero-based column index in `[0, num_attributes())`.
   * @return `true` if the bit is set, `false` otherwise.
   */
  [[nodiscard]] value_type operator()(size_type instance_num,
                                      size_type attribute_num) const noexcept {
    return data_[instance_num * num_attributes() + attribute_num];
  }

  /**
   * @brief Return the parsed name of the given attribute column.
   *
   * @param attribute_num Zero-based column index in `[0, num_attributes())`.
   * @return Const reference to the attribute name.
   */
  [[nodiscard]] std::string const &attribute_name(size_type attribute_num) const noexcept {
    return names_[attribute_num];
  }

  /**
   * @brief Return the precomputed positive count for the given attribute.
   *
   * @param attribute_num Zero-based column index in `[0, num_attributes())`.
   * @return Number of instances in the whole dataset for which this
   *         attribute is set.
   */
  [[nodiscard]] size_type positive_count(size_type attribute_num) const noexcept {
    return positive_counts_[attribute_num];
  }

  /**
   * @brief Return a const iterator to the first attribute of an instance.
   *
   * @param instance_num Zero-based row index in `[0, num_instances())`.
   * @return Iterator pointing at the first column of `instance_num`.
   */
  [[nodiscard]] const_iterator cbegin(size_type instance_num) const noexcept {
    return data_.cbegin() + static_cast<std::ptrdiff_t>(instance_num * num_attributes());
  }

  /**
   * @brief Return a const iterator past the last attribute of an instance.
   *
   * @param instance_num Zero-based row index in `[0, num_instances())`.
   * @return Iterator pointing one past the last column of `instance_num`.
   */
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
        ++instance_num;
      } else {
        throw std::runtime_error{"inconsistent column count at line " +
                                 std::to_string(instance_num + 2)};
      }
    } else {
      throw std::runtime_error{std::string{"invalid character '"} + c + "' at line " +
                               std::to_string(instance_num + 2)};
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

/**
 * @brief Stream-insert the dataset in TAB-delimited text form.
 *
 * Writes the header followed by the rows. Output is valid input to the
 * stream-parsing constructor and produces an identical dataset.
 *
 * @param os Stream to write to.
 * @param ds Dataset to serialize.
 * @return Reference to `os`.
 */
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
  // ds.data_ is std::vector<bool>; range-iter yields proxy refs, so
  // explicitly cast through int to print as 0/1 rather than relying on
  // the proxy's implicit conversion sequence.
  for (auto const val : ds.data_) {
    os << static_cast<int>(val);
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

#endif // POPC_DATASET_HPP
