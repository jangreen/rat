#include "Model.h"

#include "../../Assumption.h"
#include "../../utility.h"

EventSet Model::getEquivalenceClass(const EventType &event) const {
  // TODO: use better datastructure / transformer
  EventSet equivClass = {event};
  for (const auto &i : identities) {
    if (i.from() == event) {
      equivClass.insert(i.to());
    }
  }
  return equivClass;
}

Model::Model(const Cube &cube) {
  // add events
  events = gatherPositiveEvents(cube);

  // add identities
  for (const auto &equality : cube | std::views::filter(&Literal::isPositiveEqualityPredicate)) {
    const auto e1 = equality.leftEvent->label.value();
    const auto e2 = equality.rightEvent->label.value();
    const auto reason = Relation::idRelation();
    addIdentity({e1, e2, reason});
  }

  // add set memberships
  for (const auto &setMembership : cube | std::views::filter(&Literal::isPositiveSetPredicate)) {
    const auto baseSet = setMembership.identifier.value();
    const auto event = setMembership.leftEvent->label.value();
    const auto reason = Set::newBaseSet(baseSet);
    addBaseSet(baseSet, {event, reason});
  }

  // add edges
  for (const auto &edgeLiteral : cube | std::views::filter(&Literal::isPositiveEdgePredicate)) {
    const auto baseRelation = edgeLiteral.identifier.value();
    const auto from = edgeLiteral.leftEvent->label.value();
    const auto to = edgeLiteral.rightEvent->label.value();
    const auto reason = Relation::newBaseRelation(baseRelation);
    addBaseRelation(baseRelation, {from, to, reason});
  }
}

bool Model::evaluate(const Literal &literal) const {
  switch (literal.operation) {
    case PredicateOperation::constant:
      return !literal.negated;
    case PredicateOperation::edge: {
      const auto baseRelation = literal.identifier.value();
      const auto from = literal.leftEvent->label.value();
      const auto to = literal.rightEvent->label.value();
      // return true iff: edge exists + literal pos, edge not + literal neg
      return baseRelationContains(baseRelation, from, to) ^ literal.negated;
    }
    case PredicateOperation::equality: {
      const auto e1 = literal.leftEvent->label.value();
      const auto e2 = literal.rightEvent->label.value();
      return containsIdentity(e1, e2) ^ literal.negated;
    }
    case PredicateOperation::set: {
      const auto baseSet = literal.identifier.value();
      const auto event = literal.leftEvent->label.value();
      return baseSetContains(baseSet, event) ^ literal.negated;
    }
    case PredicateOperation::setNonEmptiness: {
      const auto interpretationTree = evaluate(literal.set);
      const auto setValue = interpretationTree->getSetValue();
      return !setValue.empty() ^ literal.negated;
    }
    default:
      throw std::logic_error("unreachable");
  }
}

