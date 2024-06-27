#include <unordered_set>

#include "OccurrenceCounter.h"

OccurrenceCounter::OccurrenceCounter(CounterType counter, CanonicalOccurrenceCounter left,
                                     CanonicalOccurrenceCounter right)
    : counter(counter), left(left), right(right) {}

bool OccurrenceCounter::operator==(const OccurrenceCounter& other) const {
  return counter == other.counter && left == other.left && right == other.right;
}

bool OccurrenceCounter::validate() const { return false; }

CanonicalOccurrenceCounter OccurrenceCounter::newOccurrenceCounter(
    CounterType counter, CanonicalOccurrenceCounter left, CanonicalOccurrenceCounter right) {
  static std::unordered_set<OccurrenceCounter> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(counter, left, right);
  return &*iter;
}

CanonicalOccurrenceCounter OccurrenceCounter::newCounter(CounterType counter) {
  return newOccurrenceCounter(counter, nullptr, nullptr);
}

CanonicalOccurrenceCounter OccurrenceCounter::newOccurence(CanonicalOccurrenceCounter left) {
  return newOccurrenceCounter(left->counter, left, nullptr);
}

CanonicalOccurrenceCounter OccurrenceCounter::newOccurence(CanonicalOccurrenceCounter left,
                                                           CanonicalOccurrenceCounter right) {
  return newOccurrenceCounter(std::min(left->counter, right->counter), left, right);
}
