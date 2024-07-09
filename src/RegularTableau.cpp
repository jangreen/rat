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

EventSet gatherActiveLabels(const Cube &cube) {
  // preconditions:
  assert(validateNormalizedCube(cube));  // cube is normal

  EventSet activeLabels;
  for (const auto &literal : cube) {
    if (literal.negated) {
      continue;
    }

    auto literalLabels = literal.events();
    activeLabels.insert(literalLabels.begin(), literalLabels.end());
  }

  return activeLabels;
}

SetOfSets gatherActiveCombinations(const Cube &cube) {
  // preconditions:
  assert(validateNormalizedCube(cube));  // cube is normal

  SetOfSets activeCombinations;
  for (const auto &literal : cube) {
    if (literal.negated) {
      continue;
    }

    auto literalLabels = literal.labelBaseCombinations();
    activeCombinations.insert(literalLabels.begin(), literalLabels.end());
  }

  return activeCombinations;
}

void findReachableNodes(RegularTableau::Node *node,
                        std::unordered_set<RegularTableau::Node *> &reach) {
  auto [_, inserted] = reach.insert(node);
  if (inserted) {
    for (const auto &child : node->childNodes) {
      const auto edgeLabels = child->parentNodes.at(node);
      if (edgeLabels.size() == 1) {
        auto [cube, _] = edgeLabels.at(0);
        const auto isInconsistentChild = cube.empty();
        if (isInconsistentChild) {
          continue;
        }
      }
      findReachableNodes(child, reach);
    }
  }
}

// removes all negated literals in cube with events that do not occur in some positive literal
// returns  - uselessLiterals = set of removed literals
//          - label = edgePredicates from cube are moved in label
// TODO: optimization that considers combinations may be incomplete:
// a cube may purge some literals, later a new incoming edge is added that would be inconsistent
// only with the purged literals. First idea was to also consider the combination in the edge
// labels. But this is not sufficient: we do not know which edge label (or fresh event constant)
// has a future incoming edge

void purge(Cube &cube, Cube &uselessLiterals, EdgeLabel &label) {
  // preconditions:
  assert(validateNormalizedCube(cube));  // cube is normal
  assert(std::ranges::none_of(cube, [](auto &lit) {
    return lit.isPositiveEqualityPredicate();
  }));                              // no e=f // FIXME is this included in Normal
                                    // FIXME what about set membership predicates? -> all not normal
  assert(uselessLiterals.empty());  // uselessLiterals is empty

  auto &edgePredicates = std::get<0>(label);

  // remove edge predicates from cube and add them to the label
  std::erase_if(cube, [&](auto &literal) {
    if (literal.isPositiveEdgePredicate()) {
      edgePredicates.push_back(literal);
      return true;
    }
    return false;
  });

  // 1) filter non-active events
  const auto activeLabels = gatherActiveLabels(cube);
  uselessLiterals = filterNegatedLiterals(cube, activeLabels);

  // 2) filter non-active label/base relation combinations
  // remove next two lines to disable optimization
  auto activePairs = gatherActiveCombinations(cube);
  filterNegatedLiterals(cube, activePairs, uselessLiterals);
}

