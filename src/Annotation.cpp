#include "Annotation.h"

#include <cassert>
#include <unordered_set>

namespace {

std::optional<AnnotationType> meet(const std::optional<AnnotationType> a,
                                   const std::optional<AnnotationType> b) {
  if (!a.has_value() && !b.has_value()) {
    return std::nullopt;
  }
  if (!a.has_value()) {
    return b;
  }
  if (!b.has_value()) {
    return a;
  }
  return AnnotationType{std::min(a->first, b->first), std::min(a->second, b->second)};
}

}  // namespace

Annotation::Annotation(std::optional<AnnotationType> value, CanonicalAnnotation left,
                       CanonicalAnnotation right)
    : value(value), left(left), right(right) {}

CanonicalAnnotation Annotation::newAnnotation(std::optional<AnnotationType> value,
                                              CanonicalAnnotation left, CanonicalAnnotation right) {
  assert((left == nullptr) == (right == nullptr));                     // Binary or leaf
  assert(value.has_value() || (left == nullptr && right == nullptr));  // No value => Leaf

  static std::unordered_set<Annotation> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(value, left, right);
  return &*iter;
}

CanonicalAnnotation Annotation::none() { return newAnnotation(std::nullopt, nullptr, nullptr); }

CanonicalAnnotation Annotation::newLeaf(AnnotationType value) {
  return newAnnotation(value, nullptr, nullptr);
}

CanonicalAnnotation Annotation::newAnnotation(const CanonicalAnnotation left,
                                              const CanonicalAnnotation right) {
  assert(left != nullptr && right != nullptr);
  if (left == right && left->isLeaf()) {
    // Two constant annotations (leafs) together form just the same constant annotation.
    return left;
  }

  const auto annotationValue = meet(left->getValue(), right->getValue());
  assert(annotationValue.has_value());  // TODO: maybe return none();?
  return newAnnotation(annotationValue.value(), left, right);
}

bool Annotation::validate() const {
  // Binary or leaf
  bool isValid = ((left == nullptr) == (right == nullptr));
  // No value => Leaf
  isValid &= value.has_value() || isLeaf();
  // No leaf => value is meet of children.
  isValid &= isLeaf() || value.value() == meet(left->value, right->value);
  // left is valid
  isValid &= left == nullptr || left->validate();
  // right is valid
  isValid &= right == nullptr || right->validate();
  return isValid;
}
