#include "RegularTableau.h"

#include <spdlog/spdlog.h>

#include <boost/pending/disjoint_sets.hpp>
#include <iostream>
#include <map>
#include <unordered_set>

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
  expandNodeInternal(rootNode.get(), &t);
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

  // get open leafs (except for currentNode)
  auto openLeafs = reachable;
  std::erase_if(openLeafs, [&](const RegularNode *node) {
    return !node->isOpenLeaf() || node == currentNode;
  });

  // open leafs are in unreduced nodes (except for currentNode)
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
        exportDebug("debug-validateReachabilityTree");
      }
      assert(isNew);
      if (!isNew) {
        return false;
      }
      cur = cur->reachabilityTreeParent;
    }
  }
  return true;
}

bool RegularNodeHeuristic::operator()(const RegularNode *first, const RegularNode *second) const {
  return first->getCube().size() > second->getCube().size();
}

// Check if isomorphic node exists and returns it or creates new nodes
// do not need to push newNode to unreducedNodes
// this happens automatically when a node becomes reachable by addEdge
std::pair<RegularNode *, Renaming> RegularTableau::newNode(const Cube &cube) {
  assert(validateNormalizedCube(cube));  // cube is in normal form

  const auto &[newNode, renaming] = RegularNode::newNode(cube);
  auto newNodePtr = std::unique_ptr<RegularNode>(newNode);
  auto [iter, added] = nodes.insert(std::move(newNodePtr));

  Stats::boolean("#nodes").count(added);
  Stats::value("node size").set(cube.size());

  assert(iter->get()->validate());
  return {iter->get(), renaming};
}

void RegularTableau::newEdge(RegularNode *parent, RegularNode *child, const EdgeLabel &label) {
  assert(parent != nullptr && (parent == rootNode.get() || parent->validate()));
  assert(child != nullptr && child->validate());

  // don't add edges that already exist
  if (child->getParents().contains(parent)) {
    return;
  }

  const auto inserted = parent->newChild(child, label);
  if (!inserted) {
    return;
  }
  newEdgeUpdateReachabilityTree(parent, child);

  exportDebug("debug");
  assert(validateReachabilityTree());

  // if child has epsilon edge -> add shortcuts
  for (const auto epsilonChildChild : child->getEpsilonChildren()) {
    const auto &childRenaming = epsilonChildChild->getEpsilonParents().at(child);
    newEdge(parent, epsilonChildChild, label.strictCompose(childRenaming));
  }
  assert(validate());
}

void RegularTableau::newChildren(RegularNode *node, const DNF &dnf) {
  for (const auto &cube : dnf) {
    const auto [child, edgeLabel] = newNode(cube);
    newEdge(node, child, edgeLabel);
  }
}

void RegularTableau::removeEdge(RegularNode *parent, RegularNode *child) const {
  parent->children.erase(child);
  child->parents.erase(parent);
  removeEdgeUpdateReachabilityTree(parent, child);
  assert(validateReachabilityTree());
}

void RegularTableau::removeChildren(RegularNode *parent) const {
  while (!parent->children.empty()) {
    removeEdge(parent, *parent->children.begin());
  }
}

void RegularTableau::newEpsilonEdge(RegularNode *parent, RegularNode *child,
                                    const EdgeLabel &label) {
  assert(parent != rootNode.get());  // never add epsilon edge from root
  assert(parent != nullptr && parent->validate());
  assert(child != nullptr && child->validate());
  assert(validate());

  const auto inserted = parent->newEpsilonChild(child, label);
  if (!inserted) {
    return;
  }
  exportDebug("debug");

  // add shortcuts
  for (const auto &[grandparentNode, grandparentLabel] : parent->getParents()) {
    newEdge(grandparentNode, child, grandparentLabel.strictCompose(label));
  }
  for (const auto &[grandparentNode, grandparentLabel] : parent->getEpsilonParents()) {
    newEpsilonEdge(grandparentNode, child, grandparentLabel.strictCompose(label));
  }

  assert(validate());
}

bool RegularTableau::solve() {
  while (!unreducedNodes.empty()) {
    Stats::counter("#iterations")++;
    currentNode = unreducedNodes.top();
    exportDebug("debug-regularTableau");
    unreducedNodes.pop();
    assert(validate());

    // skip closed nodes (aka not open leaf)
    // skip non reachable nodes
    if (!currentNode->isOpenLeaf() || !isReachableFromRoots(currentNode)) {
      continue;
    }
    assert(currentNode->isOpenLeaf());
    assert(isReachableFromRoots(currentNode));

    // current node = open leaf
    if (expandNode()) {
      continue;
    }

    // current node = complete open leaf
    if (!isSpurious(currentNode)) {
      spdlog::info("[Solver] Answer: False");
      spdlog::info("[Solver] Counterexample:");  // TODO: make clickable link to counterexample
      getModel(currentNode).exportModel("counterexample");
      exportCounterexamplePath(currentNode);
      return false;
    }

    // spurious model
    // fix inconsistencies or apply assumptions lazy
    fixLazy();
  }
  spdlog::info("[Solver] Answer: True");
  exportProof("proof");
  return true;
}

