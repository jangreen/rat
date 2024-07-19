#pragma once
#include <unordered_set>

#include "../basic/Literal.h"
#include "TableauNode.h"
#include "Worklist.h"

class Tableau {
 public:
  explicit Tableau(const Cube &cube);

  // ================== Core algorithm ==================
  DNF computeDnf();
  // methods for regular reasoning
  bool tryApplyModalRuleOnce();

  // ================== Printing ==================
  void toDotFormat(std::ofstream &output) const;
  void exportProof(const std::string &filename) const;

  // ================== Debugging ==================
  [[nodiscard]] bool validate() const;
  void exportDebug(const std::string &filename) const {
    if constexpr (DEBUG) {
      exportProof(filename);
    }
  }

 private:
  friend class Node;
  Worklist unreducedNodes;
  std::unique_ptr<Node> rootNode;

  [[nodiscard]] const Node *getRoot() const { return rootNode.get(); }

  void normalize();
  void deleteNode(Node *node);

  void renameBranches(Node *node);
  Node *renameBranchesInternalUp(Node *lastSharedNode, int from, int to,
                                 std::unordered_set<Literal> &allRenamedLiterals,
                                 std::unordered_map<const Node *, Node *> &originalToCopy);
  void renameBranchesInternalDown(Node *node, const Renaming &renaming,
                                  std::unordered_set<Literal> &allRenamedLiterals,
                                  const std::unordered_map<const Node *, Node *> &originalToCopy,
                                  std::unordered_set<const Node *> &unrollingParents);
};
