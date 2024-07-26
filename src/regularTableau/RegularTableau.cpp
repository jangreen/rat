#include "RegularTableau.h"

#include <spdlog/spdlog.h>

#include <iostream>
#include <map>
#include <unordered_set>

#include "../utility.h"

namespace {

void findReachableNodes(RegularNode *node, std::unordered_set<RegularNode *> &reach) {
  auto [_, inserted] = reach.insert(node);
  if (inserted) {
    for (const auto &child : node->getChildren()) {
      findReachableNodes(child, reach);
    }
  }
}

// returns fixed node as set, otherwise nullopt if consistent
std::optional<DNF> getFixedDnf(const RegularNode *parent, const Cube &newLiterals) {
  if (parent == nullptr) {
    return std::nullopt;
  }
  Cube mergedCube = parent->getCube();
  std::ranges::copy_if(newLiterals, std::back_inserter(mergedCube),
                       [&](const auto &literal) { return !contains(mergedCube, literal); });
  assert(validateNormalizedCube(mergedCube));

  Tableau tableau(mergedCube);
  DNF dnf = tableau.computeDnf();

  // 2) filter literal relevant for parent
  const auto parentActiveEvents = gatherActiveEvents(parent->getCube());
  for (auto &cube : dnf) {
    std::erase_if(cube, [&](const Literal &literal) {
      return !isLiteralActive(literal, parentActiveEvents);
    });
  }

  // 3) If no new literals, nothing to do
  if (std::ranges::any_of(dnf,
                          [&](const auto &cube) { return isSubset(cube, parent->getCube()); })) {
    if (dnf.size() > 1) {
      throw std::logic_error("This is no error but unexpected to happen.");
    }
    return std::nullopt;
  }

  return dnf;
}

}  // namespace

bool RegularTableau::isReachableFromRoots(const RegularNode *node) const {
  return node->reachabilityTreeParent != nullptr ||
         rootNodes.contains(const_cast<RegularNode *>(node));
}

RegularTableau::RegularTableau(const Cube &initialLiterals) {
  Tableau t(initialLiterals);
  expandNode(nullptr, &t);
}

bool RegularTableau::validate() const {
  std::unordered_set<RegularNode *> reachable;
  for (auto &root : rootNodes) {
    findReachableNodes(root, reachable);
  }

  assert(validateReachabilityTree());

  const auto allNodesValid = std::ranges::all_of(reachable, [&](auto &node) {
    const bool nodeValid = node->validate();
    assert(nodeValid);
    // all reachable nodes have reachablilityTreeParent
    assert(isReachableFromRoots(node));
    return nodeValid;
  });

  // get open leafs
  auto openLeafs = reachable;
  std::erase_if(openLeafs, [&](const RegularNode *node) {
    return !node->isOpenLeaf() || !node->getEpsilonChildren().empty() || node == currentNode;
  });

  // open leafs are in unreduced nodes
  const auto openLeafsAreUnreduced = std::ranges::all_of(openLeafs, [&](const auto &openLeaf) {
    const auto container = get_const_container(unreducedNodes);
    const auto leafValid = std::ranges::find(container, openLeaf) != container.end();
    if (!leafValid) {
      std::cout << " Leaked node: " << openLeaf << std::endl;
      print(openLeaf->cube);
    }
    assert(leafValid);
    return leafValid;
  });

  assert(openLeafsAreUnreduced);
  assert(allNodesValid);
  return openLeafsAreUnreduced && allNodesValid;
}

bool RegularTableau::validateReachabilityTree() const {
  std::unordered_set<RegularNode *> reachable;
  for (auto &root : rootNodes) {
    findReachableNodes(root, reachable);
  }

  // all reachable nodes have reachablilityTreeParent
  // tree is actual tree
  for (const auto &node : reachable) {
    std::unordered_set<RegularNode *> visited;

    RegularNode *cur = node;
    while (cur != nullptr) {
      const auto &[_, isNew] = visited.insert(cur);
      if (!isNew) {
        exportDebug("debug");
      }
      assert(isNew);
      cur = cur->reachabilityTreeParent;
    }
  }
  return true;
}

