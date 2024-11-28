#pragma once
#include <unordered_set>

#include "../Stats.h"
#include "../basic/Literal.h"
#include "TableauNode.h"
#include "Worklist.h"

class Tableau {
  friend class Node;
  Worklist unreducedNodes;
  std::unique_ptr<Node> rootNode;
  std::unordered_map<const Node *, std::unordered_set<Node *>> crossReferenceMap;

  void normalize();

  // branch manipulation
  void deleteNode(Node *node);
  void renameBranches(Node *equalityNode);
  Node *renameBranchesInternalUp(Node *lastSharedNode, int from, int to,
                                 std::unordered_set<Literal> &allRenamedLiterals,
                                 std::unordered_map<const Node *, Node *> &originalToCopy);
  void renameBranchesInternalDown(Node *equalityNode, Node *node, const Renaming &renaming,
                                  std::unordered_set<Literal> &allRenamedLiterals,
                                  const std::unordered_map<const Node *, Node *> &originalToCopy,
                                  std::unordered_set<const Node *> &unrollingParents);

  // simpification
  void removeUselessLiterals() const;

 public:
  explicit Tableau(const Cube &cube);
  ~Tableau();
  [[nodiscard]] const Node *getRoot() const;

  // ================== Core algorithm ==================
  [[nodiscard]] DNF computeDnf();
  [[nodiscard]] bool tryApplyModalRuleOnce(int applyToEvent);

  // ================== Printing ==================
  void toDotFormat(std::ofstream &output) const;
  void exportProof(const std::string &filename) const;

  // ================== Debugging ==================
  [[nodiscard]] bool validate() const;
  void exportDebug(const std::string &filename) const;
};
