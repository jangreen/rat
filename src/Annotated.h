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

template <bool first>
[[nodiscard]] std::string annotationToString(const AnnotatedRelation annotatedRelation) {
  assert(validate(annotatedRelation));
  const auto &[relation, annotation] = annotatedRelation;

  switch (relation->operation) {
    case RelationOperation::relationIntersection:
      return "(" + annotationToString<first>(getLeft(annotatedRelation)) + " & " +
             annotationToString<first>(getRight(annotatedRelation)) + ")";
    case RelationOperation::composition:
      return "(" + annotationToString<first>(getLeft(annotatedRelation)) + ";" +
             annotationToString<first>(getRight(annotatedRelation)) + ")";
    case RelationOperation::relationUnion:
      return "(" + annotationToString<first>(getLeft(annotatedRelation)) + " | " +
             annotationToString<first>(getRight(annotatedRelation)) + ")";
    case RelationOperation::converse:
      return annotationToString<first>(getLeft(annotatedRelation)) + "^-1";
    case RelationOperation::transitiveClosure:
      return annotationToString<first>(getLeft(annotatedRelation)) + "^*";
    case RelationOperation::baseRelation:
      return first ? std::to_string(annotation->getValue().value().first)
                   : std::to_string(annotation->getValue().value().second);
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      return relation->toString();
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}
template <bool first>
[[nodiscard]] std::string annotationToString(const AnnotatedSet annotatedSet) {
  const auto &[set, annotation] = annotatedSet;

  // print annotation for a given set
  // reverse order in domain case
  switch (set->operation) {
    case SetOperation::image:
      return "(" + annotationToString<first>(getLeft(annotatedSet)) + ";" +
             annotationToString<first>(std::get<AnnotatedRelation>(getRight(annotatedSet))) + ")";
    case SetOperation::domain:
      return "(" + annotationToString<first>(std::get<AnnotatedRelation>(getRight(annotatedSet))) +
             ";" + annotationToString<first>(getLeft(annotatedSet)) + ")";
    case SetOperation::setIntersection:
      return "(" + annotationToString<first>(getLeft(annotatedSet)) + " & " +
             annotationToString<first>(std::get<AnnotatedSet>(getRight(annotatedSet))) + ")";
    case SetOperation::setUnion:
      return "(" + annotationToString<first>(getLeft(annotatedSet)) + " | " +
             annotationToString<first>(std::get<AnnotatedSet>(getRight(annotatedSet))) + ")";
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return set->toString();
    default:
      throw std::logic_error("unreachable");
  }
}
template <bool first>
[[nodiscard]] std::string toString(const AnnotatedSet annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  return set->toString() + "\n" + annotationToString<first>(annotatedSet);
}
template <bool first>
[[nodiscard]] std::string toString(const AnnotatedRelation annotatedRelation) {
  const auto &[relation, annotation] = annotatedRelation;
  return relation->toString() + "\n" + annotationToString<first>(annotatedRelation);
}

}  // namespace Annotated