// checks if renaming exists when adding and returns already existing node if possible
std::pair<RegularNode *, Renaming> RegularTableau::newNode(const Cube &cube) {
  assert(validateNormalizedCube(cube));  // cube is in normal form
  assert(validate());

  // create node, add to "nodes" (returns pointer to existing node if already exists)
  const auto &[newNode, renaming] = RegularNode::newNode(cube);
  auto newNodePtr = std::unique_ptr<RegularNode>(newNode);
  auto [iter, added] = nodes.insert(std::move(newNodePtr));
  unreducedNodes.push(iter->get());

  assert(iter->get()->validate());
  assert(validate());
  return {iter->get(), renaming};
}

void RegularTableau::removeEdge(RegularNode *parent, RegularNode *child) const {
  parent->removeChild(child);
  removeEdgeUpdateReachabilityTree(parent, child);
  assert(validateReachabilityTree());
}

// parent == nullptr -> rootNode
void RegularTableau::addEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label) {
  assert(parent != nullptr && parent->validate());
  assert(child != nullptr && child->validate());
  assert(validate());

  // don't add edges that already exist
  if (child->getParents().contains(parent)) {
    return;
  }

  // don't check inconsistent edges
  // do this lazy
  // if (isInconsistent(parent, child, label)) {
  //   return;
  // }

  // add edge
  const auto inserted = parent->addChild(child, label);
  if (!inserted) {
    return;
  }
  addEdgeUpdateReachabilityTree(parent, child);

  exportDebug("debug");
  assert(validateReachabilityTree());

  // if child has epsilon edge -> add shortcuts
  for (const auto epsilonChildChild : child->getEpsilonChildren()) {
    const auto &childRenaming = epsilonChildChild->getEpsilonParents().at(child);
    addEdge(parent, epsilonChildChild, label.compose(childRenaming));
  }
  assert(validate());
}

void RegularTableau::addEpsilonEdge(RegularNode *parent, RegularNode *child,
                                    const EdgeLabel &label) {
  assert(parent != nullptr && parent->validate());
  assert(child != nullptr && child->validate());
  assert(validate());

  const auto inserted = parent->addEpsilonChild(child, label);
  if (!inserted) {
    return;
  }
  exportDebug("debug");

  // add shortcuts
  for (const auto &[grandparentNode, grandparentLabel] : parent->getParents()) {
    addEdge(grandparentNode, child, grandparentLabel.compose(label));
  }
  for (const auto &[grandparentNode, grandparentLabel] : parent->getEpsilonParents()) {
    addEpsilonEdge(grandparentNode, child, grandparentLabel.compose(label));
  }
  // add epsilon child of a root nodes to root nodes
  if (rootNodes.contains(parent)) {
    rootNodes.insert(child);
  }

  assert(validate());
}

