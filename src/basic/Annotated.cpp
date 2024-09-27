#include "Annotated.h"

namespace Annotated {

CanonicalAnnotation<SaturationAnnotation> makeWithValue(const CanonicalSet set,
                                                        const SaturationAnnotation &value) {
  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return Annotation<SaturationAnnotation>::none();
    case SetOperation::baseSet:
      return Annotation<SaturationAnnotation>::newLeaf(value);
    case SetOperation::setUnion:
    case SetOperation::setIntersection: {
      const auto left = makeWithValue(set->leftOperand, value);
      const auto right = makeWithValue(set->rightOperand, value);
      return Annotation<SaturationAnnotation>::meetAnnotation(left, right);
    }
    case SetOperation::domain:
    case SetOperation::image: {
      const auto left = makeWithValue(set->leftOperand, value);
      const auto right = makeWithValue(set->relation, value);
      return Annotation<SaturationAnnotation>::meetAnnotation(left, right);
    }
    default:
      throw std::logic_error("unreachable");
  }
}

CanonicalAnnotation<SaturationAnnotation> makeWithValue(const CanonicalRelation relation,
                                                        const SaturationAnnotation &value) {
  switch (relation->operation) {
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      return Annotation<SaturationAnnotation>::none();
    case RelationOperation::setIdentity: {
      return makeWithValue(relation->set, value);
    }
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion: {
      const auto left = makeWithValue(relation->leftOperand, value);
      const auto right = makeWithValue(relation->rightOperand, value);
      return Annotation<SaturationAnnotation>::meetAnnotation(left, right);
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      return makeWithValue(relation->leftOperand, value);
    }
    case RelationOperation::baseRelation:
      return Annotation<SaturationAnnotation>::newLeaf(value);
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

// only subtitutes in set expressions
SaturationAnnotatedSet substituteAll(const SaturationAnnotatedSet &annotatedSet,
                                     const CanonicalSet search, const CanonicalSet replace) {
  const auto &[set, annotation] = annotatedSet;

  if (set == search) {
    return {replace, makeWithValue(replace, {0, 0})};
  }
  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return annotatedSet;
    case SetOperation::image:
    case SetOperation::domain: {
      const auto &left = substituteAll(getLeft(annotatedSet), search, replace);
      const auto relation = getRightRelation<SaturationAnnotation>(annotatedSet);
      return newSet(set->operation, left, relation);
    }
    case SetOperation::setIntersection:
    case SetOperation::setUnion: {
      const auto &left = substituteAll(getLeft(annotatedSet), search, replace);
      const auto &right =
          substituteAll(getRightSet<SaturationAnnotation>(annotatedSet), search, replace);
      return newSet(set->operation, left, right);
    }
    default:
      throw std::logic_error("unreachable");
  }
}

SaturationAnnotatedRelation substituteAll(const SaturationAnnotatedRelation &annotatedRelation,
                                          const CanonicalRelation search,
                                          const CanonicalRelation replace) {
  const auto &[relation, annotation] = annotatedRelation;

  if (relation == search) {
    return {replace, makeWithValue(replace, {0, 0})};
  }

  switch (relation->operation) {
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion: {
      const auto &left =
          substituteAll(getLeftRelation<SaturationAnnotation>(annotatedRelation), search, replace);
      const auto &right = substituteAll(getRight(annotatedRelation), search, replace);
      return newRelation(relation->operation, left, right);
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      const auto &left =
          substituteAll(getLeftRelation<SaturationAnnotation>(annotatedRelation), search, replace);
      return newRelation(relation->operation, left);
    }
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    case RelationOperation::setIdentity:
      return annotatedRelation;
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

SaturationAnnotatedSet substituteAll(const SaturationAnnotatedSet &annotatedSet,
                                     const CanonicalRelation search,
                                     const CanonicalRelation replace) {
  const auto &[set, annotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return annotatedSet;
    case SetOperation::setIntersection:
    case SetOperation::setUnion: {
      const auto &left = substituteAll(getLeft(annotatedSet), search, replace);
      const auto &right =
          substituteAll(getRightSet<SaturationAnnotation>(annotatedSet), search, replace);
      return newSet(set->operation, left, right);
    }
    case SetOperation::image:
    case SetOperation::domain: {
      const auto &left = substituteAll(getLeft(annotatedSet), search, replace);
      const auto &right =
          substituteAll(getRightRelation<SaturationAnnotation>(annotatedSet), search, replace);
      return newSet(set->operation, left, right);
    }
    default:
      throw std::logic_error("unreachable");
  }
}

SaturationAnnotatedSet substitute(const SaturationAnnotatedSet &annotatedSet,
                                  const CanonicalSet search, const CanonicalSet replace, int *n) {
  assert(*n >= 0 && "Negative occurrence counter");
  const auto &[set, annotation] = annotatedSet;
  if (*n == 0) {
    return annotatedSet;
  }

  if (set == search) {
    if (*n == 1) {
      return {replace, makeWithValue(replace, {0, 0})};
    }
    (*n)--;
    return annotatedSet;
  }
  if (set->leftOperand != nullptr) {
    const auto leftSub = substitute(getLeft(annotatedSet), search, replace, n);
    if (leftSub.first != set->leftOperand) {
      switch (set->operation) {
        case SetOperation::setUnion:
        case SetOperation::setIntersection:
          return newSet(set->operation, leftSub, getRightSet<SaturationAnnotation>(annotatedSet));
        case SetOperation::domain:
        case SetOperation::image:
          return newSet(set->operation, leftSub,
                        getRightRelation<SaturationAnnotation>(annotatedSet));
        case SetOperation::event:
        case SetOperation::emptySet:
        case SetOperation::fullSet:
        case SetOperation::baseSet:
        // leftOperand != nullptr
        default:
          throw std::logic_error("unreachable");
      }
    }
  }
  if (set->rightOperand != nullptr) {
    const auto rightSub =
        substitute(getRightSet<SaturationAnnotation>(annotatedSet), search, replace, n);
    if (rightSub.first != set->rightOperand) {
      switch (set->operation) {
        case SetOperation::setUnion:
        case SetOperation::setIntersection:
          return newSet(set->operation, getLeft(annotatedSet), rightSub);
        case SetOperation::domain:
        case SetOperation::image:
        case SetOperation::event:
        case SetOperation::emptySet:
        case SetOperation::fullSet:
        case SetOperation::baseSet:
        // rightOperand != nullptr
        default:
          throw std::logic_error("unreachable");
      }
    }
  }
  return annotatedSet;
}

bool validate(const SaturationAnnotatedSet &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;

  switch (set->operation) {
    case SetOperation::baseSet:
      assert(annotation->isLeaf());
      assert(annotation->getValue().has_value());
      return annotation->isLeaf() && annotation->getValue().has_value();
    case SetOperation::event:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      assert(annotation->isLeaf() && "Non-leaf annotation at leaf set.");
      assert(!annotation->getValue().has_value() && "Unexpected annotation at set.");
      return annotation->isLeaf() && !annotation->getValue().has_value();
    case SetOperation::image:
    case SetOperation::domain: {
      const auto &leftSet = getLeft(annotatedSet);
      const auto &rightRelation = getRightRelation<SaturationAnnotation>(annotatedSet);
      bool validated = validate(leftSet);
      validated &= validate(rightRelation);
      return validated;
    }
    case SetOperation::setIntersection:
    case SetOperation::setUnion: {
      const auto &leftSet = getLeft(annotatedSet);
      const auto &rightSet = getRightSet<SaturationAnnotation>(annotatedSet);
      bool validated = validate(leftSet);
      validated &= validate(rightSet);
      return validated;
    }
    default:
      throw std::logic_error("unreachable");
  }
}
bool validate(const SaturationAnnotatedRelation &annotatedRelation) {
  const auto &[relation, annotation] = annotatedRelation;

  switch (relation->operation) {
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      assert(annotation->isLeaf() && "Non-leaf annotation at leaf set.");
      assert(!annotation->getValue().has_value() && "Unexpected annotation at set.");
      return annotation->isLeaf() && !annotation->getValue().has_value();
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion: {
      const auto &left = getLeftRelation<SaturationAnnotation>(annotatedRelation);
      const auto &right = getRight(annotatedRelation);
      bool validated = validate(left);
      validated &= validate(right);
      return validated;
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      const auto &left = getLeftRelation<SaturationAnnotation>(annotatedRelation);
      return validate(left);
    }
    case RelationOperation::baseRelation:
      assert(annotation->isLeaf());
      assert(annotation->getValue().has_value());
      return annotation->isLeaf() && annotation->getValue().has_value();
    case RelationOperation::setIdentity:
      return validate(getLeftSet<SaturationAnnotation>(annotatedRelation));
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

}  // namespace Annotated