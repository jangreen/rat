#pragma once
#include <map>
#include <set>
#include <vector>

#include "Literal.h"

class RegularNode;
typedef Renaming EdgeLabel;
typedef std::set<RegularNode *> NodeSet;

class RegularNode {
 private:
  explicit RegularNode(Cube cube);

 public:
  static std::pair<RegularNode *, Renaming> newNode(Cube cube);
  [[nodiscard]] bool validate() const;

  const Cube cube;  // must be ordered, should not be modified
  NodeSet children;
  NodeSet epsilonChildren;
  std::map<RegularNode *, EdgeLabel> parents;
  std::map<RegularNode *, EdgeLabel> epsilonParents;
  std::map<const RegularNode *, EdgeLabel> inconsistentChildrenChecked;
  bool closed = false;

  NodeSet rootParents;                     // parent nodes that are reachable by some root node
  RegularNode *firstParentNode = nullptr;  // for annotationexample extraction

  bool printed = false;  // prevent cycling in printing // FIXME refactor
  void toDotFormat(std::ofstream &output);

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