#pragma once
#include <format>
#include <iostream>
#include <map>

struct Counter {
 private:
  friend class Stats;
  typedef std::map<std::string, Counter> CounterMap;
  static CounterMap counters;

  long value = 0;
  long maxValue = 0;
  long absoluteValue = 0;
  int numberCounters = 1;

 public:
  long operator++() {
    const auto r = ++value;
    absoluteValue++;
    if (r > maxValue) {
      maxValue = r;
    }
    return r;
  }
  void reset() {
    numberCounters++;
    value = 0;
  }
};

struct Condition {
 private:
  friend class Stats;
  typedef std::map<std::string, Condition> CondMap;
  static CondMap conditions;

  long trueCounter = 0;
  long falseCounter = 0;

 public:
  void count(const bool isTrue) {
    if (isTrue) {
      trueCounter++;
    } else {
      falseCounter++;
    }
  }
};

struct Value {
 private:
  friend class Stats;
  typedef std::map<std::string, Value> ValueMap;
  static ValueMap values;

  long maxValue = 0;
  long absoluteValue = 0;
  int valueCounter = 0;

 public:
  void set(const long value) {
    if (value > maxValue) {
      maxValue = value;
    }
    absoluteValue += value;
    valueCounter++;
  }
};

struct Difference {
 private:
  friend class Stats;
  typedef std::map<std::string, Difference> DiffsMap;
  static DiffsMap diffs;

  std::optional<long> _first = std::nullopt;
  std::optional<long> _second = std::nullopt;
  long maxDiff = 0;
  long absoluteDiff = 0;
  int diffCounter = 0;

 public:
  void first(const unsigned long value) {
    if (_first || _second) {
      throw std::exception();
    }
    _first = value;
  }
  void second(const unsigned long value) {
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
};

class Stats {
 public:
  [[nodiscard]] static Counter &counter(const std::string &name) { return Counter::counters[name]; }
  [[nodiscard]] static Difference &diff(const std::string &name) { return Difference::diffs[name]; }
  [[nodiscard]] static Value &value(const std::string &name) { return Value::values[name]; }
  [[nodiscard]] static Condition &boolean(const std::string &name) {
    return Condition::conditions[name];
  }

  static void print() {
    std::cout << "\n ---------------------- Stats ---------------------- \n";

    for (const auto &[name, counter] : Counter::counters) {
      if (counter.absoluteValue == counter.maxValue) {
        std::cout << std::format("{:40} | Total: {:5}", name, counter.absoluteValue) << std::endl;
      } else {
        std::cout << std::format("{:40} | Total: {:5}, Max: {:5}, Average: {:5}", name,
                                 counter.absoluteValue, counter.maxValue,
                                 (counter.absoluteValue / counter.numberCounters))
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
      std::cout << std::format("{:40} | Max: {:5}, Average: {:5}", name, value.absoluteValue,
                               value.maxValue, (value.absoluteValue / value.valueCounter))
                << std::endl;
    }

    std::cout << "\n";

    for (const auto &[name, diff] : Difference::diffs) {
      std::cout << std::format("{:40} | Total: {:5}, Max: {:5}, Average: {:5}", name,
                               diff.absoluteDiff, diff.maxDiff,
                               (diff.absoluteDiff / diff.diffCounter))
                << std::endl;
    }
  }
};
