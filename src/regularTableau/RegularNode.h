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

  NodeSet children;
  NodeSet epsilonChildren;
  std::map<RegularNode *, EdgeLabel> parents;
  std::map<RegularNode *, EdgeLabel> epsilonParents;
  std::map<const RegularNode *, EdgeLabel> inconsistentChildrenChecked;

 public:
  static std::pair<RegularNode *, Renaming> newNode(Cube cube);
  [[nodiscard]] bool validate() const;

  const Cube cube;  // must be ordered, should not be modified
  bool closed = false;
  [[nodiscard]] const NodeSet &getChildren() const { return children; }
  [[nodiscard]] const NodeSet &getEpsilonChildren() const { return epsilonChildren; }
  [[nodiscard]] const std::map<RegularNode *, EdgeLabel> &getParents() const { return parents; }
  [[nodiscard]] const std::map<RegularNode *, EdgeLabel> &getEpsilonParents() const {
    return epsilonParents;
  }
  const EdgeLabel &getLabelForChild(const RegularNode *child) const {
    return child->parents.at(const_cast<RegularNode *>(this));
  }
  bool newChild(RegularNode *child, const EdgeLabel &label) {
    const auto [_, inserted] = children.insert(child);
    if (!inserted) {
      return false;
    }
    child->parents.insert({this, label});
    return true;
  }
  bool newEpsilonChild(RegularNode *child, const EdgeLabel &label) {
    const auto [_, inserted] = epsilonChildren.insert(child);
    if (!inserted) {
      return false;
    }
    child->epsilonParents.insert({this, label});
    return true;
  }
  void removeChild(RegularNode *child) {
    children.erase(child);
    child->parents.erase(this);
  }

  // for dynamic multi source reachability
  RegularNode *reachabilityTreeParent = nullptr;
  [[nodiscard]] bool isOpenLeaf() const { return children.empty() && !closed; }

  void toDotFormat(std::ofstream &output) const;

  bool operator==(const RegularNode &otherNode) const;

  struct Hash {
    size_t operator()(const std::unique_ptr<RegularNode> &node) const;
  };

  // special equal function that is different from ==
  struct Equal {
    bool operator()(const std::unique_ptr<RegularNode> &node1,
                    const std::unique_ptr<RegularNode> &node2) const;
  };
};

template <>
struct std::hash<RegularNode> {
  std::size_t operator()(const RegularNode &node) const noexcept {
    size_t seed = 0;
    assert(std::ranges::is_sorted(node.cube));
    for (const auto &literal : node.cube) {
      boost::hash_combine(seed, std::hash<Literal>()(literal));
    }
    return seed;
  }
};