#include "Annotation.h"

#include <cassert>
#include <unordered_set>

namespace {

  std::optional<AnnotationType> meet(const std::optional<AnnotationType> a, const std::optional<AnnotationType> b) {
    if (!a.has_value() && !b.has_value()) {
      return std::nullopt;
    }
    return std::min(a.value_or(INT32_MAX), b.value_or(INT32_MAX));
  }

}

Annotation::Annotation(const std::optional<AnnotationType> value, const CanonicalAnnotation left,
                                                                  const CanonicalAnnotation right)
    : value(value), left(left), right(right) {}

CanonicalAnnotation Annotation::newAnnotation(std::optional<AnnotationType> value, CanonicalAnnotation left,
                                              CanonicalAnnotation right) {
  assert((left == nullptr) == (right == nullptr)); // Binary or leaf
  assert(value.has_value() || (left == nullptr && right == nullptr)); // No value => Leaf

  static std::unordered_set<Annotation> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(value, left, right);
  return &*iter;
}

CanonicalAnnotation Annotation::newLeaf(const std::optional<AnnotationType> value) {
  return newAnnotation(value, nullptr, nullptr);
}

CanonicalAnnotation Annotation::newAnnotation(const CanonicalAnnotation left, const CanonicalAnnotation right) {
  assert (left != nullptr && right != nullptr);
  if (left == right && left->isLeaf()) {
    // Two constant annotations (leafs) together form just the same constant annotation.
    return left;
  }

  const auto annotationValue = meet(left->getValue(), right->getValue());
  assert(annotationValue.has_value());
  return newAnnotation(annotationValue.value(), left, right);
}

bool Annotation::validate() const {
  // Binary or leaf
  bool isValid = ((left == nullptr) == (right == nullptr));
  // No value => Leaf
  isValid &= value.has_value() || isLeaf();
  // No leaf => value is meet of children.
  isValid &= isLeaf() || value.value() == meet(left->value, right->value);
  // TODO: Check recursively?
  return isValid;
}

std::string Annotation::toString() const {
  if (isLeaf()) {
    return value.has_value() ? std::to_string(value.value()) : "_";
  }
  return "(" + left->toString() + ")(" + right->toString() + ")";
}


// ==========================================================================================
// ========================== Helper for annotated sets/relations ===========================
// ==========================================================================================

CanonicalAnnotation Annotation::newAnnotation(const CanonicalSet set, const AnnotationType value) {
  switch (set->operation) {
    case SetOperation::singleton:
    case SetOperation::empty:
    case SetOperation::full:
    case SetOperation::base:
      return newLeaf(std::nullopt);
    case SetOperation::choice:
    case SetOperation::intersection: {
      const auto left = newAnnotation(set->leftOperand, value);
      const auto right = newAnnotation(set->rightOperand, value);
      return newAnnotation(left, right);
    }
    case SetOperation::domain: {
      const auto left = newAnnotation(set->relation, value);
      const auto right = newAnnotation(set->leftOperand, value);
      return newAnnotation(left, right);
    }
    case SetOperation::image: {
      auto left = newAnnotation(set->leftOperand, value);
      auto right = newAnnotation(set->relation, value);
      return newAnnotation(left, right);
    }
    default:
      throw std::logic_error("unreachable");
  }
}

CanonicalAnnotation Annotation::newAnnotation(CanonicalRelation relation, const AnnotationType value) {
  switch (relation->operation) {
    case RelationOperation::identity:
    case RelationOperation::empty:
    case RelationOperation::full:
      return newLeaf(std::nullopt);
    case RelationOperation::intersection:
    case RelationOperation::composition:
    case RelationOperation::choice: {
      const auto left = newAnnotation(relation->leftOperand, value);
      const auto right = newAnnotation(relation->rightOperand, value);
      return newAnnotation(left, right);
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      const auto inner = newAnnotation(relation->leftOperand, value);
      return inner;
    }
    case RelationOperation::base:
      return newLeaf(value);
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

AnnotatedSet Annotation::getLeft(const AnnotatedSet &annotatedSet) {
  const auto &set = std::get<CanonicalSet>(annotatedSet);
  const auto &annotation = std::get<CanonicalAnnotation>(annotatedSet);

  switch (set->operation) {
    case SetOperation::choice:
    case SetOperation::intersection:
    case SetOperation::image:
      return { set->leftOperand, annotation->getLeft() };
    case SetOperation::domain:
      return { set->leftOperand, annotation->getRight() };
    case SetOperation::base:
    case SetOperation::empty:
    case SetOperation::full:
    case SetOperation::singleton:
    default:
      throw std::logic_error("unreachable");
  }
}

std::variant<AnnotatedSet, AnnotatedRelation> Annotation::getRight(
    const AnnotatedSet &annotatedSet) {
  const auto &set = std::get<CanonicalSet>(annotatedSet);
  const auto &annotation = std::get<CanonicalAnnotation>(annotatedSet);

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

AnnotatedSet Annotation::newAnnotatedSet(const SetOperation operation, const AnnotatedSet &left,
                                         const AnnotatedSet &right) {
  assert(operation == SetOperation::intersection);

  const auto &lSet = std::get<CanonicalSet>(left);
  const auto &lAnnotation = std::get<CanonicalAnnotation>(left);
  const auto &rSet = std::get<CanonicalSet>(right);
  const auto &rAnnotation = std::get<CanonicalAnnotation>(right);

  auto newSet = Set::newSet(operation, lSet, rSet);
  auto newAnnot = newAnnotation(lAnnotation, rAnnotation);
  return {newSet, newAnnot };
}

AnnotatedSet Annotation::newAnnotatedSet(SetOperation operation, const AnnotatedSet &annotatedSet,
                                         const AnnotatedRelation &annotatedRelation) {
  assert(operation == SetOperation::image || operation == SetOperation::domain);

  const auto &set = std::get<CanonicalSet>(annotatedSet);
  const auto &setAnnotation = std::get<CanonicalAnnotation>(annotatedSet);
  const auto &relation = std::get<CanonicalRelation>(annotatedRelation);
  const auto &relationAnnotation = std::get<CanonicalAnnotation>(annotatedRelation);

  auto newSet = Set::newSet(operation, set, relation);
  auto newAnnot = newAnnotation(setAnnotation, relationAnnotation);
  return {newSet, newAnnot };
}