// returns fixed node as set, otherwise nullopt if consistent
std::optional<Cube> getInconsistentLiterals(const RegularTableau::Node *parent,
                                            const Cube &newLiterals) {
  if (parent == nullptr) {
    return std::nullopt;
  }

  Cube inconsistenLiterals = newLiterals;
  const auto parentActiveLabels = gatherActiveLabels(parent->cube);
  const auto parentActiveCombinations = gatherActiveCombinations(parent->cube);

  // 2) filter newLiterals for parent#
  std::erase_if(inconsistenLiterals, [&](auto &literal) { return !literal.negated; });
  filterNegatedLiterals(inconsistenLiterals, parentActiveLabels);
  Cube useless;  // not needed, FIXME
  filterNegatedLiterals(inconsistenLiterals, parentActiveCombinations, useless);

  // 3) If no new literals, nothing to do
  if (isSubset(inconsistenLiterals, parent->cube)) {
    return std::nullopt;
  }

  // 4) We have new literals: add all literals from parent node to complete new node
  for (const auto &literal : parent->cube) {
    if (!contains(inconsistenLiterals, literal)) {
      inconsistenLiterals.push_back(literal);
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

bool RegularTableau::validate(const Node *currentNode) const {
  std::unordered_set<Node *> reachable;
  for (auto &root : rootNodes) {
    findReachableNodes(root, reachable);
  }

  const auto allNodesValid =
      std::ranges::all_of(reachable, [](auto &node) { return node->validate(); });

  // get open leafs
  std::erase_if(reachable, [&](Node *node) {
    return !node->childNodes.empty() || node->closed || node == currentNode;
  });

  for (auto &unreducedNode : get_const_container(unreducedNodes)) {
    reachable.erase(unreducedNode);
  }

  for (auto &leakedNode : reachable) {
    std::cout << " Leaked node: " << leakedNode << ", hash: " << std::hash<Node>()(*leakedNode)
              << std::endl;
    print(leakedNode->cube);
  }

  const auto unreducedNodesValid = reachable.empty();
  return unreducedNodesValid && allNodesValid;
}

// assumptions:
// cube is in normal form
RegularTableau::Node *RegularTableau::addNode(const Cube &cube, EdgeLabel &label) {
  // create node, add to "nodes" (returns pointer to existing node if already exists)
  auto [newNode, renaming] = Node::newNode(cube);
  auto newNodePtr = std::unique_ptr<Node>(newNode);
  std::get<1>(label) = renaming;  // update edge label
  auto [iter, added] = nodes.insert(std::move(newNodePtr));
  unreducedNodes.push(iter->get());
  return iter->get();
}

// parent == nullptr -> rootNode
void RegularTableau::addEdge(Node *parent, Node *child, const EdgeLabel &label) {
  if (parent == nullptr) {
    // root node
    rootNodes.insert(child);
    return;
  }

  const Cube &edges = std::get<0>(label);

  // don't add epsilon edges that already exist
  // what about other edges(with same label)?
  if (edges.empty() && child->parentNodes.contains(parent)) {
    for (const auto &edgeLabel : child->parentNodes.at(parent)) {
      if (std::get<0>(edgeLabel).empty()) {
        return;
      }
    }
  }

  // check if consistent (otherwise fixed child is created in isInconsistent)
  // never add inconsistent edges
  if (!edges.empty() && isInconsistent(parent, child, label)) {
    return;
  }

  // spdlog::debug(fmt::format("[Solver] Add edge  {} -> {}",
  // std::hash<Node>()(*parent),std::hash<Node>()(*child))); adding edge only add each child once
  // to child nodes
  if (!contains(parent->childNodes, child)) {
    parent->childNodes.push_back(child);
  }
  child->parentNodes[parent].push_back(label);

  // set first parent node if not already set & not epsilon
  if (child->firstParentNode == nullptr && !edges.empty()) {
    child->firstParentNode = parent;
    child->firstParentLabel = label;
  }

  if (edges.empty()) {
    // if epsilon edge is added -> add shortcuts
    for (const auto &[grandparentNode, parentLabels] : parent->parentNodes) {
      for (const auto &parentLabel : parentLabels) {
        addEdge(grandparentNode, child, parentLabel);
      }
    }
    // add epsilon child of a root nodes to root nodes
    if (rootNodes.contains(parent) && !rootNodes.contains(child)) {
      rootNodes.insert(child);
    }
  } else {
    // update rootParents of child node
    if (!parent->rootParents.empty() || rootNodes.contains(parent)) {
      child->rootParents.push_back(parent);
    }

    // if child has epsilon edge -> add shortcuts
    for (const auto childChild : child->childNodes) {
      if (childChild->parentNodes.at(child).empty()) {
        for (const auto &parentLabel : child->parentNodes[parent]) {
          addEdge(parent, childChild, parentLabel);
        }
      }
    }
  }
}

bool RegularTableau::solve() {
  while (!unreducedNodes.empty()) {
    exportDebug("debug");
    assert(validate());

    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();

    if (currentNode->closed || !currentNode->childNodes.empty() ||
        (!rootNodes.contains(currentNode) && currentNode->rootParents.empty())) {
      // skip already closed nodes and nodes that cannot be reached by a root node
      continue;
    }

    Tableau tableau{currentNode->cube};
    if (tableau.applyRuleA()) {
      expandNode(currentNode, &tableau);
    } else {
      extractAnnotationexample(currentNode);
      spdlog::info("[Solver] Answer: False");
      return false;
    }
  }
  spdlog::info("[Solver] Answer: True");
  return true;
}

// assumptions:
// node has only normal terms
void RegularTableau::expandNode(Node *node, Tableau *tableau) {
  assert(node == nullptr || node->validate());
  assert(tableau->validate());
  // node is expandable
  // calculate normal form
  // TODO: assert tableau is in DNF
  auto dnf = tableau->dnf();

  if (dnf.empty() && node != nullptr) {
    node->closed = true;
    return;
  }

  assert(validate(node));

  // for each cube: calculate potential child
  for (auto &cube : dnf) {
    Cube droppedLiterals;
    EdgeLabel edgeLabel;
    purge(cube, droppedLiterals, edgeLabel);

    // check if immediate inconsistency exists (inconsistent literals that are immediately
    // dropped)
    if (const auto alternativeNode = getInconsistentLiterals(node, droppedLiterals)) {
      // create new fixed Node
      Node *fixedNode = addNode(*alternativeNode, edgeLabel);
      addEdge(node, fixedNode, {});
      return;
    }

    // add child
    // checks if renaming exists when adding and returns already existing node if possible
    // updates edge label with renaming
    Node *newNode = addNode(cube, edgeLabel);
    addEdge(node, newNode, edgeLabel);
  }
  assert(validate());
}

// input is edge (parent,label,child)
// must not be part of the proof graph
bool RegularTableau::isInconsistent(Node *parent, const Node *child, EdgeLabel label) {
  auto &[edges, renaming] = label;
  assert(!edges.empty());  // is already fixed node to its parent (by def inconsistent)

  if (child->cube.empty()) {
    // empty child not inconsistent
    return false;
  }
  // calculate converse request
  Cube converseRequest = child->cube;
  // use parent naming: label has already parent naming, rename child cube
  renaming.invert();
  for (auto &literal : converseRequest) {
    literal.rename(renaming);
  }

  Tableau calcConverseReq(converseRequest);
  // add cube of label (adding them to converseRequest is incomplete,
  // because initial contradiction are not checked)
  for (const auto &literal : edges) {
    calcConverseReq.rootNode->appendBranch(literal);
  }

  // converseRequest is now child node + label
  // try to reduce: all rules but positive modal edge rules
  calcConverseReq.solve();

  if (calcConverseReq.rootNode->isClosed()) {
    // inconsistent
    Node *fixedNode = addNode({}, label);
    fixedNode->closed = true;
    addEdge(parent, fixedNode, {});
    return true;
  }

  const DNF dnf = calcConverseReq.rootNode->extractDNF();
  // each Disjunct must have some inconsistency to have an inconsistency
  // otherwise the proof for one cube without inconsistency would
  // subsume the others (which have more literals) if this is the case:
  // add one epsilon edge for each cube
  for (const auto &cube : dnf) {
    if (!getInconsistentLiterals(parent, cube)) {
      return false;
    }
  }

  for (const auto &cube : dnf) {
    if (auto alternativeNode = getInconsistentLiterals(parent, cube)) {
      // create new fixed Node
      Node *fixedNode = addNode(*alternativeNode, label);
      addEdge(parent, fixedNode, {});
    }
  }
  return true;
}

void RegularTableau::extractAnnotationexample(const Node *openNode) {
  std::ofstream annotationexample("./output/annotationexample.dot");
  annotationexample << "digraph { node[shape=\"point\"]" << std::endl;

  const Node *node = openNode;
  while (node->firstParentNode != nullptr) {
    for (const auto &edge : std::get<0>(node->firstParentLabel)) {
      int left = *edge.leftLabel;
      int right = *edge.rightLabel;
      std::string relation = *edge.identifier;
      // rename left
      const Node *renameNode = node->firstParentNode;
      auto currentRenaming = std::get<Renaming>(renameNode->firstParentLabel);
      while (left < currentRenaming.size() && renameNode != nullptr) {
        currentRenaming = std::get<1>(renameNode->firstParentLabel);
        left = currentRenaming.rename(left);
        renameNode = renameNode->firstParentNode;
      }
      // rename right
      renameNode = node->firstParentNode;
      currentRenaming = std::get<Renaming>(renameNode->firstParentLabel);
      while (right < currentRenaming.size() && renameNode != nullptr) {
        currentRenaming = std::get<1>(renameNode->firstParentLabel);
        right = currentRenaming.rename(right);
        renameNode = renameNode->firstParentNode;
      }

      annotationexample << "N" << left << " -> " << "N" << right << "[label = \"" << relation
                        << "\"];" << std::endl;
    }
    node = node->firstParentNode;
  }
  annotationexample << "}" << std::endl;
  annotationexample.close();
}

void RegularTableau::toDotFormat(std::ofstream &output, const bool allNodes) const {
  for (const auto &node : nodes) {  // reset printed property
    node->printed = false;
  }
  output << "digraph {" << std::endl << "node[shape=\"box\"]" << std::endl;
  std::unordered_set s(rootNodes.begin(), rootNodes.end());
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
