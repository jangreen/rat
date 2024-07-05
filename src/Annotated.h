#pragma once
#include <cassert>

#include "Annotation.h"
#include "Relation.h"
#include "Set.h"

typedef std::pair<CanonicalSet, CanonicalAnnotation> AnnotatedSet;
typedef std::pair<CanonicalRelation, CanonicalAnnotation> AnnotatedRelation;

namespace Annotated {

AnnotatedSet getLeft(const AnnotatedSet &set);
std::variant<AnnotatedSet, AnnotatedRelation> getRight(const AnnotatedSet &relation);

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
// TODO: Add remaining variants for AnnotatedRelation?!

// TODO: Add toString/print method

}  // namespace Annotated