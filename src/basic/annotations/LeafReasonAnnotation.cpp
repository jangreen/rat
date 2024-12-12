#include "LeafAnnotated.h"
#include "LeafAnnotation.h"
#include "ReasonAnnotation.h"

template <>
std::optional<Reasons> joinValues(const std::optional<Reasons> &a,
                                  const std::optional<Reasons> &b) {
  if (!a.has_value()) {
    return b;
  }
  if (!b.has_value()) {
    return a;
  }

  Reasons join;
  for (const auto &expr : a.value()) {
    join.emplace(expr);
  }
  for (const auto &expr : b.value()) {
    join.emplace(expr);
  }
  return join;
}

template <>
std::string annotationTypetoString(const Reasons &value) {
  std::string output;
  output += "{";
  for (const auto &expr : value) {
    if (std::holds_alternative<CanonicalSet>(expr)) {
      const auto set = std::get<CanonicalSet>(expr);
      output += set->toString() + ", ";
    } else {
      const auto relation = std::get<CanonicalRelation>(expr);
      output += relation->toString() + ", ";
    }
  }
  output += "}";
  return output;
}

// explicit instantiation of template class LeafAnnotation
// needs specialization of joinValues for Reasons
template class LeafAnnotation<Reasons>;

CanonicalLeafAnnotation<Reasons> annotateReasonsHelper(const CanonicalRelation relation,
                                                       const InterpretationPtr &interpretation,
                                                       const Edge &tracedValue) {
  switch (relation->operation) {
    case RelationOperation::relationIntersection: {
      const auto newLeft =
          annotateReasonsHelper(relation->leftOperand, interpretation->getLeft(), tracedValue);
      const auto newRight =
          annotateReasonsHelper(relation->rightOperand, interpretation->getRight(), tracedValue);
      // assert(Annotated::validate({relation->leftOperand, newLeft}));
      // assert(Annotated::validate({relation->rightOperand, newRight}));
      return LeafAnnotation<Reasons>::joinAnnotation(newLeft, newRight);
    }
    case RelationOperation::relationUnion: {
      const auto left = interpretation->getLeft()->getRelValue();
      const auto right = interpretation->getRight()->getRelValue();
      const auto traceLeft = interpretation->traceLeft(tracedValue);

      if (traceLeft) {
        const auto newLeft =
            annotateReasonsHelper(relation->leftOperand, interpretation->getLeft(), tracedValue);
        const auto newRight = LeafAnnotation<Reasons>::newLeaf({});
        return LeafAnnotation<Reasons>::joinAnnotation(newLeft, newRight);
      }
      const auto newLeft = LeafAnnotation<Reasons>::newLeaf({});
      const auto newRight =
          annotateReasonsHelper(relation->rightOperand, interpretation->getRight(), tracedValue);
      // assert(Annotated::validate({relation->rightOperand, newRight}));
      return LeafAnnotation<Reasons>::joinAnnotation(newLeft, newRight);
    }
    case RelationOperation::composition: {
      const auto left = interpretation->getLeft()->getRelValue();
      const auto right = interpretation->getRight()->getRelValue();
      const auto projEvent = interpretation->getProjectedEvent(tracedValue);
      const auto newLeft = annotateReasonsHelper(relation->leftOperand, interpretation->getLeft(),
                                                 Edge(tracedValue.from(), projEvent.event()));
      const auto newRight =
          annotateReasonsHelper(relation->rightOperand, interpretation->getRight(),
                                Edge(projEvent.event(), tracedValue.to()));
      return LeafAnnotation<Reasons>::joinAnnotation(newLeft, newRight);
    }
    case RelationOperation::converse: {
      return annotateReasonsHelper(relation->leftOperand, interpretation->getLeft(),
                                   tracedValue.converse());
    }
    case RelationOperation::setIdentity: {
      const auto from = tracedValue.from();
      const auto to = tracedValue.to();
      if (from != to) {
        throw std::logic_error("unreachable");
      }
      return annotateReasonsHelper(relation->set, interpretation->getLeft(), Event(from));
    }
    case RelationOperation::transitiveClosure: {
      if (tracedValue.from() == tracedValue.to()) {
        return LeafAnnotation<Reasons>::newLeaf({});
      }
      const auto underlying = interpretation->getLeft()->getRelValue();
      if (underlying.contains(tracedValue)) {
        return annotateReasonsHelper(relation->leftOperand, interpretation->getLeft(), tracedValue);
      }

      // construct join of all annotations for each iteration
      const auto projEvent = interpretation->getProjectedEvent(tracedValue);
      const auto left = annotateReasonsHelper(relation, interpretation,
                                              Edge(tracedValue.from(), projEvent.event()));
      const auto right = annotateReasonsHelper(relation, interpretation,
                                               Edge(projEvent.event(), tracedValue.to()));
      return Annotated::join(left, right);
    }
    case RelationOperation::baseRelation: {
      const auto &baseRelationValue = interpretation->getRelValue();
      const auto valueIt = baseRelationValue.find(tracedValue);
      assert(valueIt != baseRelationValue.end());
      const auto edge = *valueIt;
      // only annotate if non-trivial
      if (edge.reason() == relation) {
        return LeafAnnotation<Reasons>::newLeaf({});
      }
      return LeafAnnotation<Reasons>::newLeaf({edge.reason()});
    }
    case RelationOperation::idRelation:
    case RelationOperation::fullRelation:
      return LeafAnnotation<Reasons>::newLeaf({});
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    case RelationOperation::emptyRelation:
    default:
      throw std::logic_error("unreachable");
  }
}

