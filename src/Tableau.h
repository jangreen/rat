#pragma once

#include "Literal.h"
#include "TableauNode.h"

class Tableau {
 public:
  explicit Tableau(const Cube &cube);
  [[nodiscard]] bool validate() const;

  std::unique_ptr<const Node> rootNode;
  Worklist unreducedNodes;

  bool solve(int bound = -1);
  void removeNode(Node *node);
  void renameBranch(const Node *leaf);
  void appendBranch(Node *node, const DNF &dnf);
  inline void appendBranch(Node *node, const Cube &cube) { appendBranch(node, DNF{cube}); }
  inline void appendBranch(Node *node, const Literal &literal) {
    appendBranch(node, Cube{literal});
  }
  std::optional<DNF> applyRule(Node *node, bool modalRule = false);
  void inferModal(Node *node);
  void inferModalTop(Node *node);
  void inferModalAtomic(Node *node);
  void replaceNegatedTopOnBranch(Node *node, const std::vector<int> &events);

  // methods for regular reasoning
  bool applyRuleA();
  DNF dnf();

  void toDotFormat(std::ofstream &output) const;
  void exportProof(const std::string &filename) const;
  void exportDebug(const std::string &filename) const;

  // helper
  static Cube substitute(const Literal &literal, CanonicalSet search, CanonicalSet replace) {
    int c = 1;
    Literal copy = literal;
    Cube newLiterals;
    while (copy.substitute(search, replace, c)) {
      newLiterals.push_back(copy);
      copy = literal;
      c++;
    }
    return newLiterals;
  }

 private:
  void closeBranch(Node *node);
  void appendBranchInternalUp(const Node *node, DNF &dnf);
  void appendBranchInternalDown(Node *node, DNF &dnf);
};
