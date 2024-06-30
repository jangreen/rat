
#include "Annotated.h"

namespace Annotated {

AnnotatedSet getLeft(const AnnotatedSet &annotatedSet) {
  const auto [set, annotation] = annotatedSet;
  assert (set->leftOperand != nullptr);
  const auto leftSet = set->leftOperand;
  const auto leftAnnotation = set->operation != SetOperation::domain ?
    annotation->getLeft() : annotation->getRight();

  return {leftSet, leftAnnotation};
}

std::variant<AnnotatedSet, AnnotatedRelation> getRight(const AnnotatedSet &annotatedSet) {
  const auto [set, annotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::choice:
    case SetOperation::intersection:
      return AnnotatedSet(set->rightOperand, annotation->getRight());
    case SetOperation::domain:
      return AnnotatedRelation(set->relation, annotation->getRight());
    case SetOperation::image:
      return AnnotatedRelation(set->relation, annotation->getLeft());
    case SetOperation::base:
    case SetOperation::empty:
    case SetOperation::full:
    case SetOperation::singleton:
    default:
      throw std::logic_error("unreachable");
  }
}

AnnotatedSet makeWithValue(const CanonicalSet set, const AnnotationType value) {
  switch (set->operation) {
    case SetOperation::singleton:
    case SetOperation::empty:
    case SetOperation::full:
    case SetOperation::base:
      return {set, Annotation::none()};
    case SetOperation::choice:
    case SetOperation::intersection: {
      const auto [_1, left] = makeWithValue(set->leftOperand, value);
      const auto [_2, right] = makeWithValue(set->rightOperand, value);
      return {set, Annotation::newAnnotation(left, right)};
    }
    case SetOperation::domain: {
      const auto [_1, left] = makeWithValue(set->relation, value);
      const auto [_2, right] = makeWithValue(set->leftOperand, value);
      return {set, Annotation::newAnnotation(left, right)};
    }
    case SetOperation::image: {
      const auto [_1, left] = makeWithValue(set->leftOperand, value);
      const auto [_2, right] = makeWithValue(set->relation, value);
      return {set, Annotation::newAnnotation(left, right)};
    }
    default:
      throw std::logic_error("unreachable");
  }
}

AnnotatedRelation makeWithValue(const CanonicalRelation relation, const AnnotationType value) {
  switch (relation->operation) {
    case RelationOperation::identity:
    case RelationOperation::empty:
    case RelationOperation::full:
      return {relation, Annotation::none()};
    case RelationOperation::intersection:
    case RelationOperation::composition:
    case RelationOperation::choice: {
      const auto [_1, left] = makeWithValue(relation->leftOperand, value);
      const auto [_2, right] = makeWithValue(relation->rightOperand, value);
      return {relation, Annotation::newAnnotation(left, right)};
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      return makeWithValue(relation->leftOperand, value);
    }
    case RelationOperation::base:
      return {relation, Annotation::newLeaf(value)};
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

AnnotatedSet make(const SetOperation operation, const AnnotatedSet &left,
                                         const AnnotatedSet &right) {
  assert(operation == SetOperation::intersection); // TODO: WHY???
  auto newSet = Set::newSet(operation, left.first, right.first);
  auto newAnnot = Annotation::newAnnotation(left.second, right.second);
  return {newSet, newAnnot };
}

AnnotatedSet make(SetOperation operation, const AnnotatedSet &annotatedSet,
                                         const AnnotatedRelation &annotatedRelation) {
  assert(operation == SetOperation::image || operation == SetOperation::domain);
  auto newSet = Set::newSet(operation, annotatedSet.first, annotatedRelation.first);
  auto newAnnot = Annotation::newAnnotation(annotatedSet.second, annotatedRelation.second);
  return {newSet, newAnnot };
}

}