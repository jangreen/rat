#include <unordered_set>

#include "Literal.h"
#include "Rules.h"
#include "TableauNode.h"

class Tableau {
 public:
  explicit Tableau(const Cube &cube);
  [[nodiscard]] bool validate() const;

  std::unique_ptr<Node> rootNode;
  Worklist unreducedNodes;

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
  Node *renameBranchesInternalUp(Node *node, int from, int to,
                                 std::unordered_set<Literal> &allRenamedLiterals);
  void renameBranchesInternalDown(Node *node, const Renaming &renaming,
                                  std::unordered_set<Literal> &allRenamedLiterals);
};
