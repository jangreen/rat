#pragma once

#include <boost/graph/adjacency_list.hpp>
#include <boost/pending/disjoint_sets.hpp>

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
template <>
struct std::hash<UndirectedEdge> {
  std::size_t operator()(const UndirectedEdge &edge) const noexcept {
    return edge.e1 | edge.e2 << 16;
  }
};
struct Edge {
  std::string baseRelation;
  EventPair pair;

  [[nodiscard]] Event from() const { return pair.first; }
  [[nodiscard]] Event to() const { return pair.second; }
  bool operator==(const Edge &other) const {
    return baseRelation == other.baseRelation && pair == other.pair;
  }
};
template <>
struct std::hash<Edge> {
  std::size_t operator()(const Edge &edge) const noexcept {
    return hash<EventPair>()(edge.pair) ^ hash<std::string>()(edge.baseRelation);
  }
};

namespace {
[[nodiscard]] inline std::string annotationToString(
    const CanonicalAnnotation<SatExprValue> annotation) {
  std::string output;

  if (annotation->getValue()) {
    if (std::holds_alternative<SatSetValue>(annotation->getValue().value())) {
      const auto set = std::get<SatSetValue>(annotation->getValue().value());
      for (const auto event : set.first) {
        output += std::to_string(event) + " ";
      }
    }

    if (std::holds_alternative<SatRelationValue>(annotation->getValue().value())) {
      const auto relation = std::get<SatRelationValue>(annotation->getValue().value());
      for (const auto [from, to] : relation.first) {
        output += std::to_string(from) + "." + std::to_string(to) + " ";
      }
    }
  }

  if (annotation->getLeft() != annotation) {
    output += "(" + annotationToString(annotation->getLeft());
    if (annotation->getRight() != annotation) {
      output += "," + annotationToString(annotation->getRight());
    }
    output += ")";
  }

  return output;
}

bool validate(const AnnotatedSet<SatExprValue> &annotatedSet);
bool validate(const AnnotatedRelation<SatExprValue> &annotatedRelation) {
  const auto &[relation, annotation] = annotatedRelation;

  assert(std::holds_alternative<SatRelationValue>(annotation->getValue().value()));

  switch (relation->operation) {
    case RelationOperation::relationIntersection:
    case RelationOperation::relationUnion:
    case RelationOperation::composition:
      assert(validate(Annotated::getLeftRelation(annotatedRelation)));
      assert(validate(Annotated::getRight(annotatedRelation)));
      return true;
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      assert(validate(Annotated::getLeftRelation(annotatedRelation)));
      return true;
    case RelationOperation::setIdentity:
      assert(validate(Annotated::getLeftSet(annotatedRelation)));
      return true;
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemneted");
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::fullRelation:
    case RelationOperation::emptyRelation:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
}
bool validate(const AnnotatedSet<SatExprValue> &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  assert(std::holds_alternative<SatSetValue>(annotation->getValue().value()));

  switch (set->operation) {
    case SetOperation::image:
    case SetOperation::domain:
      assert(validate(Annotated::getLeft(annotatedSet)));
      assert(validate(Annotated::getRightRelation(annotatedSet)));
      return true;
    case SetOperation::setIntersection:
    case SetOperation::setUnion:
      assert(validate(Annotated::getLeft(annotatedSet)));
      assert(validate(Annotated::getRightSet(annotatedSet)));
      return true;
    case SetOperation::fullSet:
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
}

// std::pair<std::map<int, int>, std::map<int, EventSet>> getEquivalenceClasses(const Cube &cube) {
//   std::map<int, int> minimalRepresentatives;
//   std::map<int, EventSet> setsForRepresentatives;
//
//   // each event is in its own equivalence class
//   const auto events = gatherActiveEvents(cube);
//   for (const auto event : events) {
//     minimalRepresentatives.insert({event, event});
//     setsForRepresentatives[event].insert(event);
//   }
//
//   for (const auto &literal : cube) {
//     assert(literal.isPositiveAtomic());
//     if (literal.isPositiveEqualityPredicate()) {
//       // e1 = e2
//       const int e1 = literal.leftEvent->label.value();
//       const int e2 = literal.rightEvent->label.value();
//       if (minimalRepresentatives.at(e1) < minimalRepresentatives.at(e2)) {
//         minimalRepresentatives.at(e2) = e1;
//       } else {
//         minimalRepresentatives.at(e1) = e2;
//       }
//       const int min = std::min(minimalRepresentatives.at(e1), minimalRepresentatives.at(e2));
//       setsForRepresentatives[min].insert(setsForRepresentatives.at(e1).begin(),
//                                          setsForRepresentatives.at(e1).end());
//       setsForRepresentatives[min].insert(setsForRepresentatives.at(e2).begin(),
//                                          setsForRepresentatives.at(e2).end());
//     }
//   }
//   return {minimalRepresentatives, setsForRepresentatives};
// }
}  // namespace

// TODO: use dependency graphs?
// // dependency graph for saturation
// typedef std::variant<Edge, SetMembership> Fact;
// // TODO: for minimal reasons use hypergraphs? Currently we just save the first reason for an edge
// // // A Vertex is either:
// // // a vertex representing an edge in the model.
// // // or of type 'edge'=std::nullopt if it represents an hyedge in the dependency graph
// // struct Vertex {
// //   std::optional<Edge> edge;
// // };
//
// typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, Fact> DepGraph;
// typedef boost::graph_traits<DepGraph>::vertex_descriptor vertex_t;
// typedef boost::graph_traits<DepGraph>::edge_descriptor edge_t;

class Model {
 public:
  explicit Model(const Cube &cube) {
    // add all events to graph
    events = gatherActiveEvents(cube);

    // save all equalities
    for (const auto &equality : cube | std::views::filter(&Literal::isPositiveEqualityPredicate)) {
      const auto e1 = equality.leftEvent->label.value();
      const auto e2 = equality.rightEvent->label.value();
      addIdentity(e1, e2, {0, 0});
    }

    // add all set memberships to vertices
    for (const auto &setMembership : cube | std::views::filter(&Literal::isPositiveSetPredicate)) {
      const auto baseSet = setMembership.identifier.value();
      const auto event = setMembership.leftEvent->label.value();
      addSetMembership(SetMembership(baseSet, event), {0, 0});
    }

    // add all edges
    for (const auto &edge : cube | std::views::filter(&Literal::isPositiveEdgePredicate)) {
      const auto baseRelation = edge.identifier.value();
      const auto from = edge.leftEvent->label.value();
      const auto to = edge.rightEvent->label.value();
      addEdge(Edge(baseRelation, {from, to}), {0, 0});
    }
  }

  [[nodiscard]] bool evaluateExpression(const Literal &literal) const {
    switch (literal.operation) {
      case PredicateOperation::constant:
        return !literal.negated;
      case PredicateOperation::edge: {
        const auto baseRelation = literal.identifier.value();
        const auto from = literal.leftEvent->label.value();
        const auto to = literal.rightEvent->label.value();
        // return true iff: edge exists + literal pos, edge not + literal neg
        return contains(Edge(baseRelation, {from, to})) ^ literal.negated;
      }
      case PredicateOperation::equality: {
        const auto e1 = literal.leftEvent->label.value();
        const auto e2 = literal.rightEvent->label.value();
        return sameEquivalenceClass(e1, e2) ^ literal.negated;
      }
      case PredicateOperation::set: {
        const auto baseSet = literal.identifier.value();
        const auto event = literal.leftEvent->label.value();
        return contains(SetMembership(baseSet, event)) ^ literal.negated;
      }
      case PredicateOperation::setNonEmptiness: {
        const auto result = evaluateExpression(literal.set)->getValue().value();
        const auto satSetValue = std::get<SatSetValue>(result);
        const auto &[setValue, saturation] = satSetValue;
        return !setValue.empty() ^ literal.negated;
      }
      default:
        throw std::logic_error("unreachable");
    }
  }
  CanonicalAnnotation<SatExprValue> evaluateExpression(const CanonicalRelation relation) const {
    switch (relation->operation) {
      case RelationOperation::relationIntersection: {
        const auto left = evaluateExpression(relation->leftOperand);
        const auto right = evaluateExpression(relation->rightOperand);
        const auto [leftValue, lsMap] = std::get<SatRelationValue>(left->getValue().value());
        const auto [rightValue, rsMap] = std::get<SatRelationValue>(right->getValue().value());

        RelationValue intersect;
        EdgeSaturationMap intersectSMap;
        std::ranges::set_intersection(leftValue, rightValue,
                                      std::inserter(intersect, intersect.begin()));
        for (const auto edge : intersect) {
          intersectSMap[edge] = {lsMap.at(edge).first + rsMap.at(edge).first,
                                 lsMap.at(edge).second + rsMap.at(edge).second};
        }
        SatRelationValue satIntersect(intersect, intersectSMap);
        assert(validate(
            {relation, Annotation<SatExprValue>::newAnnotation(satIntersect, left, right)}));
        return Annotation<SatExprValue>::newAnnotation(satIntersect, left, right);
      }
      case RelationOperation::composition: {
        auto left = evaluateExpression(relation->leftOperand);
        const auto right = evaluateExpression(relation->rightOperand);
        const auto [leftValue, lsMap] = std::get<SatRelationValue>(left->getValue().value());
        const auto [rightValue, rsMap] = std::get<SatRelationValue>(right->getValue().value());

        RelationValue composition;
        EdgeSaturationMap compSMap;
        for (const auto &l : leftValue) {
          for (const auto &r : rightValue) {
            if (l.second == r.first) {
              const EventPair composed = {l.first, r.second};
              composition.insert(composed);
              compSMap[composed] = {lsMap.at(l).first + rsMap.at(r).first,
                                    lsMap.at(l).second + rsMap.at(r).second};
            }
          }
        }
        SatRelationValue satComposition(composition, compSMap);
        assert(validate(
            {relation, Annotation<SatExprValue>::newAnnotation(satComposition, left, right)}));
        return Annotation<SatExprValue>::newAnnotation(satComposition, left, right);
      }
      case RelationOperation::relationUnion: {
        const auto left = evaluateExpression(relation->leftOperand);
        const auto right = evaluateExpression(relation->rightOperand);
        const auto [leftValue, lsMap] = std::get<SatRelationValue>(left->getValue().value());
        const auto [rightValue, rsMap] = std::get<SatRelationValue>(right->getValue().value());

        RelationValue relunion;
        EdgeSaturationMap unionSMap;
        std::ranges::set_union(leftValue, rightValue, std::inserter(relunion, relunion.begin()));
        for (const auto edge : relunion) {
          auto leftSaturationCost =
              lsMap.contains(edge) ? lsMap.at(edge) : SaturationAnnotation{INT32_MAX, INT32_MAX};
          auto rightSaturationCost =
              rsMap.contains(edge) ? rsMap.at(edge) : SaturationAnnotation{INT32_MAX, INT32_MAX};
          unionSMap[edge] = std::min(leftSaturationCost, rightSaturationCost);
        }
        SatRelationValue satUnion(relunion, unionSMap);
        assert(
            validate({relation, Annotation<SatExprValue>::newAnnotation(satUnion, left, right)}));
        return Annotation<SatExprValue>::newAnnotation(satUnion, left, right);
      }
      case RelationOperation::converse: {
        auto left = evaluateExpression(relation->leftOperand);
        const auto [leftValue, lsMap] = std::get<SatRelationValue>(left->getValue().value());

        RelationValue converse;
        EdgeSaturationMap convSMap;
        for (auto &[from, to] : leftValue) {
          const EventPair inverted = {to, from};
          converse.insert(inverted);
          convSMap[inverted] = lsMap.at({from, to});
        }
        SatRelationValue satConverse(converse, convSMap);
        assert(validate({relation, Annotation<SatExprValue>::newAnnotation(satConverse, left)}));
        return Annotation<SatExprValue>::newAnnotation(satConverse, left);
      }
      case RelationOperation::transitiveClosure: {
        auto left = evaluateExpression(relation->leftOperand);
        const auto [underlying, sMap] = std::get<SatRelationValue>(left->getValue().value());

        RelationValue transitiveClosure;
        EdgeSaturationMap tClsMap;
        // insert id
        for (const auto event : events) {
          transitiveClosure.insert({event, event});
          tClsMap[{event, event}] = {0, 0};
        }
        // iterate underlying
        while (true) {
          RelationValue newPairs;
          for (const auto [clFrom, clTo] : transitiveClosure) {
            for (const auto [rFrom, rTo] : underlying) {
              const EventPair composition = {clFrom, rTo};
              if (clTo == rFrom && !transitiveClosure.contains(composition)) {
                newPairs.insert(composition);
                tClsMap[composition] = {
                    tClsMap.at({clFrom, clTo}).first + sMap.at({rFrom, rTo}).first,
                    tClsMap.at({clFrom, clTo}).second + sMap.at({rFrom, rTo}).second};
              }
            }
          }
          if (newPairs.empty()) {
            break;
          }
          transitiveClosure.insert(std::make_move_iterator(newPairs.begin()),
                                   std::make_move_iterator(newPairs.end()));
        }
        SatRelationValue satTransitiveClosure(transitiveClosure, tClsMap);
        assert(validate(
            {relation, Annotation<SatExprValue>::newAnnotation(satTransitiveClosure, left)}));
        return Annotation<SatExprValue>::newAnnotation(satTransitiveClosure, left);
      }
      case RelationOperation::baseRelation: {
        const auto baseRelation = relation->identifier.value();
        if (!baseRelations.contains(baseRelation)) {
          return Annotation<SatExprValue>::newLeaf(SatRelationValue{});
        }
        const auto value = baseRelations.at(baseRelation);
        assert(validate({relation, Annotation<SatExprValue>::newLeaf(value)}));
        return Annotation<SatExprValue>::newLeaf(value);
      }
      case RelationOperation::idRelation: {
        RelationValue value;
        EdgeSaturationMap sMap;
        for (const auto event : events) {
          value.insert({event, event});
          sMap[{event, event}] = {0, 0};
        }
        SatRelationValue satId(value, sMap);
        assert(validate({relation, Annotation<SatExprValue>::newLeaf(satId)}));
        return Annotation<SatExprValue>::newLeaf(satId);
      }
      case RelationOperation::emptyRelation:
        assert(validate({relation, Annotation<SatExprValue>::newLeaf(SatRelationValue{})}));
        return Annotation<SatExprValue>::newLeaf(SatRelationValue{});
      case RelationOperation::fullRelation: {
        RelationValue value;
        EdgeSaturationMap sMap;
        for (const auto e1 : events) {
          for (const auto e2 : events) {
            value.insert({e1, e2});
            sMap[{e1, e2}] = {0, 0};
          }
        }
        SatRelationValue satFull(value, sMap);
        assert(validate({relation, Annotation<SatExprValue>::newLeaf(satFull)}));
        return Annotation<SatExprValue>::newLeaf(satFull);
      }
      case RelationOperation::setIdentity: {
        auto left = evaluateExpression(relation->set);
        const auto [leftValue, lsMap] = std::get<SatSetValue>(left->getValue().value());

        RelationValue value;
        EdgeSaturationMap sMap;
        for (const auto event : leftValue) {
          value.insert({event, event});
          sMap[{event, event}] = lsMap.at(event);
        }
        SatRelationValue satSetId(value, sMap);
        assert(validate({relation, Annotation<SatExprValue>::newAnnotation(satSetId, left)}));
        return Annotation<SatExprValue>::newAnnotation(satSetId, left);
      }
      case RelationOperation::cartesianProduct:
        throw std::logic_error("not implemented");
      default:
        throw std::logic_error("unreachable");
    }
  }
  CanonicalAnnotation<SatExprValue> evaluateExpression(const CanonicalSet set) const {
    switch (set->operation) {
      case SetOperation::event: {
        const Event event = set->label.value();
        EventSaturationMap sMap = {{event, {0, 0}}};
        const SatSetValue satValue(SetValue{event}, sMap);
        assert(validate({set, Annotation<SatExprValue>::newLeaf(satValue)}));
        return Annotation<SatExprValue>::newLeaf(satValue);
      }
      case SetOperation::image: {
        const auto left = evaluateExpression(set->leftOperand);
        const auto right = evaluateExpression(set->relation);
        const auto [leftValue, lsMap] = std::get<SatSetValue>(left->getValue().value());
        const auto [rightValue, rsMap] = std::get<SatRelationValue>(right->getValue().value());

        SetValue image;
        EventSaturationMap imageSMap;
        for (const auto &r : rightValue) {
          if (leftValue.contains(r.first)) {
            image.insert(r.second);
            imageSMap[r.second] = {lsMap.at(r.first).first + rsMap.at(r).first,
                                   lsMap.at(r.first).second + rsMap.at(r).second};
          }
        }
        SatSetValue satImage(image, imageSMap);
        assert(validate({set, Annotation<SatExprValue>::newAnnotation(satImage, left, right)}));
        return Annotation<SatExprValue>::newAnnotation(satImage, left, right);
      }
      case SetOperation::domain: {
        const auto left = evaluateExpression(set->leftOperand);
        const auto right = evaluateExpression(set->relation);
        const auto [leftValue, lsMap] = std::get<SatSetValue>(left->getValue().value());
        const auto [rightValue, rsMap] = std::get<SatRelationValue>(right->getValue().value());

        SetValue domain;
        EventSaturationMap domainSMap;
        for (const auto &r : rightValue) {
          if (leftValue.contains(r.second)) {
            domain.insert(r.first);
            domainSMap[r.first] = {lsMap.at(r.second).first + rsMap.at(r).first,
                                   lsMap.at(r.second).second + rsMap.at(r).second};
          }
        }
        SatSetValue satDomain(domain, domainSMap);
        assert(validate({set, Annotation<SatExprValue>::newAnnotation(satDomain, left, right)}));
        return Annotation<SatExprValue>::newAnnotation(satDomain, left, right);
      }
      case SetOperation::baseSet: {
        const auto baseSet = set->identifier.value();
        if (!baseSets.contains(baseSet)) {
          return Annotation<SatExprValue>::newLeaf(SatSetValue{});
        }
        const auto value = baseSets.at(baseSet);
        assert(validate({set, Annotation<SatExprValue>::newLeaf(value)}));
        return Annotation<SatExprValue>::newLeaf(value);
      }
      case SetOperation::emptySet:
        assert(validate({set, Annotation<SatExprValue>::newLeaf(SatSetValue{})}));
        return Annotation<SatExprValue>::newLeaf(SatSetValue{});
      case SetOperation::fullSet: {
        SetValue value;
        EventSaturationMap sMap;
        for (const auto event : events) {
          value.insert(event);
          sMap[event] = {0, 0};
        }
        SatSetValue satFull(value, sMap);
        assert(validate({set, Annotation<SatExprValue>::newLeaf(satFull)}));
        return Annotation<SatExprValue>::newLeaf(satFull);
      }
      case SetOperation::setIntersection: {
        const auto left = evaluateExpression(set->leftOperand);
        const auto right = evaluateExpression(set->rightOperand);
        const auto [leftValue, lsMap] = std::get<SatSetValue>(left->getValue().value());
        const auto [rightValue, rsMap] = std::get<SatSetValue>(right->getValue().value());

        SetValue intersect;
        EventSaturationMap intersectSMap;
        std::ranges::set_intersection(leftValue, rightValue,
                                      std::inserter(intersect, intersect.begin()));
        for (const auto event : intersect) {
          intersectSMap[event] = {lsMap.at(event).first + rsMap.at(event).first,
                                  lsMap.at(event).second + rsMap.at(event).second};
        }
        SatSetValue satIntersect(intersect, intersectSMap);
        assert(validate({set, Annotation<SatExprValue>::newAnnotation(satIntersect, left, right)}));
        return Annotation<SatExprValue>::newAnnotation(satIntersect, left, right);
      }
      case SetOperation::setUnion: {
        const auto left = evaluateExpression(set->leftOperand);
        const auto right = evaluateExpression(set->rightOperand);
        const auto [leftValue, lsMap] = std::get<SatSetValue>(left->getValue().value());
        const auto [rightValue, rsMap] = std::get<SatSetValue>(right->getValue().value());

        SetValue setunion;
        EventSaturationMap unionSMap;
        std::ranges::set_union(leftValue, rightValue, std::inserter(setunion, setunion.begin()));
        for (const auto event : setunion) {
          auto leftSaturationCost =
              lsMap.contains(event) ? lsMap.at(event) : SaturationAnnotation{INT32_MAX, INT32_MAX};
          auto rightSaturationCost =
              rsMap.contains(event) ? rsMap.at(event) : SaturationAnnotation{INT32_MAX, INT32_MAX};
          unionSMap[event] = std::min(leftSaturationCost, rightSaturationCost);
        }
        SatSetValue satUnion(setunion, unionSMap);
        assert(validate({set, Annotation<SatExprValue>::newAnnotation(satUnion, left, right)}));
        return Annotation<SatExprValue>::newAnnotation(satUnion, left, right);
      }
      default:
        throw std::logic_error("unreachable");
    }
  }

  // returns true iff model changed
  bool addSetMembership(const SetMembership &setMembership, SaturationAnnotation saturationDepth) {
    if (!baseSets[setMembership.baseSet].first.contains(setMembership.event) ||
        baseSets[setMembership.baseSet].second.at(setMembership.event) > saturationDepth) {
      baseSets[setMembership.baseSet].first.insert(setMembership.event);
      baseSets[setMembership.baseSet].second[setMembership.event] = saturationDepth;

      // propagate to equivalence class
      for (const auto equivalenEvent : getEquivalenceClass(setMembership.event)) {
        const auto updatedMembership = SetMembership(setMembership.baseSet, equivalenEvent);
        auto saturationCost = saturationDepth;
        if (equivalenEvent != setMembership.event) {
          saturationCost.first += equalities.second.at({setMembership.event, equivalenEvent}).first;

          saturationCost.second +=
              equalities.second.at({setMembership.event, equivalenEvent}).second;
        }
        addSetMembership(updatedMembership, saturationCost);
      }
      return true;
    }
    return false;
  }

  // returns true iff model changed
  bool addEdge(const Edge &edge, SaturationAnnotation saturationDepth) {
    if (!baseRelations[edge.baseRelation].first.contains(edge.pair) ||
        baseRelations[edge.baseRelation].second.at(edge.pair) > saturationDepth) {
      baseRelations[edge.baseRelation].first.insert(edge.pair);
      baseRelations[edge.baseRelation].second[edge.pair] = saturationDepth;

      // propagate to equivalence class
      for (const auto e1 : getEquivalenceClass(edge.from())) {
        for (const auto e2 : getEquivalenceClass(edge.to())) {
          const auto updatedEdge = Edge(edge.baseRelation, {e1, e2});
          auto saturationCost = saturationDepth;
          if (e1 != edge.from()) {
            saturationCost.first += equalities.second.at({edge.from(), e1}).first;
            saturationCost.second += equalities.second.at({edge.from(), e1}).second;
          }
          if (e2 != edge.to()) {
            saturationCost.first += equalities.second.at({edge.to(), e2}).first;
            saturationCost.second += equalities.second.at({edge.to(), e2}).second;
          }
          addEdge(updatedEdge, saturationCost);
        }
      }
      return true;
    }
    return false;
  }

  [[nodiscard]] bool contains(const Edge &edge) const {
    return baseRelations.contains(edge.baseRelation) &&
           baseRelations.at(edge.baseRelation).first.contains(edge.pair);
  }

  [[nodiscard]] bool contains(const SetMembership &memberhsip) const {
    return baseSets.contains(memberhsip.baseSet) &&
           baseSets.at(memberhsip.baseSet).first.contains(memberhsip.event);
  }

  // returns true iff model changed
  bool addIdentity(const Event e1, const Event e2, SaturationAnnotation saturationDepth) {
    const auto equality = UndirectedEdge(e1, e2);
    if (!equalities.first.contains(equality) || equalities.second.at(equality) > saturationDepth) {
      equalities.first.insert(equality);
      equalities.second[equality] = saturationDepth;
      // update equalities, merge equivalence classes
      for (const auto undirectedEdge : equalities.first) {
        const SaturationAnnotation saturationCost = {
            equalities.second.at(equality).first + equalities.second.at(undirectedEdge).first,
            equalities.second.at(equality).second + equalities.second.at(undirectedEdge).second};
        if (undirectedEdge.contains(e1)) {
          // e1 = e2 (equality), e1 = e3 (undirectedEdge) -> e2 = e3
          const auto e3 = undirectedEdge.neighbour(e1).value();

          addIdentity(e2, e3, saturationCost);
        }
        if (undirectedEdge.contains(e2)) {
          // e1 = e2, e2 = e3 -> e1 = e3
          const auto e3 = undirectedEdge.neighbour(e2).value();
          addIdentity(e1, e3, saturationCost);
        }
      }

      // propagate to equivalence class
      // e1 = e2 -> all e1Class must copy edges from e2
      for (const auto e1Class : getEquivalenceClass(e1)) {
        const auto e2Edges = getIncidentEdges(e2);
        for (const auto e2Edge : e2Edges) {
          const SaturationAnnotation saturationCost = {
              equalities.second.at(equality).first +
                  baseRelations[e2Edge.baseRelation].second.at(e2Edge.pair).first,
              equalities.second.at(equality).second +
                  baseRelations[e2Edge.baseRelation].second.at(e2Edge.pair).second};

          if (e2Edge.from() == e2) {
            // e2 -> e3, then e1Class -> e3
            const auto e3 = e2Edge.to();
            const auto newEdge = Edge(e2Edge.baseRelation, {e1Class, e3});
            addEdge(newEdge, saturationCost);
          }
          if (e2Edge.to() == e2) {
            // e3 -> e2, then e3 -> e1Class
            const auto e3 = e2Edge.from();
            const auto newEdge = Edge(e2Edge.baseRelation, {e3, e1Class});
            addEdge(newEdge, saturationCost);
          }
        }
      }
      // e1 = e2 -> all e2Class must copy edges from e1
      for (const auto e2Class : getEquivalenceClass(e2)) {
        const auto e1Edges = getIncidentEdges(e1);
        for (const auto &e1Edge : e1Edges) {
          const SaturationAnnotation saturationCost = {
              equalities.second.at(equality).first +
                  baseRelations[e1Edge.baseRelation].second.at(e1Edge.pair).first,
              equalities.second.at(equality).second +
                  baseRelations[e1Edge.baseRelation].second.at(e1Edge.pair).second};

          if (e1Edge.from() == e1) {
            // e1 -> e3, then e2Class -> e3
            const auto e3 = e1Edge.to();
            const auto newEdge = Edge(e1Edge.baseRelation, {e2Class, e3});
            addEdge(newEdge, saturationCost);
          }
          if (e1Edge.to() == e1) {
            // e3 -> e1, then e3 -> e2Class
            const auto e3 = e1Edge.from();
            const auto newEdge = Edge(e1Edge.baseRelation, {e3, e2Class});
            addEdge(newEdge, saturationCost);
          }
        }
      }
      // TODO: update set Meberhsips
      return true;
    }
    return false;
  }
  [[nodiscard]] bool sameEquivalenceClass(const Event e1, const Event e2) const {
    return equalities.first.contains({e1, e2});
  }

  // bool validate() const {
  //   // TODO: validate equivalance classes
  //   return true;
  // }

  void exportModel(const std::string &filename) const {
    // assert(validate());

    std::ofstream counterexamleModel("./output/" + filename + ".dot");
    counterexamleModel << "digraph { node[shape=\"circle\",margin=0]\n";
    // TODO: remove
    // print(model);
    // for (auto [a, b] : representatives) {
    //   std::cout << a << " -- " << b << std::endl;
    // }
    // std::cout << std::endl;
    // for (auto [_, b] : equivalenceClasses) {
    //   for (auto s : b) {
    //     std::cout << s << " ";
    //   }
    //   std::cout << std::endl;
    // }

    // export events + set memberships
    for (const auto event : events) {
      counterexamleModel << "N" << event << "[label = \"";
      // set memberships
      for (const auto &[baseSet, satEvents] : baseSets) {
        if (satEvents.first.contains(event)) {
          counterexamleModel << baseSet << " ";
        }
      }
      counterexamleModel << "\", tooltip=\"";
      counterexamleModel << "event: " << event << "\n";
      counterexamleModel << "equivalenceClass: ";
      for (const auto equivlenceClassEvent : getEquivalenceClass(event)) {
        counterexamleModel << equivlenceClassEvent << " ";
      }
      counterexamleModel << "\n";
      counterexamleModel << "\"];";
    }

    // export edges
    for (const auto &[baseRelation, satEdges] : baseRelations) {
      for (const auto [from, to] : satEdges.first) {
        counterexamleModel << "N" << from << " -> N" << to;
        counterexamleModel << "[label = \"" << baseRelation;
        counterexamleModel << "\", tooltip=\"";
        counterexamleModel << "#id sat: " << satEdges.second.at({from, to}).first << "\n";
        counterexamleModel << "#base sat: " << satEdges.second.at({from, to}).second << "\n";
        counterexamleModel << "\"];\n";
      }
    }

    counterexamleModel << "}" << '\n';
    counterexamleModel.close();
  }

 private:
  EventSet events;
  std::unordered_map<std::string, SatSetValue> baseSets;
  std::unordered_map<std::string, SatRelationValue> baseRelations;
  // undirected edges
  std::pair<std::unordered_set<UndirectedEdge>,
            std::unordered_map<UndirectedEdge, SaturationAnnotation>>
      equalities;

  // equivalence class datastructure:
  // union-find data structure
  // typedef std::map<Event, int> rank_t;
  // typedef std::map<Event, Event> parent_t;
  // rank_t ranks;
  // parent_t parents;
  // boost::disjoint_sets equivalenceClasses(boost::make_assoc_property_map(ranks),
  //                                         boost::make_assoc_property_map(parents));
  // std::map<Event, EventSet> repToClass;

  [[nodiscard]] EventSet getEquivalenceClass(const Event event) const {
    // TODO: use union-find data structure for fast access, save neighbours
    EventSet equivClass = {event};
    for (const auto &equality : equalities.first) {
      if (equality.contains(event)) {
        equivClass.insert(equality.neighbour(event).value());
      }
    }
    return equivClass;
  }
  [[nodiscard]] std::unordered_set<Edge> getIncidentEdges(const Event event) const {
    // TODO: use better data structure for fast access, save neighbours
    std::unordered_set<Edge> incidentEdges;
    for (const auto &[baseRelation, satEdges] : baseRelations) {
      const auto [edges, sMap] = satEdges;
      for (const auto &edge : edges) {
        if (edge.first == event || edge.second == event) {
          incidentEdges.insert(Edge(baseRelation, edge));
        }
      }
    }
    return incidentEdges;
  }
  // void mergeEquivalenceClasses(const RelationValue &equivalences) {
  //   // TODO:
  // }
};

// returns true iff model changed
inline bool saturateIdAssumptions(Model &model) {
  bool modelChanged = false;

  for (const auto &idAssumption : Assumption::idAssumptions) {
    // evaluate lhs of assumption
    const auto exprValue = model.evaluateExpression(idAssumption.relation);
    const auto [lhsValue, sMap] = std::get<SatRelationValue>(exprValue->getValue().value());

    for (const auto edge : lhsValue) {
      auto saturationCost = sMap.at(edge);
      saturationCost.first++;
      modelChanged |= model.addIdentity(edge.first, edge.second, saturationCost);
    }
  }
  return modelChanged;
}

// returns true iff model changed
inline bool saturateBaseRelationAssumptions(Model &model) {
  bool modelChanged = false;

  for (const auto &[baseRelation, baseAssumption] : Assumption::baseAssumptions) {
    const auto exprValue = model.evaluateExpression(baseAssumption.relation);
    const auto [lhsValue, sMap] = std::get<SatRelationValue>(exprValue->getValue().value());

    // update model
    for (const auto edge : lhsValue) {
      auto saturationCost = sMap.at(edge);
      saturationCost.second++;
      modelChanged |= model.addEdge(Edge(baseRelation, edge), saturationCost);
    }
  }
  return modelChanged;
}

// returns true iff model changed
inline bool saturateBaseSetAssumptions(Model &model) {
  bool modelChanged = false;

  for (const auto &[baseSet, baseAssumption] : Assumption::baseSetAssumptions) {
    const auto exprValue = model.evaluateExpression(baseAssumption.set);
    const auto [lhsValue, sMap] = std::get<SatSetValue>(exprValue->getValue().value());

    // update model
    for (const auto event : lhsValue) {
      auto saturationCost = sMap.at(event);
      saturationCost.second++;
      modelChanged |= model.addSetMembership(SetMembership(baseSet, event), saturationCost);
    }
  }
  return modelChanged;
}

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
inline void saturateModel(Model &model) {
  bool modelChanged = true;
  while (modelChanged) {
    modelChanged = saturateIdAssumptions(model) | saturateBaseRelationAssumptions(model) |
                   saturateBaseSetAssumptions(model);
  }
}

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
