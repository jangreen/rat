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
      cur = cur->reachabilityTreeParent;
    }
  }
  return true;
}

// checks if renaming exists when adding and returns already existing node if possible
std::pair<RegularNode *, Renaming> RegularTableau::newNode(const Cube &cube) {
  assert(validateNormalizedCube(cube));  // cube is in normal form

  // create node, add to "nodes" (returns pointer to existing node if already exists)
  // we do not need to push newNode to unreducedNodes
  // this is handled automatically when a node becomes reachable by addEdge
  const auto &[newNode, renaming] = RegularNode::newNode(cube);
  auto newNodePtr = std::unique_ptr<RegularNode>(newNode);
  auto [iter, added] = nodes.insert(std::move(newNodePtr));

  Stats::boolean("#nodes").count(added);
  Stats::value("node size").set(cube.size());

  assert(iter->get()->validate());
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
    currentNode = unreducedNodes.top();
    exportDebug("debug-regularTableau");
    unreducedNodes.pop();
    Stats::counter("#iterations")++;
    assert(validate());

    if (!currentNode->isOpenLeaf() || !isReachableFromRoots(currentNode)) {
      // skip already closed nodes and nodes that cannot be reached by a root node
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
      assert(validate());
      continue;
    }

    exportProof("error");
    auto model = getModel(currentNode);
    saturateModel(model);
    model.exportModel("error-model");
    throw std::logic_error("unreachable: no rule applicable");
  }
  spdlog::info("[Solver] Answer: True");
  exportProof("proof");
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

  assert(validate());
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

  while (isReachableFromRoots(openLeaf)) {  // as long as a path exists
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
CanonicalAnnotation<SaturationAnnotation> makeSaturationAnnotationHelper(
    const AnnotatedSet<SatExprValue> &annotatedSet, const Event &tracedValue);
CanonicalAnnotation<SaturationAnnotation> makeSaturationAnnotationHelper(
    const AnnotatedRelation<SatExprValue> &annotatedRelation, const EventPair &tracedValue) {
  const auto &[relation, annotation] = annotatedRelation;

  switch (relation->operation) {
    case RelationOperation::relationIntersection: {
      const auto newLeft = makeSaturationAnnotationHelper(
          Annotated::getLeftRelation(annotatedRelation), tracedValue);
      const auto newRight =
          makeSaturationAnnotationHelper(Annotated::getRight(annotatedRelation), tracedValue);
      assert(Annotated::validate({relation->leftOperand, newLeft}));
      assert(Annotated::validate({relation->rightOperand, newRight}));
      return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
    }
    case RelationOperation::relationUnion: {
      const auto [left, lsMap] =
          std::get<SatRelationValue>(annotation->getLeft()->getValue().value());
      const auto [right, rsMap] =
          std::get<SatRelationValue>(annotation->getRight()->getValue().value());
      if (left.contains(tracedValue)) {
        const auto newLeft = makeSaturationAnnotationHelper(
            Annotated::getLeftRelation(annotatedRelation), tracedValue);
        const auto newRight = Annotated::makeWithValue(relation->rightOperand, {0, 0});
        return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
      }
      if (right.contains(tracedValue)) {
        const auto newLeft = Annotated::makeWithValue(relation->leftOperand, {0, 0});
        const auto newRight =
            makeSaturationAnnotationHelper(Annotated::getRight(annotatedRelation), tracedValue);
        return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
      }
      throw std::logic_error("unreachable");
    }
    case RelationOperation::composition: {
      const auto [from, to] = tracedValue;
      const auto [left, lsMap] =
          std::get<SatRelationValue>(annotation->getLeft()->getValue().value());
      const auto [right, rsMap] =
          std::get<SatRelationValue>(annotation->getRight()->getValue().value());
      for (const auto [lFrom, lTo] : left) {
        if (from == lFrom) {
          for (const auto [rFrom, rTo] : right) {
            if (lTo == rFrom && rTo == to) {
              const auto newLeft = makeSaturationAnnotationHelper(
                  Annotated::getLeftRelation(annotatedRelation), {lFrom, lTo});
              const auto newRight = makeSaturationAnnotationHelper(
                  Annotated::getRight(annotatedRelation), {rFrom, rTo});
              return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
            }
          }
        }
      }
      throw std::logic_error("unreachable");
    }
    case RelationOperation::converse: {
      const auto [from, to] = tracedValue;
      return makeSaturationAnnotationHelper(Annotated::getLeftRelation(annotatedRelation),
                                            {to, from});
    }
    case RelationOperation::setIdentity: {
      const auto [from, to] = tracedValue;
      if (from != to) {
        throw std::logic_error("unreachable");
      }
      return makeSaturationAnnotationHelper(Annotated::getLeftSet(annotatedRelation), from);
    }
    case RelationOperation::transitiveClosure: {
      // TODO:
      // do not mark relation inside transitive closure
      return Annotation<SaturationAnnotation>::newLeaf({0, 0});
      // const auto [from, to] = tracedValue;
      // if (from == to) {
      //   return Annotation<SaturationAnnotation>::newLeaf({0, 0});
      // }
      // // determine minimal length for witness
      // auto length = 1;
      // const auto underlying =
      // std::get<RelationValue>(annotation->getLeft()->getValue().value()); auto boundedClosure =
      // underlying; while (!boundedClosure.contains(tracedValue)) {
      //   RelationValue newBoundedClosure;
      //   for (const auto [lFrom, lTo] : boundedClosure) {
      //     for (const auto [rFrom, rTo] : underlying) {
      //       if (lTo == rFrom) {
      //         newBoundedClosure.insert({lFrom, rTo});
      //       }
      //     }
      //   }
      //   boundedClosure = std::move(newBoundedClosure);
      //   length++;
      // }
    }
    case RelationOperation::baseRelation: {
      const auto [_, sMap] = std::get<SatRelationValue>(annotation->getValue().value());
      return Annotation<SaturationAnnotation>::newLeaf(sMap.at(tracedValue));
    }
    case RelationOperation::idRelation:
    case RelationOperation::fullRelation:
      return Annotation<SaturationAnnotation>::none();
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    case RelationOperation::emptyRelation:
    default:
      throw std::logic_error("unreachable");
  }
}
CanonicalAnnotation<SaturationAnnotation> makeSaturationAnnotationHelper(
    const AnnotatedSet<SatExprValue> &annotatedSet, const Event &tracedValue) {
  const auto &[set, annotation] = annotatedSet;

  switch (set->operation) {
    case SetOperation::image: {
      const auto [left, lsMap] = std::get<SatSetValue>(annotation->getLeft()->getValue().value());
      const auto [right, rsMap] =
          std::get<SatRelationValue>(annotation->getRight()->getValue().value());
      for (const auto [from, to] : right) {
        if (to == tracedValue && left.contains(from)) {
          const auto newLeft =
              makeSaturationAnnotationHelper(Annotated::getLeft(annotatedSet), from);
          const auto newRight =
              makeSaturationAnnotationHelper(Annotated::getRightRelation(annotatedSet), {from, to});
          assert(Annotated::validate({set->leftOperand, newLeft}));
          assert(Annotated::validate({set->relation, newRight}));
          return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
        }
      }
    }
    case SetOperation::domain: {
      const auto [left, lsMap] = std::get<SatSetValue>(annotation->getLeft()->getValue().value());
      const auto [right, rsMap] =
          std::get<SatRelationValue>(annotation->getRight()->getValue().value());
      for (const auto [from, to] : right) {
        if (from == tracedValue && left.contains(to)) {
          const auto newLeft = makeSaturationAnnotationHelper(Annotated::getLeft(annotatedSet), to);
          const auto newRight =
              makeSaturationAnnotationHelper(Annotated::getRightRelation(annotatedSet), {from, to});
          assert(Annotated::validate({set->leftOperand, newLeft}));
          assert(Annotated::validate({set->relation, newRight}));
          return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
        }
      }
    }
    case SetOperation::baseSet: {
      const auto [_, sMap] = std::get<SatSetValue>(annotation->getLeft()->getValue().value());
      return Annotation<SaturationAnnotation>::newLeaf(sMap.at(tracedValue));
    }
    case SetOperation::fullSet:
    case SetOperation::event:
      return Annotation<SaturationAnnotation>::none();
    case SetOperation::setIntersection: {
      const auto newLeft =
          makeSaturationAnnotationHelper(Annotated::getLeft(annotatedSet), tracedValue);
      const auto newRight =
          makeSaturationAnnotationHelper(Annotated::getRightSet(annotatedSet), tracedValue);
      assert(Annotated::validate({set->leftOperand, newLeft}));
      assert(Annotated::validate({set->rightOperand, newRight}));
      return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
    }
    case SetOperation::setUnion: {
      const auto [left, lsMap] = std::get<SatSetValue>(annotation->getLeft()->getValue().value());
      const auto [right, rsMap] = std::get<SatSetValue>(annotation->getRight()->getValue().value());
      if (left.contains(tracedValue)) {
        const auto newLeft =
            makeSaturationAnnotationHelper(Annotated::getLeft(annotatedSet), tracedValue);
        const auto newRight = Annotated::makeWithValue(set->rightOperand, {0, 0});
        return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
      }
      if (right.contains(tracedValue)) {
        const auto newLeft = Annotated::makeWithValue(set->leftOperand, {0, 0});
        const auto newRight =
            makeSaturationAnnotationHelper(Annotated::getRightSet(annotatedSet), tracedValue);
        return Annotation<SaturationAnnotation>::meetAnnotation(newLeft, newRight);
      }
      throw std::logic_error("unreachable");
    }
    case SetOperation::emptySet:
    default:
      throw std::logic_error("unreachable");
  }
}

CanonicalAnnotation<SaturationAnnotation> makeSaturationAnnotation(
    const AnnotatedSet<SatExprValue> &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  const auto [value, sMap] = std::get<SatSetValue>(annotation->getValue().value());
  if (value.empty()) {
    return Annotation<SaturationAnnotation>::newLeaf({0, 0});
  }
  // The genearate Saturation annotation reflects the saturations needed for some witness in the
  // set. Currently we chosse some arbitrary witness (which may be improved)
  // TODO: choose a minimal witness: minimal (as few saturations as possible)
  // REMARK: We need the exact minimal depth of saturations needed per base relation
  // to exclude the counterexample. Otherwise we could saturate (not enough) and weaken the newly
  // saturated stuff, leading to the same state from which we began saturating
  // thus evalutating expression bottom-up we should save the minimal number of saturations needed
  // then top-down we can assign exact saturation bounds
  const auto someEvent = *value.begin();
  const auto saturationAnnotation = makeSaturationAnnotationHelper(annotatedSet, someEvent);
  assert(Annotated::validate({set, saturationAnnotation}));
  return saturationAnnotation;
}

// return std::nullopt if not spurious (== evaluated negated set non emptiness literal is empty)
// return Literal with annotated saturations otherwise
std::optional<Literal> checkAndMarkSaturation(const Model &model, const Literal &negatedLiteral) {
  assert(negatedLiteral.negated);

  switch (negatedLiteral.operation) {
    case PredicateOperation::setNonEmptiness: {
      const auto annotation = model.evaluateExpression(negatedLiteral.set);
      const auto [result, sMap] = std::get<SatSetValue>(annotation->getValue().value());
      if (result.empty()) {
        return std::nullopt;
      }

      // assert(Annotated::validate(AnnotatedSet(negatedLiteral.set, annotation)));
      const auto saturationAnnotation = makeSaturationAnnotation({negatedLiteral.set, annotation});
      auto litCopy = negatedLiteral;
      litCopy.annotation = saturationAnnotation;
      assert(Annotated::validate(litCopy.annotatedSet()));
      return litCopy;
    }
    case PredicateOperation::set:
    case PredicateOperation::edge: {
      const auto baseRelation = negatedLiteral.identifier.value();
      const auto from = negatedLiteral.leftEvent->label.value();
      const auto to = negatedLiteral.rightEvent->label.value();

      if (model.containsEdge(Edge(baseRelation, {from, to}))) {
        auto litCopy = negatedLiteral;
        litCopy.annotation = Annotation<SaturationAnnotation>::newLeaf({1, 1});
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
  assert(!openLeaf->closed);
  assert(openLeaf->getChildren().empty());

  while (isReachableFromRoots(openLeaf)) {  // as long as a path from openLeaf to some root exists
    // get model & saturated model (wrt to root namespace)
    // IMPORTANT: we must get the model inside the while loop, because each loop iteration may have
    // a different path to some root (which corrsponds to different models)
    const auto model = getModel(openLeaf);
    auto saturatedModel = model;
    saturateModel(saturatedModel);
#if DEBUG
    model.exportModel("debug-saturationLazy.model");
    saturatedModel.exportModel("debug-saturationLazy.saturatedModel");
#endif

    exportDebug("debug-saturationLazy.regularTableau");
    auto pathNeedsSaturation = false;

    // follow some path to root
    auto curNode = openLeaf;
    while (curNode != nullptr) {
      assert(validateReachabilityTree());
      pathNeedsSaturation |= saturateNodeLazy(curNode, model, saturatedModel);
      curNode = curNode->reachabilityTreeParent;
    }
    if (!pathNeedsSaturation) {
      return false;
    }
  }
  Stats::counter("#saturations (lazy)")++;
  return true;
}
// returns if node could has been saturated
bool RegularTableau::saturateNodeLazy(RegularNode *node, const Model &model,
                                      const Model &saturatedModel) {
  // evaluate each node on model & saturatedModel
  // evaluate every negated literal
  // Inconsistent iff for some literal the results from both models differ
  const auto &nodeRenaming = getRootRenaming(node);
  for (const auto &cubeLiteral : node->cube | std::views::filter(&Literal::negated)) {
    // rename cubeLiteral to root + saturationRenaming
    auto renamedLiteral = cubeLiteral;
    renamedLiteral.rename(nodeRenaming);
    const auto result = checkAndMarkSaturation(model, renamedLiteral);
    const auto resultSaturated = checkAndMarkSaturation(saturatedModel, renamedLiteral);

    if (!result && resultSaturated) {
      // REMARK: saturation leads to different evaluation
      // We want to do the following: anaylze the counterexample and determine exactly
      // which saturations we needed (currently only saturation depth)
      // Then, apply the same sequence of saturations in the proof to exclude the spurious
      // counterexample
      // We do this by decorating the base relations and base sets in the respective expression
      // in the proof. (currently this is not as precise as it could be)
      // decorating the atomic expressions is already done insde checkAndMarkSaturation
      // here we just have to modify the proof accordingly
      const auto &annotatedLiteral = resultSaturated.value();

      // remove old children (if there are any)
      removeChildren(node);
      exportDebug("debug-saturateNodeLazy.regularTableau");
      // IMPORTANT: invariant in validation of tableau is temporally violated
      // after removing all children we may have an open leaf that is not on unreduced nodes

      // literal is only contradicting if we saturate
      // mark cubeLiterals to saturate it during normalization
      auto cubeCopy = node->getCube();
      for (auto &copyLiteral : cubeCopy) {
        if (copyLiteral == cubeLiteral) {
          // IMPORTANT: here the annotation should be added:
          // copyLiteral could have some SatAnnotation from the previous iteration of
          // saturationLazy(probably from another path to another openLeaf)
          // If we do not add we get the counterexample to this other openLeaf again
          // So we want to add all SatAnnotation for all branches to be sure that we saturate enough
          // to finish all branches
          copyLiteral.annotation =
              Annotated::sum(copyLiteral.annotation, annotatedLiteral.annotation);
          assert(Annotated::validate(copyLiteral.annotatedSet()));
        }
      }

      // IMPORTANT performance optimization
      // compare with solve method
      if (cubeHasPositiveAtomic(cubeCopy)) {
        std::erase_if(cubeCopy, std::mem_fn(&Literal::isPositiveAtomic));
        removeUselessLiterals(cubeCopy);
      }
      Tableau tableau(cubeCopy);

      const auto &dnf = tableau.computeDnf();  // dont weakening, do saturate
      tableau.exportDebug("debug-saturateNodeLazy.fixedDnfCalculation");

      // IMPORTANT: invariant in validation of tableau is valid again (after next if/else)
      if (dnf.empty()) {
        node->closed = true;
      } else {
        newChildren(node, dnf);
      }

      return true;
    }
  }
  return false;
}

auto RegularTableau::getPathEvents(const RegularNode *openLeaf) const {
  // union-find data structure
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
  model.exportModel("debug-isSpurious.model");

  // check if any negated literals of initial cube evaluates to false
  for (const auto &negatedLiteral : initialCube | std::views::filter(&Literal::negated)) {
    if (!model.evaluateExpression(negatedLiteral)) {
      return true;
    }
  }
  // no negated Literal violated -> not spurious
  spdlog::info("[Solver] Counterexample:");  // TODO: make clickable link to counterexample
  getModel(openLeaf).exportModel("counterexample");
  model.exportModel("counterexample-saturated");
  exportCounterexamplePath(currentNode);
  return false;
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
