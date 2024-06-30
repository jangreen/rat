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
 *  Let T be a binary tree-like structure. A leaf annotation L is a function that maps each
 *  leaf of T to some value (the annotation) in a complete lattice, or possibly a special
 *  empty value for unannotated leafs.
 *
 *  The leaf annotation L can naively be represented as a binary tree-like structure itself:
 *  It is shaped like T but has values on the leafs.
 *  However, the Annotation class is based on a more compact representation based on summarization:
 *
 *  (1) If all leaves in a subtree of T have the same (possibly empty) value v,
 *      then we can put the value v on the corresponding root of this subtree
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
 *      For the purpose of the meet, the empty value is considered the largest.
 *      Together with (1), this has the following consequences for non-leafs with value v:
 *      (a) v is not the empty value.
 *      (a) one of the subtrees contains a leaf with value >= v.
 *      (b) If v is from a totally ordered set and both subtrees have a value, then
 *          there exists a leaf with value precisely v and another leaf with value > v.
 *
 *   (3) We can make non-leaf nodes always binary. The reason is that a node with a single child
 *       has the same leafs/annotations as the child itself, i.e., the parent does not contain
 *       any useful information.
 *       This has the following consequence when traversing T and L simultaneously:
 *       L only reflects branching moves of T and does nothing when T performs a non-branching move.
 *
 *
 *    Implementation notes:
 *      - Annotation is not a binary tree, but a binary DAG to allow for better sharing.
 *      - Annotation is immutable and we use hash-consing for maximal sharing.
 *      - Annotation is a coinductive structure: the simplest elements (leafs) stand
 *        for constant annotations of infinite trees.
 *
 *    NOTE on alternative definition: If the leaves of trees can be implicitly classified
 *    as annotatable and non-annotatable, then we can generate even more compact representations
 *    for leaf annotations. It does come with the disadvantage that the annotation itself cannot
 *    be used to avoid exploration of subtrees without annotatable leaves.
 */
class Annotation {
 private:
  // Factory method to generate canonical instances.
  static CanonicalAnnotation newAnnotation(std::optional<AnnotationType> value, CanonicalAnnotation left,
                                           CanonicalAnnotation right);
  [[nodiscard]] bool validate() const;  // TODO:

  const std::optional<AnnotationType> value;
  const CanonicalAnnotation left;
  const CanonicalAnnotation right;

 public:
 // WARNING: Never call this constructor: it is only public for technicaly reasons
  Annotation(std::optional<AnnotationType> value, CanonicalAnnotation left, CanonicalAnnotation right);
  static CanonicalAnnotation newLeaf(std::optional<AnnotationType> value);
  static CanonicalAnnotation newAnnotation(CanonicalAnnotation left, CanonicalAnnotation right);
  static CanonicalAnnotation none() { return newLeaf(std::nullopt); }

  // Due to canonicalization, moving or copying is not allowed
  Annotation(const Annotation &other) = delete;
  Annotation(const Annotation &&other) = delete;

  [[nodiscard]] std::string toString() const;

  [[nodiscard]] std::optional<AnnotationType> getValue() const { return value; }
  [[nodiscard]] CanonicalAnnotation getLeft() const { return left == nullptr ? this : left; }
  [[nodiscard]] CanonicalAnnotation getRight() const { return right == nullptr ? this : right; }
  [[nodiscard]] bool isLeaf() const { return left == nullptr && right == nullptr; }

  bool operator==(const Annotation &other) const {
     return value == other.value && left == other.left && right == other.right;
  }

 // ----------------------------------------------------------------------------------------
 // Helpers for working with annotated sets and relations.
 // TODO: Maybe have a proper class for annotated sets/relations and put the helper methods there.
 static CanonicalAnnotation newAnnotation(CanonicalRelation relation, AnnotationType value);
 static CanonicalAnnotation newAnnotation(CanonicalSet set, AnnotationType value);
 static AnnotatedSet getLeft(const AnnotatedSet &annotatedSet);
 static std::variant<AnnotatedSet, AnnotatedRelation> getRight(const AnnotatedSet &annotatedSet);
 // TODO: Add version for AnnotatedRelation?!
 static AnnotatedSet newAnnotatedSet(SetOperation operation, const AnnotatedSet &left,
                                     const AnnotatedSet &right);
 static AnnotatedSet newAnnotatedSet(SetOperation operation, const AnnotatedSet &annotatedSet,
                                     const AnnotatedRelation &annotatedRelation);

 friend std::hash<Annotation>;
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
