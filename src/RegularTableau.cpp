#include "RegularTableau.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <tuple>
#include <utility>

// helper
// assumption: input is dnf cube
// purges useless literals
Cube purge(const Cube &cube, Cube &dropped, EdgeLabel &label) {
  Cube newCube;

  // remove edge predicates and put them in the edge label
  for (const auto &literal : cube) {
    if (literal.isPositiveEdgePredicate() || literal.isPositiveEqualityPredicate()) {
      std::get<0>(label).push_back(literal);
    } else {
      newCube.push_back(literal);
    }
  }

  // filter none active labels + active (label,baseRel) combinations
  // but: do not purge incoming edge labels (for recursive inconsistency checks) (*)
  // assume already normal
  // 1) gather all active labels
  std::vector<int> activeLabels;
  std::vector<CanonicalSet> activeLabelBaseCombinations;
  for (const auto &literal : newCube) {
    if (literal.negated) {
      continue;
    }

    auto literalLabels = literal.labels();
    auto literalCombinations = literal.labelBaseCombinations();
    activeLabels.insert(std::end(activeLabels), std::begin(literalLabels),
                        std::end(literalLabels));
    activeLabelBaseCombinations.insert(std::end(activeLabelBaseCombinations),
                                       std::begin(literalCombinations),
                                       std::end(literalCombinations));
  }
  // (*) above
  Cube edges = std::get<0>(label);
  for (const auto &edgeLiteral : edges) {
    auto literalCombinations = edgeLiteral.labelBaseCombinations();
    for (auto combination : literalCombinations) {
      activeLabelBaseCombinations.push_back(combination);
    }
  }

  // calculate equivalence class
  std::map<int, int> minimalRepresentatives;
  for (const auto &literal : std::get<0>(label)) {
    if (!literal.isPositiveEqualityPredicate()) {
      continue;
    }

    const int i = *literal.leftLabel;
    const int j = *literal.rightLabel;
    const int iMin = minimalRepresentatives.contains(i) ? minimalRepresentatives.at(i) : i;
    const int jMin = minimalRepresentatives.contains(j) ? minimalRepresentatives.at(j) : j;
    const int min = std::min(iMin, jMin);
    minimalRepresentatives.insert({i, min});
    minimalRepresentatives.insert({j, min});
  }

  // 2) filtering: non-active labels, non-minimal labels, and non-active combinations
  auto isNonMinimal = [&](int label) {
    return minimalRepresentatives[label] != label;
  };

  Cube copy = std::move(newCube);
  newCube = {};
  for (const auto &literal : copy) {
    auto literalLabels = literal.labels();
    auto literalCombinations = literal.labelBaseCombinations();

    // find intersection: nonMinimal & literalLabels
    bool intersectionNonempty = std::find_if(literalLabels.begin(),literalLabels.end(),
                                             isNonMinimal) != literalLabels.end();
    if (intersectionNonempty) {
      continue;
    }

    if (isSubset(literalLabels, activeLabels) &&
        isSubset(literalCombinations, activeLabelBaseCombinations)) {
      newCube.push_back(literal);
    } else {
      // are minimal
      // check for immediate inconsistencies
      dropped.push_back(literal);
    }
  }
  return newCube;
}

