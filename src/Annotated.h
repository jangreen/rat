#pragma once
#include <cassert>

#include "Annotation.h"
#include "Relation.h"
#include "Set.h"

typedef std::pair<CanonicalSet, CanonicalAnnotation> AnnotatedSet;
typedef std::pair<CanonicalRelation, CanonicalAnnotation> AnnotatedRelation;

namespace Annotated {

inline AnnotatedSet getLeft(const AnnotatedSet &annotatedSet) {
  const auto [set, annotation] = annotatedSet;
  assert(set->leftOperand != nullptr);
  return {set->leftOperand, annotation->getLeft()};
};
std::variant<AnnotatedSet, AnnotatedRelation> getRight(const AnnotatedSet &relation);
inline AnnotatedRelation getLeft(const AnnotatedRelation &annotatedRelation) {
  const auto [relation, annotation] = annotatedRelation;
  assert(relation->leftOperand != nullptr);
  return {relation->leftOperand, annotation->getLeft()};
};
inline AnnotatedRelation getRight(const AnnotatedRelation &annotatedRelation) {
  const auto [relation, annotation] = annotatedRelation;
  assert(relation->rightOperand != nullptr);
  return {relation->rightOperand, annotation->getRight()};
};

AnnotatedSet makeWithValue(CanonicalSet set, AnnotationType value);
AnnotatedRelation makeWithValue(CanonicalRelation relation, AnnotationType value);

// wrapped newSet
inline AnnotatedSet newSet(const SetOperation operation, const AnnotatedSet &left,
                           const AnnotatedSet &right) {
  return {Set::newSet(operation, left.first, right.first),
          Annotation::newAnnotation(left.second, right.second)};
}
inline AnnotatedSet newSet(const SetOperation operation, const AnnotatedSet &left,
                           const AnnotatedRelation &relation) {
  return {Set::newSet(operation, left.first, relation.first),
          Annotation::newAnnotation(left.second, relation.second)};
}
inline AnnotatedSet newEvent(int label) { return {Set::newEvent(label), Annotation::none()}; }
inline AnnotatedSet newBaseSet(std::string &identifier) {
  return {Set::newBaseSet(identifier), Annotation::none()};
}

AnnotatedSet substitute(const AnnotatedSet annotatedSet, const CanonicalSet search,
                        const CanonicalSet replace, int *n);

[[nodiscard]] bool validate(const AnnotatedSet annotatedSet);
[[nodiscard]] bool validate(const AnnotatedRelation annotatedRelation);

// TODO: Add toString/print method

}  // namespace Annotated