bool RegularTableau::solve() {
  while (!unreducedNodes.empty()) {
    exportDebug("debug");
    assert(validate());

    currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    assert(validate());

    if (!currentNode->isOpenLeaf() || !currentNode->getEpsilonChildren().empty() ||
        !isReachableFromRoots(currentNode)) {
      // skip already closed nodes and nodes that cannot be reached by a root node
      assert(validate());
      continue;
    }
    // current node is open leaf
    assert(currentNode->isOpenLeaf());
    Cube currentCube = currentNode->cube;

    // 1) weaken positive edge predicates
    if (cubeHasPositiveEdgePredicate(currentCube)) {
      std::erase_if(currentCube, std::mem_fn(&Literal::isPositiveEdgePredicate));
      removeUselessLiterals(currentCube);
    }

    Tableau tableau{currentCube};
    // IMPORTANT: currently we rely on this property to be correct.
    // intuition: using always an event that occurrs prefers events that occcur once to events that
    // occurr multiple times. This ensures that we keep the number of events used in a cube minimal
    auto minimalOccurringActiveEvent = gatherMinimalOccurringActiveEvent(currentCube);
    if (minimalOccurringActiveEvent &&
        tableau.tryApplyModalRuleOnce(minimalOccurringActiveEvent.value())) {
      expandNode(currentNode, &tableau);
      assert(validate());
      continue;
    }

    // 2) Test whether counterexample is spurious
    // 3) Check inconsistencies lazy
    if (isSpurious(currentNode) && isInconsistentLazy(currentNode)) {
      assert(validate());
      continue;
    }

    // 4) open leaf, extract counterexample
    extractCounterexample(currentNode);
    extractCounterexamplePath(currentNode);
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
  assert(validate());

  // calculate dnf
  const auto dnf = tableau->computeDnf();
  if (dnf.empty() && node != nullptr) {
    node->closed = true;
    return;
  }

  // add node and edge for each cube
  for (auto &cube : dnf) {
    const auto &[childNode, edgeLabel] = newNode(cube);
    const bool isRootNode = node == nullptr;
    if (isRootNode) {
      rootNodes.insert(childNode);
    } else {
      addEdge(node, childNode, edgeLabel);
    }
  }
  assert(validate());
}

// input is edge (parent,label,child)
// must not be part of the proof graph
// returns if given edge is inconsistent
bool RegularTableau::isInconsistent(RegularNode *parent, const RegularNode *child,
                                    const EdgeLabel &label) {
  assert(parent != nullptr);
  assert(child != nullptr);
  assert(validateNormalizedCube(child->cube));

  // TODO: fix
  // if (parent->inconsistentChildrenChecked.contains(child)) {
  //   return true;  // already inconsistent
  // }
  // parent->inconsistentChildrenChecked.insert({child, label});

  if (child->cube.empty()) {
    // empty child not inconsistent
    return false;
  }

  // use parent naming: rename child cube
  Cube renamedChild = child->cube;
  const Renaming inverted = label.inverted();
  // erase literals that cannot be renamed
  std::erase_if(renamedChild, [&](const Literal &literal) {
    const bool isRenamable = inverted.isStrictlyRenameable(literal.events()) /* TODO (topEvent
                             optimization): && inverted.isStrictlyRenameable(literal.topEvents())*/
        ;
    const bool isPositiveEdgeOrNegated = literal.isPositiveEdgePredicate() || literal.negated;
    const bool keep = isRenamable && isPositiveEdgeOrNegated;
    return !keep;
  });
  renameCube(inverted, renamedChild);
  assert(validateNormalizedCube(renamedChild));

  if (const auto fixedDNF = getFixedDnf(parent, renamedChild)) {
    // create new fixed Node
    for (const auto &cube : fixedDNF.value()) {
      const auto [fixedNode, renaming] = newNode(cube);
      addEpsilonEdge(parent, fixedNode, renaming);
    }
    return true;
  }

  return false;
}

void RegularTableau::removeEdgeUpdateReachabilityTree(const RegularNode *parent,
                                                      const RegularNode *child) const {
  if (child->reachabilityTreeParent != parent) {
    return;
  }

  // reset all nodes
  for (const auto &node : nodes) {
    node->reachabilityTreeParent = nullptr;
  }

  // bfs from root nodes
  std::deque<RegularNode *> worklist;
  std::unordered_set<RegularNode *> visited;
  for (const auto rootNode : rootNodes) {
    worklist.push_back(rootNode);
  }

  while (!worklist.empty()) {
    const auto node = worklist.front();
    worklist.pop_front();
    visited.insert(node);

    for (const auto curChild : node->getChildren()) {
      if (!visited.contains(curChild)) {
        curChild->reachabilityTreeParent = node;
        worklist.push_back(curChild);
      }
    }
  }
}

void RegularTableau::addEdgeUpdateReachabilityTree(RegularNode *parent, RegularNode *child) {
  if (isReachableFromRoots(child) || !isReachableFromRoots(parent)) {
    return;
  }
  child->reachabilityTreeParent = parent;

  std::deque<RegularNode *> worklist;
  worklist.push_back(child);

  // push reachable open leafs to unreduced nodes
  while (!worklist.empty()) {
    const auto node = worklist.front();
    worklist.pop_front();

    if (node->isOpenLeaf()) {
      unreducedNodes.push(node);
    }

    for (const auto nodeChild : node->getChildren()) {
      if (!isReachableFromRoots(nodeChild)) {
        nodeChild->reachabilityTreeParent = node;
        worklist.push_back(nodeChild);
      }
    }
  }
}

void RegularTableau::findAllPathsToRoots(RegularNode *node, Path &currentPath,
                                         std::vector<Path> &allPaths) const {
  if (contains(currentPath, node)) {
    return;
  }

  currentPath.push_back(node);

  if (rootNodes.contains(node)) {
    allPaths.push_back(currentPath);
  }

  for (const auto &[parent, _] : node->getParents()) {
    findAllPathsToRoots(parent, currentPath, allPaths);
  }

  currentPath.pop_back();
}

// inconsistencies
// soundness: there are more(or equal) models for children than parent
// you cannot guarantee equal during the proof immediately, but in hindsight
// inconsistency indicates that children might have more models (but this is not guaranteed)
// fixing: fixed parent could have less parents
// fixing: fixed parent + consistent children of parent = models of parent

bool RegularTableau::isInconsistentLazy(RegularNode *openLeaf) {
  assert(openLeaf != nullptr);
  assert(!openLeaf->closed);
  assert(openLeaf->getChildren().empty());

  std::vector<Path> allPaths;
  Path intialPath;
  findAllPathsToRoots(openLeaf, intialPath, allPaths);

  while (!allPaths.empty()) {
    const auto path = std::move(allPaths.back());
    allPaths.pop_back();
    bool pathInconsistent = false;

    for (size_t i = path.size() - 1; i > 0; i--) {
      auto parent = path.at(i);
      const auto child = path.at(i - 1);

      const auto &renaming = parent->getLabelForChild(child);
      if (isInconsistent(parent, child, renaming)) {
        pathInconsistent = true;
        // remove inconsistent edge parent -> child
        removeEdge(parent, child);
        if (parent->children.empty() && parent->epsilonChildren.empty()) {
          // is leaf // TODO: use function
          parent->closed = true;
        }

        // remove all paths that contain parent -> child
        const auto &[begin, end] = std::ranges::remove_if(allPaths, [&](const Path &path) {
          for (auto it = path.rbegin(); it != path.rend(); ++it) {
            const bool parentMatch = parent == it[0];
            const bool childMatch = child == it[1];
            if (parentMatch || childMatch) {
              return parentMatch && childMatch;
            }
          }
          return false;
        });
        allPaths.erase(begin, end);
        break;  // only fix first inconsistency on path
      }
    }
    if (!pathInconsistent) {
      return false;
    }
  }
  return true;

  // if (node->firstParentNode == nullptr) {
  //   return false;
  // }

  // if (isInconsistentLazy(node->firstParentNode)) {
  //   // edge: firstParentNode -> node is inconsistent
  //   // remove it
  //   node->firstParentNode->removeChild(node);
  //   return true;
  // }

  // return isInconsistent(node->firstParentNode, node, node->parents.at(node->firstParentNode));
}
Cube RegularTableau::getModel(const RegularNode *openLeaf) const {
  const RegularNode *cur = openLeaf;
  Cube edges;
  while (cur != nullptr) {
    std::ranges::copy_if(cur->cube, std::back_inserter(edges), &Literal::isPositiveEdgePredicate);

    if (cur->reachabilityTreeParent != nullptr) {
      auto renaming = cur->reachabilityTreeParent->getLabelForChild(cur).inverted();
      renameCube(renaming, edges);
    }

    cur = cur->reachabilityTreeParent;
  }
  return edges;
}

Renaming RegularTableau::getRootRenaming(const RegularNode *node) const {
  assert(node != nullptr);
  assert(isReachableFromRoots(node));

  if (rootNodes.contains(const_cast<RegularNode *>(node))) {
    return Renaming::identity(gatherActiveEvents(node->cube));
  }

  auto rootRenaming = node->reachabilityTreeParent->getLabelForChild(node).inverted();
  auto renCur = node->reachabilityTreeParent;
  while (renCur->reachabilityTreeParent != nullptr) {
    auto curRenaming = renCur->reachabilityTreeParent->getLabelForChild(renCur).inverted();
    rootRenaming = rootRenaming.totalCompose(curRenaming);
    renCur = renCur->reachabilityTreeParent;
  }

  return rootRenaming;
}

bool RegularTableau::isSpurious(const RegularNode *openLeaf) const {
  const auto model = getModel(openLeaf);

  auto rootNode = openLeaf;
  while (rootNode->reachabilityTreeParent != nullptr) {
    rootNode = rootNode->reachabilityTreeParent;
  }

  Cube checkedCube = model;
  std::ranges::copy_if(rootNode->cube, std::back_inserter(checkedCube), &Literal::negated);
  Tableau finiteTableau(checkedCube);
  return finiteTableau.computeDnf().empty();
}

void RegularTableau::extractCounterexample(const RegularNode *openLeaf) const {
  assert(!openLeaf->closed);
  assert(openLeaf->getChildren().empty());
  assert(isReachableFromRoots(openLeaf));

  std::ofstream counterexamleModel("./output/counterexampleModel.dot");
  counterexamleModel << "digraph { node[shape=\"point\"]\n";

  const RegularNode *cur = openLeaf;
  Cube edges;
  while (cur != nullptr) {
    std::ranges::copy_if(cur->cube, std::back_inserter(edges),
                         [](const Literal &literal) { return literal.isPositiveEdgePredicate(); });
    assert(isReachableFromRoots(cur));
    if (cur->reachabilityTreeParent != nullptr) {
      // rename to parent
      const auto renaming = cur->reachabilityTreeParent->getLabelForChild(cur).inverted();
      for (auto &edge : edges) {
        edge.rename(renaming);
      }
    }

    cur = cur->reachabilityTreeParent;
  }

  for (const auto &edge : edges) {
    const int left = edge.leftEvent->label.value();
    const int right = edge.rightEvent->label.value();
    auto baseRelation = edge.identifier.value();

    counterexamleModel << "N" << left << " -> N" << right;
    counterexamleModel << "[label = \"" << baseRelation << "\"];" << std::endl;
  }

  counterexamleModel << "}" << std::endl;
  counterexamleModel.close();
}

void RegularTableau::extractCounterexamplePath(const RegularNode *openLeaf) const {
  assert(!openLeaf->closed);
  assert(openLeaf->getChildren().empty());
  assert(isReachableFromRoots(openLeaf));

  std::ofstream counterexamplePath("./output/counterexamplePath.dot");
  counterexamplePath << "digraph {\nnode[shape=\"box\"]\n";
  std::vector<RegularNode *> openBranch;

  const RegularNode *cur = openLeaf;
  while (cur != nullptr) {
    assert(isReachableFromRoots(cur));
    if (cur->reachabilityTreeParent != nullptr) {
      // determine renaming to root node for cur
      auto rootRenaming = getRootRenaming(cur);

      Cube cube = cur->cube;
      renameCube(rootRenaming, cube);
      std::ranges::sort(cube);
      auto newNode = new RegularNode(std::move(cube));
      if (!openBranch.empty()) {
        newNode->addChild(openBranch.back(), Renaming::minimal({}));
        openBranch.back()->reachabilityTreeParent = newNode;
      }
      openBranch.push_back(newNode);
    }

    cur = cur->reachabilityTreeParent;
  }

  // generate path
  if (openBranch.empty()) {
    // TODO: extend to this case
  } else {
    counterexamplePath << "root -> N" << openBranch.back() << "[color=\"red\"];\n";
    openBranch.back()->reachabilityTreeParent =
        openBranch.back();  // hack because there are no root Nodes
    for (const auto node : openBranch) {
      nodeToDotFormat(node, counterexamplePath);
    }
  }

  counterexamplePath << "}" << std::endl;
  counterexamplePath.close();
}

void RegularTableau::toDotFormat(std::ofstream &output) const {
  std::unordered_set<RegularNode *> printed;

  output << "digraph {\nnode[shape=\"box\"]\n";

  std::deque<RegularNode *> worklist;
  for (const auto rootNode : rootNodes) {
    worklist.push_back(rootNode);
    output << "root -> N" << rootNode << "[color=\"red\"];\n";
  }
  while (!worklist.empty()) {
    const auto node = worklist.front();
    worklist.pop_front();
    if (printed.contains(node)) {
      continue;
    }
    printed.insert(node);

    nodeToDotFormat(node, output);

    for (const auto epsilonChild : node->getEpsilonChildren()) {
      worklist.push_back(epsilonChild);
    }
    for (const auto child : node->getChildren()) {
      worklist.push_back(child);
    }
  }

  output << "}" << std::endl;
}

void RegularTableau::nodeToDotFormat(const RegularNode *node, std::ofstream &output) const {
  node->toDotFormat(output);

  // non reachable nodes are dotted
  if (!isReachableFromRoots(node)) {
    output << "N" << node << " [style=dotted, fontcolor=\"grey\"]";
  }

  // edges
  for (const auto epsilonChild : node->getEpsilonChildren()) {
    const auto label = epsilonChild->getEpsilonParents().at(const_cast<RegularNode *>(node));
    output << "N" << node << " -> N" << epsilonChild;
    output << "[tooltip=\"";
    label.toDotFormat(output);
    output << "\", color=\"grey\"];\n";
  }
  for (const auto child : node->getChildren()) {
    const auto label = node->getLabelForChild(child);
    output << "N" << node << " ->  N" << child;
    output << "[";
    if (child->reachabilityTreeParent == node) {
      output << "color=\"red\", ";
    }
    if (!isReachableFromRoots(node)) {
      output << "style=dotted, color=\"grey\", ";  // edges in unreachable proof are dotted
    }
    output << "tooltip=\"";
    label.toDotFormat(output);
    output << "\"];\n";
  }
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
