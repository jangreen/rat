#pragma once
#include <boost/container/flat_set.hpp>

#include "../../helper/assert_catch.h"
#include "../Set.h"
#include "Event.h"

// typedef + template are not supported, i.e.:
// template <typename ElementType>
// typedef const Container<ElementType> ContainerType;
template <typename ElementType>
using SetContainerType = boost::container::flat_set<ElementType>;

typedef SetContainerType<Event> SetValue;
typedef SetContainerType<Edge> RelationValue;
typedef SetContainerType<Edge> IdentitiesValue;
typedef std::variant<SetValue, RelationValue> ExprValue;
typedef std::variant<Event, Edge> EventOrEdge;

class Interpretation;
typedef std::unique_ptr<Interpretation> InterpretationPtr;

class Interpretation {
  ExprValue value;
  InterpretationPtr left;
  InterpretationPtr right;

  // minimal witness tracked
  std::unordered_map<EventOrEdge, bool> isLeftWitness;    // for union
  std::unordered_map<EventOrEdge, Event> projectedEvent;  // composition, domain, image

  Interpretation(ExprValue value, InterpretationPtr left, InterpretationPtr right,
                 std::unordered_map<EventOrEdge, bool> isLeftWitness,
                 std::unordered_map<EventOrEdge, Event> projectedEvent)
      : value(std::move(value)),
        left(std::move(left)),
        right(std::move(right)),
        isLeftWitness(std::move(isLeftWitness)),
        projectedEvent(std::move(projectedEvent)) {
    assert_void([&] {
      if (std::holds_alternative<SetValue>(value)) {
        const auto events = std::get<SetValue>(value);
        for (const auto &event : events) {
          assert(event.reason() != nullptr);
        }
      } else {
        const auto edges = std::get<RelationValue>(value);
        for (const auto &edge : edges) {
          assert(edge.reason() != nullptr);
        }
      }
    });
  }

 public:
  explicit Interpretation(ExprValue value)
      : Interpretation(std::move(value), nullptr, nullptr, {}, {}) {}

  Interpretation(RelationValue value, InterpretationPtr left)
      : Interpretation(std::move(value), std::move(left), nullptr, {}, {}) {}

  Interpretation(ExprValue value, InterpretationPtr left, InterpretationPtr right)
      : Interpretation(std::move(value), std::move(left), std::move(right), {}, {}) {}

  Interpretation(ExprValue value, InterpretationPtr left, InterpretationPtr right,
                 std::unordered_map<EventOrEdge, bool> isLeftWitness)
      : Interpretation(std::move(value), std::move(left), std::move(right),
                       std::move(isLeftWitness), {}) {}

  Interpretation(ExprValue value, InterpretationPtr left, InterpretationPtr right,
                 std::unordered_map<EventOrEdge, Event> projectedEvent)
      : Interpretation(std::move(value), std::move(left), std::move(right), {},
                       std::move(projectedEvent)) {}

  [[nodiscard]] const SetValue &getSetValue() const { return std::get<SetValue>(value); }
  [[nodiscard]] const RelationValue &getRelValue() const { return std::get<RelationValue>(value); }
  [[nodiscard]] const Event &getProjectedEvent(const EventOrEdge &e) const {
    return projectedEvent.at(e);
  }
  [[nodiscard]] bool traceLeft(const EventOrEdge &e) const { return isLeftWitness.at(e); }
  [[nodiscard]] const std::unique_ptr<Interpretation> &getLeft() const { return left; }
  [[nodiscard]] const std::unique_ptr<Interpretation> &getRight() const { return right; }
  [[nodiscard]] std::string toString() const;

  static InterpretationPtr relationIntersection(InterpretationPtr left, InterpretationPtr right);
  static InterpretationPtr relationUnion(InterpretationPtr left, InterpretationPtr right);
  static InterpretationPtr composition(InterpretationPtr left, InterpretationPtr right);
  static InterpretationPtr transitiveClosure(InterpretationPtr left, const EventSet &events);
  static InterpretationPtr setIntersection(InterpretationPtr left, InterpretationPtr right);
  static InterpretationPtr setUnion(InterpretationPtr left, InterpretationPtr right);
  static InterpretationPtr image(InterpretationPtr left, InterpretationPtr right);
  static InterpretationPtr domain(InterpretationPtr left, InterpretationPtr right);
};
