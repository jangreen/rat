#pragma once
#include <unordered_set>

#include "Literal.h"
#include "TableauNode.h"

class Tableau {
 public:
  explicit Tableau(const Cube &cube);
  [[nodiscard]] bool validate() const;

  Worklist unreducedNodes;

  [[nodiscard]] const Node *getRoot() const { return rootNode.get(); }
  bool solve(int bound = -1);
  void removeNode(Node *node);
  void renameBranches(Node *node);

  // methods for regular reasoning
  bool applyRuleA();
  DNF dnf();

  void toDotFormat(std::ofstream &output) const;
  void exportProof(const std::string &filename) const;
  void exportDebug(const std::string &filename) const;

 private:
  std::unique_ptr<Node> rootNode;

  Node *renameBranchesInternalUp(Node *lastSharedNode, int from, int to,
                                 std::unordered_set<Literal> &allRenamedLiterals,
                                 std::unordered_map<const Node *, Node *> &originalToCopy);
  void renameBranchesInternalDown(Node *node, const Renaming &renaming,
                                  std::unordered_set<Literal> &allRenamedLiterals,
                                  const std::unordered_map<const Node *, Node *> &originalToCopy,
                                  std::unordered_set<const Node *> &unrollingParents);
};
