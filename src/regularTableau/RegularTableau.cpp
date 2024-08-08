#include "RegularTableau.h"

#include <spdlog/spdlog.h>

#include <boost/pending/disjoint_sets.hpp>
#include <iostream>
#include <map>
#include <unordered_set>

#include "../Assumption.h"
#include "../tableau/Rules.h"
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
  auto dnf = tableau.computeDnf();

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
  return node->reachabilityTreeParent != nullptr || rootNode.get() == node;
}

RegularTableau::RegularTableau(const Cube &initialLiterals)
    : initialCube(initialLiterals), rootNode(new RegularNode(initialLiterals)) {
  Tableau t(initialLiterals);
  expandNode(rootNode.get(), &t);
}

bool RegularTableau::validate() const {
  std::unordered_set<RegularNode *> reachable;
  findReachableNodes(rootNode.get(), reachable);

  assert(validateReachabilityTree());

  const auto allNodesValid = std::ranges::all_of(reachable, [&](auto &node) {
    const bool nodeValid = node == rootNode.get() || node->validate();
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
    const auto leafValid =
        std::ranges::find(container, openLeaf) != container.end() || openLeaf == rootNode.get();
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
  findReachableNodes(rootNode.get(), reachable);

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
  // we do not need to push newNode to unreducedNodes
  // this is handled automatically when a node becomes reachable by addEdge
  const auto &[newNode, renaming] = RegularNode::newNode(cube);
  auto newNodePtr = std::unique_ptr<RegularNode>(newNode);
  auto [iter, added] = nodes.insert(std::move(newNodePtr));

  Stats::boolean("#nodes").count(added);
  Stats::value("node size").set(cube.size());

  assert(iter->get()->validate());
  assert(validate());
  return {iter->get(), renaming};
}

void RegularTableau::newChildren(RegularNode *node, const DNF &dnf) {
  for (const auto &cube : dnf) {
    const auto [child, edgeLabel] = newNode(cube);
    addEdge(node, child, edgeLabel);
  }
}

void RegularTableau::removeEdge(RegularNode *parent, RegularNode *child) const {
  parent->children.erase(child);
  child->parents.erase(parent);
  removeEdgeUpdateReachabilityTree(parent, child);
  assert(validateReachabilityTree());
}

// parent == nullptr -> rootNode
void RegularTableau::addEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label) {
  assert(parent != nullptr && (parent == rootNode.get() || parent->validate()));
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
    addEdge(parent, epsilonChildChild, label.strictCompose(childRenaming));
  }
  assert(validate());
}

void RegularTableau::addEpsilonEdge(RegularNode *parent, RegularNode *child,
                                    const EdgeLabel &label) {
  assert(parent != rootNode.get());  // never add epsilon edge from root
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
    addEdge(grandparentNode, child, grandparentLabel.strictCompose(label));
  }
  for (const auto &[grandparentNode, grandparentLabel] : parent->getEpsilonParents()) {
    addEpsilonEdge(grandparentNode, child, grandparentLabel.strictCompose(label));
  }

  assert(validate());
}

