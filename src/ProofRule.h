#pragma once
#include <string_view>

class ProofRule {
 public:
  enum Rule {
    none,
    bottom,
    empty,
    id,
    negId,
    propagation,
    at,
    negAt,
    intersection,
    negIntersection,
    composition,
    negComposition,
    transitiveClosure,
    negTransitiveClosure,
    choice,
    negChoice,
    a,
    negA,
    converseA,
    negConverseA
  };

  ProofRule() = delete;
  constexpr ProofRule(Rule rule) : rule(rule) {}
  constexpr operator Rule() const { return rule; }
  explicit operator bool() const = delete;
  constexpr bool operator==(ProofRule other) const { return rule == other.rule; }
  constexpr bool operator!=(ProofRule other) const { return rule != other.rule; }

  constexpr std::string_view toString() {
    switch (rule) {
      case Rule::none:
        return std::string_view("(none)");
      case Rule::bottom:
        return std::string_view("(bot)");
      case Rule::empty:
        return std::string_view("(empty)");
      case Rule::id:
        return std::string_view("(id)");
      case Rule::negId:
        return std::string_view("(-id)");
      case Rule::propagation:
        return std::string_view("(=)");
      case Rule::at:
        return std::string_view("(@)");
      case Rule::negAt:
        return std::string_view("(-@)");
      case Rule::intersection:
        return std::string_view("(cap)");
      case Rule::negIntersection:
        return std::string_view("(-cap)");
      case Rule::composition:
        return std::string_view("(;)");
      case Rule::negComposition:
        return std::string_view("(-;)");
      case Rule::transitiveClosure:
        return std::string_view("(*)");
      case Rule::negTransitiveClosure:
        return std::string_view("(-*)");
      case Rule::choice:
        return std::string_view("(cup)");
      case Rule::negChoice:
        return std::string_view("(-cup)");
      case Rule::a:
        return std::string_view("(a)");
      case Rule::negA:
        return std::string_view("(-a)");
      case Rule::converseA:
        return std::string_view("(a^-1)");
      case Rule::negConverseA:
        return std::string_view("(-a^-1)");
    }
  }

 private:
  Rule rule;
};
