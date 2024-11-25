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
  long operator++(int);
  void reset();
};

struct Condition {
 private:
  friend class Stats;
  typedef std::map<std::string, Condition> CondMap;
  static CondMap conditions;

  long trueCounter = 0;
  long falseCounter = 0;

 public:
  void count(bool isTrue);
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
  void set(long value);
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
  void first(unsigned long value);
  void second(unsigned long value);
};

class Stats {
 public:
  [[nodiscard]] static constexpr Counter &counter(const std::string &name);
  [[nodiscard]] static constexpr Difference &diff(const std::string &name);
  [[nodiscard]] static constexpr Value &value(const std::string &name);
  [[nodiscard]] static constexpr Condition &boolean(const std::string &name);

  static void reset();
  static void print();
};

constexpr Counter &Stats::counter(const std::string &name) { return Counter::counters[name]; }
constexpr Difference &Stats::diff(const std::string &name) { return Difference::diffs[name]; }
constexpr Value &Stats::value(const std::string &name) { return Value::values[name]; }
constexpr Condition &Stats::boolean(const std::string &name) { return Condition::conditions[name]; }
