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
#include "Tableau.h"

class RegularTableau {
 public:
  class Node {
   public:
    Node(std::initializer_list<Relation> relations);
    explicit Node(Clause relations);

    Clause relations;
    std::vector<std::tuple<Node *, std::vector<int>>> childNodes;
    Node *parentNode = nullptr;
    std::vector<int> parentNodeRenaming;                   // TODO
    std::optional<Metastatement> parentNodeExpansionMeta;  // TODO
    std::vector<Metastatement> parentEquivalences;         // TODO
    Node *epsilonChild;
    bool closed = false;

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

  RegularTableau(std::initializer_list<Relation> initalRelations);
  explicit RegularTableau(Clause initalRelations);

  std::vector<Node *> rootNodes;
  std::unordered_set<std::unique_ptr<Node>, Node::Hash, Node::Equal> nodes;
  std::stack<Node *> unreducedNodes;
  static std::vector<Assumption> assumptions;

  template <typename ClauseType = Clause>
  static std::vector<ClauseType> DNF(const Clause &clause) {
    Tableau tableau{clause};
    return tableau.DNF<ClauseType>();
  }
  bool expandNode(Node *node);
  void addNode(Node *parent, ExtendedClause clause,
               const std::optional<Metastatement> &metastatement =
                   std::nullopt);  // TODO: move in node class
  std::optional<Relation> saturateRelation(const Relation &relation);
  std::optional<Relation> saturateIdRelation(const Assumption &assumption,
                                             const Relation &relation);
  void saturate(Clause &clause);
  bool solve();
  void extractCounterexample(Node *openNode);

  void toDotFormat(std::ofstream &output) const;
  void exportProof(std::string filename) const;
};

namespace std {
template <>
struct hash<RegularTableau::Node> {
  std::size_t operator()(const RegularTableau::Node &node) const;
};
}  // namespace std
