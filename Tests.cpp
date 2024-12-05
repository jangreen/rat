#include <gtest/gtest.h>

#include <filesystem>

#include "src/Rat.h"

void unitTest(const bool testResult) {
  std::vector<std::string> failedAssertions;

  const auto filePath = testResult ? "benchmarks/tests/true" : "benchmarks/tests/false";
  for (const auto &entry : std::filesystem::recursive_directory_iterator(filePath)) {
    if (entry.is_directory()) {
      continue;
    }
    // std::function wrappedRat = [&] { return rat(entry.path(), true); };
    // const auto answers = callWithTimeout(3, wrappedRat);
    const auto answers = rat(entry.path(), 3, true);

    for (int i = 1; const auto &answer : answers) {
      std::stringstream ss;
      ss << entry.path() << "[assertion " << i << "]";
      const auto assertion = ss.str();
      const auto passed = answer == testResult;
      if (!passed) {
        failedAssertions.push_back(assertion);
      }
      std::cout << assertion << ": ";
      if (answer) {
        std::cout << (passed ? "Passed\n" : "Failed\n");
      } else {
        std::cout << "Timeout\n";
      }
      i++;
    }
  }

  if (!failedAssertions.empty()) {
    std::cout << "\nFAILED TESTS:\n";
    for (const auto &failedAssertion : failedAssertions) {
      std::cout << failedAssertion << "\n";
    }
    std::cout << std::endl;
    ASSERT_TRUE(false);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(Tests, Unit) {
  unitTest(true);
  unitTest(false);
}

TEST(Tests, Kater) {
  ASSERT_TRUE(rat("benchmarks/kater/kater_3_1-eco", 3, true).at(0).value());
  ASSERT_TRUE(rat("benchmarks/kater/kater_3_2-ra", 3, true).at(0).value());
  ASSERT_TRUE(rat("benchmarks/kater/kater_3_3-ra", 3, true).at(0).value());
}

TEST(Tests, MemoryModels) {
  ASSERT_TRUE(rat("benchmarks/memorymodels/uniproc+rfi_po", 3, true).at(0).value());
  ASSERT_FALSE(rat("benchmarks/memorymodels/uniproc+rfi_po#f1", 3, true).at(0).value());
  ASSERT_FALSE(rat("benchmarks/memorymodels/uniproc+rfi_po#f2", 3, true).at(0).value());

  ASSERT_FALSE(rat("benchmarks/memorymodels/lkmm-counterexample", 3, true).at(0).value());
}