CanonicalLeafAnnotation<Reasons> annotateReasonsHelper(const CanonicalSet set,
                                                       const InterpretationPtr &interpretation,
                                                       const Event &tracedValue) {
  switch (set->operation) {
    case SetOperation::setIntersection: {
      const auto aLeft =
          annotateReasonsHelper(set->leftOperand, interpretation->getLeft(), tracedValue);
      const auto aRight =
          annotateReasonsHelper(set->rightOperand, interpretation->getRight(), tracedValue);
      // assert(Annotated::validate({set->leftOperand, newLeft}));
      // assert(Annotated::validate({set->rightOperand, newRight}));
      return LeafAnnotation<Reasons>::joinAnnotation(aLeft, aRight);
    }
    case SetOperation::image:
    case SetOperation::domain: {
      const auto left = interpretation->getLeft()->getSetValue();
      const auto right = interpretation->getRight()->getRelValue();
      const auto proj = interpretation->getProjectedEvent(tracedValue);

      // currently we annotate also events
      // TODO: should we use id insertions?
      // currently we annotated reasons for identity at base relations occurring after events
      // we could insert explicit id relation and annotate it instead
      // if (set->isEvent()) {
      //   // join annotation of event into base relation annotation
      //   const auto aLeft = annotateReasonsHelper(set->leftOperand, interpretation->getLeft(),
      //   proj); const auto aRight = annotateReasonsHelper(set->relation,
      //   interpretation->getRight(),
      //                                             Edge(proj.event(), tracedValue.event()));
      //   const auto joinedValue = joinValues<Reasons>(aLeft->getValue(), aRight->getValue());
      //   return LeafAnnotation<Reasons>::newLeaf(joinedValue ? joinedValue.value() : Reasons{});
      // }

      // otherwise annotate recursive
      const auto aLeft = annotateReasonsHelper(set->leftOperand, interpretation->getLeft(), proj);
      const auto tracedValueRelation = set->operation == SetOperation::image
                                           ? Edge(proj.event(), tracedValue.event())
                                           : Edge(tracedValue.event(), proj.event());
      const auto aRight =
          annotateReasonsHelper(set->relation, interpretation->getRight(), tracedValueRelation);
      return LeafAnnotation<Reasons>::joinAnnotation(aLeft, aRight);
    }
    case SetOperation::baseSet: {
      const auto &baseSetValue = interpretation->getSetValue();
      const auto valueIt = baseSetValue.find(tracedValue);
      assert(valueIt != baseSetValue.end());
      const auto event = *valueIt;
      assert(event.reason() != nullptr);
      // only annotate if non-trivial
      if (event.reason() == set) {
        return LeafAnnotation<Reasons>::newLeaf({});
      }
      return LeafAnnotation<Reasons>::newLeaf({event.reason()});
    }
    case SetOperation::fullSet:
      return LeafAnnotation<Reasons>::newLeaf({});
    case SetOperation::event: {
      const auto value = *interpretation->getSetValue().find(tracedValue);
      // only annotate if non-trivial
      if (value.reason()->isEvent() && value.reason()->label.value() == tracedValue.event()) {
        return LeafAnnotation<Reasons>::newLeaf({});
      }
      assert(value.reason() != nullptr);
      return LeafAnnotation<Reasons>::newLeaf({value.reason()});
    }
    case SetOperation::setUnion: {
      const auto left = interpretation->getLeft()->getSetValue();
      const auto right = interpretation->getRight()->getSetValue();
      const auto traceLeft = interpretation->traceLeft(tracedValue);

      if (traceLeft) {
        const auto newLeft =
            annotateReasonsHelper(set->leftOperand, interpretation->getLeft(), tracedValue);
        const auto newRight = LeafAnnotation<Reasons>::newLeaf({});
        return LeafAnnotation<Reasons>::joinAnnotation(newLeft, newRight);
      }
      const auto newLeft = LeafAnnotation<Reasons>::newLeaf({});
      const auto newRight =
          annotateReasonsHelper(set->rightOperand, interpretation->getRight(), tracedValue);
      return LeafAnnotation<Reasons>::joinAnnotation(newLeft, newRight);
    }
    case SetOperation::emptySet:
    default:
      throw std::logic_error("unreachable");
  }
}

