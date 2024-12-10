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

    RatSolver solver;
    const auto answers = solver.rat(entry.path(), 10, true);

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
  ASSERT_TRUE(RatSolver().rat("benchmarks/kater/kater_3_1-eco", 3, true).at(0).value());
  ASSERT_TRUE(RatSolver().rat("benchmarks/kater/kater_3_2-ra", 3, true).at(0).value());
  ASSERT_TRUE(RatSolver().rat("benchmarks/kater/kater_3_3-ra", 3, true).at(0).value());
}

TEST(Tests, MemoryModels) {
  ASSERT_TRUE(RatSolver().rat("benchmarks/memorymodels/uniproc+rfi_po", 3, true).at(0).value());
  ASSERT_FALSE(RatSolver().rat("benchmarks/memorymodels/uniproc+rfi_po#f1", 3, true).at(0).value());
  ASSERT_FALSE(RatSolver().rat("benchmarks/memorymodels/uniproc+rfi_po#f2", 3, true).at(0).value());

  ASSERT_FALSE(
      RatSolver().rat("benchmarks/memorymodels/lkmm-counterexample", 3, true).at(0).value());
}

TEST(Demo, lkmm1) {
  ASSERT_FALSE(RatSolver().rat("benchmarks/demo/lkmm/lkmm-oota", 30, true).at(0).value());
}

TEST(Demo, lkmm2) {
  const auto lkmm_ppo = RatSolver().rat("benchmarks/demo/lkmm/lkmm-compare_ppo", 30, true);
  ASSERT_FALSE(lkmm_ppo.at(0).value());
  ASSERT_TRUE(lkmm_ppo.at(1).value());
  ASSERT_FALSE(lkmm_ppo.at(2).value());
  ASSERT_TRUE(lkmm_ppo.at(3).value());
}

TEST(Demo, lkmm3) {
  ASSERT_FALSE(
      RatSolver().rat("benchmarks/demo/lkmm/lkmm-compare_rmw-seq", 30, true).at(0).value());
}

TEST(Demo, arm) {
  ASSERT_TRUE(RatSolver().rat("benchmarks/demo/arm8/arm_oota", 30, true).at(0).value());
}

TEST(Demo, tso) {
  ASSERT_TRUE(RatSolver().rat("benchmarks/demo/tso/tso_oota", 30, true).at(0).value());
}

TEST(Demo, vmm) {
  ASSERT_FALSE(RatSolver().rat("benchmarks/demo/vmm/vmm-oota", 30, true).at(0).value());
  const auto vmm_monotonic = RatSolver().rat("benchmarks/demo/vmm/vmm-monotonic", 30, true);
  ASSERT_TRUE(vmm_monotonic.at(0).value());
  ASSERT_FALSE(vmm_monotonic.at(1).value());
  const auto vmm_monotonic2 = RatSolver().rat("benchmarks/demo/vmm/vmm-monotonic-2", 30, true);
  ASSERT_TRUE(vmm_monotonic2.at(0).value());
  ASSERT_FALSE(vmm_monotonic2.at(1).value());
}