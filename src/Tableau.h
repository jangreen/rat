#pragma once
#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "Metastatement.h"
#include "Relation.h"

class Tableau {
 public:
  class Node {
   public:
    Node(Tableau *tableau, const Relation &&relation);
    Node(Tableau *tableau, const Metastatement &&metastatement);

    Tableau *tableau;
    std::optional<Relation> relation = std::nullopt;
    std::optional<Metastatement> metastatement = std::nullopt;
    std::unique_ptr<Node> leftNode = nullptr;
    std::unique_ptr<Node> rightNode = nullptr;
    Node *parentNode = nullptr;
    Node *parentMetastatement = nullptr;  // metastatement chain
    bool closed = false;

    bool isClosed();
    bool isLeaf() const;
    void appendBranches(const Relation &leftRelation, const Relation &rightRelation);
    void appendBranches(const Relation &leftRelation);
    void appendBranches(const Metastatement &metastatement);

    void toDotFormat(std::ofstream &output) const;

    struct CompareNodes {
      bool operator()(const Node *left, const Node *right) const;
    };
  };

  Tableau(std::initializer_list<Relation> initalRelations);
  explicit Tableau(Clause initalRelations);

  std::unique_ptr<Node> rootNode;
  std::priority_queue<Node *, std::vector<Node *>, Node::CompareNodes> unreducedNodes;

  bool applyRule(Node *node);
  bool solve(int bound = 30);

  // methods for regular reasoning
  std::vector<Clause> DNF();
  bool applyModalRule();
  Clause calcReuqest();

  void toDotFormat(std::ofstream &output) const;
  void exportProof(std::string filename) const;
};
