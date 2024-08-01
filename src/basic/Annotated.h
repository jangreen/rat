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
}

template <typename AnnotatedType>
AnnotatedType getRight(const AnnotatedSet &annotatedSet);

template <>
inline AnnotatedRelation getRight(const AnnotatedSet &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::domain:
    case SetOperation::image:
      return AnnotatedRelation(set->relation, annotation->getRight());
    case SetOperation::setUnion:
    case SetOperation::setIntersection:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::event:
    default:
      throw std::logic_error("unreachable");
  }
}

template <>
inline AnnotatedSet getRight(const AnnotatedSet &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::setUnion:
    case SetOperation::setIntersection:
      return AnnotatedSet(set->rightOperand, annotation->getRight());
    case SetOperation::domain:
    case SetOperation::image:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::event:
    default:
      throw std::logic_error("unreachable");
  }
}

template <typename AnnotatedType>
AnnotatedType getLeft(const AnnotatedRelation &annotatedRelation);

template <>
inline AnnotatedRelation getLeft(const AnnotatedRelation &annotatedRelation) {
  const auto [relation, annotation] = annotatedRelation;
  switch (relation->operation) {
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      // On unary operators we simulate the left move by doing nothing!
      return AnnotatedRelation{relation->leftOperand, annotation};
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion:
      return AnnotatedRelation{relation->leftOperand, annotation->getLeft()};
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    case RelationOperation::setIdentity:
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    default:
      throw std::logic_error("unreachable");
  }
}

template <>
inline AnnotatedSet getLeft(const AnnotatedRelation &annotatedRelation) {
  const auto [relation, annotation] = annotatedRelation;
  switch (relation->operation) {
    case RelationOperation::setIdentity:
      return AnnotatedSet{relation->set, annotation};
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion:
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    default:
      throw std::logic_error("unreachable");
  }
}

inline AnnotatedRelation getRight(const AnnotatedRelation &annotatedRelation) {
  const auto [relation, annotation] = annotatedRelation;
  assert(relation->rightOperand != nullptr);
  return {relation->rightOperand, annotation->getRight()};
}

AnnotatedSet makeWithValue(CanonicalSet set, const AnnotationType &value);
AnnotatedRelation makeWithValue(CanonicalRelation relation, const AnnotationType &value);

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
inline AnnotatedSet newBaseSet(const std::string &identifier) {
  return {Set::newBaseSet(identifier), Annotation::none()};
}

// wrapped newRelation
inline AnnotatedRelation newRelation(const RelationOperation operation,
                                     const AnnotatedRelation &left) {
  return {Relation::newRelation(operation, left.first), left.second};
}
inline AnnotatedRelation newRelation(const RelationOperation operation,
                                     const AnnotatedRelation &left,
                                     const AnnotatedRelation &right) {
  return {Relation::newRelation(operation, left.first, right.first),
          Annotation::newAnnotation(left.second, right.second)};
}

AnnotatedSet substituteAll(const AnnotatedSet &annotatedSet, CanonicalSet search,
                           CanonicalSet replace);
AnnotatedSet substitute(const AnnotatedSet &annotatedSet, CanonicalSet search, CanonicalSet replace,
                        int *n);
AnnotatedRelation substituteAll(const AnnotatedRelation &annotatedRelation,
                                CanonicalRelation search, CanonicalRelation replace);

AnnotatedSet substituteAll(const AnnotatedSet &annotatedSet, CanonicalRelation search,
                           CanonicalRelation replace);

[[nodiscard]] bool validate(const AnnotatedSet &annotatedSet);
[[nodiscard]] bool validate(const AnnotatedRelation &annotatedRelation);

template <bool first>
[[nodiscard]] std::string annotationToString(AnnotatedSet annotatedSet);
template <bool first>
[[nodiscard]] std::string annotationToString(const AnnotatedRelation annotatedRelation) {
  assert(validate(annotatedRelation));
  const auto &[relation, annotation] = annotatedRelation;

  switch (relation->operation) {
    case RelationOperation::relationIntersection:
      return "(" + annotationToString<first>(getLeft<AnnotatedRelation>(annotatedRelation)) +
             " & " + annotationToString<first>(getRight(annotatedRelation)) + ")";
    case RelationOperation::composition:
      return "(" + annotationToString<first>(getLeft<AnnotatedRelation>(annotatedRelation)) + ";" +
             annotationToString<first>(getRight(annotatedRelation)) + ")";
    case RelationOperation::relationUnion:
      return "(" + annotationToString<first>(getLeft<AnnotatedRelation>(annotatedRelation)) +
             " | " + annotationToString<first>(getRight(annotatedRelation)) + ")";
    case RelationOperation::converse:
      return annotationToString<first>(getLeft<AnnotatedRelation>(annotatedRelation)) + "^-1";
    case RelationOperation::transitiveClosure:
      return annotationToString<first>(getLeft<AnnotatedRelation>(annotatedRelation)) + "^*";
    case RelationOperation::baseRelation:
      return first ? std::to_string(annotation->getValue().value().first)
                   : std::to_string(annotation->getValue().value().second);
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      return relation->toString();
    case RelationOperation::setIdentity:
      return annotationToString<first>(getLeft<AnnotatedSet>(annotatedRelation));
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
             annotationToString<first>(getRight<AnnotatedRelation>(annotatedSet)) + ")";
    case SetOperation::domain:
      return "(" + annotationToString<first>(getRight<AnnotatedRelation>(annotatedSet)) + ";" +
             annotationToString<first>(getLeft(annotatedSet)) + ")";
    case SetOperation::setIntersection:
      return "(" + annotationToString<first>(getLeft(annotatedSet)) + " & " +
             annotationToString<first>(getRight<AnnotatedSet>(annotatedSet)) + ")";
    case SetOperation::setUnion:
      return "(" + annotationToString<first>(getLeft(annotatedSet)) + " | " +
             annotationToString<first>(getRight<AnnotatedSet>(annotatedSet)) + ")";
    // TODO (topEvent optimization): case SetOperation::topEvent:
    case SetOperation::event:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return set->toString();
    case SetOperation::baseSet:
      return first ? std::to_string(annotation->getValue().value().first)
                   : std::to_string(annotation->getValue().value().second);
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