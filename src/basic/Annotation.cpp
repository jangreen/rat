#include "Annotation.h"

#include <cassert>
#include <unordered_set>

template <typename AnnotationType>
bool Annotation<AnnotationType>::validate() const {
  // Binary or leaf
  bool isValid = ((left == nullptr) == (right == nullptr));
  assert(isValid && "Unary annotation node");
  // No value => Leaf
  isValid &= value.has_value() || isLeaf();
  assert(isValid && "Non-leaf annotation without value");
  // No leaf => value is meet of children.
  isValid &= isLeaf() || value.value() == meet(left->value, right->value);
  assert(isValid && "Non-leaf annotation with incorrect value");
  // left is valid
  isValid &= left == nullptr || left->validate();
  // right is valid
  isValid &= right == nullptr || right->validate();
  return isValid;
}
