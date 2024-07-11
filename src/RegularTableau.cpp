#include "RegularTableau.h"

#include <spdlog/spdlog.h>

#include <iostream>
#include <map>
#include <unordered_set>

#include "utility.h"

namespace {

// TODO: remove
void addActivePairsFromEdges(const Cube &edges, SetOfSets &activePairs) {
  for (const auto &edgeLiteral : edges) {
    for (auto pair : edgeLiteral.labelBaseCombinations()) {
      activePairs.insert(pair);
    }
  }
}

void findReachableNodes(RegularNode *node, std::unordered_set<RegularNode *> &reach) {
  auto [_, inserted] = reach.insert(node);
  if (inserted) {
    for (const auto &child : node->children) {
      const auto edgeLabels = child->parents.at(node);
      findReachableNodes(child, reach);
    }
  }
}

// returns fixed node as set, otherwise nullopt if consistent
std::optional<Cube> getInconsistentLiterals(const RegularNode *parent, const Cube &newLiterals) {
  if (parent == nullptr) {
    return std::nullopt;
  }
  const Cube &parentCube = parent->cube;
  Cube inconsistenLiterals = newLiterals;

  const auto parentActiveEvents = gatherActiveEvents(parentCube);

  // 2) filter newLiterals for parent
  std::erase_if(inconsistenLiterals, [&](auto &literal) { return !literal.negated; });
  filterNegatedLiterals(inconsistenLiterals, parentActiveEvents);

  // 3) If no new literals, nothing to do
  if (isSubset(inconsistenLiterals, parentCube)) {
    return std::nullopt;
  }

  // 4) We have new literals: add all literals from parent node to complete new node
  for (const auto &literal : parentCube) {
    if (!contains(inconsistenLiterals, literal)) {
      inconsistenLiterals.push_back(literal);  // TODO: make cube flat_set?
    }
  }
  // alternative child
  return inconsistenLiterals;
}

}  // namespace

RegularTableau::RegularTableau(const std::initializer_list<Literal> initialLiterals)
    : RegularTableau(std::vector(initialLiterals)) {}
RegularTableau::RegularTableau(const Cube &initialLiterals) {
  Tableau t{initialLiterals};
  expandNode(nullptr, &t);
}

bool RegularTableau::validate(const RegularNode *currentNode) const {
  std::unordered_set<RegularNode *> reachable;
  for (auto &root : rootNodes) {
    findReachableNodes(root, reachable);
  }

  const auto allNodesValid = std::ranges::all_of(reachable, [](auto &node) {
    const bool nodeValid = node->validate();
    assert(nodeValid);
    return nodeValid;
  });

  // get open leafs
  std::erase_if(reachable, [&](const RegularNode *node) {
    return !node->children.empty() || !node->epsilonChildren.empty() || node->closed ||
           node == currentNode;
  });

  for (auto &unreducedNode : get_const_container(unreducedNodes)) {
    reachable.erase(unreducedNode);
  }

  for (auto &leakedNode : reachable) {
    std::cout << " Leaked node: " << leakedNode
              << ", hash: " << std::hash<RegularNode>()(*leakedNode) << std::endl;
    print(leakedNode->cube);
  }

  const auto unreducedNodesValid = reachable.empty();
  assert(unreducedNodesValid);
  assert(allNodesValid);
  return unreducedNodesValid && allNodesValid;
}

// assumptions:
// cube is in normal form
// checks if renaming exists when adding and returns already existing node if possible
std::pair<RegularNode *, Renaming> RegularTableau::newNode(const Cube &cube) {
  // create node, add to "nodes" (returns pointer to existing node if already exists)
  const auto &[newNode, renaming] = RegularNode::newNode(cube);
  assert(newNode->validate());
  auto newNodePtr = std::unique_ptr<RegularNode>(newNode);
  auto [iter, added] = nodes.insert(std::move(newNodePtr));
  unreducedNodes.push(iter->get());
  return {iter->get(), renaming};
}