CanonicalLeafAnnotation<Reasons> annotateReasons(const CanonicalSet set,
                                                 const InterpretationPtr &interpretation) {
  const auto value = interpretation->getSetValue();
  if (value.empty()) {
    return LeafAnnotation<Reasons>::newLeaf({});
  }
  // find event with minimal reason
  std::optional<Event> minimalEvent = std::nullopt;
  for (const auto &event : value) {
    if (minimalEvent.has_value()) {
      if (event.reason()->isSmallerReason(minimalEvent->reason())) {
        minimalEvent = event;
      }
    } else {
      minimalEvent = event;
    }
  }
  const auto saturationAnnotation =
      annotateReasonsHelper(set, interpretation, minimalEvent.value());
  // TODO: assert(Annotated::validate({set, saturationAnnotation}));
  return saturationAnnotation;
}

// only subtitutes in set expressions
LeafAnnotatedSet<Reasons> substituteAll(const LeafAnnotatedSet<Reasons> &annotatedSet,
                                        const CanonicalSet search, const CanonicalSet replace) {
  const auto &[set, annotation] = annotatedSet;
  if (set == search) {
    return {replace, LeafAnnotation<Reasons>::newLeaf({})};
  }
  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return annotatedSet;
    case SetOperation::image:
    case SetOperation::domain: {
      const auto &left = substituteAll(Annotated::getLeft(annotatedSet), search, replace);
      const auto relation =
          substituteAll(Annotated::getRightRelation<Reasons>(annotatedSet), search, replace);
      return Annotated::newSet(set->operation, left, relation);
    }
    case SetOperation::setIntersection:
    case SetOperation::setUnion: {
      const auto &left = substituteAll(Annotated::getLeft(annotatedSet), search, replace);
      const auto &right =
          substituteAll(Annotated::getRightSet<Reasons>(annotatedSet), search, replace);
      return Annotated::newSet(set->operation, left, right);
    }
    default:
      throw std::logic_error("unreachable");
  }
}

