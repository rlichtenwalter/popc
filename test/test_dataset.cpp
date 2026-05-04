#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <popc/dataset.hpp>

TEST_CASE("dataset: default-constructed is empty", "[dataset]") {
  popc::dataset ds;
  CHECK(ds.num_instances() == 0);
  CHECK(ds.num_attributes() == 0);
}

TEST_CASE("dataset: stream constructor parses TSV correctly", "[dataset]") {
  std::istringstream in{"a\tb\tc\n1\t0\t1\n0\t1\t0\n"};
  popc::dataset ds{in};

  REQUIRE(ds.num_instances() == 2);
  REQUIRE(ds.num_attributes() == 3);
  CHECK(ds.attribute_name(0) == "a");
  CHECK(ds.attribute_name(1) == "b");
  CHECK(ds.attribute_name(2) == "c");

  CHECK(ds(0, 0) == true);
  CHECK(ds(0, 1) == false);
  CHECK(ds(0, 2) == true);
  CHECK(ds(1, 0) == false);
  CHECK(ds(1, 1) == true);
  CHECK(ds(1, 2) == false);

  CHECK(ds.positive_count(0) == 1);
  CHECK(ds.positive_count(1) == 1);
  CHECK(ds.positive_count(2) == 1);
}

TEST_CASE("dataset: stream constructor accepts custom delimiter", "[dataset]") {
  std::istringstream in{"x,y\n1,1\n0,0\n"};
  popc::dataset ds{in, ','};

  REQUIRE(ds.num_instances() == 2);
  REQUIRE(ds.num_attributes() == 2);
  CHECK(ds(0, 0) == true);
  CHECK(ds(1, 1) == false);
  CHECK(ds.positive_count(0) == 1);
  CHECK(ds.positive_count(1) == 1);
}

TEST_CASE("dataset: parser rejects non-binary values", "[dataset][error]") {
  std::istringstream in{"a\tb\n1\t2\n"};
  CHECK_THROWS_AS(popc::dataset{in}, std::runtime_error);
}

TEST_CASE("dataset: parser rejects inconsistent column counts", "[dataset][error]") {
  std::istringstream in{"a\tb\tc\n1\t0\n0\t1\t0\n"};
  CHECK_THROWS_AS(popc::dataset{in}, std::runtime_error);
}

TEST_CASE("dataset: parser rejects unexpected delimiter at start of row", "[dataset][error]") {
  std::istringstream in{"a\tb\n\t0\n"};
  CHECK_THROWS_AS(popc::dataset{in}, std::runtime_error);
}

TEST_CASE("dataset: parser error reports the actual offending line number", "[dataset][error]") {
  // Three good rows, then a malformed row. The reported line must be 5
  // (1 header + 3 good data rows + the bad row), not 2 (which is what the
  // parser used to report when instance_num was never incremented).
  std::istringstream in{"a\tb\n1\t0\n0\t1\n1\t1\nbad\t0\n"};
  try {
    popc::dataset ds{in};
    FAIL("expected parser to throw on malformed row");
  } catch (std::runtime_error const &e) {
    std::string const msg{e.what()};
    CHECK(msg.find("line 5") != std::string::npos);
  }
}

TEST_CASE("dataset: data-vector constructor", "[dataset]") {
  // 2 instances x 3 attributes, row-major
  std::vector<bool> data{true, false, true, false, true, false};
  popc::dataset ds{data, 2, 3};

  CHECK(ds.num_instances() == 2);
  CHECK(ds.num_attributes() == 3);
  CHECK(ds(0, 0) == true);
  CHECK(ds(0, 1) == false);
  CHECK(ds(0, 2) == true);
  CHECK(ds(1, 1) == true);

  // Default attribute names are attr1, attr2, ...
  CHECK(ds.attribute_name(0) == "attr1");
  CHECK(ds.attribute_name(2) == "attr3");

  CHECK(ds.positive_count(0) == 1);
  CHECK(ds.positive_count(1) == 1);
  CHECK(ds.positive_count(2) == 1);
}

TEST_CASE("dataset: data-vector constructor with custom names", "[dataset]") {
  std::vector<bool> data{true, false};
  std::vector<std::string> names{"alpha", "beta"};
  popc::dataset ds{data, 1, 2, names};

  CHECK(ds.attribute_name(0) == "alpha");
  CHECK(ds.attribute_name(1) == "beta");
}

TEST_CASE("dataset: data-vector constructor rejects size mismatches", "[dataset][error]") {
  std::vector<bool> data{true, false, true}; // 3 elements but 2x2 = 4
  CHECK_THROWS_AS(popc::dataset(data, 2, 2), std::logic_error);
}

TEST_CASE("dataset: data-vector constructor rejects names size mismatch", "[dataset][error]") {
  std::vector<bool> data{true, false, true, false};
  std::vector<std::string> bad_names{"only_one"};
  CHECK_THROWS_AS(popc::dataset(data, 2, 2, bad_names), std::logic_error);
}

TEST_CASE("dataset: row iterators return correct slices", "[dataset]") {
  std::vector<bool> data{true, false, true, false, true, true};
  popc::dataset ds{data, 2, 3};

  std::vector<bool> row0{ds.cbegin(0), ds.cend(0)};
  CHECK(row0 == std::vector<bool>{true, false, true});

  std::vector<bool> row1{ds.cbegin(1), ds.cend(1)};
  CHECK(row1 == std::vector<bool>{false, true, true});
}

TEST_CASE("dataset: positive_counts aggregate across instances", "[dataset]") {
  // 4 instances x 3 attributes
  // attr0: 1 positive (instance 0)
  // attr1: 2 positives (instances 1, 3)
  // attr2: 3 positives (instances 0, 2, 3)
  std::vector<bool> data{
      true, false, true, false, true, false, false, false, true, false, true, true,
  };
  popc::dataset ds{data, 4, 3};
  CHECK(ds.positive_count(0) == 1);
  CHECK(ds.positive_count(1) == 2);
  CHECK(ds.positive_count(2) == 3);
}
