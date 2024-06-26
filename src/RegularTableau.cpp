#include "RegularTableau.h"

#include <spdlog/spdlog.h>

#include <iostream>
#include <map>
#include <unordered_set>

#include "utility.h"

namespace {

std::pair<std::vector<int>, std::vector<CanonicalSet>> gatherActiveLabels(const Cube &cube) {
  // preconditions:
  assert(validateNormalizedCube(cube));  // cube is normal

  std::vector<int> activeLabels;
  std::vector<CanonicalSet> activeLabelBaseCombinations;
  for (const auto &literal : cube) {
    if (literal.negated) {
      continue;
    }

    auto literalLabels = literal.labels();
    auto literalCombinations = literal.labelBaseCombinations();
    activeLabels.insert(std::end(activeLabels), std::begin(literalLabels), std::end(literalLabels));
    activeLabelBaseCombinations.insert(std::end(activeLabelBaseCombinations),
                                       std::begin(literalCombinations),
                                       std::end(literalCombinations));
  }

  return std::make_pair(activeLabels, activeLabelBaseCombinations);
}

void addActiveLabelsFromEdges(const Cube &edges,
                              std::vector<CanonicalSet> &activeLabelBaseCombinations) {
  for (const auto &edgeLiteral : edges) {
    auto literalCombinations = edgeLiteral.labelBaseCombinations();
    for (auto combination : literalCombinations) {
      activeLabelBaseCombinations.push_back(combination);
    }
  }
}

std::map<int, int> computeMinimalRepresentatives(const Cube &cube) {
  std::map<int, int> minimalRepresentatives;
  for (const auto &literal : cube) {
    if (!literal.isPositiveEqualityPredicate()) {
      continue;
    }
    assert(false);

    const int i = *literal.leftLabel;
    const int j = *literal.rightLabel;
    const int iMin = minimalRepresentatives.contains(i) ? minimalRepresentatives.at(i) : i;
    const int jMin = minimalRepresentatives.contains(j) ? minimalRepresentatives.at(j) : j;
    const int min = std::min(iMin, jMin);
    minimalRepresentatives.insert({i, min});
    minimalRepresentatives.insert({j, min});
  }

  return minimalRepresentatives;
}

bool isLiteralActive(const Literal &literal, const std::vector<int> &activeLabels,
                     const std::vector<CanonicalSet> &activeLabelBaseCombinations) {
  return isSubset(literal.labels(), activeLabels) &&
         isSubset(literal.labelBaseCombinations(), activeLabelBaseCombinations);
}

void saturateWith(DNF &dnf, const std::function<void(Literal &)> &saturationFunc) {
  DNF newDnf;
  for (auto &cube : dnf) {
    // saturation phase
    for (Literal &literal : cube) {
      saturationFunc(literal);
    }
    // normalize
    Tableau saturatedTableau{cube};
    auto saturatedDnf = saturatedTableau.dnf();
    newDnf.insert(newDnf.end(), std::make_move_iterator(saturatedDnf.begin()),
                  std::make_move_iterator(saturatedDnf.end()));
  }
  swap(dnf, newDnf);
}

void findReachableNodes(RegularTableau::Node *node,
                        std::unordered_set<RegularTableau::Node *> &reach) {
  auto [_, inserted] = reach.insert(node);
  if (inserted) {
    for (auto &child : node->childNodes) {
      auto edgeLabels = child->parentNodes.at(node);
      if (edgeLabels.size() == 1) {
        auto [cube, _] = edgeLabels.at(0);
        auto isInconsistentChild = cube.empty();
        if (isInconsistentChild) {
          continue;
        }
      }
      findReachableNodes(child, reach);
    }
  }
}

Cube purge(const Cube &cube, Cube &dropped, EdgeLabel &label) {
  // preconditions:
  assert(validateNormalizedCube(cube));  // cube is normal
  assert(dropped.empty());               // dropped is empty

  Cube purgedCube;
  auto &edgePredicates = std::get<0>(label);

  // remove edge predicates from cube and add them to the label
  for (const auto &literal : cube) {
    if (literal.isPositiveEdgePredicate() ||
        literal.isPositiveEqualityPredicate()) {  // FIXME: do we need isPositiveEqualityPredicate?
      edgePredicates.push_back(literal);
    } else {
      purgedCube.push_back(literal);
    }
  }

  // 1) filter none-active labels + non-active (label,baseRel) combinations
  // also respect edgeLabel (for recursive inconsistency checks)
  auto [activeLabels, activeLabelBaseCombinations] = gatherActiveLabels(purgedCube);
  addActiveLabelsFromEdges(edgePredicates, activeLabelBaseCombinations);

  // calculate equivalence class
  const std::map<int, int> minimalRepresentatives =
      computeMinimalRepresentatives(std::get<0>(label));

  // 2) filtering: non-active labels, non-minimal labels, and non-active combinations
  auto isNonMinimal = [&](const int l) {
    const auto iter = minimalRepresentatives.find(l);
    return iter != minimalRepresentatives.end() && iter->second != l;
  };

  const Cube copy = std::move(purgedCube);
  purgedCube = {};
  for (const auto &literal : copy) {
    const auto &literalLabels = literal.labels();
    if (std::ranges::find_if(literalLabels, isNonMinimal) != literalLabels.end()) {
      // Has non-minimal labels
      continue;
    }

    if (isLiteralActive(literal, activeLabels, activeLabelBaseCombinations)) {
      purgedCube.push_back(literal);
    } else {
      // are minimal
      // check for immediate inconsistencies
      dropped.push_back(literal);
    }
  }
  return purgedCube;
}

// returns fixed node as set, otherwise nullopt if consistent
std::optional<Cube> getInconsistentLiterals(const RegularTableau::Node *parent,
                                            const Cube &newLiterals) {
  if (parent == nullptr) {
    return std::nullopt;
  }
  // purge also here before adding new literals // TODO: use purge function
  // but: do not purge incoming edge labels (for recursive inconsistency checks) (*)
  // 1) gather parent active labels
  auto [parentActiveLabels, parentActiveLabelBaseCombinations] = gatherActiveLabels(parent->cube);
  // (*) above
  for (const auto &[grandparentNode, parentLabels] : parent->parentNodes) {
    for (const auto &parentLabel : parentLabels) {
      const Cube &edges = std::get<0>(parentLabel);
      Renaming renaming = std::get<1>(parentLabel);
      for (const auto &edgeLiteral : edges) {
        const auto literalCombinations = edgeLiteral.labelBaseCombinations();
        // only keep combinations with labels that are still there after renaming
        for (const auto combination : literalCombinations) {
          if (isSubset(combination->getLabels(), renaming.from)) {
            // push renamed combination (using inverse of renaming)
            renaming.invert();
            auto renamedCombination = combination->rename(renaming);
            parentActiveLabelBaseCombinations.push_back(renamedCombination);
          }
        }
      }
    }
  }

  // 2) filter newLiterals for parent
  Cube filteredNewLiterals;
  for (const auto &literal : newLiterals) {
    if (literal != TOP &&
        isLiteralActive(literal, parentActiveLabels, parentActiveLabelBaseCombinations)) {
      filteredNewLiterals.push_back(literal);
    }
  }

  // 3) If no new literals, nothing to do
  if (isSubset(filteredNewLiterals, parent->cube)) {
    return std::nullopt;
  }

  // 4) We have new literals: add all literals from parent node to complete new node
  for (const auto &literal : parent->cube) {
    if (!contains(filteredNewLiterals, literal)) {
      filteredNewLiterals.push_back(literal);
    }
  }
  // spdlog::debug(fmt::format("[Solver] Inconsistent Node  {}",
  // std::hash<RegularTableau::Node>()(*parent))); at this point filteredNewLiterals is the
  // alternative child
  return filteredNewLiterals;
}

}  // namespace