// parent == nullptr -> rootNode
void RegularTableau::newEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label) {
  assert(parent != nullptr);
  assert(child != nullptr);
  assert(validate(parent));

  // don't add edges that already exist
  if (child->parents.contains(parent)) {
    return;
  }

  // don't add inconsistent edges
  if (isInconsistent(parent, child, label)) {
    return;
  }

  // add edge
  const auto [_, inserted] = parent->children.insert(child);
  if (!inserted) {
    assert(child->parents.contains(parent));
    return;
  }
  child->parents.insert({parent, label});
  exportDebug("debug");

  // counterexample: set first parent node if not already set
  if (child->firstParentNode == nullptr) {
    child->firstParentNode = parent;
    child->firstParentLabel = label;
  }
  // update rootParents of child node
  if (!parent->rootParents.empty() || rootNodes.contains(parent)) {
    child->rootParents.insert(parent);
  }

  // if child has epsilon edge -> add shortcuts
  for (const auto epsilonChildChild : child->epsilonChildren) {
    newEdge(parent, epsilonChildChild, label);
  }
}

void RegularTableau::newEpsilonEdge(RegularNode *parent, RegularNode *child) {
  assert(parent != nullptr);
  assert(child != nullptr);

  const auto [_, inserted] = parent->epsilonChildren.insert(child);
  if (!inserted) {
    assert(child->epsilonParents.contains(parent));
    return;
  }
  child->epsilonParents.insert(parent);
  exportDebug("debug");

  // add shortcuts
  for (const auto &[grandparentNode, grandparentLabel] : parent->parents) {
    newEdge(grandparentNode, child, grandparentLabel);
  }
  for (const auto &grandparentNode : parent->epsilonParents) {
    newEpsilonEdge(grandparentNode, child);
  }
  // add epsilon child of a root nodes to root nodes
  if (rootNodes.contains(parent)) {
    rootNodes.insert(child);
  }
}

bool RegularTableau::solve() {
  while (!unreducedNodes.empty()) {
    exportDebug("debug");
    assert(validate());

    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();

    if (currentNode->closed || !currentNode->children.empty() ||
        !currentNode->epsilonChildren.empty() ||
        (!rootNodes.contains(currentNode) && currentNode->rootParents.empty())) {
      // skip already closed nodes and nodes that cannot be reached by a root node
      continue;
    }

    // 1) weaken positive edge predicates
    if (std::ranges::any_of(currentNode->cube, [](const Literal &literal) {
          return literal.isPositiveEdgePredicate();
        })) {
      Cube newCube = currentNode->cube;
      std::erase_if(newCube,
                    [](const Literal &literal) { return literal.isPositiveEdgePredicate(); });
      filterNegatedLiterals(newCube);

      const auto &[childNode, edgeLabel] = newNode(newCube);
      newEdge(currentNode, childNode, edgeLabel);
      continue;
    }

    Tableau tableau{currentNode->cube};
    if (tableau.applyRuleA()) {
      expandNode(currentNode, &tableau);
      continue;
    }

    // 3) open leaf, extract counterexample
    extractAnnotationexample(currentNode);
    spdlog::info("[Solver] Answer: False");
    return false;
  }
  spdlog::info("[Solver] Answer: True");
  return true;
}

// assumptions:
// node has only normal terms
// node == nullptr -> node is rootNode
void RegularTableau::expandNode(RegularNode *node, Tableau *tableau) {
  assert(node == nullptr || node->validate());
  assert(tableau->validate());
  assert(validate(node));
  // node is expandable
  // calculate dnf
  auto dnf = tableau->dnf();

  if (dnf.empty() && node != nullptr) {
    node->closed = true;
    return;
  }

  // for each cube: calculate potential child
  for (auto &cube : dnf) {
    assert(validateCube(cube));
    assert(validate(node));

    // add node and edge
    assert(validateCube(cube));
    const auto &[childNode, edgeLabel] = newNode(cube);
    assert(validateCube(cube));
    assert(childNode->validate());
    const bool isRootNode = node == nullptr;
    if (isRootNode) {
      rootNodes.insert(childNode);
    } else {
      newEdge(node, childNode, edgeLabel);
    }
  }
  assert(validate());
}

