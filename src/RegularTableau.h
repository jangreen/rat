#pragma once
#include <fstream>
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

typedef std::vector<int> Renaming;
typedef std::optional<std::tuple<Literal, Renaming>> EdgeLabel;  // TODO: use

class RegularTableau {
 public:
  class Node {
   public:
    Node(std::initializer_list<Formula> formulas);
    explicit Node(FormulaSet formulas);

    FormulaSet formulas;
    std::vector<Node *> childNodes;
    std::map<Node *, EdgeLabel> parentNodes;
    std::vector<Node *> rootParents;      // parent nodes that are reachable by some root node
    Node *parentNode = nullptr;           // TODO: for counterexample first parent
    std::vector<int> parentNodeRenaming;  // TODO
    std::optional<Metastatement> parentNodeExpansionMeta;  // TODO
    std::vector<Metastatement> parentEquivalences;         // TODO
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

  RegularTableau(std::initializer_list<Formula> initalFormulas);
  explicit RegularTableau(FormulaSet initalFormulas);

  std::vector<Node *> rootNodes;
  std::unordered_set<std::unique_ptr<Node>, Node::Hash, Node::Equal> nodes;
  std::stack<Node *> unreducedNodes;
  static std::vector<Assumption> assumptions;

  // CALCULATE REDUCED DNF
  static DNF calcDNF(const Formula &initalFormula) {  // used only with clauses
    // TODO: more direct computation of DNF
    /*FormulaSet initalConjunction = {initalFormula};
    std::vector<FormulaSet> disjunction = {initalConjunction};
    int i = 0;
    int j = 0;
    while (i < disjunction.size()) {
      const auto &formulas = disjunction.at(i);
      while (j < formulas.size()) {
        const auto &formula = formulas.at(j);

        const auto reduced = formula.isReduced();
      }
    }

    for (const auto formula : formulas) {
      if (formula.isReduced) {
        reducedFormulas.push_back(formula);
      } else {
      }
    }*/

    Tableau tableau{initalFormula};
    return tableau.DNF();
  }
  bool expandNode(Node *node);
  void addNode(Node *parent, Clause clause);  // TODO: move in node class
  void addEdge(Node *parent, Node *child, EdgeLabel label);
  void updateRootParents(Node *node);
  void removeEdge(Node *parent, Node *child);
  std::optional<Relation> saturateRelation(const Relation &relation);
  std::optional<Relation> saturateIdRelation(const Assumption &assumption,
                                             const Relation &relation);
  void saturate(Clause &clause);
  bool isInconsistent(Node *node, Node *newNode);
  bool solve();
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
