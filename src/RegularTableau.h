#pragma once
#include <fstream>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <unordered_set>
#include <vector>

#include "Assumption.h"
#include "Literal.h"
#include "Tableau.h"

typedef std::pair<Cube, Renaming> EdgeLabel;

class RegularTableau {
 public:
  class Node {
   public:
    explicit Node(Cube cube);

    Cube cube;          // must be ordered // FIXME assert this
    Renaming renaming;  // renaming for ordered cube
    std::vector<Node *> childNodes;
    std::map<Node *, std::vector<EdgeLabel>> parentNodes;  // TODO: use multimap instead?
    bool closed = false;

    std::vector<Node *> rootParents;  // parent nodes that are reachable by some root node
    Node *firstParentNode = nullptr;  // for counterexample extraction
    EdgeLabel firstParentLabel;       // for counterexample extraction

    bool printed = false;  // prevent cycling in printing // FIXME refactor
    void toDotFormat(std::ofstream &output);

    bool operator==(const Node &otherNode) const;

    struct Hash {
      size_t operator()(const std::unique_ptr<Node> &node) const;
    };

    // special equal function that is different from ==
    struct Equal {
      bool operator()(const std::unique_ptr<Node> &node1, const std::unique_ptr<Node> &node2) const;
    };
  };

  RegularTableau(std::initializer_list<Literal> initialLiterals);
  explicit RegularTableau(const Cube &initialLiterals);
  bool validate(Node *currentNode = nullptr) const;

  std::vector<Node *> rootNodes;
  std::unordered_set<std::unique_ptr<Node>, Node::Hash, Node::Equal> nodes;
  std::stack<Node *> unreducedNodes;

  bool solve();
  Node *addNode(const Cube &cube, EdgeLabel &label);
  void addEdge(Node *parent, Node *child, const EdgeLabel &label);
  void expandNode(Node *node, Tableau *tableau);
  bool isInconsistent(Node *parent, const Node *child, EdgeLabel label);
  static void extractCounterexample(const Node *openNode);
  static void saturate(DNF &dnf);

  void toDotFormat(std::ofstream &output, bool allNodes = true) const;
  void exportProof(const std::string &filename) const;
};

template <>
struct std::hash<RegularTableau::Node> {
  std::size_t operator()(const RegularTableau::Node &node) const noexcept;
};  // namespace std

// helper
// TODO: move into general vector class
template <typename T>
bool isSubset(std::vector<T> smallerSet, std::vector<T> largerSet) {
  std::unordered_set<T> set(std::make_move_iterator(largerSet.begin()),
                            std::make_move_iterator(largerSet.end()));
  return std::ranges::all_of(smallerSet, [&](T &element) { return set.contains(element); });
};