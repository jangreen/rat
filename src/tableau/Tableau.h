#pragma once
#include <unordered_set>

#include "../Stats.h"
#include "../basic/Literal.h"
#include "TableauNode.h"
#include "Worklist.h"

class Tableau {
 public:
  explicit Tableau(const Cube &cube);

  // ================== Core algorithm ==================
  void normalize(bool weakenening = true);
  DNF computeDnf(bool weakenening = true);
  [[nodiscard]] DNF extractDNF() const;
  [[nodiscard]] const Node *getRoot() const { return rootNode.get(); }
  // methods for regular reasoning
  bool tryApplyModalRuleOnce(int applyToEvent);

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
  std::unordered_map<const Node *, std::unordered_set<Node *>> crossReferenceMap;
  std::unique_ptr<Node> rootNode;

  void deleteNode(Node *node);

  void renameBranches(Node *node);
  Node *renameBranchesInternalUp(Node *lastSharedNode, int from, int to,
                                 std::unordered_set<Literal> &allRenamedLiterals,
                                 std::unordered_map<const Node *, Node *> &originalToCopy);
  void renameBranchesInternalDown(Node *nodeWithEquality, Node *node, const Renaming &renaming,
                                  std::unordered_set<Literal> &allRenamedLiterals,
                                  const std::unordered_map<const Node *, Node *> &originalToCopy,
                                  std::unordered_set<const Node *> &unrollingParents);

  void removeUselessLiterals() {
    boost::container::flat_set<SetOfSets> activePairCubes = {{}};
    Stats::counter("removeUselessLiterals tabl").reset();
    exportDebug("debug");
    rootNode->removeUselessLiterals(activePairCubes);
  }
};
