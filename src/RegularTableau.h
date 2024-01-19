#pragma once
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "Assumption.h"
#include "Formula.h"
#include "Tableau.h"

typedef std::vector<Formula> FormulaSet;
typedef std::vector<Formula> EdgeLabel;

class RegularTableau {
 public:
  class Node {
   public:
    Node(std::initializer_list<Formula> formulas);
    explicit Node(FormulaSet formulas);

    FormulaSet formulas;
    std::vector<Node *> childNodes;
    std::map<Node *, std::vector<EdgeLabel>> parentNodes;  // TODO: use multimap instead?
    bool closed = false;

    std::vector<Node *> rootParents;  // parent nodes that are reachable by some root node
    Node *firstParentNode = nullptr;  // for counterexample extration
    EdgeLabel firstParentLabel;       // for counterexample extration

    bool printed = false;  // prevent cycling in printing
    void toDotFormat(std::ofstream &output);

    bool operator==(const Node &otherNode) const;

    struct Hash {
      size_t operator()(const std::unique_ptr<Node> &node) const;
    };

    struct Equal {
      bool operator()(const std::unique_ptr<Node> &node1, const std::unique_ptr<Node> &node2) const;
    };
  };

  RegularTableau(std::initializer_list<Formula> initalFormulas);
  explicit RegularTableau(FormulaSet initalFormulas);

  std::vector<Node *> rootNodes;
  std::unordered_set<std::unique_ptr<Node>, Node::Hash, Node::Equal> nodes;
  std::stack<Node *> unreducedNodes;
  static std::vector<Assumption> emptinessAssumptions;
  static std::vector<Assumption> idAssumptions;
  static std::map<std::string, Assumption> baseAssumptions;

  bool solve();
  Node *addNode(FormulaSet clause);  // TODO: assert clause
  void addEdge(Node *parent, Node *child, EdgeLabel label);
  void removeEdge(Node *parent, Node *child, EdgeLabel label);

  bool checkAndExpandNode(Node *node);
  void expandNode(Node *node, Tableau *tableau);
  void updateRootParents(Node *node);
  void saturate(FormulaSet &formulas);
  void saturate(GDNF &dnf);
  FormulaSet purge(const FormulaSet &formulas, FormulaSet &dropped, FormulaSet &label) const;
  std::optional<FormulaSet> checkInconsistency(Node *parent, const FormulaSet &newFormulas);
  bool isInconsistent(Node *parent, Node *child, EdgeLabel label);
  void extractCounterexample(Node *openNode);

  void toDotFormat(std::ofstream &output, bool allNodes = true) const;
  void exportProof(std::string filename) const;
};

namespace std {
template <>
struct hash<RegularTableau::Node> {
  std::size_t operator()(const RegularTableau::Node &node) const;
};
}  // namespace std

// helper
// TODO: move into general vector class
template <typename T>
bool isSubset(std::vector<T> smallerSet, std::vector<T> largerSet) {
  std::sort(largerSet.begin(), largerSet.end());
  std::sort(smallerSet.begin(), smallerSet.end());
  smallerSet.erase(unique(smallerSet.begin(), smallerSet.end()), smallerSet.end());
  return std::includes(largerSet.begin(), largerSet.end(), smallerSet.begin(), smallerSet.end());
};
