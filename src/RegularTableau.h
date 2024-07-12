#pragma once
#include <fstream>
#include <map>
#include <stack>
#include <string>
#include <unordered_set>
#include <vector>

#include "Literal.h"
#include "RegularNode.h"
#include "Tableau.h"

class RegularTableau {
 public:
  RegularTableau(std::initializer_list<Literal> initialLiterals);
  explicit RegularTableau(const Cube &initialLiterals);
  bool validate(const RegularNode *currentNode = nullptr) const;

  std::unordered_set<RegularNode *> rootNodes;
  std::unordered_set<std::unique_ptr<RegularNode>, RegularNode::Hash, RegularNode::Equal> nodes;
  std::stack<RegularNode *> unreducedNodes;

  bool solve();
  std::pair<RegularNode *, Renaming> newNode(const Cube &cube);
  void newEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label);
  void newEpsilonEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label);
  void expandNode(RegularNode *node, Tableau *tableau);
  bool isInconsistent(RegularNode *parent, const RegularNode *child, const EdgeLabel &label);
  static void extractAnnotationexample(const RegularNode *openNode);

  void toDotFormat(std::ofstream &output, bool allNodes = true) const;
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