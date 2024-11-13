#include "InterpretationTree.h"

template <typename ExprType>  // Event or Edge
bool insertOrUpdateReason(SetContainerType<ExprType> &set, const ExprType &element) {
  auto existingElementIt = set.find(element);
  if (existingElementIt == set.end()) {
    set.insert(element);
    return true;
  }
  ExprType existingElement = *existingElementIt;
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

InterpretationPtr Interpretation::transitiveClosure(InterpretationPtr left,
                                                    const EventSet &events) {
  assert(left != nullptr);
  const auto underlying = left->getRelValue();

  RelationValue refltransClosure = underlying;
  std::unordered_map<EventOrEdge, Event> projectedEvent;

  // iterate underlying
  RelationValue newPairs;
  std::unordered_map<EventOrEdge, Event> newProjectedEvent;
  while (true) {
    for (const auto &closureEdge : refltransClosure) {
      for (const auto &rEdge : underlying) {
        const auto composedEdge = closureEdge.compose(rEdge);
        if (composedEdge && insertOrUpdateReason(newPairs, composedEdge.value())) {
          newProjectedEvent.insert({composedEdge.value(), Event(closureEdge.to())});
        }
      }
    }
    if (newPairs.empty()) {
      break;
    }
    auto fixpointReached = true;
    for (const auto &newEdge : newPairs) {
      if (insertOrUpdateReason(refltransClosure, newEdge)) {
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
  // insert id
  for (const auto event : events) {
    refltransClosure.emplace(event, event, Relation::idRelation());
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
