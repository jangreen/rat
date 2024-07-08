
#include "Annotated.h"

namespace Annotated {

std::variant<AnnotatedSet, AnnotatedRelation> getRight(const AnnotatedSet &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  switch (set->operation) {
    case SetOperation::setUnion:
    case SetOperation::setIntersection:
      return AnnotatedSet(set->rightOperand, annotation->getRight());
    case SetOperation::domain:
    case SetOperation::image:
      return AnnotatedRelation(set->relation, annotation->getRight());
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::event:
    default:
      throw std::logic_error("unreachable");
  }
}

AnnotatedSet makeWithValue(const CanonicalSet set, const AnnotationType value) {
  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::baseSet:
      return {set, Annotation::none()};
    case SetOperation::setUnion:
    case SetOperation::setIntersection: {
      const auto [_1, left] = makeWithValue(set->leftOperand, value);
      const auto [_2, right] = makeWithValue(set->rightOperand, value);
      return {set, Annotation::newAnnotation(left, right)};
    }
    case SetOperation::domain: {
      const auto [_1, left] = makeWithValue(set->leftOperand, value);
      const auto [_2, right] = makeWithValue(set->relation, value);
      return {set, Annotation::newAnnotation(left, right)};
    }
    case SetOperation::image: {
      const auto [_1, left] = makeWithValue(set->leftOperand, value);
      const auto [_2, right] = makeWithValue(set->relation, value);
      return {set, Annotation::newAnnotation(left, right)};
    }
    default:
      throw std::logic_error("unreachable");
  }
}

AnnotatedRelation makeWithValue(const CanonicalRelation relation, const AnnotationType value) {
  switch (relation->operation) {
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      return {relation, Annotation::none()};
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion: {
      const auto [_1, left] = makeWithValue(relation->leftOperand, value);
      const auto [_2, right] = makeWithValue(relation->rightOperand, value);
      return {relation, Annotation::newAnnotation(left, right)};
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      return makeWithValue(relation->leftOperand, value);
    }
    case RelationOperation::baseRelation:
      return {relation, Annotation::newLeaf(value)};
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

AnnotatedSet substitute(const AnnotatedSet annotatedSet, const CanonicalSet search,
                        const CanonicalSet replace, int *n) {
  const auto &[set, annotation] = annotatedSet;
  if (*n == 0) {
    return annotatedSet;
  }

  if (set == search) {
    if (*n == 1 || *n == -1) {
      return Annotated::makeWithValue(replace, 0);
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
          return Annotated::newSet(set->operation, leftSub,
                                   std::get<AnnotatedSet>(getRight(annotatedSet)));
        case SetOperation::domain:
        case SetOperation::image:
          return Annotated::newSet(set->operation, leftSub,
                                   std::get<AnnotatedRelation>(getRight(annotatedSet)));
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
        substitute(std::get<AnnotatedSet>(getRight(annotatedSet)), search, replace, n);
    if (rightSub.first != set->rightOperand) {
      switch (set->operation) {
        case SetOperation::setUnion:
        case SetOperation::setIntersection:
          return Annotated::newSet(set->operation, getLeft(annotatedSet), rightSub);
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

bool validate(const AnnotatedSet annotatedSet) {
  const auto &[set, annotation] = annotatedSet;

  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      assert(annotation->isLeaf() && !annotation->getValue().has_value());
      return annotation->isLeaf() && !annotation->getValue().has_value();
    case SetOperation::image:
    case SetOperation::domain: {
      const auto &leftSet = getLeft(annotatedSet);
      const auto &right = getRight(annotatedSet);
      const auto &rightRelation = std::get<AnnotatedRelation>(right);
      return validate(leftSet) && validate(rightRelation);
    }
    case SetOperation::setIntersection:
    case SetOperation::setUnion: {
      const auto &leftSet = getLeft(annotatedSet);
      const auto &right = getRight(annotatedSet);
      const auto &rightSet = std::get<AnnotatedSet>(right);
      return validate(leftSet) && validate(rightSet);
    }
    default:
      throw std::logic_error("unreachable");
  }
}
bool validate(const AnnotatedRelation annotatedRelation) {
  const auto &[relation, annotation] = annotatedRelation;

  switch (relation->operation) {
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      assert(annotation->isLeaf());
      assert(!annotation->getValue().has_value());
      return annotation->isLeaf() && !annotation->getValue().has_value();
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
    case RelationOperation::relationUnion: {
      const auto &left = getLeft(annotatedRelation);
      const auto &right = getRight(annotatedRelation);
      return validate(left) && validate(right);
    }
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure: {
      const auto &left = getLeft(annotatedRelation);
      return validate(left);
    }
    case RelationOperation::baseRelation:
      assert(annotation->isLeaf());
      assert(annotation->getValue().has_value());
      return annotation->isLeaf() && annotation->getValue().has_value();
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

std::string toString(const AnnotatedSet annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  return set->toString() + "\n" + annotationToString(annotatedSet);
}

std::string toString(const AnnotatedRelation annotatedRelation) {
  const auto &[relation, annotation] = annotatedRelation;
  return relation->toString() + "\n" + annotationToString(annotatedRelation);
}

std::string annotationToString(const AnnotatedSet annotatedSet) {
  const auto &[set, annotation] = annotatedSet;

  // print annotation for a given set
  // reverse order in domain case
  switch (set->operation) {
    case SetOperation::image:
      return "(" + annotationToString(getLeft(annotatedSet)) + ";" +
             annotationToString(std::get<AnnotatedRelation>(getRight(annotatedSet))) + ")";
    case SetOperation::domain:
      return "(" + annotationToString(std::get<AnnotatedRelation>(getRight(annotatedSet))) + ";" +
             annotationToString(getLeft(annotatedSet)) + ")";
    case SetOperation::setIntersection:
      return "(" + annotationToString(getLeft(annotatedSet)) + " & " +
             annotationToString(std::get<AnnotatedSet>(getRight(annotatedSet))) + ")";
    case SetOperation::setUnion:
      return "(" + annotationToString(getLeft(annotatedSet)) + " | " +
             annotationToString(std::get<AnnotatedSet>(getRight(annotatedSet))) + ")";
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return set->toString();
    default:
      throw std::logic_error("unreachable");
  }
}

std::string annotationToString(const AnnotatedRelation annotatedRelation) {
  assert(validate(annotatedRelation));
  const auto &[relation, annotation] = annotatedRelation;

  switch (relation->operation) {
    case RelationOperation::relationIntersection:
      return "(" + annotationToString(getLeft(annotatedRelation)) + " & " +
             annotationToString(getRight(annotatedRelation)) + ")";
    case RelationOperation::composition:
      return "(" + annotationToString(getLeft(annotatedRelation)) + ";" +
             annotationToString(getRight(annotatedRelation)) + ")";
    case RelationOperation::relationUnion:
      return "(" + annotationToString(getLeft(annotatedRelation)) + " | " +
             annotationToString(getRight(annotatedRelation)) + ")";
    case RelationOperation::converse:
      return annotationToString(getLeft(annotatedRelation)) + "^-1";
    case RelationOperation::transitiveClosure:
      return annotationToString(getLeft(annotatedRelation)) + "^*";
    case RelationOperation::baseRelation:
      return std::to_string(annotation->getValue().value());
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

}  // namespace Annotated