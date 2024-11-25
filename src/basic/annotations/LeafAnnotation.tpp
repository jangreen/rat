#pragma once
#include <cassert>
#include <unordered_set>

template <typename AnnotationType>
LeafAnnotation<AnnotationType>::LeafAnnotation(std::optional<AnnotationType> value,
                                               const CanonicalLeafAnnotation<AnnotationType> left,
                                               const CanonicalLeafAnnotation<AnnotationType> right)
    : value(std::move(value)), left(left), right(right) {}

template <typename AnnotationType>
CanonicalLeafAnnotation<AnnotationType> LeafAnnotation<AnnotationType>::newAnnotation(
    const std::optional<AnnotationType> &value, const CanonicalLeafAnnotation<AnnotationType> left,
    const CanonicalLeafAnnotation<AnnotationType> right) {
  assert(value.has_value() || (left == nullptr && right == nullptr));  // No value => Leaf

  static std::unordered_set<LeafAnnotation> canonicalizer;
  auto [iter, created] = canonicalizer.insert(std::move(LeafAnnotation(value, left, right)));
  return &*iter;
}

template <typename AnnotationType>
bool LeafAnnotation<AnnotationType>::validate() const {
  // Binary or leaf
  bool isValid = ((left == nullptr) == (right == nullptr));
  assert(isValid && "Unary annotation node");
  // No value => Leaf
  isValid &= value.has_value() || isLeaf();
  assert(isValid && "Non-leaf annotation without value");
  // No leaf => value is meet of children.
  isValid &= isLeaf() || value.value() == joinValues(left->value, right->value);
  assert(isValid && "Non-leaf annotation with incorrect value");
  // left is valid
  isValid &= left == nullptr || left->validate();
  // right is valid
  isValid &= right == nullptr || right->validate();
  return isValid;
}

template <typename AnnotationType>
CanonicalLeafAnnotation<AnnotationType> LeafAnnotation<AnnotationType>::none() {
  static CanonicalLeafAnnotation<AnnotationType> cached = nullptr;
  if (cached == nullptr) {
    cached = newAnnotation(std::nullopt, nullptr, nullptr);
  }
  return cached;
}

template <typename AnnotationType>
CanonicalLeafAnnotation<AnnotationType> LeafAnnotation<AnnotationType>::newLeaf(
    AnnotationType value) {
  return newAnnotation(value, nullptr, nullptr);
}

template <typename AnnotationType>
CanonicalLeafAnnotation<AnnotationType> LeafAnnotation<AnnotationType>::joinAnnotation(
    const CanonicalLeafAnnotation<AnnotationType> left,
    const CanonicalLeafAnnotation<AnnotationType> right) {
  assert(left != nullptr && right != nullptr);
  if (left == right && left->isLeaf()) {
    // Two constant annotations (leafs) together form just the same constant annotation.
    return left;
  }

  const auto joinValue = joinValues(left->value, right->value);
  assert(joinValue.has_value());  // TODO: maybe return none();?
  return newAnnotation(joinValue.value(), left, right);
}

// template <typename AnnotationType>
// CanonicalLeafAnnotation<AnnotationType> LeafAnnotation<AnnotationType>::min(
//     const CanonicalLeafAnnotation<AnnotationType> first,
//     const CanonicalLeafAnnotation<AnnotationType> second) {
//   if (first->isLeaf() && first->getValue() <= second->getValue()) {
//     return first;
//   }
//   if (second->isLeaf() && second->getValue() <= first->getValue()) {
//     return second;
//   }
//   const auto minLeft = min(first->getLeft(), second->getLeft());
//   const auto minRight = min(first->getRight(), second->getRight());
//
//   return meetAnnotation(minLeft, minRight);
// }

template <typename AnnotationType>
bool LeafAnnotation<AnnotationType>::hasValue() const {
  return value.has_value();
}

template <typename AnnotationType>
AnnotationType LeafAnnotation<AnnotationType>::getValue() const {
  return value.value();
}

template <typename AnnotationType>
std::optional<AnnotationType> LeafAnnotation<AnnotationType>::getOptionalValue() const {
  return value;
}

template <typename AnnotationType>
CanonicalLeafAnnotation<AnnotationType> LeafAnnotation<AnnotationType>::getLeft() const {
  return left == nullptr ? this : left;
}

template <typename AnnotationType>
CanonicalLeafAnnotation<AnnotationType> LeafAnnotation<AnnotationType>::getRight() const {
  return right == nullptr ? this : right;
}

template <typename AnnotationType>
bool LeafAnnotation<AnnotationType>::isLeaf() const {
  return left == nullptr && right == nullptr;
}

template <typename AnnotationType>
bool LeafAnnotation<AnnotationType>::operator==(const LeafAnnotation &other) const {
  return value == other.value && left == other.left && right == other.right;
}

// prints yield, each leaf in new line
template <typename AnnotationType>
std::string LeafAnnotation<AnnotationType>::toString() const {
  std::string output;
  if (isLeaf()) {
    output += hasValue() ? annotationTypetoString(getValue()) : "?";
    return output + "\n";
  }
  if (left != nullptr) {
    output += left->toString();
  }
  if (right != nullptr) {
    output += right->toString();
  }
  return output;
}

template <typename AnnotationType>
std::size_t std::hash<LeafAnnotation<AnnotationType>>::operator()(
    const LeafAnnotation<AnnotationType> &annotation) const noexcept {
  const size_t leftHash = hash<CanonicalLeafAnnotation<AnnotationType>>()(annotation.left);
  const size_t rightHash = hash<CanonicalLeafAnnotation<AnnotationType>>()(annotation.right);
  const size_t annotationHash = hash<std::optional<AnnotationType>>()(annotation.value);

  return (leftHash ^ (rightHash << 1) >> 1) + 31 * annotationHash;
}
