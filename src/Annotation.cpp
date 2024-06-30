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