void RegularTableau::fixLazy() {
  while (isReachableFromRoots(currentNode) && currentNode->isOpenLeaf()) {
    // IMPORTANT: each loop iteration corresponds to a different path to the root
    // which gives a different model

    // 3) Check inconsistencies lazy
    // TODO: test in isolation
    if (isInconsistentLazy(currentNode)) {
      assert(validate());
      exportDebug("debug-regularTableau");
      continue;
    }

    // 4) Check saturation lazy
    /*
     *
     * Goal: compute needed saturations per occurrence such that counterexample gets removed
     * Issue: one edge may belong to multiple occurrences (example po & po)
     *        one occurrence may have multiple edges (example Kleene Star)
     * Approach: compute per occurrence the max saturation of all edges that belong to the
     * counterexample
     *
     *  1. Compute reason (edges that witness spuriousness of counterexample) -> doable
     *      - we know the saturations needed for a reason
     *      - we don't know to which occurrences do the edges belong
     */
    if (saturationLazy(currentNode)) {
      assert(validate());
      // guarantee: currentNode is either not reachableFromRoot anymore or has a larger saturation
      // annotation and has been pushed to unreduced nodes
      continue;
    }

    // only reachable if no fixes apply
    exportProof("error-proof");
    auto model = getModel(currentNode);
    saturateModel(model);
    model.exportModel("error-model");
    throw std::logic_error("unreachable: no fix applicable for spurious model");
  }
}