// return fixed node as set, otherwise nullopt if consistent
std::optional<Cube> getInconsistentLiterals(RegularTableau::Node *parent, const Cube &newLiterals) {
  if (parent == nullptr) {
    return std::nullopt;
  }
  // purge also here before adding new literals // TODO: use purge function
  // but: do not purge incoming edge labels (for recursive inconsistency checks) (*)
  // 1) gather parent active labels
  std::vector<int> parentActiveLabels;
  std::vector<CanonicalSet> parentActiveLabelBaseCombinations;
  for (const auto &literal : parent->cube) {
    if (!literal.negated) {
      auto literalLabels = literal.labels();
      auto literalCombinations = literal.labelBaseCombinations();
      parentActiveLabels.insert(std::end(parentActiveLabels), std::begin(literalLabels),
                                std::end(literalLabels));
      parentActiveLabelBaseCombinations.insert(std::end(parentActiveLabelBaseCombinations),
                                               std::begin(literalCombinations),
                                               std::end(literalCombinations));
    }
  }
  // (*) above
  for (const auto &[grandparentNode, parentLabels] : parent->parentNodes) {
    for (const auto &parentLabel : parentLabels) {
      Cube edges = std::get<0>(parentLabel);
      Renaming renaming = std::get<1>(parentLabel);
      for (const auto &edgeLiteral : edges) {
        auto literalCombinations = edgeLiteral.labelBaseCombinations();
        // only keep combinations with labels that are still there after renaming
        for (auto combination : literalCombinations) {
          if (isSubset(combination->labels, renaming)) {
            // push renamed combination (using inverse of renaming)
            auto renamedCombination = combination->rename(renaming, true);
            parentActiveLabelBaseCombinations.push_back(renamedCombination);
          }
        }
      }
    }
  }

  // 2) filter newLiterals for parent
  Cube filteredNewLiterals;
  for (const auto &literal : newLiterals) {
    auto literalLabels = literal.labels();
    auto literalCombinations = literal.labelBaseCombinations();

    if (isSubset(literalLabels, parentActiveLabels) &&
        isSubset(literalCombinations, parentActiveLabelBaseCombinations) && literal != TOP) {
      filteredNewLiterals.push_back(literal);
    }
  }
  // 3) check if new derived literal exists
  if (!isSubset(filteredNewLiterals, parent->cube)) {
    for (const auto &literal : parent->cube) {
      if (std::find(filteredNewLiterals.begin(), filteredNewLiterals.end(), literal) ==
          filteredNewLiterals.end()) {
        filteredNewLiterals.push_back(literal);
      }
    }
    // spdlog::debug(fmt::format("[Solver] Inconsistent Node  {}",
    // std::hash<RegularTableau::Node>()(*parent))); at this point filteredNewLiterals is the
    // alternative child
    return filteredNewLiterals;
  }
  return std::nullopt;
}
// helper end

RegularTableau::RegularTableau(std::initializer_list<Literal> initialLiterals)
    : RegularTableau(std::vector(initialLiterals)) {}
RegularTableau::RegularTableau(const Cube &initialLiterals) {
  Tableau t{initialLiterals};
  expandNode(nullptr, &t);
}

// assumptions:
// cube is in normal form
RegularTableau::Node *RegularTableau::addNode(const Cube& cube, EdgeLabel &label) {
  // create node, add to "nodes" (returns pointer to existing node if already exists)
  auto newNode = std::make_unique<Node>(cube);
  std::get<1>(label) = newNode->renaming;  // update edge label
  auto insertion = nodes.insert(std::move(newNode));
  if (insertion.second) {
    // new node has been added (no isomorphic node existed)
    unreducedNodes.push(insertion.first->get());

    // spdlog::debug(fmt::format("[Solver] Add node  {}",
    // std::hash<Node>()(*insertion.first->get())));
  }
  return insertion.first->get();
}

