#pragma once

#include "../utility.h"
#include "Literal.h"

typedef int Event;
typedef std::pair<Event, Event> EventPair;
typedef boost::container::flat_set<EventPair> RelationValue;
typedef boost::container::flat_set<Event> SetValue;
typedef std::unordered_map<EventPair, SaturationAnnotation> EdgeSaturationMap;
typedef std::unordered_map<Event, SaturationAnnotation> EventSaturationMap;
typedef std::pair<RelationValue, EdgeSaturationMap> SatRelationValue;
typedef std::pair<SetValue, EventSaturationMap> SatSetValue;
typedef std::variant<SatRelationValue, SatSetValue> SatExprValue;
struct SetMembership {
  std::string baseSet;
  Event event;

  SetMembership(std::string baseSet, const Event event)
      : baseSet(std::move(baseSet)), event(event) {}
};
struct UndirectedEdge {
  Event e1;
  Event e2;

  UndirectedEdge(const Event _e1, const Event _e2) {
    e1 = _e1 <= _e2 ? _e1 : _e2;
    e2 = _e1 <= _e2 ? _e2 : _e1;
  }

  bool operator==(const UndirectedEdge &other) const { return e1 == other.e1 && e2 == other.e2; }
  [[nodiscard]] bool contains(const Event event) const { return e1 == event || e2 == event; };
  [[nodiscard]] std::optional<Event> neighbour(const Event event) const {
    if (!contains(event)) {
      return std::nullopt;
    }
    return event == e1 ? e2 : e1;
  }
};
struct Edge {
  std::string baseRelation;
  EventPair pair;

  Edge(std::string baseRelation, EventPair pair)
      : baseRelation(std::move(baseRelation)), pair(std::move(pair)) {}

  [[nodiscard]] Event from() const { return pair.first; }
  [[nodiscard]] Event to() const { return pair.second; }
  bool operator==(const Edge &other) const {
    return baseRelation == other.baseRelation && pair == other.pair;
  }
};
// hashing
template <>
struct std::hash<UndirectedEdge> {
  std::size_t operator()(const UndirectedEdge &edge) const noexcept {
    return edge.e1 | edge.e2 << 16;
  }
};
template <>
struct std::hash<Edge> {
  std::size_t operator()(const Edge &edge) const noexcept {
    return hash<EventPair>()(edge.pair) ^ hash<std::string>()(edge.baseRelation);
  }
};
template <>
struct std::hash<SatExprValue> {
  std::size_t operator()(const SatExprValue &value) const noexcept {
    if (std::holds_alternative<SatSetValue>(value)) {
      const auto events = std::get<SatSetValue>(value);
      size_t seed = 0;
      for (const auto event : events.first) {
        boost::hash_combine(seed, std::hash<int>()(event));
      }
      return seed;
    }
    const auto eventPairs = std::get<SatRelationValue>(value);
    size_t seed = 0;
    for (const auto pair : eventPairs.first) {
      boost::hash_combine(seed, std::hash<EventPair>()(pair));
    }
    return seed;
  }
};

class Model {
 public:
  explicit Model(const Cube &cube);

  [[nodiscard]] bool evaluateExpression(const Literal &literal) const;
  CanonicalAnnotation<SatExprValue> evaluateExpression(CanonicalSet set) const;
  CanonicalAnnotation<SatExprValue> evaluateExpression(CanonicalRelation relation) const;

  bool addSetMembership(const SetMembership &setMembership, SaturationAnnotation saturationDepth);
  bool addEdge(const Edge &edge, SaturationAnnotation saturationDepth);
  bool addIdentity(Event e1, Event e2, SaturationAnnotation saturationDepth);

  [[nodiscard]] bool containsEdge(const Edge &edge) const;
  [[nodiscard]] bool containsSetMembership(const SetMembership &memberhsip) const;
  [[nodiscard]] bool containsIdentity(Event e1, Event e2) const;

  [[nodiscard]] SaturationAnnotation getEdgeSaturation(const Edge &edge) const;
  [[nodiscard]] SaturationAnnotation getSetMembershipSaturation(
      const SetMembership &memberhsip) const;
  [[nodiscard]] SaturationAnnotation getIdentitySaturation(Event e1, Event e2) const;

  void exportModel(const std::string &filename) const;

 private:
  EventSet events;
  std::unordered_map<std::string, SatSetValue> baseSets;
  std::unordered_map<std::string, SatRelationValue> baseRelations;
  std::pair<std::unordered_set<UndirectedEdge>,
            std::unordered_map<UndirectedEdge, SaturationAnnotation>>
      identities;

  [[nodiscard]] EventSet getEquivalenceClass(Event event) const;
  [[nodiscard]] std::unordered_set<Edge> getIncidentEdges(Event event) const;
};

// the method saturates a model by adding edges
// the set of vertices remains unchanged
// for identity one could merge the vertices. but we add equality edges
// this makes tha analysis more easy: saturations can be counted on edges
// all edges of the given model have count 0. The count of a saturated edge is:
// min_[lhs assumption path witness] max{edge in path} + 1
// IMPORTANT: modeling equalities explicit in model requires us to ensure that model is consistent
// wrt. these equalities. Otherwise a naive evaluation of an expression in such a model may me
// wrong. We ensure consistency by the fact that all vertices in the same equivalence class (wrt
// equalities) have the same edges
void saturateModel(Model &model);