// assumptions:
// node has only normal terms
void RegularTableau::expandNodeInternal(RegularNode *node, Tableau *tableau) {
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
    const bool isRenamable = inverted.isStrictlyRenameable(literal.events());
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

bool RegularTableau::expandNode() {
  // 1. weaken positive edge predicates and positive setMembership
  Cube currentCube = currentNode->cube;
  if (cubeHasPositiveAtomic(currentCube)) {
    std::erase_if(currentCube, std::mem_fn(&Literal::isPositiveAtomic));
    removeUselessLiterals(currentCube);
  }

  // 2. apply modlal rule & normalize
  Tableau tableau{currentCube};
  // IMPORTANT: currently we rely on this property to be correct.
  // intuition: using always an event that occurrs prefers events that occcur once to events that
  // occurr multiple times. This ensures that we keep the number of events used in a cube minimal
  auto minimalOccurringActiveEvent = gatherMinimalOccurringActiveEvent(currentCube);
  if (minimalOccurringActiveEvent &&
      tableau.tryApplyModalRuleOnce(minimalOccurringActiveEvent.value())) {
    expandNodeInternal(currentNode, &tableau);
    assert(validate());
    return true;
  }
  return false;
}

void RegularTableau::newEdgeUpdateReachabilityTree(RegularNode *parent, RegularNode *child) {
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

  assert(validate());
}

// inconsistencies
// soundness: there are more(or equal) models for children than parent
// you cannot guarantee equal during the proof immediately, but in hindsight
// inconsistency indicates that children might have more models (but this is not guaranteed)
// fixing: fixed parent could have less models
// fixing: fixed parent + consistent children of parent = models of parent
bool RegularTableau::isInconsistentLazy(RegularNode *openLeaf) {
  assert(openLeaf != nullptr);
  assert(openLeaf->isOpenLeaf());

  Path curPath;
  auto cur = openLeaf;
  while (cur != nullptr) {
    curPath.push_back(cur);
    cur = cur->reachabilityTreeParent;
  }

  bool pathInconsistent = false;

  for (size_t i = curPath.size() - 1; i > 0; i--) {
    auto parent = curPath.at(i);
    const auto child = curPath.at(i - 1);

    const auto &renaming = parent->getLabelForChild(child);
    if (isInconsistent(parent, child, renaming)) {
      pathInconsistent = true;
      // remove inconsistent edge parent -> child
      removeEdge(parent, child);
      if (parent->isLeaf()) {
        parent->closed = true;
      }

      break;  // only fix first inconsistency on path
    }
  }
  if (!pathInconsistent) {
    return false;
  }

  Stats::counter("#inconsistencies (lazy)")++;
  return true;
}

// returns std::nullopt if evaluated literal is false
// returns Literal with annotated reasons otherwise
std::optional<Literal> evaluateAndAnnotate(const Model &model, const Literal &negatedLiteral) {
  assert(negatedLiteral.negated);

  switch (negatedLiteral.operation) {
    case PredicateOperation::setNonEmptiness: {
      const auto interpretation = model.evaluate(negatedLiteral.set);
      const auto value = interpretation->getSetValue();
      if (value.empty()) {
        return std::nullopt;
      }

      // TODO: assert(Annotated::validate(AnnotatedSet(negatedLiteral.set, annotation)));
      const auto saturationAnnotation = annotateReasons(negatedLiteral.set, interpretation);
      auto litCopy = negatedLiteral;
      litCopy.annotation = saturationAnnotation;
      // TODO: assert(Annotated::validate(litCopy.annotatedSet()));
      return litCopy;
    }
    case PredicateOperation::set: {
      const auto baseSet = negatedLiteral.identifier.value();
      const auto event = negatedLiteral.leftEvent->label.value();

      if (model.baseSetContains(baseSet, event)) {
        auto litCopy = negatedLiteral;
        const auto reason = model.getReason(baseSet, event).value();
        assert(reason != nullptr);
        litCopy.annotation = LeafAnnotation<Reasons>::newLeaf({reason});
        return litCopy;
      }

      return std::nullopt;
    }
    case PredicateOperation::edge: {
      const auto baseRelation = negatedLiteral.identifier.value();
      const auto from = negatedLiteral.leftEvent->label.value();
      const auto to = negatedLiteral.rightEvent->label.value();

      if (model.baseRelationContains(baseRelation, from, to)) {
        auto litCopy = negatedLiteral;
        const auto reason = model.getReason(baseRelation, from, to).value();
        assert(reason != nullptr);
        litCopy.annotation = LeafAnnotation<Reasons>::newLeaf({reason});
        return litCopy;
      }

      return std::nullopt;
    }
    case PredicateOperation::equality: {
      const auto e1 = negatedLiteral.leftEvent->label.value();
      const auto e2 = negatedLiteral.rightEvent->label.value();
      if (model.containsIdentity(e1, e2)) {
        throw std::logic_error("does this happen?");
        return negatedLiteral;
      }
      return std::nullopt;
    }
    case PredicateOperation::constant:
    default:
      throw std::logic_error("unreachable");
  }
}

bool RegularTableau::saturationLazy(RegularNode *const openLeaf) {
  assert(openLeaf != nullptr);
  assert(openLeaf->isOpenLeaf());

  // get model & saturated model (wrt to root namespace)
  const auto model = getModel(openLeaf);
  auto saturatedModel = model;
  saturateModel(saturatedModel);
#if DEBUG
  model.exportModel("debug-saturationLazy.model");
  saturatedModel.exportInternalModel("debug-saturationLazy.saturatedInternalModel");
  saturatedModel.exportModel("debug-saturationLazy.saturatedModel");
#endif

  // TODO: maybe check every node, not just leafs
  // follow some path to root
  auto curNode = openLeaf;
  while (curNode != nullptr) {
    assert(validateReachabilityTree());
    if (saturateNodeLazy(curNode, model, saturatedModel)) {
      Stats::counter("#saturations (lazy)")++;
      exportDebug("debug-regularTableau");
      return true;
    }
    curNode = curNode->reachabilityTreeParent;
  }
  return false;
}

// returns true if node needs assumptions
bool RegularTableau::saturateNodeLazy(RegularNode *node, const Model &model,
                                      const Model &saturatedModel) {
  // Evaluate every negated literal in nodes's cube on model/saturatedModel
  // Needs saturation iff some literal has different evaluations for both models
  const auto &nodeRenaming = getRootRenaming(node);
  for (const auto &cubeLiteral : node->cube | std::views::filter(&Literal::negated)) {
    // IMPORTANT: use event naming from root for model/saturatedModel
    auto renamedLiteral = cubeLiteral;
    renamedLiteral.rename(nodeRenaming);
    const auto result = evaluateAndAnnotate(model, renamedLiteral);
    const auto resultSaturated = evaluateAndAnnotate(saturatedModel, renamedLiteral);

    // TODO: remove
    // std::cout << "cube: " << cubeLiteral.toString() << "\n"
    //           << (result.has_value() ? result->annotation->toString() : "?") << "#\n"
    //           << (resultSaturated.has_value() ? resultSaturated->annotation->toString() : "?")
    //           << std::endl;
    if (!result && resultSaturated) {  // node needs saturation
      // TODO: check T
      std::cout << "violated literal: " << cubeLiteral.toString()
                << "\n\treason:" << resultSaturated->annotation->toString() << std::endl;
      // Analyze the counterexample and determine which assumptions we need. Then, we apply
      // the same sequence of assumptions in the proof to exclude the spurious counterexample. We do
      // this by decorating the base relations and base sets in the respective expression in the
      // proof.
      // - Currently we do this by computing the reason for each base relation.
      // - Decorating the expressions is already done insde checkAndMarkSaturation.
      // - Here we just have to modify the proof accordingly.
      const auto &annotatedLiteral = resultSaturated.value();
      removeChildren(node);  // remove old children
      cubeLiteral.annotation = Annotated::join(cubeLiteral.annotation, annotatedLiteral.annotation);
      // IMPORTANT: invariant in validation of tableau is temporally violated
      // after removing all children we may have an open leaf that is not on unreduced nodes
      // IMPORTANT: we cannot just push it to unreducedNodes.push(node);
      // new cube could be not normalized/complete wrt. to positive base literals
      // example: A<=B |- ~A&B, A(0). B gets saturation annotation, but then ~B(0) would be active
      // TODO: assert(Annotated::validate(cubeLiteral.annotatedSet()));

      // normalize/dnf
      Tableau tableau(node->getCube());
      const auto &dnf = tableau.computeDnf();
      if (dnf.empty()) {
        node->closed = true;
      } else {
        newChildren(node, dnf);
      }
      // IMPORTANT: invariant in validation of tableau is valid again
      return true;
    }
  }
  return false;
}

Model RegularTableau::getModel(const RegularNode *openLeaf) const {
  const RegularNode *cur = openLeaf;
  Cube model;
  while (cur != nullptr) {
    std::ranges::copy_if(cur->cube, std::back_inserter(model), &Literal::isPositiveAtomic);
    removeDuplicates(model);

    if (cur->reachabilityTreeParent != nullptr) {
      auto renaming = cur->reachabilityTreeParent->getLabelForChild(cur).inverted();
      renameCube(renaming, model);
    }

    cur = cur->reachabilityTreeParent;
  }
  assert(validateCube(model));
  return Model(model);
}

// TODO: this function does not respect renmaing due to appendBranches
// generalization of getRootRenaming
// Renaming RegularTableau::getPathRenaming(const RegularNode *from, const RegularNode *to) const
// {
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

bool RegularTableau::isSpurious(const RegularNode *openLeaf) const {
  auto model = getModel(openLeaf);
  saturateModel(model);
#if DEBUG
  model.exportModel("debug-isSpurious.model");
#endif

  // spurious if any negated literal of initial cube evaluates to false
  return std::ranges::any_of(
      initialCube | std::views::filter(&Literal::negated),
      [&](const auto &negatedLiteral) { return !model.evaluate(negatedLiteral); });
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
      newNode->newChild(openBranch.back(), Renaming::minimal({}));
      openBranch.back()->reachabilityTreeParent = newNode;
    }
    openBranch.push_back(newNode);

    cur = cur->reachabilityTreeParent;
  }

  // generate path
  if (openBranch.empty()) {
    // TODO: extend to this case
  } else {
    counterexamplePath << "root -> N" << openBranch.back() << "[color=red];\n";
    openBranch.back()->reachabilityTreeParent =
        openBranch.back();  // hack because there are no root Nodes
    for (const auto node : openBranch) {
      nodeToDotFormat(node, counterexamplePath);
    }
  }

  counterexamplePath << "}\n";
  counterexamplePath.close();
}

