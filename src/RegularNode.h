#pragma once
#include <map>
#include <vector>

#include "Literal.h"
typedef std::optional<Renaming> EdgeLabel;

class RegularNode {
 private:
  explicit RegularNode(Cube cube);

 public:
  static std::pair<RegularNode *, Renaming> newNode(Cube cube);
  [[nodiscard]] bool validate() const;

  const Cube cube;  // must be ordered, should not be modified
  std::vector<RegularNode *> childNodes;
  std::map<RegularNode *, std::vector<EdgeLabel>> parentNodes;  // TODO: use multimap instead?
  bool closed = false;

  std::vector<RegularNode *> rootParents;  // parent nodes that are reachable by some root node
  RegularNode *firstParentNode = nullptr;  // for annotationexample extraction
  EdgeLabel firstParentLabel;              // for annotationexample extraction

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