#pragma once

#include <optional>
#include <string>

/*
 *  Let T be a binary tree-like structure. A leaf annotation L is a function that maps each
 *  leaf of T to some value (the annotation) in a complete lattice, or possibly a special
 *  empty value for unannotated leafs.
 *
 *  The leaf annotation L can naively be represented as a binary tree-like structure itself:
 *  It is shaped like T but has values on the leafs.
 *  However, the LeafAnnotation class is based on a more compact representation based on
 *  summarization:
 *
 *  (1) If all leaves in a subtree of T have the same (possibly empty) value v,
 *      then we can put the value v on the corresponding root of this subtree
 *      in L and drop the whole subtree. This has the following consequences:
 *      (a) Leaf nodes in L do not necessarily correspond to leaf nodes in T but rather to
 *          subtress in T.
 *      (b) The shape of T is not captured in L; indeed, the same L now represents an infinitude of
 *          leaf annotations. In particular, a single leaf (and thus root) node with value v
 *          represents a constant leaf annotation for all possible T.
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
 *      - We use integer values as fixed annotation type.
 *
 *    NOTE on alternative definition: If the leaves of trees can be implicitly classified
 *    as annotatable and non-annotatable, then we can generate even more compact representations
 *    for leaf annotations. It does come with the disadvantage that the annotation itself cannot
 *    be used to avoid exploration of subtrees without annotatable leaves.
 */

template <typename AnnotationType>
class LeafAnnotation;
// typedef + template are not supported, i.e.:
// template <typename AnnotationType>
// typedef const Annotation<AnnotationType> *CanonicalAnnotation;
template <typename AnnotationType>
using CanonicalLeafAnnotation = const LeafAnnotation<AnnotationType> *;

template <typename AnnotationType>
std::optional<AnnotationType> joinValues(const std::optional<AnnotationType> &a,
                                         const std::optional<AnnotationType> &b);
template <typename AnnotationType>
std::string annotationTypetoString(const AnnotationType &value);

template <typename AnnotationType>
class LeafAnnotation {
  LeafAnnotation(std::optional<AnnotationType> value, CanonicalLeafAnnotation<AnnotationType> left,
                 CanonicalLeafAnnotation<AnnotationType> right);
  static CanonicalLeafAnnotation<AnnotationType> newAnnotation(
      const std::optional<AnnotationType> &value, CanonicalLeafAnnotation<AnnotationType> left,
      CanonicalLeafAnnotation<AnnotationType> right);

  const std::optional<AnnotationType> value;            // nullopt: subtree has default annotation
  const CanonicalLeafAnnotation<AnnotationType> left;   // is set iff operation unary/binary
  const CanonicalLeafAnnotation<AnnotationType> right;  // is set iff operation binary

  [[nodiscard]] bool validate() const;

 public:
  LeafAnnotation(const LeafAnnotation &other) = default;

  static CanonicalLeafAnnotation<AnnotationType> none();
  static CanonicalLeafAnnotation<AnnotationType> newLeaf(AnnotationType value);
  static CanonicalLeafAnnotation<AnnotationType> joinAnnotation(
      CanonicalLeafAnnotation<AnnotationType> left, CanonicalLeafAnnotation<AnnotationType> right);
  // static CanonicalLeafAnnotation<AnnotationType> min(
  //     CanonicalLeafAnnotation<AnnotationType> first,
  //     CanonicalLeafAnnotation<AnnotationType> second);

  [[nodiscard]] bool hasValue() const;
  [[nodiscard]] AnnotationType getValue() const;
  [[nodiscard]] std::optional<AnnotationType> getOptionalValue() const;
  [[nodiscard]] CanonicalLeafAnnotation<AnnotationType> getLeft() const;
  [[nodiscard]] CanonicalLeafAnnotation<AnnotationType> getRight() const;
  [[nodiscard]] bool isLeaf() const;
  [[nodiscard]] bool operator==(const LeafAnnotation &other) const;
  [[nodiscard]] std::string toString() const;

  friend std::hash<LeafAnnotation>;
};

template <typename AnnotationType>
struct std::hash<LeafAnnotation<AnnotationType>> {
  std::size_t operator()(const LeafAnnotation<AnnotationType> &annotation) const noexcept;
};

#include "LeafAnnotation.tpp"