void RegularTableau::toDotFormat(std::ofstream &output) const {
  std::unordered_set<RegularNode *> printed;

  output << "digraph {\nnode[shape=\"box\"]\n";

  std::deque<RegularNode *> worklist;
  worklist.push_back(rootNode.get());
  output << "root -> N" << rootNode.get() << "[color=red];\n";
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

  output << "}\n";
}

void RegularTableau::nodeToDotFormat(const RegularNode *node, std::ofstream &output) const {
  node->toDotFormat(output);

  // non reachable nodes are dotted
  if (!isReachableFromRoots(node)) {
    output << "N" << node << " [style=dotted, fontcolor=grey]";
  }

  // unreduced nodes are blue
  const auto container = get_const_container(unreducedNodes);
  if (std::ranges::find(container, node) != container.end()) {
    output << "N" << node << " [color=blue, fontcolor=blue";
    output << (node == currentNode ? ", fillcolor=lightgrey, style=filled]" : "]");
  }

  // edges
  for (const auto epsilonChild : node->getEpsilonChildren()) {
    const auto label = epsilonChild->getEpsilonParents().at(const_cast<RegularNode *>(node));
    output << "N" << node << " -> N" << epsilonChild;
    output << "[tooltip=\"";
    label.toDotFormat(output);
    output << "\", color=grey];\n";
  }
  for (const auto child : node->getChildren()) {
    const auto label = node->getLabelForChild(child);
    output << "N" << node << " ->  N" << child;
    output << "[";
    if (child->reachabilityTreeParent == node) {
      output << "color=red, ";
    }
    if (!isReachableFromRoots(node)) {
      output << "style=dotted, color=grey, ";  // edges in unreachable proof are dotted
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
