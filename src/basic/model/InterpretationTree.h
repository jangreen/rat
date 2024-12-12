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
                 std::unordered_map<EventOrEdge, Event> projectedEvent);

 public:
  static InterpretationPtr newLeaf(ExprValue value);
  static InterpretationPtr unary(RelationValue value, InterpretationPtr left);
  static InterpretationPtr binary(ExprValue value, InterpretationPtr left, InterpretationPtr right);
  static InterpretationPtr binaryUnion(ExprValue value, InterpretationPtr left,
                                       InterpretationPtr right,
                                       std::unordered_map<EventOrEdge, bool> isLeftWitness);
  static InterpretationPtr binaryProjection(ExprValue value, InterpretationPtr left,
                                            InterpretationPtr right,
                                            std::unordered_map<EventOrEdge, Event> projectedEvent);

  [[nodiscard]] const SetValue &getSetValue() const;
  [[nodiscard]] const RelationValue &getRelValue() const;
  [[nodiscard]] const Event &getProjectedEvent(const EventOrEdge &e) const;
  [[nodiscard]] bool traceLeft(const EventOrEdge &e) const;
  [[nodiscard]] const std::unique_ptr<Interpretation> &getLeft() const;
  [[nodiscard]] const std::unique_ptr<Interpretation> &getRight() const;
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
