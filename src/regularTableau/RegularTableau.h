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
  std::unordered_set<std::unique_ptr<RegularNode>, RegularNode::Hash, RegularNode::Equal> nodes;
  std::unordered_set<RegularNode *> rootNodes;
  std::stack<RegularNode *> unreducedNodes;
  RegularNode *currentNode = nullptr;

  // ================== Node management ==================
  std::pair<RegularNode *, Renaming> newNode(const Cube &cube);

  void addEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label);
  void addEpsilonEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label);
  void removeEdge(RegularNode *parent, RegularNode *child) const;
  void addEdgeUpdateReachabilityTree(RegularNode *parent, RegularNode *child);
  void removeEdgeUpdateReachabilityTree(const RegularNode *parent, const RegularNode *child) const;

  // ================== Solving ==================
  void expandNode(RegularNode *node, Tableau *tableau);
  bool isInconsistent(RegularNode *parent, const RegularNode *child, const EdgeLabel &label);
  bool isInconsistentLazy(RegularNode *openLeaf);
  Cube getModel(const RegularNode *openLeaf) const;
  Renaming getRootRenaming(const RegularNode *node) const;
  bool isSpurious(const RegularNode *openLeaf) const;
  bool isReachableFromRoots(const RegularNode *node) const;
  typedef std::vector<RegularNode *> Path;
  void findAllPathsToRoots(RegularNode *node, Path &currentPath, std::vector<Path> &allPaths) const;

  // ================== Printing ==================
  void extractCounterexample(const RegularNode *openLeaf) const;
  void extractCounterexamplePath(const RegularNode *openLeaf) const;
  void toDotFormat(std::ofstream &output) const;
  void nodeToDotFormat(const RegularNode *node, std::ofstream &output) const;
  void exportDebug(const std::string &filename) const;

  // ================== Validation ==================
  bool validate() const;
  bool validateReachabilityTree() const;

 public:
  explicit RegularTableau(const Cube &initialLiterals);

  bool solve();
  void exportProof(const std::string &filename) const;
};
