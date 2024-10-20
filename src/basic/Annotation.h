#pragma once
#include <sys/stat.h>

#include <boost/container/flat_set.hpp>
#include <optional>
#include <string>
#include <unordered_set>

template <typename AnnotationType>
class Annotation;
// typedef + template are not supported
// template <typename AnnotationType>
// typedef const Annotation<AnnotationType> *CanonicalAnnotation;
template <typename AnnotationType>
using CanonicalAnnotation = const Annotation<AnnotationType> *;
typedef std::pair<int, int> SaturationAnnotation;  // <id, base> saturation bounds

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
 *      - We use integer values as fixed annotation type.
 *
 *    NOTE on alternative definition: If the leaves of trees can be implicitly classified
 *    as annotatable and non-annotatable, then we can generate even more compact representations
 *    for leaf annotations. It does come with the disadvantage that the annotation itself cannot
 *    be used to avoid exploration of subtrees without annotatable leaves.
 */
namespace {

template <typename AnnotationType>
std::optional<AnnotationType> meet(const std::optional<AnnotationType> &a,
                                   const std::optional<AnnotationType> &b);
template <>
std::optional<SaturationAnnotation> meet(const std::optional<SaturationAnnotation> &a,
                                         const std::optional<SaturationAnnotation> &b) {
  if (!a.has_value() && !b.has_value()) {
    return std::nullopt;
  }
  if (!a.has_value()) {
    return b;
  }
  if (!b.has_value()) {
    return a;
  }
  // use max: complex terms should indicate if some subterm can be saturated
  return SaturationAnnotation{std::max(a->first, b->first), std::max(a->second, b->second)};
}
}  // namespace

template <typename AnnotationType>
class Annotation {
 private:
  Annotation(std::optional<AnnotationType> value, const CanonicalAnnotation<AnnotationType> left,
             const CanonicalAnnotation<AnnotationType> right)
      : value(std::move(value)), left(left), right(right) {}

  [[nodiscard]] bool validate() const;

  const std::optional<AnnotationType> value;        // nullopt: subtree has default annotation
  const CanonicalAnnotation<AnnotationType> left;   // is set iff operation unary/binary
  const CanonicalAnnotation<AnnotationType> right;  // is set iff operation binary

 public:
  // WARNING: Never call these constructors: they are only public for technical reasons
  Annotation(const Annotation &other) = default;
  // Annotation(const Annotation &&other) = default;

  static CanonicalAnnotation<AnnotationType> newAnnotation(
      const std::optional<AnnotationType> &value, const CanonicalAnnotation<AnnotationType> left,
      const CanonicalAnnotation<AnnotationType> right) {
    assert(value.has_value() || (left == nullptr && right == nullptr));  // No value => Leaf

    static std::unordered_set<Annotation> canonicalizer;
    auto [iter, created] = canonicalizer.insert(std::move(Annotation(value, left, right)));
    return &*iter;
  }
  static CanonicalAnnotation<AnnotationType> newAnnotation(
      const std::optional<AnnotationType> &value, CanonicalAnnotation<AnnotationType> left) {
    return newAnnotation(value, left, nullptr);
  }
  static CanonicalAnnotation<AnnotationType> none() {
    static CanonicalAnnotation<AnnotationType> cached = nullptr;
    if (cached == nullptr) {
      cached = newAnnotation(std::nullopt, nullptr, nullptr);
    }
    return cached;
  }
  static CanonicalAnnotation<AnnotationType> newLeaf(AnnotationType value) {
    return newAnnotation(value, nullptr, nullptr);
  }
  static CanonicalAnnotation<AnnotationType> meetAnnotation(
      const CanonicalAnnotation<AnnotationType> left,
      const CanonicalAnnotation<AnnotationType> right) {
    assert(left != nullptr && right != nullptr);
    if (left == right && left->isLeaf()) {
      // Two constant annotations (leafs) together form just the same constant annotation.
      return left;
    }

    const auto annotationValue = meet(left->getValue(), right->getValue());
    assert(annotationValue.has_value());  // TODO: maybe return none();?
    return newAnnotation(annotationValue.value(), left, right);
  }

  // make non-optional return type
  [[nodiscard]] std::optional<AnnotationType> getValue() const { return value; }
  [[nodiscard]] CanonicalAnnotation<AnnotationType> getLeft() const {
    return left == nullptr ? this : left;
  }
  [[nodiscard]] CanonicalAnnotation<AnnotationType> getRight() const {
    return right == nullptr ? this : right;
  }
  [[nodiscard]] CanonicalAnnotation<AnnotationType> getLeftInteral() const { return left; }
  [[nodiscard]] CanonicalAnnotation<AnnotationType> getRightInteral() const { return right; }
  [[nodiscard]] bool isLeaf() const { return left == nullptr && right == nullptr; }
  static CanonicalAnnotation<AnnotationType> min(const CanonicalAnnotation<AnnotationType> first,
                                                 const CanonicalAnnotation<AnnotationType> second) {
    if (first->isLeaf() && first->getValue() <= second->getValue()) {
      return first;
    }
    if (second->isLeaf() && second->getValue() <= first->getValue()) {
      return second;
    }
    const auto minLeft = min(first->getLeft(), second->getLeft());
    const auto minRight = min(first->getRight(), second->getRight());

    return meetAnnotation(minLeft, minRight);
  }

  bool operator==(const Annotation &other) const {
    return value == other.value && left == other.left && right == other.right;
  }

  friend std::hash<Annotation>;
};

template <>
struct std::hash<SaturationAnnotation> {
  std::size_t operator()(const SaturationAnnotation &pair) const noexcept {
    return (static_cast<size_t>(pair.first) << 32) | pair.second;
  }
};

template <typename AnnotationType>
struct std::hash<Annotation<AnnotationType>> {
  std::size_t operator()(const Annotation<AnnotationType> &annotation) const noexcept {
    const size_t leftHash = hash<CanonicalAnnotation<AnnotationType>>()(annotation.left);
    const size_t rightHash = hash<CanonicalAnnotation<AnnotationType>>()(annotation.right);
    const size_t annotationHash = hash<std::optional<AnnotationType>>()(annotation.value);

    return (leftHash ^ (rightHash << 1) >> 1) + 31 * annotationHash;
  }
};