bool RegularTableau::solve() {
  while (!unreducedNodes.empty()) {
    exportDebug("debug");
    assert(validate());

    currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    Stats::counter("#iterations")++;
    assert(validate());

    if (!currentNode->isOpenLeaf() || !isReachableFromRoots(currentNode)) {
      // skip already closed nodes and nodes that cannot be reached by a root node
      assert(validate());
      continue;
    }
    // current node is open leaf
    assert(currentNode->isOpenLeaf());
    Cube currentCube = currentNode->cube;

    // 1) weaken positive edge predicates and positive setMembership
    if (cubeHasPositiveAtomic(currentCube)) {
      std::erase_if(currentCube, std::mem_fn(&Literal::isPositiveAtomic));
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

    assert(!currentNode->closed);
    assert(currentNode->getChildren().empty());
    assert(isReachableFromRoots(currentNode));

    // 2) Test whether counterexample is spurious
    if (!isSpurious(currentNode)) {
      spdlog::info("[Solver] Answer: False");
      return false;
    }

    // 3) Check inconsistencies lazy
    if (isInconsistentLazy(currentNode)) {
      assert(validate());
      continue;
    }

    // 4) Check saturation lazy
    if (saturationLazy(currentNode)) {
      continue;
    }

    throw std::logic_error("unreachable");
  }
  spdlog::info("[Solver] Answer: True");
  return true;
}

// assumptions:
// node has only normal terms
void RegularTableau::expandNode(RegularNode *node, Tableau *tableau) {
  assert(node != nullptr && (node == rootNode.get() || node->validate()));
  assert(tableau->validate());
  assert(validate());

  // calculate dnf
  const auto dnf = tableau->computeDnf();
  if (dnf.empty() && node != nullptr) {
    node->closed = true;
    return;
  }

  // add node and edge for each cube
  newChildren(node, dnf);
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

  if (child->cube.empty() || parent == rootNode.get()) {
    // not inconsistent if
    // - parent is rootNode
    // - child is empty
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
    newChildren(parent, fixedDNF.value());
    Stats::counter("isInconsistent")++;
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
  worklist.push_back(rootNode.get());

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

// TODO: remove
void RegularTableau::findAllPathsToRoots(RegularNode *node, Path &currentPath,
                                         std::vector<Path> &allPaths) const {
  if (contains(currentPath, node)) {
    return;
  }

  currentPath.push_back(node);

  if (rootNode.get() == node) {
    allPaths.push_back(currentPath);
  }

  for (const auto &parent : node->getParents() | std::views::keys) {
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
  Stats::counter("#inconsistencies (lazy)")++;
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

bool RegularTableau::saturationLazy(RegularNode *openLeaf) {
  assert(openLeaf != nullptr);
  assert(!openLeaf->closed);
  assert(openLeaf->getChildren().empty());

  const auto model = getModel(openLeaf);
  auto saturatedNodeModel = model;
  const auto saturationRenaming = saturateModel(saturatedNodeModel);

  while (isReachableFromRoots(openLeaf)) {  // as long as a path exists
    auto pathNeedsSaturation = false;

    // follow some path to root
    auto curNode = openLeaf;
    while (curNode->reachabilityTreeParent != nullptr) {
      curNode = curNode->reachabilityTreeParent;

      // check
      const auto &nodeRenaming = getRootRenaming(curNode).inverted();
      for (const auto &cubeLiteral : curNode->cube | std::views::filter(&Literal::negated)) {
        // rename cubeLiteral to root + saturationRenaming
        auto renamedLiteral = cubeLiteral;
        renamedLiteral.rename(nodeRenaming);
        Cube checkedCube = model;
        checkedCube.push_back(renamedLiteral);

        renamedLiteral.rename(saturationRenaming);
        Cube checkedCubeSaturated = saturatedNodeModel;
        checkedCubeSaturated.push_back(renamedLiteral);

        Tableau finiteTableau(checkedCube);
        Tableau finiteTableauSaturated(checkedCubeSaturated);
        const auto spurious = finiteTableau.computeDnf().empty();
        const auto saturatedSpurious = finiteTableauSaturated.computeDnf().empty();

        if (!spurious && saturatedSpurious) {
          // remove old children
          removeChildren(curNode);

          // literal is only contradicting if we saturate
          // mark cubeLiterals to saturate it during normalization
          auto saturatedLiteral = cubeLiteral;
          saturatedLiteral.applySaturation = true;
          Tableau tableau({saturatedLiteral});
          for (const auto &rootChild : tableau.getRoot()->getChildren()) {
            rootChild->appendBranch(curNode->getCube());
          }

          const auto &dnf = tableau.computeDnf(false);  // dont weakening, do saturate
          tableau.exportProof("debug");

          if (dnf.empty()) {
            curNode->closed = true;
          } else {
            newChildren(curNode, dnf);
          }

          pathNeedsSaturation = true;
          Stats::counter("#saturations (lazy)")++;
        }
      }
    }
    if (!pathNeedsSaturation) {
      return false;
    }
  }
  return true;
}

auto RegularTableau::getPathEvents(const RegularNode *openLeaf) const {
  typedef std::pair<const RegularNode *, int> EventPointer;
  typedef std::map<EventPointer, int> rank_t;
  typedef std::map<EventPointer, EventPointer> parent_t;
  rank_t ranks;
  parent_t parents;
  boost::disjoint_sets globalEvents(boost::make_assoc_property_map(ranks),
                                    boost::make_assoc_property_map(parents));

  // get all global events
  auto node = openLeaf;
  while (node != nullptr) {
    auto nodeEvents = gatherActiveEvents(node->getCube());
    for (const auto event : nodeEvents) {
      globalEvents.make_set(EventPointer{node, event});
    }
    node = node->reachabilityTreeParent;
  }

  // calculate equivalence classes
  node = openLeaf;
  while (node->reachabilityTreeParent != nullptr) {
    const auto nodeParent = node->reachabilityTreeParent;
    assert(nodeParent != nullptr);

    auto renaming = nodeParent->getLabelForChild(node).inverted();
    for (const auto [parentEvent, nodeEvent] : renaming.getMapping()) {
      globalEvents.union_set(EventPointer{node, nodeEvent}, EventPointer{nodeParent, parentEvent});
    }
    node = node->reachabilityTreeParent;
  }

  return globalEvents;
}

Cube RegularTableau::getModel(const RegularNode *openLeaf) const {
  const RegularNode *cur = openLeaf;
  Cube model;
  while (cur != nullptr) {
    std::ranges::copy_if(cur->cube, std::back_inserter(model), &Literal::isPositiveAtomic);

    if (cur->reachabilityTreeParent != nullptr) {
      auto renaming = cur->reachabilityTreeParent->getLabelForChild(cur).inverted();
      renameCube(renaming, model);
    }

    cur = cur->reachabilityTreeParent;
  }
  return model;
}

void saturateModelHelper(Cube &model, Renaming &renaming) {
  bool wasSaturated = false;

  const auto events = gatherActiveEvents(model);
  for (const auto from : events) {
    const auto f = Set::newEvent(from);
    for (const auto to : events) {
      const auto t = Set::newEvent(to);

      if (from != to) {
        for (const auto &idAssumption : Assumption::idAssumptions) {
          Cube checkedCube = model;
          const auto r = idAssumption.relation;
          const auto fromR = Set::newSet(SetOperation::image, f, r);
          const auto fromR_and_to = Set::newSet(SetOperation::setIntersection, fromR, t);
          checkedCube.emplace_back(Annotated::makeWithValue(fromR_and_to, {0, 0}), false);
          Tableau finiteTableau(checkedCube);
          if (finiteTableau.computeDnf().empty()) {
            // from == to
            // rename from -> to
            renameCube(Renaming::simple(from, to), model);
            removeDuplicates(model);

            // update renaming
            renaming = renaming.totalCompose(Renaming::simple(from, to));

            wasSaturated = true;
          }
        }
      }
      for (const auto &[baseRelation, baseAssumption] : Assumption::baseAssumptions) {
        Cube checkedCube = model;
        const auto r = baseAssumption.relation;
        const auto fromR = Set::newSet(SetOperation::image, f, r);
        const auto fromR_and_to = Set::newSet(SetOperation::setIntersection, fromR, t);
        checkedCube.emplace_back(Annotated::makeWithValue(fromR_and_to, {0, 0}), false);
        Tableau finiteTableau(checkedCube);
        if (finiteTableau.computeDnf().empty()) {
          // add edge (from, to) \in baseRelation
          auto edge = Literal(f, t, baseRelation);
          if (!contains(model, edge)) {
            model.push_back(std::move(edge));

            wasSaturated = true;

            saturateModelHelper(model, renaming);  // refresh events
            return;
          }
        }
      }
    }
    for (const auto &[baseSet, baseAssumption] : Assumption::baseSetAssumptions) {
      Cube checkedCube = model;
      const auto s = baseAssumption.set;
      const auto fS = Set::newSet(SetOperation::setIntersection, f, s);
      checkedCube.emplace_back(Annotated::makeWithValue(fS, {0, 0}), false);
      Tableau finiteTableau(checkedCube);
      if (finiteTableau.computeDnf().empty()) {
        // add set membership from \in baseSet
        auto setMembership = Literal(f, baseSet);
        if (!contains(model, setMembership)) {
          model.push_back(std::move(setMembership));

          wasSaturated = true;
        }
      }
    }
  }

  if (wasSaturated) {
    saturateModelHelper(model, renaming);
  }
}

Renaming RegularTableau::saturateModel(Cube &model) const {
  assert(validateCube(model));

  auto renaming = Renaming::empty();
  saturateModelHelper(model, renaming);

  assert(validateCube(model));
  return renaming;
}

// TODO: this function does not respect renmaing due to appendBranches
// generalization of getRootRenaming
// Renaming RegularTableau::getPathRenaming(const RegularNode *from, const RegularNode *to) const {
//   assert(from != nullptr);
//   assert(to != nullptr);
//
//   if (from == to) {
//     return Renaming::empty();
//   }
//
//   auto pathRenaming = to->reachabilityTreeParent->getLabelForChild(to).inverted();
//   auto renCur = to->reachabilityTreeParent;
//   while (renCur != from) {
//     // from must be a reachabilityTree reachble from to
//     assert(renCur->reachabilityTreeParent != nullptr);
//     auto curRenaming = renCur->reachabilityTreeParent->getLabelForChild(renCur).inverted();
//     pathRenaming = pathRenaming.compose(curRenaming);
//     renCur = renCur->reachabilityTreeParent;
//   }
//
//   return pathRenaming.inverted();
// }

Renaming RegularTableau::getRootRenaming(const RegularNode *node) const {
  assert(node != nullptr);
  assert(isReachableFromRoots(node));

  if (rootNode.get() == node) {
    return Renaming::empty();
  }
  auto rootRenaming = node->reachabilityTreeParent->getLabelForChild(node).inverted();
  auto renCur = node->reachabilityTreeParent;
  while (renCur->reachabilityTreeParent != nullptr) {
    auto curRenaming = renCur->reachabilityTreeParent->getLabelForChild(renCur).inverted();
    rootRenaming = rootRenaming.compose(curRenaming);
    renCur = renCur->reachabilityTreeParent;
  }

  return rootRenaming;
}

// this spurious check does not consider the case where a branch is renamed
bool RegularTableau::isSpurious(const RegularNode *openLeaf) const {
  auto model = getModel(openLeaf);
  auto updatedInitialCube = initialCube;
  const auto saturationRenaming = saturateModel(model);
  renameCube(saturationRenaming, updatedInitialCube);

  // check if negated literal of initial cube are satisfied
  Cube checkedCube = model;
  std::ranges::copy_if(updatedInitialCube, std::back_inserter(checkedCube), &Literal::negated);
  Tableau finiteTableau(checkedCube);
  const auto spurious = finiteTableau.computeDnf().empty();
  if (!spurious) {  // counterexample found
    finiteTableau.exportProof("spurious-check");
    exportModel("counterexample", getModel(openLeaf));
    exportModel("counterexample-saturated", model);
    exportCounterexamplePath(currentNode);
  }
  return spurious;
}

std::pair<std::map<int, int>, std::map<int, std::unordered_set<int>>> getEquivalenceClasses(
    const Cube &model) {
  std::map<int, int> minimalRepresentatives;
  std::map<int, std::unordered_set<int>> setsForRepresentatives;

  // each event is in its own equivalence class
  const auto events = gatherActiveEvents(model);
  for (const auto event : events) {
    minimalRepresentatives.insert({event, event});
    setsForRepresentatives[event].insert(event);
  }

  for (const auto &literal : model) {
    assert(literal.isPositiveAtomic());
    if (literal.isPositiveEqualityPredicate()) {
      // e1 = e2
      const int e1 = literal.leftEvent->label.value();
      const int e2 = literal.rightEvent->label.value();
      if (minimalRepresentatives.at(e1) < minimalRepresentatives.at(e2)) {
        minimalRepresentatives.at(e2) = e1;
      } else {
        minimalRepresentatives.at(e1) = e2;
      }
      const int min = std::min(minimalRepresentatives.at(e1), minimalRepresentatives.at(e2));
      setsForRepresentatives[min].insert(setsForRepresentatives.at(e1).begin(),
                                         setsForRepresentatives.at(e1).end());
      setsForRepresentatives[min].insert(setsForRepresentatives.at(e2).begin(),
                                         setsForRepresentatives.at(e2).end());
    }
  }
  return {minimalRepresentatives, setsForRepresentatives};
}

void RegularTableau::exportModel(const std::string &filename, const Cube &model) const {
  std::ofstream counterexamleModel("./output/" + filename + ".dot");
  counterexamleModel << "digraph { node[shape=\"circle\",margin=0]\n";

  const auto [representatives, equivalenceClasses] = getEquivalenceClasses(model);

  // TODO: remove
  // print(model);
  // for (auto [a, b] : representatives) {
  //   std::cout << a << " -- " << b << std::endl;
  // }
  // std::cout << std::endl;
  // for (auto [_, b] : equivalenceClasses) {
  //   for (auto s : b) {
  //     std::cout << s << " ";
  //   }
  //   std::cout << std::endl;
  // }

  auto setMemberships = model | std::views::filter(&Literal::isPositiveSetPredicate);
  std::map<int, std::unordered_set<std::string>> representativeSetMap;
  for (const auto &setMembership : setMemberships) {
    const auto representativ = representatives.at(setMembership.leftEvent->label.value());
    representativeSetMap[representativ].insert(setMembership.identifier.value());
  }

  // export events + set memberships
  for (const auto [_, event] : representatives) {
    counterexamleModel << "N" << event << "[label = \"";
    // set memberships
    if (representativeSetMap.contains(event)) {
      for (const auto &baseSet : representativeSetMap.at(event)) {
        counterexamleModel << baseSet << " ";
      }
    }
    counterexamleModel << "";
    counterexamleModel << "\", tooltip=\"";
    counterexamleModel << "event: " << event << "\n";
    counterexamleModel << "equivalenceClass: ";
    for (const auto equivlenceClassEvent : equivalenceClasses.at(event)) {
      counterexamleModel << equivlenceClassEvent << " ";
    }
    counterexamleModel << "\n";
    counterexamleModel << "\"];";
  }

  // export edges
  auto edges = model | std::views::filter(&Literal::isPositiveEdgePredicate);
  for (const auto &edge : edges) {
    const int left = edge.leftEvent->label.value();
    const int right = edge.rightEvent->label.value();
    auto baseRelation = edge.identifier.value();

    counterexamleModel << "N" << representatives.at(left) << " -> N" << representatives.at(right);
    counterexamleModel << "[label = \"" << baseRelation << "\"];" << std::endl;
  }

  counterexamleModel << "}" << std::endl;
  counterexamleModel.close();
}

void RegularTableau::exportCounterexamplePath(const RegularNode *openLeaf) const {
  assert(!openLeaf->closed);
  assert(openLeaf->getChildren().empty());
  assert(isReachableFromRoots(openLeaf));

  std::ofstream counterexamplePath("./output/counterexamplePath.dot");
  counterexamplePath << "digraph {\nnode[shape=\"box\"]\n";
  std::vector<RegularNode *> openBranch;

  const RegularNode *cur = openLeaf;
  while (cur != nullptr) {
    assert(isReachableFromRoots(cur));
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
  worklist.push_back(rootNode.get());
  output << "root -> N" << rootNode.get() << "[color=\"red\"];\n";
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
