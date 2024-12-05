#include <gtest/gtest.h>

#include <filesystem>
#include <future>

#include "src/Rat.h"

// std::mutex mutex;
// std::condition_variable cv;
// std::atomic ready = false;  // avoid spurious wakeup
//
// template <typename ReturnType>
// std::optional<ReturnType> callWithTimeout(const int duration,
//                                           const std::function<ReturnType()> &f) {
//   ReturnType returnValue;
//   ready = false;
//
//   std::thread thread([&]() {
//     returnValue = f();
//
//     std::lock_guard lock(mutex);
//     ready = true;
//     cv.notify_one();
//   });
//   thread.detach();
//
//   std::unique_lock lock(mutex);
//   while (!ready) {
//     if (cv.wait_for(lock, std::chrono::seconds(duration)) == std::cv_status::timeout) {
//       return std::nullopt;
//     }
//   }
//
//   return returnValue;
// }

void test(const bool testResult) {
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

// void test(const bool testResult) {
//   auto allPassed = true;
//
//   std::vector<std::future<std::vector<bool>>> futures;
//
//   const auto filePath = testResult ? "benchmarks/tests/true" : "benchmarks/tests/false";
//   for (const auto &entry : std::filesystem::recursive_directory_iterator(filePath)) {
//     if (entry.is_directory()) {
//       continue;
//     }
//
//     futures.emplace_back(std::async(std::launch::async, rat, entry.path(), true));
//     auto &future = futures.back();
//     std::cout << entry.path() << ": " << std::endl;
//
//     const auto futureStatus = future.wait_for(std::chrono::seconds(3));
//     std::cout << "asdsadadsad" << std::endl;
//
//     if (futureStatus == std::future_status::ready) {
//       const auto answers = future.get();
//       if (answers.empty()) {
//         std::cout << "empy" << std::endl;
//       }
//       const auto passed = std::ranges::all_of(answers, [&](bool b) { return b == testResult; });
//       allPassed = allPassed && passed;
//       std::cout << (passed ? "Passed\n" : "Failed\n");
//     } else {
//       std::cout << "Timeout\n";
//     }
//   }
//
//   ASSERT_TRUE(allPassed);
// }

TEST(Test, all) {
  test(true);
  test(false);
}

TEST(Test, True) { test(true); }

TEST(Test, False) { test(false); }