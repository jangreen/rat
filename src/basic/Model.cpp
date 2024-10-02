#include "Model.h"

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
}  // namespace

Model::Model(const Cube &cube) {
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

[[nodiscard]] bool Model::evaluateExpression(const Literal &literal) const {
  switch (literal.operation) {
    case PredicateOperation::constant:
      return !literal.negated;
    case PredicateOperation::edge: {
      const auto baseRelation = literal.identifier.value();
      const auto from = literal.leftEvent->label.value();
      const auto to = literal.rightEvent->label.value();
      // return true iff: edge exists + literal pos, edge not + literal neg
      return containsEdge(Edge(baseRelation, {from, to})) ^ literal.negated;
    }
    case PredicateOperation::equality: {
      const auto e1 = literal.leftEvent->label.value();
      const auto e2 = literal.rightEvent->label.value();
      return containsIdentity(e1, e2) ^ literal.negated;
    }
    case PredicateOperation::set: {
      const auto baseSet = literal.identifier.value();
      const auto event = literal.leftEvent->label.value();
      return containsSetMembership(SetMembership(baseSet, event)) ^ literal.negated;
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

CanonicalAnnotation<SatExprValue> Model::evaluateExpression(
    const CanonicalRelation relation) const {
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
      assert(
          validate({relation, Annotation<SatExprValue>::newAnnotation(satIntersect, left, right)}));
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
      assert(validate({relation, Annotation<SatExprValue>::newAnnotation(satUnion, left, right)}));
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

CanonicalAnnotation<SatExprValue> Model::evaluateExpression(const CanonicalSet set) const {
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
bool Model::addSetMembership(const SetMembership &setMembership,
                             SaturationAnnotation saturationDepth) {
  if (!baseSets[setMembership.baseSet].first.contains(setMembership.event) ||
      baseSets[setMembership.baseSet].second.at(setMembership.event) > saturationDepth) {
    baseSets[setMembership.baseSet].first.insert(setMembership.event);
    baseSets[setMembership.baseSet].second[setMembership.event] = saturationDepth;

    // propagate to equivalence class
    for (const auto equivalenEvent : getEquivalenceClass(setMembership.event)) {
      const auto updatedMembership = SetMembership(setMembership.baseSet, equivalenEvent);
      auto saturationCost = saturationDepth;
      if (equivalenEvent != setMembership.event) {
        saturationCost.first += identities.second.at({setMembership.event, equivalenEvent}).first;

        saturationCost.second += identities.second.at({setMembership.event, equivalenEvent}).second;
      }
      addSetMembership(updatedMembership, saturationCost);
    }
    return true;
  }
  return false;
}

bool Model::addEdge(const Edge &edge, SaturationAnnotation saturationDepth) {
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
          saturationCost.first += identities.second.at({edge.from(), e1}).first;
          saturationCost.second += identities.second.at({edge.from(), e1}).second;
        }
        if (e2 != edge.to()) {
          saturationCost.first += identities.second.at({edge.to(), e2}).first;
          saturationCost.second += identities.second.at({edge.to(), e2}).second;
        }
        addEdge(updatedEdge, saturationCost);
      }
    }
    return true;
  }
  return false;
}

// returns true iff model changed
bool Model::addIdentity(const Event e1, const Event e2, SaturationAnnotation saturationDepth) {
  const auto equality = UndirectedEdge(e1, e2);
  if (!identities.first.contains(equality) || identities.second.at(equality) > saturationDepth) {
    identities.first.insert(equality);
    identities.second[equality] = saturationDepth;
    // update equalities, merge equivalence classes
    for (const auto undirectedEdge : identities.first) {
      const SaturationAnnotation saturationCost = {
          identities.second.at(equality).first + identities.second.at(undirectedEdge).first,
          identities.second.at(equality).second + identities.second.at(undirectedEdge).second};
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
            identities.second.at(equality).first +
                baseRelations[e2Edge.baseRelation].second.at(e2Edge.pair).first,
            identities.second.at(equality).second +
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
            identities.second.at(equality).first +
                baseRelations[e1Edge.baseRelation].second.at(e1Edge.pair).first,
            identities.second.at(equality).second +
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

[[nodiscard]] bool Model::containsEdge(const Edge &edge) const {
  return baseRelations.contains(edge.baseRelation) &&
         baseRelations.at(edge.baseRelation).first.contains(edge.pair);
}

[[nodiscard]] bool Model::containsSetMembership(const SetMembership &memberhsip) const {
  return baseSets.contains(memberhsip.baseSet) &&
         baseSets.at(memberhsip.baseSet).first.contains(memberhsip.event);
}

[[nodiscard]] bool Model::containsIdentity(const Event e1, const Event e2) const {
  return identities.first.contains({e1, e2});
}

[[nodiscard]] EventSet Model::getEquivalenceClass(const Event event) const {
  // TODO: use union-find data structure for fast access, save neighbours
  EventSet equivClass = {event};
  for (const auto &equality : identities.first) {
    if (equality.contains(event)) {
      equivClass.insert(equality.neighbour(event).value());
    }
  }
  return equivClass;
}
[[nodiscard]] std::unordered_set<Edge> Model::getIncidentEdges(const Event event) const {
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

void Model::exportModel(const std::string &filename) const {
  std::ofstream counterexamleModel("./output/" + filename + ".dot");
  counterexamleModel << "digraph { node[shape=\"circle\",margin=0]\n";

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

// returns true iff model changed
bool saturateIdAssumptions(Model &model) {
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
bool saturateBaseRelationAssumptions(Model &model) {
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
bool saturateBaseSetAssumptions(Model &model) {
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

void saturateModel(Model &model) {
  bool modelChanged = true;
  while (modelChanged) {
    modelChanged = saturateIdAssumptions(model) | saturateBaseRelationAssumptions(model) |
                   saturateBaseSetAssumptions(model);
  }
}