#pragma once
#include <string>

static int saturationBound = 1;

// TODO: make occurrence trees sparse: compact representation for 0

class Annotation;
typedef const Annotation *CanonicalAnnotation;
typedef int AnnotationType;

class Annotation {
 private:
  static CanonicalAnnotation newAnnotation(AnnotationType annotation, CanonicalAnnotation left,
                                           CanonicalAnnotation right);
  bool validate() const;  // TODO:

 public:
  Annotation(AnnotationType annotation, CanonicalAnnotation left, CanonicalAnnotation right);
  static CanonicalAnnotation newLeaf(AnnotationType annotation);
  static CanonicalAnnotation newAnnotation(CanonicalAnnotation left, CanonicalAnnotation right);

  // Due to canonicalization, moving or copying is not allowed
  Annotation(const Annotation &other) = delete;
  Annotation(const Annotation &&other) = delete;

  bool operator==(const Annotation &other) const;
  bool isLeaf() const;

  const AnnotationType annotation;
  const CanonicalAnnotation left;   // is set iff operation unary/binary
  const CanonicalAnnotation right;  // is set iff operation binary

  [[nodiscard]] std::string toString() const;
};

template <>
struct std::hash<Annotation> {
  std::size_t operator()(const Annotation &annotation) const noexcept {
    const size_t leftHash = hash<CanonicalAnnotation>()(annotation.left);
    const size_t rightHash = hash<CanonicalAnnotation>()(annotation.right);
    const size_t annotationHash = hash<AnnotationType>()(annotation.annotation);

    return (leftHash ^ (rightHash << 1) >> 1) + 31 * annotationHash;
  }
};