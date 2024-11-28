#include "InterpretationTree.h"

#include <iostream>

#include "../annotations/ReasonAnnotation.h"

template <typename ExprType>  // Event or Edge
bool insertOrUpdateReason(SetContainerType<ExprType> &set, const ExprType &element) {
  auto existingElementIt = set.find(element);
  if (existingElementIt == set.end()) {
    set.insert(element);
    return true;
  }
  ExprType &existingElement = *existingElementIt;
  return existingElement.updateReason(element.reason());
}

std::string Interpretation::toString() const {
  std::string output;
  output += "{";
  if (std::holds_alternative<SetValue>(value)) {
    for (const auto &event : getSetValue()) {
      output += std::to_string(event.event()) + ",";
    }
  } else {
    for (const auto &edge : getRelValue()) {
      output += "(" + std::to_string(edge.from()) + "," + std::to_string(edge.to()) + ")";
    }
  }
  output += "}";
  return output;
}

InterpretationPtr Interpretation::relationIntersection(InterpretationPtr left,
                                                       InterpretationPtr right) {
  RelationValue intersect;
  for (const auto &lEdge : left->getRelValue()) {
    if (right->getRelValue().contains(lEdge)) {
      const auto rEdge = *right->getRelValue().find(lEdge);
      const auto intersectEdge = lEdge.intersect(rEdge).value();
      insertOrUpdateReason(intersect, intersectEdge);
    }
  }
  return std::make_unique<Interpretation>(intersect, std::move(left), std::move(right));
}

InterpretationPtr Interpretation::relationUnion(InterpretationPtr left, InterpretationPtr right) {
  RelationValue relunion = left->getRelValue();
  std::unordered_map<EventOrEdge, bool> isLeftWitness;
  for (const auto &edge : left->getRelValue()) {
    isLeftWitness.insert({edge, true});
  }
  for (const auto &edge : right->getRelValue()) {
    if (insertOrUpdateReason(relunion, edge)) {
      isLeftWitness[edge] = false;
    }
  }
  return std::make_unique<Interpretation>(relunion, std::move(left), std::move(right),
                                          std::move(isLeftWitness));
}

InterpretationPtr Interpretation::composition(InterpretationPtr left, InterpretationPtr right) {
  RelationValue composition;
  std::unordered_map<EventOrEdge, Event> projectedEvent;
  for (const auto &l : left->getRelValue()) {
    for (const auto &r : right->getRelValue()) {
      if (const auto composedEdge = l.compose(r)) {
        if (insertOrUpdateReason(composition, composedEdge.value())) {
          projectedEvent.insert({composedEdge.value(), Event(l.to())});
        }
      }
    }
  }
  return std::make_unique<Interpretation>(composition, std::move(left), std::move(right),
                                          std::move(projectedEvent));
}

CanonicalRelation computeTransitiveReason(const RelationsSet &underlyingReasons) {
  // compute transitive reason
  // overwrite reasons for all new edges
  // we could be more precise (use different reasons for different edges)
  CanonicalRelation unionReason = nullptr;
  for (const auto &reason : underlyingReasons) {
    if (unionReason == nullptr) {
      unionReason = reason;
      continue;
    }
    unionReason = Relation::newRelation(RelationOperation::relationUnion, unionReason, reason);
  }
  return Relation::newRelation(RelationOperation::transitiveClosure, unionReason);
}

bool insertOrUpdateReason(std::unordered_map<Edge, RelationsSet> &edgeToUnderlyingReasons,
                          const Edge &edge, const RelationsSet &newReason) {
  if (!edgeToUnderlyingReasons.contains(edge)) {
    edgeToUnderlyingReasons[edge] = newReason;
    return true;
  }

  auto newReasonExpr = computeTransitiveReason(newReason);
  auto oldReasonExpr = computeTransitiveReason(edgeToUnderlyingReasons.at(edge));
  if (newReasonExpr->isSmallerReason(oldReasonExpr)) {
    edgeToUnderlyingReasons[edge] = newReason;
    return true;
  }
  return false;
}

