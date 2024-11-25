#pragma once
#include <map>
#include <set>
#include <vector>

#include "../basic/Literal.h"

class RegularNode;
typedef Renaming EdgeLabel;
typedef std::set<RegularNode *> NodeSet;

class RegularNode {
 private:
  friend class RegularTableau;
  explicit RegularNode(Cube cube);

  const Cube cube;  // must be ordered, should not be modified
  NodeSet children;
  NodeSet epsilonChildren;
  std::map<RegularNode *, EdgeLabel> parents;
  std::map<RegularNode *, EdgeLabel> epsilonParents;
  bool closed = false;
  RegularNode *reachabilityTreeParent = nullptr;  // for dynamic single source reachability

  // =========== Node manipulation ===========
  static std::pair<RegularNode *, Renaming> newNode(Cube cube);
  bool newChild(RegularNode *child, const EdgeLabel &label);
  bool newEpsilonChild(RegularNode *child, const EdgeLabel &label);

  // ============== Validation ===============
  [[nodiscard]] bool validate() const;

 public:
  [[nodiscard]] const Cube &getCube() const { return cube; }
  [[nodiscard]] const NodeSet &getChildren() const { return children; }
  [[nodiscard]] const NodeSet &getEpsilonChildren() const { return epsilonChildren; }
  [[nodiscard]] const std::map<RegularNode *, EdgeLabel> &getParents() const { return parents; }
  [[nodiscard]] const std::map<RegularNode *, EdgeLabel> &getEpsilonParents() const {
    return epsilonParents;
  }
  const EdgeLabel &getLabelForChild(const RegularNode *child) const {
    return child->parents.at(const_cast<RegularNode *>(this));
  }
  [[nodiscard]] bool isLeaf() const { return children.empty() && epsilonChildren.empty(); }
  [[nodiscard]] bool isOpenLeaf() const { return isLeaf() && !closed; }

  void toDotFormat(std::ofstream &output) const;

  // FIXME calculate cached lazy property
  // hashing and comparison is insensitive to label renaming
  bool operator==(const RegularNode &otherNode) const { return cube == otherNode.cube; }

  struct Hash {
    size_t operator()(const std::unique_ptr<RegularNode> &node) const;
  };

  // special equal function that is different from ==
  struct Equal {
    bool operator()(const std::unique_ptr<RegularNode> &node1,
                    const std::unique_ptr<RegularNode> &node2) const {
      return *node1 == *node2;
    }
  };
};

template <>
struct std::hash<RegularNode> {
  std::size_t operator()(const RegularNode &node) const noexcept {
    size_t seed = 0;
    assert(std::ranges::is_sorted(node.getCube()));
    for (const auto &literal : node.getCube()) {
      boost::hash_combine(seed, std::hash<Literal>()(literal));
    }
    return seed;
  }
};