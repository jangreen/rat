#include "Stats.h"

Counter::CounterMap Counter::counters;
Difference::DiffsMap Difference::diffs;
Condition::CondMap Condition::conditions;
Value::ValueMap Value::values;

long Counter::operator++(int) {
  const auto r = ++value;
  absoluteValue++;
  if (r > maxValue) {
    maxValue = r;
  }
  return r;
}

void Counter::reset() {
  numberCounters++;
  value = 0;
}

void Condition::count(const bool isTrue) {
  if (isTrue) {
    trueCounter++;
  } else {
    falseCounter++;
  }
}

void Value::set(const long value) {
  if (value > maxValue) {
    maxValue = value;
  }
  absoluteValue += value;
  valueCounter++;
}

void Difference::first(const unsigned long value) {
  if (_first || _second) {
    throw std::exception();
  }
  _first = value;
}
void Difference::second(const unsigned long value) {
  if (!_first || _second) {
    throw std::exception();
  }
  _second = value;
  const auto diff = _first.value() - _second.value();
  if (diff > maxDiff) {
    maxDiff = diff;
  }
  absoluteDiff += diff;
  diffCounter++;
  _first = std::nullopt;
  _second = std::nullopt;
}

void Stats::reset() {
  Counter::counters.clear();
  Difference::diffs.clear();
  Value::values.clear();
  Condition::conditions.clear();
}

void Stats::print() {
  std::cout << "\n ---------------------- Stats ---------------------- \n";

  for (const auto &[name, counter] : Counter::counters) {
    if (counter.absoluteValue == counter.maxValue) {
      std::cout << std::format("{:40} | Total: {:5}", name, counter.absoluteValue) << std::endl;
    } else {
      std::cout << std::format("{:40} | Total: {:5}, Max: {:5}, Average: {:5}, Calls: {:5}", name,
                               counter.absoluteValue, counter.maxValue,
                               (counter.absoluteValue / counter.numberCounters),
                               counter.numberCounters)
                << std::endl;
    }
  }
  std::cout << "\n";

  for (const auto &[name, condition] : Condition::conditions) {
    std::cout << std::format("{:40} | Yes: {:5}, No: {:5}", name, condition.trueCounter,
                             condition.falseCounter)
              << std::endl;
  }

  std::cout << "\n";

  for (const auto &[name, value] : Value::values) {
    std::cout << std::format("{:40} | Total: {:5}, Max: {:5}, Average: {:5}", name,
                             value.absoluteValue, value.maxValue,
                             (value.absoluteValue / value.valueCounter))
              << std::endl;
  }

  std::cout << "\n";

  for (const auto &[name, diff] : Difference::diffs) {
    std::cout << std::format("{:40} | Total: {:5}, Max: {:5}, Average: {:5}, Calls: {:5}", name,
                             diff.absoluteDiff, diff.maxDiff,
                             (diff.absoluteDiff / diff.diffCounter), diff.diffCounter)
              << std::endl;
  }
}