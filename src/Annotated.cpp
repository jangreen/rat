
#include "Annotated.h"

namespace Annotated {

AnnotatedSet getLeft(const AnnotatedSet &annotatedSet) {
  const auto [set, annotation] = annotatedSet;
  assert(set->leftOperand != nullptr);
  const auto leftSet = set->leftOperand;
  const auto leftAnnotation =
      set->operation != SetOperation::domain ? annotation->getLeft() : annotation->getRight();

  return {leftSet, leftAnnotation};
}

std::variant<AnnotatedSet, AnnotatedRelation> getRight(const AnnotatedSet &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
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

AnnotatedSet substitute(const AnnotatedSet annotatedSet, const CanonicalSet search,
                        const CanonicalSet replace, int *n) {
  const auto &[set, annotation] = annotatedSet;
  if (*n == 0) {
    return annotatedSet;
  }

  if (set == search) {
    if (*n == 1 || *n == -1) {
      return Annotated::makeWithValue(replace, 0);
    }
    (*n)--;
    return annotatedSet;
  }
  if (set->leftOperand != nullptr) {
    const auto leftSub = substitute(getLeft(annotatedSet), search, replace, n);
    if (leftSub.first != set->leftOperand) {
      switch (set->operation) {
        case SetOperation::choice:
        case SetOperation::intersection:
          return Annotated::newSet(set->operation, leftSub,
                                   std::get<AnnotatedSet>(getRight(annotatedSet)));
        case SetOperation::domain:
        case SetOperation::image:
          return Annotated::newSet(set->operation, leftSub,
                                   std::get<AnnotatedRelation>(getRight(annotatedSet)));
        case SetOperation::singleton:
        case SetOperation::empty:
        case SetOperation::full:
        case SetOperation::base:
        // leftOperand != nullptr
        default:
          throw std::logic_error("unreachable");
      }
    }
  }
  if (set->rightOperand != nullptr) {
    const auto rightSub =
        substitute(std::get<AnnotatedSet>(getRight(annotatedSet)), search, replace, n);
    if (rightSub.first != set->rightOperand) {
      switch (set->operation) {
        case SetOperation::choice:
        case SetOperation::intersection:
          return Annotated::newSet(set->operation, getLeft(annotatedSet), rightSub);
        case SetOperation::domain:
        case SetOperation::image:
        case SetOperation::singleton:
        case SetOperation::empty:
        case SetOperation::full:
        case SetOperation::base:
        // rightOperand != nullptr
        default:
          throw std::logic_error("unreachable");
      }
    }
  }
  return annotatedSet;
}

}  // namespace Annotated