InterpretationPtr Interpretation::transitiveClosure(InterpretationPtr left,
                                                    const EventSet &events) {
  assert(left != nullptr);
  const auto underlying = left->getRelValue();

  RelationValue refltransClosure;
  std::unordered_map<EventOrEdge, Event> projectedEvent;
  if (!underlying.empty()) {
    // - a reason is conceptually an expression (r1 | r2 | ...)^* where ri's are reasons of an
    // underlying edge needed to justify the edge
    // - to be able to calculate minimal reasons we keep the ri's as a set
    // - then reason composition becomes set union
    // - to compare the size of reasons we use computeTransitiveReason (which computes the reason
    // from a set of ri's) and compare the resulting expressions
    std::unordered_map<Edge, RelationsSet> edgeToUnderlyingReasons;
    for (const auto &edge : underlying) {
      edgeToUnderlyingReasons[edge] = {edge.reason()};
    }

    // iterate underlying
    std::unordered_map<Edge, RelationsSet> newPairs;
    std::unordered_map<EventOrEdge, Event> newProjectedEvent;

    while (true) {
      for (const auto &[closureEdge, closureEdgeReason] : edgeToUnderlyingReasons) {
        for (const auto &rEdge : underlying) {
          const auto composedEdge = closureEdge.compose(rEdge);
          auto unionReason = closureEdgeReason;
          unionReason.insert(rEdge.reason());
          // if (composedEdge->from() == 0 & composedEdge->to() == 1) {
          //   std::cout << computeTransitiveReason(unionReason)->toString() << std::endl;
          // }
          if (composedEdge && insertOrUpdateReason(newPairs, composedEdge.value(), unionReason)) {
            newProjectedEvent.insert({composedEdge.value(), Event(closureEdge.to())});
          }
        }
      }
      if (newPairs.empty()) {
        break;
      }
      auto fixpointReached = true;
      for (auto &[newEdge, newEdgeReason] : newPairs) {
        if (insertOrUpdateReason(edgeToUnderlyingReasons, newEdge, newEdgeReason)) {
          projectedEvent.insert({newEdge, newProjectedEvent.at(newEdge)});
          fixpointReached = false;
        }
      }
      if (fixpointReached) {
        break;
      }
      // clear
      newPairs.clear();
      newProjectedEvent.clear();
    }
    for (auto &[edge, reason] : edgeToUnderlyingReasons) {
      auto edgeCopy = edge;
      edgeCopy.resetReason();
      edgeCopy.updateReason(computeTransitiveReason(reason));
      refltransClosure.insert(edgeCopy);
    }
  }
  // insert id
  for (const auto event : events) {
    insertOrUpdateReason(refltransClosure, {event, event, Relation::idRelation()});
  }
  return std::make_unique<Interpretation>(refltransClosure, std::move(left), nullptr,
                                          std::move(projectedEvent));
}

InterpretationPtr Interpretation::setIntersection(InterpretationPtr left, InterpretationPtr right) {
  SetValue intersect;
  for (const auto &lEvent : left->getSetValue()) {
    if (right->getSetValue().contains(lEvent)) {
      const auto rEvent = *right->getSetValue().find(lEvent);
      const auto intersectEvent = lEvent.intersect(rEvent).value();
      insertOrUpdateReason(intersect, intersectEvent);
    }
  }
  return std::make_unique<Interpretation>(intersect, std::move(left), std::move(right));
}

InterpretationPtr Interpretation::setUnion(InterpretationPtr left, InterpretationPtr right) {
  SetValue setunion = left->getSetValue();
  std::unordered_map<EventOrEdge, bool> isLeftWitness;
  for (const auto &event : left->getSetValue()) {
    isLeftWitness.insert({event, true});
  }
  for (const auto &event : right->getSetValue()) {
    if (insertOrUpdateReason(setunion, event)) {
      isLeftWitness[event] = false;
    }
  }
  return std::make_unique<Interpretation>(setunion, std::move(left), std::move(right),
                                          std::move(isLeftWitness));
}

InterpretationPtr Interpretation::image(InterpretationPtr left, InterpretationPtr right) {
  const auto &setValue = left->getSetValue();
  const auto &relationValue = right->getRelValue();

  SetValue image;
  std::unordered_map<EventOrEdge, Event> projectedEvent;
  for (const auto &edge : relationValue) {
    const auto it = setValue.find(Event(edge.from()));
    if (it != setValue.end()) {
      const auto imageEvent = it->image(edge).value();
      if (insertOrUpdateReason(image, imageEvent)) {
        projectedEvent.insert({imageEvent, *it});
      }
    }
  }
  return std::make_unique<Interpretation>(image, std::move(left), std::move(right),
                                          std::move(projectedEvent));
}

InterpretationPtr Interpretation::domain(InterpretationPtr left, InterpretationPtr right) {
  const auto &setValue = left->getSetValue();
  const auto &relationValue = right->getRelValue();

  SetValue domain;
  std::unordered_map<EventOrEdge, Event> projectedEvent;
  for (const auto &edge : relationValue) {
    const auto it = setValue.find(Event(edge.to()));
    if (it != setValue.end()) {
      const auto domainEvent = it->domain(edge).value();
      if (insertOrUpdateReason(domain, domainEvent)) {
        projectedEvent.insert({domainEvent, *it});
      }
    }
  }
  return std::make_unique<Interpretation>(domain, std::move(left), std::move(right),
                                          std::move(projectedEvent));
}
