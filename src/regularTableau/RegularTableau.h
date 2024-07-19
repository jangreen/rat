#pragma once
#include <fstream>
#include <stack>
#include <unordered_set>
#include <vector>

#include "../basic/Literal.h"
#include "../tableau/Tableau.h"
#include "RegularNode.h"

class RegularTableau {
 private:
  bool isReachableFromRoots(const RegularNode *node) const;

 public:
  RegularTableau(std::initializer_list<Literal> initialLiterals);
  explicit RegularTableau(const Cube &initialLiterals);
  bool validate(const RegularNode *currentNode = nullptr) const;
  [[nodiscard]] bool validateReachabilityTree() const;

  std::unordered_set<RegularNode *> rootNodes;
  std::unordered_set<std::unique_ptr<RegularNode>, RegularNode::Hash, RegularNode::Equal> nodes;
  std::stack<RegularNode *> unreducedNodes;

  bool solve();
  std::pair<RegularNode *, Renaming> newNode(const Cube &cube);
  void newEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label);
  void removeEdge(RegularNode *parent, RegularNode *child) const;
  void newEpsilonEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label);
  void expandNode(RegularNode *node, Tableau *tableau);
  bool isInconsistent(RegularNode *parent, const RegularNode *child, const EdgeLabel &label);
  bool isInconsistentLazy(RegularNode *openLeaf);
  typedef std::vector<RegularNode *> Path;
  void findAllPathsToRoots(RegularNode *node, Path &currentPath, std::vector<Path> &allPaths) const;
  void newEdgeUpdateReachabilityTree(RegularNode *parent, RegularNode *child);
  void removeEdgeUpdateReachabilityTree(const RegularNode *parent, const RegularNode *child) const;
  void extractCounterexample(const RegularNode *openLeaf) const;
  void extractCounterexamplePath(const RegularNode *openLeaf) const;

  void toDotFormat(std::ofstream &output) const;
  void nodeToDotFormat(const RegularNode *node, std::ofstream &output) const;
  void exportProof(const std::string &filename) const;
  void exportDebug(const std::string &filename) const;
};

// helper
// TODO: move into general vector class
template <typename T>
bool isSubset(std::vector<T> smallerSet, std::vector<T> largerSet) {
  std::unordered_set<T> set(std::make_move_iterator(largerSet.begin()),
                            std::make_move_iterator(largerSet.end()));
  return std::ranges::all_of(smallerSet, [&](T &element) { return set.contains(element); });
};