InterpretationPtr Model::evaluate(const CanonicalRelation relation) const {
  switch (relation->operation) {
    case RelationOperation::relationIntersection: {
      auto left = evaluate(relation->leftOperand);
      auto right = evaluate(relation->rightOperand);
      return Interpretation::relationIntersection(std::move(left), std::move(right));
    }
    case RelationOperation::composition: {
      auto left = evaluate(relation->leftOperand);
      auto right = evaluate(relation->rightOperand);
      return Interpretation::composition(std::move(left), std::move(right));
    }
    case RelationOperation::relationUnion: {
      auto left = evaluate(relation->leftOperand);
      auto right = evaluate(relation->rightOperand);
      return Interpretation::relationUnion(std::move(left), std::move(right));
    }
    case RelationOperation::converse: {
      auto left = evaluate(relation->leftOperand);
      RelationValue converse;
      for (const auto &edge : left->getRelValue()) {
        converse.insert(edge.converse());
      }
      return std::make_unique<Interpretation>(converse, std::move(left));
    }
    case RelationOperation::transitiveClosure: {
      auto left = evaluate(relation->leftOperand);
      return Interpretation::transitiveClosure(std::move(left), events);
    }
    case RelationOperation::baseRelation: {
      const auto baseRelation = relation->identifier.value();
      return baseRelations.contains(baseRelation)
                 ? std::make_unique<Interpretation>(baseRelations.at(baseRelation))
                 : std::make_unique<Interpretation>(RelationValue{});
    }
    case RelationOperation::idRelation: {
      RelationValue value;
      for (const auto event : events) {
        value.emplace(event, event, Relation::idRelation());
      }
      return std::make_unique<Interpretation>(value);
    }
    case RelationOperation::emptyRelation:
      return std::make_unique<Interpretation>(RelationValue{});
    case RelationOperation::fullRelation: {
      RelationValue value;
      for (const auto e1 : events) {
        for (const auto e2 : events) {
          value.emplace(e1, e2, Relation::fullRelation());
        }
      }
      return std::make_unique<Interpretation>(value);
    }
    case RelationOperation::setIdentity: {
      auto left = evaluate(relation->set);
      RelationValue value;
      for (const auto &event : left->getSetValue()) {
        const auto e = event.event();
        const auto reason = Relation::setIdentity(event.reason());
        value.emplace(e, e, reason);
      }
      return std::make_unique<Interpretation>(value, std::move(left));
    }
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

InterpretationPtr Model::evaluate(const CanonicalSet set) const {
  switch (set->operation) {
    case SetOperation::event: {
      const auto event = Event(set->label.value());
      SetValue value = {event};
      for (const auto &identity : identities) {
        if (const auto equivalentEvent = event.image(identity)) {
          value.emplace(equivalentEvent.value());
        }
        if (const auto equivalentEvent = event.domain(identity)) {
          value.emplace(equivalentEvent.value());
        }
      }
      return std::make_unique<Interpretation>(value);
    }
    case SetOperation::image: {
      auto left = evaluate(set->leftOperand);
      auto right = evaluate(set->relation);
      return Interpretation::image(std::move(left), std::move(right));
    }
    case SetOperation::domain: {
      auto left = evaluate(set->leftOperand);
      auto right = evaluate(set->relation);
      return Interpretation::domain(std::move(left), std::move(right));
    }
    case SetOperation::baseSet: {
      const auto baseSet = set->identifier.value();
      if (!baseSets.contains(baseSet)) {
        return std::make_unique<Interpretation>(SetValue{});
      }
      const auto value = baseSets.at(baseSet);
      return std::make_unique<Interpretation>(value);
    }
    case SetOperation::emptySet:
      return std::make_unique<Interpretation>(SetValue{});
    case SetOperation::fullSet: {
      SetValue value;
      for (const auto event : events) {
        value.emplace(event, Set::fullSet());
      }
      return std::make_unique<Interpretation>(value);
    }
    case SetOperation::setIntersection: {
      auto left = evaluate(set->leftOperand);
      auto right = evaluate(set->rightOperand);
      return Interpretation::setIntersection(std::move(left), std::move(right));
    }
    case SetOperation::setUnion: {
      auto left = evaluate(set->leftOperand);
      auto right = evaluate(set->rightOperand);
      return Interpretation::setUnion(std::move(left), std::move(right));
    }
    default:
      throw std::logic_error("unreachable");
  }
}

// returns true iff model changed
bool Model::addBaseSet(const std::string &baseSet, const Event &event) {
  const auto newReason = event.reason();
  assert(newReason != nullptr);
  if (!baseSetContains(baseSet, event.event())) {
    baseSets[baseSet].insert(event);
  } else if (!baseSets.at(baseSet).find(event)->updateReason(newReason)) {
    return false;
  }

  // propagate change to identity closure
  for (const auto &identity : identities) {
    if (const auto derivedEvent = event.image(identity)) {
      addBaseSet(baseSet, derivedEvent.value());
    }
    if (const auto derivedEvent = event.domain(identity)) {
      addBaseSet(baseSet, derivedEvent.value());
    }
  }
  return true;
}

bool Model::addBaseRelation(const std::string &baseRelation, const Edge &edge) {
  const auto newReason = edge.reason();
  assert(newReason != nullptr);
  if (!baseRelationContains(baseRelation, edge.from(), edge.to())) {
    baseRelations[baseRelation].insert(edge);
  } else if (!baseRelations.at(baseRelation).find(edge)->updateReason(newReason)) {
    return false;
  }

  // propagate change to identity closure
  for (const auto &identity : identities) {
    if (const auto derivedEdge = edge.compose(identity)) {
      addBaseRelation(baseRelation, derivedEdge.value());
    }
    if (const auto derivedEvent = identity.compose(edge)) {
      addBaseRelation(baseRelation, derivedEvent.value());
    }
  }
  return true;
}

// returns true iff model changed
bool Model::addIdentity(const Edge &edge) {
  if (edge.from() == edge.to()) {
    return false;
  }

  const auto newReason = edge.reason();
  assert(newReason != nullptr);
  if (!containsIdentity(edge.from(), edge.to())) {
    identities.insert(edge);
  } else if (!identities.find(edge)->updateReason(newReason)) {
    return false;
  }
  assert_void([&] { this->validate(); });

  // insert converse edge (identity is symmetic)
  addIdentity(edge.converse());

  // propagate change to identity closure (identity is transitive)
  // TODO: use delta
  const auto identitiesCopy = identities;
  for (const auto &identity : identitiesCopy) {
    if (const auto derivedEdge = edge.compose(identity)) {
      addIdentity(derivedEdge.value());
    }
    if (const auto derivedEdge = identity.compose(edge)) {
      addIdentity(derivedEdge.value());
    }
  }
  assert_void([&] { this->validate(); });

  // propagate base relations
  const auto baseRelationsCopy = baseRelations;
  for (const auto &[baseRelation, value] : baseRelationsCopy) {
    for (const auto &baseEdge : value) {
      if (const auto derivedEdge = edge.compose(baseEdge)) {
        addBaseRelation(baseRelation, derivedEdge.value());
      }
      assert(baseEdge.reason() != nullptr);
      if (const auto derivedEdge = baseEdge.compose(edge)) {
        addBaseRelation(baseRelation, derivedEdge.value());
      }
    }
  }

  // propagate base sets
  const auto baseSetsCopy = baseSets;
  for (const auto &[baseSet, value] : baseSetsCopy) {
    for (const auto &event : value) {
      if (const auto derivedEvent = event.image(edge)) {
        addBaseSet(baseSet, derivedEvent.value());
      }
      if (const auto derivedEvent = event.domain(edge)) {
        addBaseSet(baseSet, derivedEvent.value());
      }
    }
  }
  return true;
}

std::optional<CanonicalSet> Model::getReason(const std::string &baseSet,
                                             const EventType event) const {
  if (!baseSets.contains(baseSet)) {
    return std::nullopt;
  }
  const auto it = baseSets.at(baseSet).find(Event(event));
  if (it == baseSets.at(baseSet).end()) {
    return std::nullopt;
  }
  return std::optional<CanonicalSet>(it->reason());
}

std::optional<CanonicalRelation> Model::getReason(const std::string &baseRelation,
                                                  const EventType from, const EventType to) const {
  if (!baseRelations.contains(baseRelation)) {
    return std::nullopt;
  }
  const auto it = baseRelations.at(baseRelation).find(Edge(from, to));
  if (it == baseRelations.at(baseRelation).end()) {
    return std::nullopt;
  }
  return std::optional<CanonicalRelation>(it->reason());
}

std::optional<CanonicalRelation> Model::getReason(const EventType from, const EventType to) const {
  const auto it = identities.find(Edge(from, to));
  if (it == identities.end()) {
    return std::nullopt;
  }
  return std::optional<CanonicalRelation>(it->reason());
}

bool Model::baseSetContains(const std::string &baseSet, const EventType event) const {
  return baseSets.contains(baseSet) && baseSets.at(baseSet).contains(Event(event));
}

bool Model::baseRelationContains(const std::string &baseRelation, const EventType from,
                                 const EventType to) const {
  return baseRelations.contains(baseRelation) &&
         baseRelations.at(baseRelation).contains(Edge(from, to));
}

bool Model::containsIdentity(const EventType from, const EventType to) const {
  return identities.contains(Edge(from, to));
}

void Model::exportInternalModel(const std::string &filename) const {
  std::ofstream counterexamleModel("./output/" + filename + ".dot");
  counterexamleModel << "digraph { node[shape=\"circle\",margin=0]\n";

  // export events + set memberships
  for (const auto event : events) {
    counterexamleModel << "N" << event << "[label = \"";
    // set memberships
    for (const auto &[baseSet, satEvents] : baseSets) {
      if (satEvents.contains(Event(event))) {
        counterexamleModel << baseSet << " ";
      }
    }
    counterexamleModel << "\", tooltip=\"";
    counterexamleModel << "event: " << event << "\n";
    counterexamleModel << "\"];";
  }

  // export edges
  for (const auto &[baseRelation, edges] : baseRelations) {
    for (const auto &edge : edges) {
      const auto from = edge.from();
      const auto to = edge.to();

      counterexamleModel << "N" << from << " -> N" << to;
      counterexamleModel << "[label = \"" << baseRelation;
      counterexamleModel << "\", tooltip=\"";
      counterexamleModel << "reason: " << edge.reason()->toString() << "\n";
      counterexamleModel << "\"];\n";
    }
  }

  // export identities
  for (const auto &edge : identities) {
    const auto from = edge.from();
    const auto to = edge.to();

    counterexamleModel << "N" << from << " -> N" << to;
    counterexamleModel << "[label = \""
                       << "id";
    counterexamleModel << "\", tooltip=\"";
    counterexamleModel << "reason: " << edge.reason()->toString() << "\n";
    counterexamleModel << "\"];\n";
  }

  counterexamleModel << "}" << '\n';
  counterexamleModel.close();
}

void Model::exportModel(const std::string &filename) const {
  std::ofstream counterexamleModel("./output/" + filename + ".dot");
  counterexamleModel << "digraph { node[shape=\"circle\",margin=0]\n";

  // export events + set memberships
  for (const auto event : events) {
    // skip non minimal representative
    const auto eventClass = getEquivalenceClass(event);
    const auto minRepresentative = *std::ranges::min_element(eventClass);
    if (minRepresentative != event) {
      continue;
    }

    counterexamleModel << "N" << event << "[label = \"";
    // set memberships
    for (const auto &[baseSet, satEvents] : baseSets) {
      if (satEvents.contains(Event(event))) {
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
  for (const auto &[baseRelation, edges] : baseRelations) {
    for (const auto &edge : edges) {
      const auto from = edge.from();
      const auto to = edge.to();
      // skip edge containing non minimal representative
      const auto fromClass = getEquivalenceClass(from);
      const auto toClass = getEquivalenceClass(to);
      const auto minFromRep = *std::ranges::min_element(fromClass);
      const auto minToRep = *std::ranges::min_element(toClass);
      if (minFromRep != from || minToRep != to) {
        continue;
      }

      counterexamleModel << "N" << from << " -> N" << to;
      counterexamleModel << "[label = \"" << baseRelation;
      counterexamleModel << "\", tooltip=\"";
      counterexamleModel << "reason: " << edge.reason()->toString() << "\n";
      counterexamleModel << "\"];\n";
    }
  }

  counterexamleModel << "}" << '\n';
  counterexamleModel.close();
}

void Model::validate() const {
  // all base relations have a reason
  for (const auto &value : baseRelations | std::views::values) {
    assert(std::ranges::all_of(value, [](const Edge &edge) { return edge.reason() != nullptr; }));
  }
}

// returns true iff model changed
bool saturateIdAssumptions(Model &model) {
  assert_void([&] { model.validate(); });
  bool modelChanged = false;

  for (const auto &idAssumption : Assumption::idAssumptions) {
    // evaluate lhs of assumption
    const auto exprValue = model.evaluate(idAssumption.relation);
    for (const auto &edge : exprValue->getRelValue()) {
      assert_void([&] { model.validate(); });
      modelChanged |= model.addIdentity(edge);
    }
  }
  return modelChanged;
}

// returns true iff model changed
bool saturateBaseRelationAssumptions(Model &model) {
  bool modelChanged = false;

  for (const auto &[baseRelation, baseAssumption] : Assumption::baseAssumptions) {
    const auto exprValue = model.evaluate(baseAssumption.relation);
    for (const auto &edge : exprValue->getRelValue()) {
      modelChanged |= model.addBaseRelation(baseRelation, edge);
    }
  }
  return modelChanged;
}

// returns true iff model changed
bool saturateBaseSetAssumptions(Model &model) {
  bool modelChanged = false;

  for (const auto &[baseSet, baseAssumption] : Assumption::baseSetAssumptions) {
    const auto exprValue = model.evaluate(baseAssumption.set);
    for (const auto &event : exprValue->getSetValue()) {
      modelChanged |= model.addBaseSet(baseSet, event);
    }
  }
  return modelChanged;
}

void saturateModel(Model &model) {
  bool modelChanged = true;
  while (modelChanged) {
    assert_void([&] { model.validate(); });
    modelChanged = saturateIdAssumptions(model) | saturateBaseRelationAssumptions(model) |
                   saturateBaseSetAssumptions(model);
  }
}