LeafAnnotatedRelation<Reasons> substituteAll(
    const LeafAnnotatedRelation<Reasons> &annotatedRelation, CanonicalSet search,
    CanonicalSet replace) {
  const auto &[relation, annotation] = annotatedRelation;

  switch (relation->operation) {
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion: {
      const auto &left =
          substituteAll(Annotated::getLeftRelation<Reasons>(annotatedRelation), search, replace);
      const auto &right = substituteAll(Annotated::getRight(annotatedRelation), search, replace);
      return Annotated::newRelation(relation->operation, left, right);
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      const auto &left =
          substituteAll(Annotated::getLeftRelation<Reasons>(annotatedRelation), search, replace);
      return Annotated::newRelation(relation->operation, left);
    }
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      return annotatedRelation;
    case RelationOperation::setIdentity: {
      const auto &left =
          substituteAll(Annotated::getLeftSet<Reasons>(annotatedRelation), search, replace);
      return Annotated::newSetIdentity(left);
    }
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

LeafAnnotatedRelation<Reasons> substituteAll(
    const LeafAnnotatedRelation<Reasons> &annotatedRelation, const CanonicalRelation search,
    const CanonicalRelation replace) {
  const auto &[relation, annotation] = annotatedRelation;
  if (relation == search) {
    return {replace, LeafAnnotation<Reasons>::newLeaf({})};
  }

  switch (relation->operation) {
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion: {
      const auto &left =
          substituteAll(Annotated::getLeftRelation<Reasons>(annotatedRelation), search, replace);
      const auto &right = substituteAll(Annotated::getRight(annotatedRelation), search, replace);
      return Annotated::newRelation(relation->operation, left, right);
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      const auto &left =
          substituteAll(Annotated::getLeftRelation<Reasons>(annotatedRelation), search, replace);
      return Annotated::newRelation(relation->operation, left);
    }
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    case RelationOperation::setIdentity:
      return annotatedRelation;
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

LeafAnnotatedSet<Reasons> substituteAll(const LeafAnnotatedSet<Reasons> &annotatedSet,
                                        const CanonicalRelation search,
                                        const CanonicalRelation replace) {
  const auto &[set, annotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return annotatedSet;
    case SetOperation::setIntersection:
    case SetOperation::setUnion: {
      const auto &left = substituteAll(Annotated::getLeft(annotatedSet), search, replace);
      const auto &right =
          substituteAll(Annotated::getRightSet<Reasons>(annotatedSet), search, replace);
      return Annotated::newSet(set->operation, left, right);
    }
    case SetOperation::image:
    case SetOperation::domain: {
      const auto &left = substituteAll(Annotated::getLeft(annotatedSet), search, replace);
      const auto &right =
          substituteAll(Annotated::getRightRelation<Reasons>(annotatedSet), search, replace);
      return Annotated::newSet(set->operation, left, right);
    }
    default:
      throw std::logic_error("unreachable");
  }
}

LeafAnnotatedSet<Reasons> substitute(const LeafAnnotatedSet<Reasons> &annotatedSet,
                                     const CanonicalSet search, const CanonicalSet replace,
                                     int *n) {
  const auto &[set, annotation] = annotatedSet;
  assert(*n >= 0 && "Negative occurrence counter");
  if (*n == 0) {
    return annotatedSet;
  }

  if (set == search) {
    if (*n == 1) {
      return {replace, LeafAnnotation<Reasons>::newLeaf({})};
    }
    (*n)--;
    return annotatedSet;
  }
  if (set->leftOperand != nullptr) {
    const auto leftSub = substitute(Annotated::getLeft(annotatedSet), search, replace, n);
    if (leftSub.first != set->leftOperand) {
      switch (set->operation) {
        case SetOperation::setUnion:
        case SetOperation::setIntersection:
          return Annotated::newSet(set->operation, leftSub,
                                   Annotated::getRightSet<Reasons>(annotatedSet));
        case SetOperation::domain:
        case SetOperation::image:
          return Annotated::newSet(set->operation, leftSub,
                                   Annotated::getRightRelation<Reasons>(annotatedSet));
        case SetOperation::event:
        case SetOperation::emptySet:
        case SetOperation::fullSet:
        case SetOperation::baseSet:
        // leftOperand != nullptr
        default:
          throw std::logic_error("unreachable");
      }
    }
  }
  if (set->rightOperand != nullptr) {
    const auto rightSub =
        substitute(Annotated::getRightSet<Reasons>(annotatedSet), search, replace, n);
    if (rightSub.first != set->rightOperand) {
      switch (set->operation) {
        case SetOperation::setUnion:
        case SetOperation::setIntersection:
          return Annotated::newSet(set->operation, Annotated::getLeft(annotatedSet), rightSub);
        case SetOperation::domain:
        case SetOperation::image:
        case SetOperation::event:
        case SetOperation::emptySet:
        case SetOperation::fullSet:
        case SetOperation::baseSet:
        // rightOperand != nullptr
        default:
          throw std::logic_error("unreachable");
      }
    }
  }
  return annotatedSet;
}

bool validateReasons(const LeafAnnotatedRelation<Reasons> &annotatedRelation) {
  const auto &[relation, annotation] = annotatedRelation;
  switch (relation->operation) {
    case RelationOperation::relationIntersection:
    case RelationOperation::relationUnion:
    case RelationOperation::composition:
      assert(validateReasons(Annotated::getLeftRelation(annotatedRelation)));
      assert(validateReasons(Annotated::getRight(annotatedRelation)));
      return true;
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      assert(validateReasons(Annotated::getLeftRelation(annotatedRelation)));
      return true;
    case RelationOperation::setIdentity:
      assert(validateReasons(Annotated::getLeftSet(annotatedRelation)));
      return true;
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemneted");
    case RelationOperation::baseRelation:
      assert(std::ranges::all_of(annotation->getValue(), [](auto expr) {
        return std::holds_alternative<CanonicalRelation>(expr);
      }));
      return true;
    case RelationOperation::idRelation:
    case RelationOperation::fullRelation:
    case RelationOperation::emptyRelation:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
}

bool validateReasons(const LeafAnnotatedSet<Reasons> &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::image:
    case SetOperation::domain:
      assert(validateReasons(Annotated::getLeft(annotatedSet)));
      assert(validateReasons(Annotated::getRightRelation(annotatedSet)));
      return true;
    case SetOperation::setIntersection:
    case SetOperation::setUnion:
      assert(validateReasons(Annotated::getLeft(annotatedSet)));
      assert(validateReasons(Annotated::getRightSet(annotatedSet)));
      return true;
    case SetOperation::baseSet:
      assert(std::ranges::all_of(annotation->getValue(), [](auto expr) {
        return std::holds_alternative<CanonicalSet>(expr);
      }));
    case SetOperation::fullSet:
    case SetOperation::event:
      return true;
    case SetOperation::emptySet:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
}