// input is edge (parent,label,child)
// must not be part of the proof graph
// returns if given edge is inconsistent
bool RegularTableau::isInconsistent(RegularNode *parent, const RegularNode *child,
                                    EdgeLabel label) {
  assert(parent != nullptr);
  assert(child != nullptr);

  if (child->cube.empty()) {
    // empty child not inconsistent
    return false;
  }

  // use parent naming: label has already parent naming, rename child cube
  Cube renamedChild = child->cube;
  label.invert();
  for (auto &literal : renamedChild) {
    literal.rename(label);
  }

  const auto fixedCube = getInconsistentLiterals(parent, renamedChild);
  if (fixedCube.has_value()) {
    Tableau t(fixedCube.value());
    const DNF dnf = t.dnf();
    if (dnf.empty()) {
      return true;
    }
    // create new fixed Node
    for (const auto &cube : dnf) {
      const auto [fixedNode, _label] = newNode(cube);
      newEpsilonEdge(parent, fixedNode);
    }
    return true;
  }

  return false;
}

void RegularTableau::extractAnnotationexample(const RegularNode *openNode) {
  // TODO:
  // std::ofstream annotationexample("./output/annotationexample.dot");
  // annotationexample << "digraph { node[shape=\"point\"]" << std::endl;

  // const Node *node = openNode;
  // while (node->firstParentNode != nullptr) {
  //   for (const auto &edge : std::get<0>(node->firstParentLabel)) {
  //     int left = *edge.leftLabel;
  //     int right = *edge.rightLabel;
  //     std::string relation = *edge.identifier;
  //     // rename left
  //     const Node *renameNode = node->firstParentNode;
  //     auto currentRenaming = std::get<Renaming>(renameNode->firstParentLabel);
  //     while (left < currentRenaming.size() && renameNode != nullptr) {
  //       currentRenaming = std::get<1>(renameNode->firstParentLabel);
  //       left = currentRenaming.rename(left);
  //       renameNode = renameNode->firstParentNode;
  //     }
  //     // rename right
  //     renameNode = node->firstParentNode;
  //     currentRenaming = std::get<Renaming>(renameNode->firstParentLabel);
  //     while (right < currentRenaming.size() && renameNode != nullptr) {
  //       currentRenaming = std::get<1>(renameNode->firstParentLabel);
  //       right = currentRenaming.rename(right);
  //       renameNode = renameNode->firstParentNode;
  //     }

  //     annotationexample << "N" << left << " -> " << "N" << right << "[label = \"" << relation
  //                       << "\"];" << std::endl;
  //   }
  //   node = node->firstParentNode;
  // }
  // annotationexample << "}" << std::endl;
  // annotationexample.close();
}

void RegularTableau::toDotFormat(std::ofstream &output, const bool allNodes) const {
  for (const auto &node : nodes) {  // reset printed property
    node->printed = false;
  }
  output << "digraph {" << std::endl << "node[shape=\"box\"]" << std::endl;
  const std::unordered_set s(rootNodes.begin(), rootNodes.end());
  assert(s.size() == rootNodes.size());
  for (const auto rootNode : rootNodes) {
    rootNode->toDotFormat(output);
    output << "root -> " << "N" << rootNode << ";" << std::endl;
  }
  // for (const auto &node : nodes) {
  //   node->toDotFormat(output);
  // }
  output << "}" << std::endl;
}

void RegularTableau::exportProof(const std::string &filename) const {
  std::ofstream file("./output/" + filename + ".dot");
  toDotFormat(file);
  file.close();
}

void RegularTableau::exportDebug(const std::string &filename) const {
#if (DEBUG)
  exportProof(filename);
#endif
}
