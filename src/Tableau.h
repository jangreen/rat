#pragma once
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "Literal.h"
#include "Rules.h"

// Uncomment to use the old worklist implementation (ordered sets).
// #define WORKLIST_ALTERNATIVE

class Tableau {
  // ============================================================================
  // =================================== Node ===================================
  // ============================================================================
 public:
  class Worklist;
  class Node {
   private:
#ifndef WORKLIST_ALTERNATIVE
    // Intrusive worklist design for O(1) insertion and removal.
    friend class Worklist;
    Node *nextInWorkList = nullptr;
    Node *prevInWorkList = nullptr;
#endif
   public:
    Node(Node *parent, Literal literal);
    explicit Node(const Node *other) = delete;
    ~Node();
    [[nodiscard]] bool validate() const;
    [[nodiscard]] bool validateRecursive() const;

    Tableau *tableau;
    const Literal literal;
    std::vector<std::unique_ptr<Node>> children;
    Node *parentNode = nullptr;

    [[nodiscard]] bool isClosed() const;
    [[nodiscard]] bool isLeaf() const;
    void appendBranch(const DNF &dnf);
    void appendBranch(const Cube &cube);
    void appendBranch(const Literal &literal);
    std::optional<DNF> applyRule(bool modalRule = false);
    void inferModal();
    void inferModalTop();
    void inferModalAtomic();
    void replaceNegatedTopOnBranch(const std::vector<int> &labels);

    // this method assumes that tableau is already reduced
    [[nodiscard]] DNF extractDNF() const;
    void dnfBuilder(DNF &dnf) const;

    void toDotFormat(std::ofstream &output) const;

   private:
    void appendBranchInternalUp(DNF &dnf) const;
    void appendBranchInternalDown(DNF &dnf);
    void closeBranch();
  };

  // ============================================================================
  // ================================= Worklist =================================
  // ============================================================================

  /*
   * The worklist manages the nodes that need to get processed.
   * Importantly, it makes sure the processing order is sound and efficient:
   *  1. Positive equalities
   *1.5(?) Set membership (currently not used/supported)
   *  2. top set literals
   *  3. Negative literals
   *  4. Positive literals (i.e., the rest)
   *
   *  Rules 1-2 are for soundness.
   *  Rule 3. is for efficiency in order to close branches as soon as possible.
   */
  class Worklist {
   private:
#ifndef WORKLIST_ALTERNATIVE
    // (1)
    std::unique_ptr<Node> posEqualitiesHeadDummy;
    std::unique_ptr<Node> posEqualitiesTailDummy;
    // (2)
    std::unique_ptr<Node> posTopSetHeadDummy;
    std::unique_ptr<Node> posTopSetTailDummy;
    // (3)
    std::unique_ptr<Node> negativesHeadDummy;
    std::unique_ptr<Node> negativesTailDummy;
    ;
    // (4)
    std::unique_ptr<Node> positivesHeadDummy;
    std::unique_ptr<Node> positivesTailDummy;

    static void connect(Node &left, Node &right);
    static void disconnect(Node &node);
    static void insertAfter(Node &location, Node &node);
    static bool isEmpty(const std::unique_ptr<Node> &head, const std::unique_ptr<Node> &tail);
#else
    struct CompareNodes {
      bool operator()(const Node *left, const Node *right) const;
    };

    std::set<Node *, CompareNodes> queue;
#endif
   public:
    Worklist();
    void push(Node *node);
    Node *pop();
    void erase(Node *node);
    bool contains(const Node *node) const;

    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] bool validate() const;
  };

  // ============================================================================
  // ================================= Tableau ==================================
  // ============================================================================

  explicit Tableau(const Cube &cube);
  [[nodiscard]] bool validate() const;

  std::unique_ptr<Node> rootNode;
  Worklist unreducedNodes;

  bool solve(int bound = -1);
  void removeNode(Node *node);
  void renameBranch(const Node *leaf);

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
};
