#pragma once
#include <cassert>

#include "Annotation.h"
#include "Relation.h"
#include "Set.h"

// TODO: useful? separate Literal class from Annotation class
// typedef std::pair<PartialLiteral, CanonicalAnnotation<SaturationAnnotation>>
//     AnnotatedPartialLiteral;
// typedef std::pair<Literal, CanonicalAnnotation<SaturationAnnotation>> AnnotatedLiteral;
template <typename AnnotationType>
using AnnotatedSet = std::pair<CanonicalSet, CanonicalAnnotation<AnnotationType>>;
template <typename AnnotationType>
using AnnotatedRelation = std::pair<CanonicalRelation, CanonicalAnnotation<AnnotationType>>;

typedef AnnotatedSet<SaturationAnnotation> SaturationAnnotatedSet;
typedef AnnotatedRelation<SaturationAnnotation> SaturationAnnotatedRelation;

namespace Annotated {

template <typename AnnotationType>
AnnotatedSet<AnnotationType> getLeft(const AnnotatedSet<AnnotationType> &annotatedSet) {
  const auto [set, annotation] = annotatedSet;
  assert(set->leftOperand != nullptr);
  return {set->leftOperand, annotation->getLeft()};
}

template <typename AnnotationType>
AnnotatedSet<AnnotationType> getRightSet(const AnnotatedSet<AnnotationType> &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::setUnion:
    case SetOperation::setIntersection:
      return {set->rightOperand, annotation->getRight()};
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

template <typename AnnotationType>
AnnotatedRelation<AnnotationType> getRightRelation(
    const AnnotatedSet<AnnotationType> &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::domain:
    case SetOperation::image:
      return {set->relation, annotation->getRight()};
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

template <typename AnnotationType>
AnnotatedSet<AnnotationType> getLeftSet(
    const AnnotatedRelation<AnnotationType> &annotatedRelation) {
  const auto [relation, annotation] = annotatedRelation;
  switch (relation->operation) {
    case RelationOperation::setIdentity:
      if (std::is_same_v<AnnotationType, SaturationAnnotation>) {
        // On unary operators we simulate the left move by doing nothing!
        return {relation->set, annotation};
      }
      return {relation->set, annotation->getLeft()};
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion:
    case RelationOperation::cartesianProduct:
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    default:
      throw std::logic_error("unreachable");
  }
}

template <typename AnnotationType>
AnnotatedRelation<AnnotationType> getLeftRelation(
    const AnnotatedRelation<AnnotationType> &annotatedRelation) {
  const auto [relation, annotation] = annotatedRelation;
  switch (relation->operation) {
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      if (std::is_same_v<AnnotationType, SaturationAnnotation>) {
        // On unary operators we simulate the left move by doing nothing!
        return {relation->leftOperand, annotation};
      }
      return {relation->leftOperand, annotation->getLeft()};
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion:
      return {relation->leftOperand, annotation->getLeft()};
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

template <typename AnnotationType>
AnnotatedRelation<AnnotationType> getRight(
    const AnnotatedRelation<AnnotationType> &annotatedRelation) {
  const auto [relation, annotation] = annotatedRelation;
  assert(relation->rightOperand != nullptr);
  return {relation->rightOperand, annotation->getRight()};
}

CanonicalAnnotation<SaturationAnnotation> makeWithValue(CanonicalSet set,
                                                        const SaturationAnnotation &value);
CanonicalAnnotation<SaturationAnnotation> makeWithValue(CanonicalRelation relation,
                                                        const SaturationAnnotation &value);

// wrapped newSet
inline SaturationAnnotatedSet newSet(const SetOperation operation,
                                     const SaturationAnnotatedSet &left,
                                     const SaturationAnnotatedSet &right) {
  return {Set::newSet(operation, left.first, right.first),
          Annotation<SaturationAnnotation>::meetAnnotation(left.second, right.second)};
}
inline SaturationAnnotatedSet newSet(const SetOperation operation,
                                     const SaturationAnnotatedSet &left,
                                     const SaturationAnnotatedRelation &relation) {
  return {Set::newSet(operation, left.first, relation.first),
          Annotation<SaturationAnnotation>::meetAnnotation(left.second, relation.second)};
}
inline SaturationAnnotatedSet newEvent(int label) {
  return {Set::newEvent(label), Annotation<SaturationAnnotation>::none()};
}
inline SaturationAnnotatedSet newBaseSet(const std::string &identifier) {
  return {Set::newBaseSet(identifier), Annotation<SaturationAnnotation>::none()};
}

// wrapped newRelation
inline SaturationAnnotatedRelation newRelation(const RelationOperation operation,
                                               const SaturationAnnotatedRelation &left) {
  return {Relation::newRelation(operation, left.first), left.second};
}
inline SaturationAnnotatedRelation newRelation(const RelationOperation operation,
                                               const SaturationAnnotatedRelation &left,
                                               const SaturationAnnotatedRelation &right) {
  return {Relation::newRelation(operation, left.first, right.first),
          Annotation<SaturationAnnotation>::meetAnnotation(left.second, right.second)};
}

SaturationAnnotatedSet substituteAll(const SaturationAnnotatedSet &annotatedSet,
                                     CanonicalSet search, CanonicalSet replace);
SaturationAnnotatedSet substitute(const SaturationAnnotatedSet &annotatedSet, CanonicalSet search,
                                  CanonicalSet replace, int *n);
SaturationAnnotatedRelation substituteAll(const SaturationAnnotatedRelation &annotatedRelation,
                                          CanonicalRelation search, CanonicalRelation replace);

SaturationAnnotatedSet substituteAll(const SaturationAnnotatedSet &annotatedSet,
                                     CanonicalRelation search, CanonicalRelation replace);

[[nodiscard]] bool validate(const SaturationAnnotatedSet &annotatedSet);
[[nodiscard]] bool validate(const SaturationAnnotatedRelation &annotatedRelation);

template <bool first>
[[nodiscard]] std::string annotationToString(SaturationAnnotatedSet annotatedSet);
template <bool first>
[[nodiscard]] std::string annotationToString(const SaturationAnnotatedRelation annotatedRelation) {
  assert(validate(annotatedRelation));
  const auto &[relation, annotation] = annotatedRelation;

  switch (relation->operation) {
    case RelationOperation::relationIntersection:
      return "(" +
             annotationToString<first>(getLeftRelation<SaturationAnnotation>(annotatedRelation)) +
             " & " + annotationToString<first>(getRight(annotatedRelation)) + ")";
    case RelationOperation::composition:
      return "(" +
             annotationToString<first>(getLeftRelation<SaturationAnnotation>(annotatedRelation)) +
             ";" + annotationToString<first>(getRight(annotatedRelation)) + ")";
    case RelationOperation::relationUnion:
      return "(" +
             annotationToString<first>(getLeftRelation<SaturationAnnotation>(annotatedRelation)) +
             " | " + annotationToString<first>(getRight(annotatedRelation)) + ")";
    case RelationOperation::converse:
      return annotationToString<first>(getLeftRelation<SaturationAnnotation>(annotatedRelation)) +
             "^-1";
    case RelationOperation::transitiveClosure:
      return annotationToString<first>(getLeftRelation<SaturationAnnotation>(annotatedRelation)) +
             "^*";
    case RelationOperation::baseRelation:
      return first ? std::to_string(annotation->getValue().value().first)
                   : std::to_string(annotation->getValue().value().second);
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      return relation->toString();
    case RelationOperation::setIdentity:
      return annotationToString<first>(getLeftSet<SaturationAnnotation>(annotatedRelation));
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}
template <bool first>
[[nodiscard]] std::string annotationToString(const SaturationAnnotatedSet annotatedSet) {
  const auto &[set, annotation] = annotatedSet;

  // print annotation for a given set
  // reverse order in domain case
  switch (set->operation) {
    case SetOperation::image:
      return "(" + annotationToString<first>(getLeft(annotatedSet)) + ";" +
             annotationToString<first>(getRightRelation<SaturationAnnotation>(annotatedSet)) + ")";
    case SetOperation::domain:
      return "(" + annotationToString<first>(getRightRelation<SaturationAnnotation>(annotatedSet)) +
             ";" + annotationToString<first>(getLeft(annotatedSet)) + ")";
    case SetOperation::setIntersection:
      return "(" + annotationToString<first>(getLeft(annotatedSet)) + " & " +
             annotationToString<first>(getRightSet<SaturationAnnotation>(annotatedSet)) + ")";
    case SetOperation::setUnion:
      return "(" + annotationToString<first>(getLeft(annotatedSet)) + " | " +
             annotationToString<first>(getRightSet<SaturationAnnotation>(annotatedSet)) + ")";
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
[[nodiscard]] std::string toString(const SaturationAnnotatedSet annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  return set->toString() + "\n" + annotationToString<first>(annotatedSet);
}
template <bool first>
[[nodiscard]] std::string toString(const SaturationAnnotatedRelation annotatedRelation) {
  const auto &[relation, annotation] = annotatedRelation;
  return relation->toString() + "\n" + annotationToString<first>(annotatedRelation);
}
// requires two annotations for the same structure
inline CanonicalAnnotation<SaturationAnnotation> sum(
    const CanonicalAnnotation<SaturationAnnotation> a,
    const CanonicalAnnotation<SaturationAnnotation> b) {
  if (a->isLeaf() || b->isLeaf()) {
    if (a->getValue().has_value() && b->getValue().has_value()) {
      const SaturationAnnotation summedValue = {
          a->getValue().value().first + b->getValue().value().first,
          a->getValue().value().second + b->getValue().value().second};
      return Annotation<SaturationAnnotation>::newLeaf(summedValue);
    }
    if (a->getValue().has_value()) {
      return a;
    }
    return b;  // in either case (if b has value or not)
  }
  const auto left = a->getLeftInteral();
  auto newLeft = Annotation<SaturationAnnotation>::none();
  if (left != nullptr) {
    newLeft = sum(a->getLeftInteral(), b->getLeftInteral());
  }
  const auto right = a->getRightInteral();
  auto newRight = Annotation<SaturationAnnotation>::none();
  if (right != nullptr) {
    newRight = sum(a->getRightInteral(), b->getRightInteral());
  }
  return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
}

}  // namespace Annotated