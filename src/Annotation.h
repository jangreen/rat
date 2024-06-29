#pragma once
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

/*
 *  Let T be a binary tree-like structure whose leaves can be categorized in annotatable
 *  and non-annotatable leaves. A leaf annotation L is a function that maps each annotatable
 *  leaf of T to some value (the annotation) in a complete lattice.
 *
 *  The leaf annotation L can naively be represented as a binary tree-like structure itself:
 *  It is shaped like T but has values on the leafs.
 *  However, the Annotation class is based on a more compact representation based on summarization:
 *
 *  (1) If all annotatable leaves in a subtree of T have the same value v (or none of them are annotatable),
 *      then we can put the value v (resp. the empty optional) on the corresponding root of this subtree
 *      in L and drop the whole subtree. This has the following consequences:
 *      (a) Leaf nodes in L do not necessarily correspond to leaf nodes in T but rather to
 *          subtress in T.
 *      (b) The shape of T is not captured in L; indeed, the same L now represents an infinitude of
 *          leaf annotations. In particular, a single leaf (and thus root) node with value v
*           represents a constant leaf annotation for all possible T.
 *      (c) Given T and L, finding L(n) for some leaf n in T requires carefully
 *          traversing T and L simultanously from their roots. Furthermore, not every
 *          move on T is reflected by an identical move in L: (e.g. T may branch, while L
 *          does nothing because it is in summarizing leaf node).
 *
 *  (2) On non-leaf nodes in L, we can use value v to summarize the annotations below.
 *      We make v the meet of the values of the below subtree, i.e., it designates the
 *      least annotation that can be found in the subtree.
 *      Together with (1), this has the following consequences for non-leafs with value v:
 *      (a) There exists at least two leaf nodes with different values
 *      (b) If v is from a totally ordered set, then there exists a leaf with value v and another
 *          leaf with value > v.
 *
 *   TODO (possible compactification):
 *   (3) We can make non-leaf nodes always binary such that both children have different values.
 *       The reason is that if all children of a node have the same values, then we can summarize
 *       again, in particular, this automatically holds true if a node has only a single child.
 *       In this case, when traversing T and L simultaneously, L would only reflect
 *       branching moves of T, while doing nothing when T performs non-branching moves.
 *
 *
 *    Implementation notes:
 *      - Annotation is not a binary tree, but a binary DAG to allow for better sharing.
 *      - Annotation is immutable and we use hash-consing for maximal sharing.
 */
class Annotation {
 private:
  static CanonicalAnnotation newAnnotation(AnnotationType value, CanonicalAnnotation left,
                                           CanonicalAnnotation right);
  [[nodiscard]] bool validate() const;  // TODO:

 public:
  Annotation(AnnotationType value, CanonicalAnnotation left, CanonicalAnnotation right);
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

  const AnnotationType value;
  const CanonicalAnnotation left;   // is set iff operation unary/binary
  const CanonicalAnnotation right;  // is set iff operation binary

  [[nodiscard]] std::string toString() const;
};

template <>
struct std::hash<Annotation> {
  std::size_t operator()(const Annotation &annotation) const noexcept {
    const size_t leftHash = hash<CanonicalAnnotation>()(annotation.left);
    const size_t rightHash = hash<CanonicalAnnotation>()(annotation.right);
    const size_t annotationHash = hash<AnnotationType>()(annotation.value);

    return (leftHash ^ (rightHash << 1) >> 1) + 31 * annotationHash;
  }
};