// parent == nullptr -> rootNode
void RegularTableau::addEdge(Node *parent, Node *child, EdgeLabel label) {
  if (parent == nullptr) {
    // root node
    rootNodes.push_back(child);
    return;
  }

  // don't add epsilon edges that already exist
  // what about other edges(with same label)?
  if (std::get<0>(label).empty()) {
    for (const auto &edgeLabel : child->parentNodes[parent]) {
      if (std::get<0>(edgeLabel).empty()) {
        return;
      }
    }
  }

  // check if consistent (otherwise fixed child is created in isInconsistent)
  // never add inconsistent edges
  if (std::get<0>(label).empty() || !isInconsistent(parent, child, label)) {
    // spdlog::debug(fmt::format("[Solver] Add edge  {} -> {}",
    // std::hash<Node>()(*parent),std::hash<Node>()(*child))); adding edge only add each child once
    // to child nodes
    auto childIt = std::find(parent->childNodes.begin(), parent->childNodes.end(), child);
    if (childIt == parent->childNodes.end()) {
      parent->childNodes.push_back(child);
    }
    child->parentNodes[parent].push_back(label);

    // set first parent node if not already set & not epsilon
    if (child->firstParentNode == nullptr && !std::get<0>(label).empty()) {
      child->firstParentNode = parent;
      child->firstParentLabel = label;
    }

    if (std::get<0>(label).empty()) {
      // if epsilon edge is added -> add shortcuts
      for (const auto &[grandparentNode, parentLabels] : parent->parentNodes) {
        for (const auto &parentLabel : parentLabels) {
          addEdge(grandparentNode, child, parentLabel);
        }
      }
      // add epsilon child of a root nodes to root nodes
      if (std::find(rootNodes.begin(), rootNodes.end(), parent) != rootNodes.end()) {
        rootNodes.push_back(child);
      }
    } else {
      // update rootParents of child node
      if (std::find(rootNodes.begin(), rootNodes.end(), parent) != rootNodes.end() ||
          !parent->rootParents.empty()) {
        child->rootParents.push_back(parent);
      }

      // if child has epsilon edge -> add shortcuts
      for (const auto childChild : child->childNodes) {
        if (childChild->parentNodes[child].empty()) {
          for (const auto &parentLabel : child->parentNodes[parent]) {
            addEdge(parent, childChild, parentLabel);
          }
        }
      }
    }
  }
}

bool RegularTableau::solve() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();

    if ((std::find(rootNodes.begin(), rootNodes.end(), currentNode) == rootNodes.end() &&
         currentNode->rootParents.empty()) ||
        currentNode->closed) {
      // skip already closed nodes and nodes that cannot be reached by a root node
      continue;
    }

    Tableau tableau{currentNode->cube};
    auto atomicLiteral = tableau.applyRuleA();  // TODO: make return value boolean

    if (atomicLiteral) {
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
  // node is expandable
  // calculate normal form
  auto dnf = tableau->dnf();
  tableau->exportProof("debug");
  int maxSaturationBound = std::max(Literal::saturationBoundId, Literal::saturationBoundBase);
  for (size_t i = 0; i < maxSaturationBound; i++) {
    saturate(dnf);
  }

  if (dnf.empty() && node != nullptr) {
    node->closed = true;
    return;
  }
  // spdlog::debug(fmt::format("[Solver] Expand node {} ", (node == nullptr ? 0 :
  // std::hash<Node>()(*node))));

  // for each cube: calculate potential child
  for (const auto &cube : dnf) {
    Cube droppedLiterals;
    EdgeLabel edgeLabel;
    auto newCube = purge(cube, droppedLiterals, edgeLabel);

    // check if immediate inconsistency exists (inconsistent literals that are immediately
    // dropped)
    auto alternativeNode = getInconsistentLiterals(node, droppedLiterals);
    if (alternativeNode) {
      // create new fixed Node
      Node *fixedNode = addNode(*alternativeNode, edgeLabel);
      addEdge(node, fixedNode, {});
      return;
    }

    // add child
    // checks if renaming exists when adding and returns already existing node if possible
    // updates edge Label with renaming
    Node *newNode = addNode(newCube, edgeLabel);
    addEdge(node, newNode, edgeLabel);
  }
}

