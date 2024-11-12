#pragma once
#include <cassert>

// runs code after an assertion failed (for debugging)
// does this inside the assertion to prevent that the code runs in release mode
inline void assert_catch(const bool condition, const std::function<void()>& catchFunc) {
  assert(condition || [&] {
    catchFunc();
    return false;
  }());
}

// runs code in debug that contains assertions
inline void assert_void(const std::function<void()>& assertionFunc) {
  assert([&] {
    assertionFunc();
    return true;
  }());
}