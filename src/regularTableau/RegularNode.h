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

  // =============== Metadata ===============
  bool closed = false;
  RegularNode *reachabilityTreeParent = nullptr;   // for dynamic multi source reachability
  std::map<const RegularNode *, EdgeLabel> inconsistentChildrenChecked;

  bool connect(RegularNode *child, const EdgeLabel& label, NodeSet &children, std::map<RegularNode *, EdgeLabel> &parents) {
    const auto [_, inserted] = children.insert(child);
    if (!inserted) {
      return false;
    }
    parents.insert({this, label});
    return true;
  }

 public:

  static std::pair<RegularNode *, Renaming> newNode(Cube cube);
  [[nodiscard]] bool validate() const;

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

  [[nodiscard]] bool isOpenLeaf() const {
    return children.empty() && epsilonChildren.empty() && !closed;
  }

  bool addChild(RegularNode *child, const EdgeLabel &label) {
    return connect(child, label, children, child->parents);
  }
  bool addEpsilonChild(RegularNode *child, const EdgeLabel &label) {
    return connect(child, label, epsilonChildren, child->epsilonParents);
  }
  void removeChild(RegularNode *child) {
    children.erase(child);
    child->parents.erase(this);
  }


  void toDotFormat(std::ofstream &output) const;

  // FIXME calculate cached lazy property
  // hashing and comparison is insensitive to label renaming
  bool operator==(const RegularNode &otherNode) const {
    return cube == otherNode.cube;
  }

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