// input is edge (parent,label,child)
// must not be part of the proof graph
bool RegularTableau::isInconsistent(Node *parent, Node *child, EdgeLabel label) {
  if (std::get<0>(label).empty()) {  // is already fixed node to its parent (by def inconsistent)
    return false;                    // should return true?
  }
  // spdlog::debug(fmt::format("[Solver] Check consistency {} -> {}",
  // std::hash<Node>()(*parent),std::hash<Node>()(*child)));
  if (child->cube.empty()) {
    // empty child not inconsistent
    return false;
  }
  // calculate converse request
  Cube converseRequest = child->cube;
  // use parent naming: label has already parent naming, rename child cube
  for (auto &literal : converseRequest) {
    literal.rename(std::get<1>(label), true);
  }

  Tableau calcConverseReq(converseRequest);
  // add cube of label (adding them to converseRequest is incomplete,
  // because initial contradiction are not checked)
  for (const auto &literal : std::get<0>(label)) {
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

  DNF dnf = calcConverseReq.rootNode->extractDNF();
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
    auto alternativeNode = getInconsistentLiterals(parent, cube);
    if (alternativeNode) {
      // create new fixed Node
      Node *fixedNode = addNode(*alternativeNode, label);
      addEdge(parent, fixedNode, {});
    }
  }
  return true;
}

void RegularTableau::saturate(DNF &dnf) {
  if (!Assumption::baseAssumptions.empty()) {
    DNF newDnf;
    for (auto &cube : dnf) {
      // saturation phase
      for (Literal &literal : cube) {
        literal.saturateBase();
      }
      // normalize
      Tableau saturatedTableau{cube};
      auto saturatedDnf = saturatedTableau.dnf();
      newDnf.insert(newDnf.end(), std::make_move_iterator(saturatedDnf.begin()),
                    std::make_move_iterator(saturatedDnf.end()));
    }
    swap(dnf, newDnf);
  }
  if (!Assumption::idAssumptions.empty()) {
    DNF newDnf;
    for (auto &cube : dnf) {
      // saturation phase
      for (Literal &literal : cube) {
        literal.saturateId();
      }
      // normalize
      Tableau saturatedTableau{cube};
      auto saturatedDnf = saturatedTableau.dnf();
      newDnf.insert(newDnf.end(), std::make_move_iterator(saturatedDnf.begin()),
                    std::make_move_iterator(saturatedDnf.end()));
    }
    swap(dnf, newDnf);
  }
}

void RegularTableau::extractCounterexample(Node *openNode) {
  std::ofstream counterexample("./output/counterexample.dot");
  counterexample << "digraph { node[shape=\"point\"]" << std::endl;

  Node *node = openNode;
  while (node->firstParentNode != nullptr) {
    for (const auto &edge : std::get<0>(node->firstParentLabel)) {
      int left = *edge.leftLabel;
      int right = *edge.rightLabel;
      std::string relation = *edge.identifier;
      // rename left
      Node *renameNode = node->firstParentNode;
      auto currentRenaming = std::get<Renaming>(renameNode->firstParentLabel);
      while (left < currentRenaming.size() && renameNode != nullptr) {
        currentRenaming = std::get<1>(renameNode->firstParentLabel);
        left = currentRenaming[left];
        renameNode = renameNode->firstParentNode;
      }
      // rename right
      renameNode = node->firstParentNode;
      currentRenaming = std::get<Renaming>(renameNode->firstParentLabel);
      while (right < currentRenaming.size() && renameNode != nullptr) {
        currentRenaming = std::get<1>(renameNode->firstParentLabel);
        right = currentRenaming[right];
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

void RegularTableau::toDotFormat(std::ofstream &output, bool allNodes) const {
  for (const auto &node : nodes) {  // reset printed property
    node->printed = false;
  }
  output << "digraph {" << std::endl << "node[shape=\"box\"]" << std::endl;
  for (const auto rootNode : rootNodes) {
    rootNode->toDotFormat(output);
    output << "root -> " << "N" << rootNode << ";" << std::endl;
  }
  if (allNodes) {
    for (const auto &node : nodes) {
      node->toDotFormat(output);
    }
  }
  output << "}" << std::endl;
}

void RegularTableau::exportProof(const std::string &filename) const {
  std::ofstream file("./output/" + filename + ".dot");
  toDotFormat(file);
  file.close();
}
