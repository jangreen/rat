#include "Annotation.h"

#include <cassert>
#include <unordered_set>

namespace {

std::optional<AnnotationType> meet(const std::optional<AnnotationType> a,
                                   const std::optional<AnnotationType> b) {
  if (!a.has_value() && !b.has_value()) {
    return std::nullopt;
  }
  return std::min(a.value_or(INT32_MAX), b.value_or(INT32_MAX));
}

}  // namespace

Annotation::Annotation(std::optional<AnnotationType> value, CanonicalAnnotation left,
                       CanonicalAnnotation right)
    : value(value), left(left), right(right) {}

bool Annotation::operator==(const Annotation &other) const {
  return value == other.value && left == other.left && right == other.right;
}

bool Annotation::validate() const {
  // TODO: validate annotation values
  return true;
}

CanonicalAnnotation Annotation::newAnnotation(std::optional<AnnotationType> value,
                                              CanonicalAnnotation left, CanonicalAnnotation right) {
  static std::unordered_set<Annotation> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(value, left, right);
  return &*iter;
}

CanonicalAnnotation Annotation::newDummy() { return newAnnotation(std::nullopt, nullptr, nullptr); }

CanonicalAnnotation Annotation::newLeaf(AnnotationType value) {
  return newAnnotation(value, nullptr, nullptr);
}

CanonicalAnnotation Annotation::newAnnotation(CanonicalAnnotation left, CanonicalAnnotation right) {
  assert(left != nullptr && right != nullptr);
  const auto annotationValue = meet(left->value, right->value);
  if (!annotationValue.has_value()) {
    return newDummy();
  }
  return newAnnotation(annotationValue.value(), left, right);
}

CanonicalAnnotation Annotation::newAnnotation(CanonicalSet set, AnnotationType value) {
  switch (set->operation) {
    case SetOperation::singleton:
    case SetOperation::empty:
    case SetOperation::full:
    case SetOperation::base:
      return nullptr;
    case SetOperation::choice:
    case SetOperation::intersection: {
      auto left = newAnnotation(set->leftOperand, value);
      auto right = newAnnotation(set->rightOperand, value);
      return Annotation::newAnnotation(left, right);
    }
    case SetOperation::domain: {
      auto left = newAnnotation(set->relation, value);
      auto right = newAnnotation(set->leftOperand, value);
      return Annotation::newAnnotation(left, right);
    }
    case SetOperation::image: {
      auto left = newAnnotation(set->leftOperand, value);
      auto right = newAnnotation(set->relation, value);
      return Annotation::newAnnotation(left, right);
    }
    default:
      throw std::logic_error("unreachable");
  }
};

CanonicalAnnotation Annotation::newAnnotation(CanonicalRelation relation, AnnotationType value) {
  switch (relation->operation) {
    case RelationOperation::identity:
    case RelationOperation::empty:
    case RelationOperation::full:
      return nullptr;
    case RelationOperation::intersection:
    case RelationOperation::composition:
    case RelationOperation::choice: {
      auto left = newAnnotation(relation->leftOperand, value);
      auto right = newAnnotation(relation->rightOperand, value);
      return Annotation::newAnnotation(left, right);
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      auto left = newAnnotation(relation->leftOperand, value);
      return Annotation::newAnnotation(left, nullptr);
    }
    case RelationOperation::base:
      return Annotation::newLeaf(value);
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
};

AnnotatedSet Annotation::getLeft(const AnnotatedSet &annotatedSet) {
  const auto &set = std::get<CanonicalSet>(annotatedSet);
  const auto &annotation = std::get<CanonicalAnnotation>(annotatedSet);

  switch (set->operation) {
    case SetOperation::choice:
    case SetOperation::intersection:
    case SetOperation::image:
      return AnnotatedSet(set->leftOperand, annotation->left);
    case SetOperation::domain:
      return AnnotatedSet(set->leftOperand, annotation->right);
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
    case SetOperation::domain:
      return AnnotatedRelation(set->relation, annotation->right);
    case SetOperation::image:
      return AnnotatedRelation(set->relation, annotation->left);
    case SetOperation::base:
    case SetOperation::empty:
    case SetOperation::full:
    case SetOperation::singleton:
    default:
      throw std::logic_error("unreachable");
  }
}

AnnotatedSet Annotation::newAnnotatedSet(SetOperation operation, const AnnotatedSet &left,
                                         const AnnotatedSet &right) {
  assert(operation == SetOperation::intersection);

  const auto &lSet = std::get<CanonicalSet>(left);
  const auto &lAnnotation = std::get<CanonicalAnnotation>(left);
  const auto &rSet = std::get<CanonicalSet>(right);
  const auto &rAnnotation = std::get<CanonicalAnnotation>(right);

  auto newSet = Set::newSet(operation, lSet, rSet);
  auto newAnnot = Annotation::newAnnotation(lAnnotation, rAnnotation);
  return AnnotatedSet(newSet, newAnnot);
}

AnnotatedSet Annotation::newAnnotatedSet(SetOperation operation, const AnnotatedSet &annotatedSet,
                                         const AnnotatedRelation &annotatedRelation) {
  assert(operation == SetOperation::image || operation == SetOperation::domain);

  const auto &set = std::get<CanonicalSet>(annotatedSet);
  const auto &setAnnotation = std::get<CanonicalAnnotation>(annotatedSet);
  const auto &relation = std::get<CanonicalRelation>(annotatedRelation);
  const auto &relationAnnotation = std::get<CanonicalAnnotation>(annotatedRelation);

  auto newSet = Set::newSet(operation, set, relation);
  CanonicalAnnotation newAnnot = Annotation::newAnnotation(setAnnotation, relationAnnotation);
  return AnnotatedSet(newSet, newAnnot);
}

bool Annotation::isLeaf() const { return left == nullptr & right == nullptr; }

std::string Annotation::toString() const {
  if (isLeaf()) {
    return value ? "?" : std::to_string(value.value());
  }

  assert(left != nullptr);

  if (right != nullptr) {
    return "(" + left->toString() + ")(" + right->toString() + ")";
  }

  return left->toString();
}