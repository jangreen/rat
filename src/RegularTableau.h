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
#include "Literal.h"
#include "Tableau.h"

typedef std::tuple<Cube, Renaming> EdgeLabel;

class RegularTableau {
 public:
  class Node {
   public:
    Node(std::initializer_list<Literal> cube);
    explicit Node(Cube cube);

    Cube cube;
    std::vector<Node *> childNodes;
    std::map<Node *, std::vector<EdgeLabel>> parentNodes;  // TODO: use multimap instead?
    bool closed = false;

    std::vector<Node *> rootParents;  // parent nodes that are reachable by some root node
    Node *firstParentNode = nullptr;  // for counterexample extraction
    EdgeLabel firstParentLabel;       // for counterexample extraction

    bool printed = false;  // prevent cycling in printing
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

  std::vector<Node *> rootNodes;
  std::unordered_set<std::unique_ptr<Node>, Node::Hash, Node::Equal> nodes;
  std::stack<Node *> unreducedNodes;

  static int saturationBoundId;
  static int saturationBoundBase;

  bool solve();
  Node *addNode(Cube cube, EdgeLabel &label);
  void addEdge(Node *parent, Node *child, EdgeLabel label);
  void expandNode(Node *node, Tableau *tableau);
  bool isInconsistent(Node *parent, Node *child, EdgeLabel label);
  static void extractCounterexample(Node *openNode);
  static void saturate(DNF &dnf);

  void toDotFormat(std::ofstream &output, bool allNodes = true) const;
  void exportProof(const std::string &filename) const;

  // helper
  static Renaming rename(Cube &cube) {
    // calculate renaming
    std::sort(cube.begin(), cube.end());
    Renaming renaming;
    for (auto &literal : cube) {
      for (const auto &l : literal.labels()) {
        if (std::find(renaming.begin(), renaming.end(), l) == renaming.end()) {
          renaming.push_back(l);
        }
      }
    }

    for (auto &literal : cube) {
      literal.rename(renaming, false);
    }
    return renaming;
  }
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