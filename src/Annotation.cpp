#include "Annotation.h"

#include <cassert>
#include <unordered_set>

Annotation::Annotation(AnnotationType annotation, CanonicalAnnotation left,
                       CanonicalAnnotation right)
    : annotation(annotation), left(left), right(right) {}

bool Annotation::operator==(const Annotation& other) const {
  return annotation == other.annotation && left == other.left && right == other.right;
}

bool Annotation::validate() const {
  // TODO: validate annotation values
  return true;
}

CanonicalAnnotation Annotation::newAnnotation(AnnotationType annotation, CanonicalAnnotation left,
                                              CanonicalAnnotation right) {
  static std::unordered_set<Annotation> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(annotation, left, right);
  return &*iter;
}

CanonicalAnnotation Annotation::newLeaf(AnnotationType annotation) {
  return newAnnotation(annotation, nullptr, nullptr);
}

CanonicalAnnotation Annotation::newAnnotation(CanonicalAnnotation left, CanonicalAnnotation right) {
  return newAnnotation(std::min(left->annotation, right->annotation), left, right);
}

bool Annotation::isLeaf() const { return left == nullptr & right == nullptr; }

std::string Annotation::toString() const {
  if (isLeaf()) {
    return std::to_string(annotation);
  }

  assert(left != nullptr);

  if (right != nullptr) {
    return "(" + left->toString() + ")(" + right->toString() + ")";
  }

  return left->toString();
}