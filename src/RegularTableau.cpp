#include "RegularTableau.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <boost/pending/disjoint_sets.hpp>
#include <format>
#include <iostream>
#include <map>
#include <tuple>
#include <utility>

// helper
// assumption: input is dnf clause
// purges useless literals
Cube purge(const Cube &clause, Cube &dropped, EdgeLabel &label) {
  Cube newCube;

  // remove edge predicates and put them in the edge label
  for (const auto &literal : clause) {
    if (literal.isPositiveEdgePredicate() || literal.isPositiveEqualityPredicate()) {
      std::get<0>(label).push_back(literal);
    } else {
      newCube.push_back(literal);
    }
  }

  // filter none active labels + active (label,baseRel) combinations
  // assume already normal
  // 1) gather all active labels
  std::vector<int> activeLabels;
  std::vector<Set> activeLabelBaseCombinations;
  for (const auto &literal : newCube) {
    if (!literal.negated) {
      auto literalLabels = literal.labels();
      auto literalCobinations = literal.labelBaseCombinations();
      activeLabels.insert(std::end(activeLabels), std::begin(literalLabels),
                          std::end(literalLabels));
      activeLabelBaseCombinations.insert(std::end(activeLabelBaseCombinations),
                                         std::begin(literalCobinations),
                                         std::end(literalCobinations));
    }
  }
  // calculate equivalence class
  std::map<int, int> minimalRepresentants;
  for (const auto &literal : std::get<0>(label)) {
    if (literal.isPositiveEqualityPredicate()) {
      int i = *literal.leftOperand->label;
      int j = *literal.rightOperand->label;

      std::optional<int> iMin, jMin;
      if (minimalRepresentants.contains(i)) {
        iMin = minimalRepresentants.at(i);
      } else {
        iMin = i;
      }
      if (minimalRepresentants.contains(j)) {
        jMin = minimalRepresentants.at(j);
      } else {
        jMin = j;
      }
      int min = std::min(*iMin, *jMin);
      minimalRepresentants.insert({i, min});
      minimalRepresentants.insert({j, min});
    }
  }
  // calculate set of non minimal labels
  std::vector<int> nonMinimal;
  for (const auto &[i, representant] : minimalRepresentants) {
    if (i != representant) {
      nonMinimal.push_back(i);
    }
  }
  // 2) filtering: non active labels, non minimal labels, and non active combinations
  Cube copy = std::move(newCube);
  newCube = {};
  for (const auto &literal : copy) {
    auto literalLabels = literal.labels();
    auto literalCobinations = literal.labelBaseCombinations();

    // find intersection: nonMinimal & literalLabels
    bool intersectionNonempty = false;
    for (const auto nonMin : nonMinimal) {
      if (std::find(literalLabels.begin(), literalLabels.end(), nonMin) != literalLabels.end()) {
        // contains non Minimal label -> drop it and do not remember
        intersectionNonempty = true;
        break;
      }
    }
    if (intersectionNonempty) {
      continue;
    }

    // TODO: remove:
    // std::cout << "------ " << literal.toString() << std::endl;
    // for (auto s : literalCobinations) {
    //   std::cout << s.toString() << std::endl;
    // }
    // std::cout << "-" << std::endl;
    // for (auto s : activeLabelBaseCombinations) {
    //   std::cout << s.toString() << std::endl;
    // }
    // std::cout << isSubset(literalCobinations, activeLabelBaseCombinations) << std::endl;

    if (isSubset(literalLabels, activeLabels) &&
        isSubset(literalCobinations, activeLabelBaseCombinations)) {
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
  std::vector<Set> parentActiveLabelBaseCombinations;
  for (const auto &literal : parent->cube) {
    if (!literal.negated) {
      auto literalLabels = literal.labels();
      auto literalCobinations = literal.labelBaseCombinations();
      parentActiveLabels.insert(std::end(parentActiveLabels), std::begin(literalLabels),
                                std::end(literalLabels));
      parentActiveLabelBaseCombinations.insert(std::end(parentActiveLabelBaseCombinations),
                                               std::begin(literalCobinations),
                                               std::end(literalCobinations));
    }
  }
  // (*) above
  for (const auto &[grandparentNode, parentLabels] : parent->parentNodes) {
    for (const auto &parentLabel : parentLabels) {
      std::vector<Literal> edges = std::get<0>(parentLabel);
      Renaming renaming = std::get<1>(parentLabel);
      for (const auto edgeLiteral : edges) {
        auto literalCobinations = edgeLiteral.labelBaseCombinations();
        // only keep combinations with labels that are still there after renaming
        for (auto combination : literalCobinations) {
          if (isSubset(combination.labels(), renaming)) {
            // push renamed combination (using inverse of renaming)
            combination.rename(renaming, true);
            parentActiveLabelBaseCombinations.push_back(combination);
          }
        }
      }
    }
  }

  // 2) filter newLiterals for parent
  Cube filteredNewLiterals;
  for (const auto &literal : newLiterals) {
    auto literalLabels = literal.labels();
    auto literalCobinations = literal.labelBaseCombinations();

    if (isSubset(literalLabels, parentActiveLabels) &&
        isSubset(literalCobinations, parentActiveLabelBaseCombinations)) {
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
    spdlog::debug(
        fmt::format("[Solver] Inconsistent Node  {}", std::hash<RegularTableau::Node>()(*parent)));
    // TODO: remove:
    // printCube(filteredNewLiterals);
    // at this point filteredNewLiterals is the alternative child
    return filteredNewLiterals;
  }
  return std::nullopt;
}
// helper end

std::vector<Assumption> RegularTableau::emptinessAssumptions;
std::vector<Assumption> RegularTableau::idAssumptions;
std::map<std::string, Assumption> RegularTableau::baseAssumptions;
int RegularTableau::saturationBound = 1;

RegularTableau::RegularTableau(std::initializer_list<Literal> initalLiterals)
    : RegularTableau(std::vector(initalLiterals)) {}
RegularTableau::RegularTableau(Cube initalLiterals) {
  Tableau t{initalLiterals};
  expandNode(nullptr, &t);
#if DEBUG
  exportProof("regular-debug");
#endif
}

// assumptions:
// clause is in normal form
RegularTableau::Node *RegularTableau::addNode(Cube clause, EdgeLabel &label) {
  // rename clause events such that two isomorphic nodes are equal after renaming
  auto renaming = rename(clause);
  // update edge label
  std::get<1>(label) = renaming;
  // create node, add to "nodes" (returns pointer to existing node if already exists)
  auto newNode = std::make_unique<Node>(clause);
  auto insertion = nodes.insert(std::move(newNode));
  if (insertion.second) {
    // new node has been added added (no isomorphic node existed)
    unreducedNodes.push(insertion.first->get());

    spdlog::debug(fmt::format("[Solver] Add node  {}", std::hash<Node>()(*insertion.first->get())));
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

  // dont add epsilon edges that already exist
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
    spdlog::debug(fmt::format("[Solver] Add edge  {} -> {}", std::hash<Node>()(*parent),
                              std::hash<Node>()(*child)));
    // adding edge
    // only add each child once to child nodes
    auto childIt = std::find(parent->childNodes.begin(), parent->childNodes.end(), child);
    if (childIt == parent->childNodes.end()) {
      parent->childNodes.push_back(child);
    }
    child->parentNodes[parent].push_back(label);

    // set first parent node if not already set & not epslilon
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
#if DEBUG
    exportProof("regular-debug");
#endif
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
  for (size_t i = 0; i < RegularTableau::saturationBound; i++) {
    saturate(dnf);
  }

  if (dnf.empty() && node != nullptr) {
    node->closed = true;
    return;
  }
  spdlog::debug(
      fmt::format("[Solver] Expand node {} ", (node == nullptr ? 0 : std::hash<Node>()(*node))));

  // for each clause: calculate potential child
  for (const auto &clause : dnf) {
    Cube droppedLiterals;
    EdgeLabel edgeLabel;
    auto newCube = purge(clause, droppedLiterals, edgeLabel);

    // check if immediate inconsistency exists (inconsistent literals that are immdediatly
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
  spdlog::debug(fmt::format("[Solver] Check consistency {} -> {}", std::hash<Node>()(*parent),
                            std::hash<Node>()(*child)));
  if (child->cube.empty()) {
    // empty child not incsonistent
    return false;
  }
  // calculate converse request
  Cube converseRequest = child->cube;
  // use parent naming: label has already parent naming, rename child cube
  for (auto &literal : converseRequest) {
    literal.rename(std::get<1>(label), true);
  }

  Tableau calcConverseReq(converseRequest);
  // add cube of label (adding them to converseRequest is incomplete, because inital
  // contradiction are not checked)
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

  // TODO: check this: must this be the intersection of inconsistentLiterals for all branches?
  DNF dnf = calcConverseReq.rootNode->extractDNF();
  // each Disjunct must have some inconsistency to have an inconsistency
  // otherwise the proof for one clause without inconsistency would subsume the others (which have
  // more literals) if this is the case: add one epsilon edge for each clause
  for (const auto &clause : dnf) {
    if (!getInconsistentLiterals(parent, clause)) {
      return false;
    }
  }

  for (const auto &clause : dnf) {
    auto alternativeNode = getInconsistentLiterals(parent, clause);
    if (alternativeNode) {
      // create new fixed Node
      Node *fixedNode = addNode(*alternativeNode, label);
      addEdge(parent, fixedNode, {});
    }
  }
  return true;
}

void RegularTableau::saturate(DNF &dnf) {
  if (!RegularTableau::baseAssumptions.empty()) {
    DNF newDnf;
    for (auto &clause : dnf) {
      // saturation phase
      for (Literal &literal : clause) {
        literal.saturateBase();
      }
      // normalize
      Tableau saturatedTableau{clause};
      auto saturatedDnf = saturatedTableau.dnf();
      newDnf.insert(std::end(newDnf), std::begin(saturatedDnf), std::end(saturatedDnf));
    }
    swap(dnf, newDnf);
  }
  if (!RegularTableau::idAssumptions.empty()) {
    DNF newDnf;
    for (auto &clause : dnf) {
      // saturation phase
      for (Literal &literal : clause) {
        literal.saturateId();
      }
      // normalize
      Tableau saturatedTableau{clause};
      auto saturatedDnf = saturatedTableau.dnf();
      newDnf.insert(std::end(newDnf), std::begin(saturatedDnf), std::end(saturatedDnf));
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
      int left = *edge.leftOperand->label;
      int right = *edge.rightOperand->label;
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

void RegularTableau::exportProof(std::string filename) const {
  std::ofstream file("./output/" + filename + ".dot");
  toDotFormat(file);
  file.close();
}