RegularTableau::RegularTableau(const std::initializer_list<Literal> initialLiterals)
    : RegularTableau(std::vector(initialLiterals)) {}
RegularTableau::RegularTableau(const Cube &initialLiterals) {
  Tableau t{initialLiterals};
  expandNode(nullptr, &t);
}

bool RegularTableau::validate(Node *currentNode) const {
  std::unordered_set<Node *> reachable;
  for (auto &root : rootNodes) {
    findReachableNodes(root, reachable);
  }

  auto allNodesValid = std::ranges::all_of(reachable, [](auto &node) { return node->validate(); });

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
    Literal::print(leakedNode->cube);
  }

  auto unreducedNodesValid = reachable.empty();
  return unreducedNodesValid && allNodesValid;
}

// assumptions:
// cube is in normal form
RegularTableau::Node *RegularTableau::addNode(const Cube &cube, EdgeLabel &label) {
  // create node, add to "nodes" (returns pointer to existing node if already exists)
  auto newNode = std::make_unique<Node>(cube);
  std::get<1>(label) = newNode->renaming;  // update edge label
  auto [iter, added] = nodes.insert(std::move(newNode));
  unreducedNodes.push(iter->get());
  return iter->get();
}

// parent == nullptr -> rootNode
void RegularTableau::addEdge(Node *parent, Node *child, const EdgeLabel &label) {
  if (parent == nullptr) {
    // root node
    assert(!contains(rootNodes, child));
    rootNodes.push_back(child);
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
    if (contains(rootNodes, parent) && !contains(rootNodes, child)) {
      rootNodes.push_back(child);
    }
  } else {
    // update rootParents of child node
    if (!parent->rootParents.empty() || contains(rootNodes, parent)) {
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
        (!contains(rootNodes, currentNode) && currentNode->rootParents.empty())) {
      // skip already closed nodes and nodes that cannot be reached by a root node
      continue;
    }

    Tableau tableau{currentNode->cube};
    if (tableau.applyRuleA()) {
      expandNode(currentNode, &tableau);
    } else {
      extractCounterexample(currentNode);
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
  // node is expandable
  // calculate normal form
  auto dnf = tableau->dnf();
  const int maxSaturationBound = std::max(Literal::saturationBoundId, Literal::saturationBoundBase);
  for (size_t i = 0; i < maxSaturationBound; i++) {
    saturate(dnf);
  }

  if (dnf.empty() && node != nullptr) {
    node->closed = true;
    return;
  }

  assert(validate(node));

  // for each cube: calculate potential child
  for (const auto &cube : dnf) {
    Cube droppedLiterals;
    EdgeLabel edgeLabel;
    const auto newCube = purge(cube, droppedLiterals, edgeLabel);

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
    Node *newNode = addNode(newCube, edgeLabel);
    addEdge(node, newNode, edgeLabel);
  }
  assert(validate());
}

// input is edge (parent,label,child)
// must not be part of the proof graph
bool RegularTableau::isInconsistent(Node *parent, const Node *child, EdgeLabel label) {
  auto &[edges, renaming] = label;
  if (edges.empty()) {  // is already fixed node to its parent (by def inconsistent)
    return false;       // should return true?
  }

  if (child->cube.empty()) {
    // empty child not inconsistent
    return false;
  }
  // calculate converse request
  Cube converseRequest = child->cube;
  // use parent naming: label has already parent naming, rename child cube
  for (auto &literal : converseRequest) {
    renaming.invert();
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

void RegularTableau::saturate(DNF &dnf) {
  if (!Assumption::baseAssumptions.empty()) {
    saturateWith(dnf, &Literal::saturateBase);
  }
  if (!Assumption::idAssumptions.empty()) {
    saturateWith(dnf, &Literal::saturateId);
  }
}

void RegularTableau::extractCounterexample(const Node *openNode) {
  std::ofstream counterexample("./output/counterexample.dot");
  counterexample << "digraph { node[shape=\"point\"]" << std::endl;

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

      counterexample << "N" << left << " -> " << "N" << right << "[label = \"" << relation << "\"];"
                     << std::endl;
    }
    node = node->firstParentNode;
  }
  counterexample << "}" << std::endl;
  counterexample.close();
}

void RegularTableau::toDotFormat(std::ofstream &output, const bool allNodes) const {
  for (const auto &node : nodes) {  // reset printed property
    node->printed = false;
  }
  output << "digraph {" << std::endl << "node[shape=\"box\"]" << std::endl;
  std::unordered_set<Node *> s(rootNodes.begin(), rootNodes.end());
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
