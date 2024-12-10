#pragma once

template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> Annotated::getLeft(
    const LeafAnnotatedSet<AnnotationType>& LeafAnnotatedSet) {
  const auto [set, annotation] = LeafAnnotatedSet;
  assert(set->leftOperand != nullptr);
  return {set->leftOperand, annotation->getLeft()};
}

template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> Annotated::getRightSet(
    const LeafAnnotatedSet<AnnotationType>& LeafAnnotatedSet) {
  const auto& [set, annotation] = LeafAnnotatedSet;
  switch (set->operation) {
    case SetOperation::setUnion:
    case SetOperation::setIntersection:
      return {set->rightOperand, annotation->getRight()};
    case SetOperation::domain:
    case SetOperation::image:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::event:
    default:
      throw std::logic_error("unreachable");
  }
}

template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> Annotated::getRightRelation(
    const LeafAnnotatedSet<AnnotationType>& LeafAnnotatedSet) {
  const auto& [set, annotation] = LeafAnnotatedSet;
  switch (set->operation) {
    case SetOperation::domain:
    case SetOperation::image:
      return {set->relation, annotation->getRight()};
    case SetOperation::setUnion:
    case SetOperation::setIntersection:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::event:
    default:
      throw std::logic_error("unreachable");
  }
}

template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> Annotated::getLeftSet(
    const LeafAnnotatedRelation<AnnotationType>& LeafAnnotatedRelation) {
  const auto [relation, annotation] = LeafAnnotatedRelation;
  switch (relation->operation) {
    case RelationOperation::setIdentity:
      // On unary operators we simulate the left move by doing nothing!
      return {relation->set, annotation};
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion:
    case RelationOperation::cartesianProduct:
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    default:
      throw std::logic_error("unreachable");
  }
}

template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> Annotated::getLeftRelation(
    const LeafAnnotatedRelation<AnnotationType>& LeafAnnotatedRelation) {
  const auto [relation, annotation] = LeafAnnotatedRelation;
  switch (relation->operation) {
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      // On unary operators we simulate the left move by doing nothing!
      return {relation->leftOperand, annotation};
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion:
      return {relation->leftOperand, annotation->getLeft()};
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    case RelationOperation::setIdentity:
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    default:
      throw std::logic_error("unreachable");
  }
}

template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> Annotated::getRight(
    const LeafAnnotatedRelation<AnnotationType>& LeafAnnotatedRelation) {
  const auto [relation, annotation] = LeafAnnotatedRelation;
  assert(relation->rightOperand != nullptr);
  return {relation->rightOperand, annotation->getRight()};
}

template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> Annotated::newSet(const SetOperation operation,
                                                   const LeafAnnotatedSet<AnnotationType>& left,
                                                   const LeafAnnotatedSet<AnnotationType>& right) {
  return {Set::newSet(operation, left.first, right.first),
          LeafAnnotation<AnnotationType>::joinAnnotation(left.second, right.second)};
}

template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> Annotated::newSet(
    const SetOperation operation, const LeafAnnotatedSet<AnnotationType>& left,
    const LeafAnnotatedRelation<AnnotationType>& relation) {
  return {Set::newSet(operation, left.first, relation.first),
          LeafAnnotation<AnnotationType>::joinAnnotation(left.second, relation.second)};
}

template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> Annotated::newEvent(const int label) {
  return {Set::newEvent(label), LeafAnnotation<AnnotationType>::newLeaf({})};
}

template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> Annotated::newBaseSet(const std::string& identifier) {
  return {Set::newBaseSet(identifier), LeafAnnotation<AnnotationType>::newLeaf({})};
}

template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> Annotated::newSetIdentity(
    const LeafAnnotatedSet<AnnotationType>& left) {
  return {Relation::setIdentity(left.first), left.second};
}

template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> Annotated::newRelation(
    const RelationOperation operation, const LeafAnnotatedRelation<AnnotationType>& left) {
  return {Relation::newRelation(operation, left.first), left.second};
}

template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> Annotated::newRelation(
    const RelationOperation operation, const LeafAnnotatedRelation<AnnotationType>& left,
    const LeafAnnotatedRelation<AnnotationType>& right) {
  return {Relation::newRelation(operation, left.first, right.first),
          LeafAnnotation<AnnotationType>::joinAnnotation(left.second, right.second)};
}

template <typename AnnotationType>
CanonicalLeafAnnotation<AnnotationType> Annotated::join(CanonicalLeafAnnotation<AnnotationType> a,
                                                        CanonicalLeafAnnotation<AnnotationType> b) {
  if (!a->hasValue()) {
    return b;
  }

  if (!b->hasValue()) {
    return a;
  }

  if (a->isLeaf() && b->isLeaf()) {
    const auto joinValue = joinValues(a->getOptionalValue(), b->getOptionalValue());
    return joinValue.has_value() ? LeafAnnotation<AnnotationType>::newLeaf(joinValue.value())
                                 : LeafAnnotation<AnnotationType>::newLeaf({});
  }

  const auto aLeft = a->getLeft();
  const auto bLeft = b->getLeft();
  auto newLeft = Annotated::join(aLeft, bLeft);

  const auto aRight = a->getRight();
  const auto bRight = b->getRight();
  auto newRight = Annotated::join(aRight, bRight);
  return LeafAnnotation<AnnotationType>::joinAnnotation(newLeft, newRight);
}