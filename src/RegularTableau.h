#pragma once
#include <fstream>
#include <memory>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "Assumption.h"
#include "Metastatement.h"
#include "Relation.h"

class RegularTableau {
 public:
  class Node {
   public:
    Node(std::initializer_list<Relation> relations);
    Node(Clause relations);

    Clause relations;
    std::vector<std::tuple<std::shared_ptr<Node>, std::vector<int>>> childNodes;
    Node *parentNode = nullptr;
    bool closed = false;

    bool printed = false;  // prevent cycling in printing
    void toDotFormat(std::ofstream &output);

    bool operator==(const Node &otherNode) const;

    struct Hash {
      size_t operator()(const std::shared_ptr<Node> node) const;
    };

    struct Equal {
      bool operator()(const std::shared_ptr<Node> node1, const std::shared_ptr<Node> node2) const;
    };
  };

  RegularTableau(std::initializer_list<Relation> initalRelations);
  RegularTableau(Clause initalRelations);

  std::vector<std::shared_ptr<Node>> rootNodes;
  std::unordered_set<std::shared_ptr<Node>, Node::Hash, Node::Equal> nodes;
  std::stack<std::shared_ptr<Node>> unreducedNodes;
  static std::vector<Assumption> assumptions;

  static std::vector<Clause> DNF(const Clause &clause);
  bool expandNode(std::shared_ptr<Node> node);
  void addNode(std::shared_ptr<Node> parent, Clause clause);  // TODO: move in node class
  std::optional<Relation> saturateRelation(const Relation &relation);
  std::optional<Relation> saturateIdRelation(const Assumption &assumption,
                                             const Relation &relation);
  void saturate(Clause &clause);
  bool solve();

  void toDotFormat(std::ofstream &output) const;
  void exportProof(std::string filename) const;
};

namespace std {
template <>
struct hash<RegularTableau::Node> {
  std::size_t operator()(const RegularTableau::Node &node) const;
};
}  // namespace std
