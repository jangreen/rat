#pragma once
#include <fstream>
#include <queue>
#include <stack>
#include <unordered_set>
#include <vector>

#include "../basic/Literal.h"
#include "../basic/model/Model.h"
#include "../tableau/Tableau.h"
#include "RegularNode.h"

// heuristic which determines which node is considered first
struct RegularNodeHeuristic {
  bool operator()(const RegularNode *first, const RegularNode *second) const;
};

class RegularTableau {
  typedef std::vector<RegularNode *> Path;
  const Cube initialCube;
  const std::unique_ptr<RegularNode> rootNode;
  std::unordered_set<std::unique_ptr<RegularNode>, RegularNode::Hash, RegularNode::Equal> nodes;
  std::priority_queue<RegularNode *, std::vector<RegularNode *>, RegularNodeHeuristic>
      unreducedNodes;
  RegularNode *currentNode = nullptr;

  // ================== Node management ==================
  std::pair<RegularNode *, Renaming> newNode(const Cube &cube);
  void newEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label);
  void newEpsilonEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label);
  void newChildren(RegularNode *node, const DNF &dnf);
  void newEpsilonChildren(RegularNode *node, const DNF &dnf);
  void removeEdge(RegularNode *parent, RegularNode *child) const;
  void removeChildren(RegularNode *parent) const;
  void newEdgeUpdateReachabilityTree(RegularNode *parent, RegularNode *child);
  void removeEdgeUpdateReachabilityTree(const RegularNode *parent, const RegularNode *child) const;

  // ================== Solving ==================
  bool expandNode(RegularNode *node);
  void expandNodeInternal(RegularNode *node, Tableau *tableau);
  bool isInconsistent(RegularNode *parent, const RegularNode *child, const EdgeLabel &label);
  bool isInconsistentLazy(RegularNode *openLeaf);
  bool saturationLazy(RegularNode *openLeaf);
  bool saturateNodeLazy(RegularNode *node, const Model &model, const Model &saturatedModel);
  Model getModel(const RegularNode *openLeaf) const;
  Renaming getRootRenaming(const RegularNode *node) const;
  bool isSpurious(const RegularNode *openLeaf) const;
  void fixLazy();
  bool isReachableFromRoots(const RegularNode *node) const;

  // ================== Printing ==================
  void exportCounterexamplePath(const RegularNode *openLeaf) const;
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
