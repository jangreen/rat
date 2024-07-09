#pragma once
#include <fstream>
#include <map>
#include <stack>
#include <string>
#include <unordered_set>
#include <vector>

#include "Literal.h"
#include "Tableau.h"

typedef std::pair<Cube, Renaming> EdgeLabel;

class RegularTableau {
 public:
  class Node {
   private:
    explicit Node(Cube cube);

   public:
    static std::pair<Node *, Renaming> newNode(Cube cube);
    [[nodiscard]] bool validate() const;

    const Cube cube;  // must be ordered, should not be modified
    std::vector<Node *> childNodes;
    std::map<Node *, std::vector<EdgeLabel>> parentNodes;  // TODO: use multimap instead?
    bool closed = false;

    std::vector<Node *> rootParents;  // parent nodes that are reachable by some root node
    Node *firstParentNode = nullptr;  // for annotationexample extraction
    EdgeLabel firstParentLabel;       // for annotationexample extraction

    bool printed = false;  // prevent cycling in printing // FIXME refactor
    void toDotFormat(std::ofstream &output);

    bool operator==(const Node &otherNode) const;

    struct Hash {
      size_t operator()(const std::unique_ptr<Node> &node) const;
    };

    // special equal function that is different from ==
    struct Equal {
      bool operator()(const std::unique_ptr<Node> &node1, const std::unique_ptr<Node> &node2) const;
    };
  };

  RegularTableau(std::initializer_list<Literal> initialLiterals);
  explicit RegularTableau(const Cube &initialLiterals);
  bool validate(const Node *currentNode = nullptr) const;

  std::unordered_set<Node *> rootNodes;
  std::unordered_set<std::unique_ptr<Node>, Node::Hash, Node::Equal> nodes;
  std::stack<Node *> unreducedNodes;

  bool solve();
  Node *addNode(const Cube &cube, EdgeLabel &label);
  void addEdge(Node *parent, Node *child, const EdgeLabel &label);
  void expandNode(Node *node, Tableau *tableau);
  bool isInconsistent(Node *parent, const Node *child, EdgeLabel label);
  static void extractAnnotationexample(const Node *openNode);

  void toDotFormat(std::ofstream &output, bool allNodes = true) const;
  void exportProof(const std::string &filename) const;
  void exportDebug(const std::string &filename) const;
};

template <>
struct std::hash<RegularTableau::Node> {
  std::size_t operator()(const RegularTableau::Node &node) const noexcept;
};  // namespace std

// helper
// TODO: move into general vector class
template <typename T>
bool isSubset(std::vector<T> smallerSet, std::vector<T> largerSet) {
  std::unordered_set<T> set(std::make_move_iterator(largerSet.begin()),
                            std::make_move_iterator(largerSet.end()));
  return std::ranges::all_of(smallerSet, [&](T &element) { return set.contains(element); });
};