#pragma once
#include <optional>
#include <string>

#include "Relation.h"
#include "Set.h"

// TODO: make occurrence trees sparse: compact representation for 0
class Annotation;
typedef const Annotation *CanonicalAnnotation;
typedef std::pair<CanonicalSet, CanonicalAnnotation> AnnotatedSet;
typedef std::pair<CanonicalRelation, CanonicalAnnotation> AnnotatedRelation;
typedef int AnnotationType;

static int saturationBound = 1;

class Annotation {
 private:
  static CanonicalAnnotation newAnnotation(std::optional<AnnotationType> value,
                                           CanonicalAnnotation left, CanonicalAnnotation right);
  [[nodiscard]] bool validate() const;  // TODO:

 public:
  Annotation(std::optional<AnnotationType> value, CanonicalAnnotation left,
             CanonicalAnnotation right);
  static CanonicalAnnotation newDummy();
  static CanonicalAnnotation newLeaf(AnnotationType value);
  static CanonicalAnnotation newAnnotation(CanonicalAnnotation left, CanonicalAnnotation right);
  static CanonicalAnnotation newAnnotation(CanonicalRelation relation, AnnotationType value);
  static CanonicalAnnotation newAnnotation(CanonicalSet set, AnnotationType value);

  static AnnotatedSet getLeft(const AnnotatedSet &annotatedSet);
  static std::variant<AnnotatedSet, AnnotatedRelation> getRight(const AnnotatedSet &annotatedSet);
  static AnnotatedSet newAnnotatedSet(SetOperation operation, const AnnotatedSet &left,
                                      const AnnotatedSet &right);
  static AnnotatedSet newAnnotatedSet(SetOperation operation, const AnnotatedSet &annotatedSet,
                                      const AnnotatedRelation &annotatedRelation);
  // Due to canonicalization, moving or copying is not allowed
  Annotation(const Annotation &other) = delete;
  Annotation(const Annotation &&other) = delete;

  bool operator==(const Annotation &other) const;
  [[nodiscard]] bool isLeaf() const;

  const std::optional<AnnotationType> value;  // nullopt: subtree has default annotation
  const CanonicalAnnotation left;             // is set iff operation unary/binary
  const CanonicalAnnotation right;            // is set iff operation binary

  [[nodiscard]] std::string toString() const;
};

template <>
struct std::hash<Annotation> {
  std::size_t operator()(const Annotation &annotation) const noexcept {
    const size_t leftHash = hash<CanonicalAnnotation>()(annotation.left);
    const size_t rightHash = hash<CanonicalAnnotation>()(annotation.right);
    const size_t annotationHash = hash<std::optional<AnnotationType>>()(annotation.value);

    return (leftHash ^ (rightHash << 1) >> 1) + 31 * annotationHash;
  }
};
