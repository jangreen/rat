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

bool validate(const AnnotatedSet<ExprValue> &annotatedSet);
bool validate(const AnnotatedRelation<ExprValue> &annotatedRelation) {
  const auto &[relation, annotation] = annotatedRelation;

  assert(std::holds_alternative<RelationValue>(annotation->getValue().value()));

  switch (relation->operation) {
    case RelationOperation::relationIntersection:
    case RelationOperation::relationUnion:
    case RelationOperation::composition:
      assert(validate(Annotated::getLeftRelation(annotatedRelation)));
      assert(validate(Annotated::getRight(annotatedRelation)));
      return true;
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      assert(validate(Annotated::getLeftRelation(annotatedRelation)));
      return true;
    case RelationOperation::setIdentity:
      assert(validate(Annotated::getLeftSet(annotatedRelation)));
      return true;
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemneted");
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::fullRelation:
    case RelationOperation::emptyRelation:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
}
bool validate(const AnnotatedSet<ExprValue> &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  assert(std::holds_alternative<SetValue>(annotation->getValue().value()));

  switch (set->operation) {
    case SetOperation::image:
    case SetOperation::domain:
      assert(validate(Annotated::getLeft(annotatedSet)));
      assert(validate(Annotated::getRightRelation(annotatedSet)));
      return true;
    case SetOperation::setIntersection:
    case SetOperation::setUnion:
      assert(validate(Annotated::getLeft(annotatedSet)));
      assert(validate(Annotated::getRightSet(annotatedSet)));
      return true;
    case SetOperation::fullSet:
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
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

    currentNode = unreducedNodes.top();
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
    exportDebug("debug");
    if (saturationLazy(currentNode)) {
      assert(validate());
      continue;
    }

    exportProof("error");
    auto model = getModel(currentNode);
    saturateModel(model);
    exportModel("error-model", model);
    throw std::logic_error("unreachable: no rule applicable");
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
        if (parent->children.empty() && parent->epsilonChildren.empty()) {
          // is leaf // TODO: use function
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

template <>
struct std::hash<ExprValue> {
  std::size_t operator()(const ExprValue &value) const noexcept {
    if (std::holds_alternative<SetValue>(value)) {
      const auto events = std::get<SetValue>(value);
      size_t seed = 0;
      for (const auto event : events) {
        boost::hash_combine(seed, std::hash<int>()(event));
      }
      return seed;
    }
    const auto eventPairs = std::get<RelationValue>(value);
    size_t seed = 0;
    for (const auto pair : eventPairs) {
      boost::hash_combine(seed, std::hash<EventPair>()(pair));
    }
    return seed;
  }
};
CanonicalAnnotation<ExprValue> annotateLiteral(const Cube &model, CanonicalSet set);
CanonicalAnnotation<ExprValue> annotateLiteral(const Cube &model,
                                               const CanonicalRelation relation) {
  const auto [representatives, equivalenceClasses] = getEquivalenceClasses(model);

  switch (relation->operation) {
    case RelationOperation::relationIntersection: {
      const auto left = annotateLiteral(model, relation->leftOperand);
      const auto right = annotateLiteral(model, relation->rightOperand);
      const auto leftValue = std::get<RelationValue>(left->getValue().value());
      const auto rightValue = std::get<RelationValue>(right->getValue().value());

      RelationValue intersect;
      std::ranges::set_intersection(leftValue, rightValue,
                                    std::inserter(intersect, intersect.begin()));
      assert(validate({relation, Annotation<ExprValue>::newAnnotation(intersect, left, right)}));
      return Annotation<ExprValue>::newAnnotation(intersect, left, right);
    }
    case RelationOperation::composition: {
      auto left = annotateLiteral(model, relation->leftOperand);
      const auto right = annotateLiteral(model, relation->rightOperand);
      const auto leftValue = std::get<RelationValue>(left->getValue().value());
      const auto rightValue = std::get<RelationValue>(right->getValue().value());

      RelationValue composition;
      for (const auto &l : leftValue) {
        for (const auto &r : rightValue) {
          if (l.second == r.first) {
            composition.insert({l.first, r.second});
          }
        }
      }
      assert(validate({relation, Annotation<ExprValue>::newAnnotation(composition, left, right)}));
      return Annotation<ExprValue>::newAnnotation(composition, left, right);
    }
    case RelationOperation::relationUnion: {
      const auto left = annotateLiteral(model, relation->leftOperand);
      const auto right = annotateLiteral(model, relation->rightOperand);
      const auto leftValue = std::get<RelationValue>(left->getValue().value());
      const auto rightValue = std::get<RelationValue>(right->getValue().value());

      RelationValue relunion;
      std::ranges::set_union(leftValue, rightValue, std::inserter(relunion, relunion.begin()));
      assert(validate({relation, Annotation<ExprValue>::newAnnotation(relunion, left, right)}));
      return Annotation<ExprValue>::newAnnotation(relunion, left, right);
    }
    case RelationOperation::converse: {
      auto left = annotateLiteral(model, relation->leftOperand);
      auto leftValue = std::get<RelationValue>(left->getValue().value());

      RelationValue converse;
      for (auto &[from, to] : leftValue) {
        converse.insert({to, from});
      }
      assert(validate({relation, Annotation<ExprValue>::newAnnotation(converse, left)}));
      return Annotation<ExprValue>::newAnnotation(converse, left);
    }
    case RelationOperation::transitiveClosure: {
      auto left = annotateLiteral(model, relation->leftOperand);
      const auto underlying = std::get<RelationValue>(left->getValue().value());

      RelationValue transitiveClosure;
      // insert id
      const auto events = gatherActiveEvents(model);
      for (const auto event : events) {
        transitiveClosure.insert({event, event});
      }
      // iterate underlying
      while (true) {
        RelationValue newPairs;
        for (const auto [clFrom, clTo] : transitiveClosure) {
          for (const auto [rFrom, rTo] : underlying) {
            const EventPair composition = {clFrom, rTo};
            if (clTo == rFrom && !transitiveClosure.contains(composition)) {
              newPairs.insert(composition);
            }
          }
        }
        if (newPairs.empty()) {
          break;
        }
        transitiveClosure.insert(std::make_move_iterator(newPairs.begin()),
                                 std::make_move_iterator(newPairs.end()));
      }
      assert(validate({relation, Annotation<ExprValue>::newAnnotation(transitiveClosure, left)}));
      return Annotation<ExprValue>::newAnnotation(transitiveClosure, left);
    }
    case RelationOperation::baseRelation: {
      RelationValue value;
      for (const auto &edge : model | std::views::filter(&Literal::isPositiveEdgePredicate)) {
        const int left = representatives.at(edge.leftEvent->label.value());
        const int right = representatives.at(edge.rightEvent->label.value());
        auto baseRelation = edge.identifier.value();

        if (baseRelation == relation->identifier.value()) {
          value.insert({left, right});
        }
      }
      assert(validate({relation, Annotation<ExprValue>::newLeaf(value)}));
      return Annotation<ExprValue>::newLeaf(value);
    }
    case RelationOperation::idRelation: {
      RelationValue value;
      const auto events = gatherActiveEvents(model);
      for (const auto event : events) {
        const auto repEvent = representatives.at(event);
        value.insert({repEvent, repEvent});
      }
      assert(validate({relation, Annotation<ExprValue>::newLeaf(value)}));
      return Annotation<ExprValue>::newLeaf(value);
    }
    case RelationOperation::emptyRelation:
      assert(validate({relation, Annotation<ExprValue>::newLeaf(RelationValue{})}));
      return Annotation<ExprValue>::newLeaf(RelationValue{});
    case RelationOperation::fullRelation: {
      RelationValue value;
      const auto events = gatherActiveEvents(model);
      for (const auto e1 : events) {
        const auto repE1 = representatives.at(e1);
        for (const auto e2 : events) {
          const auto repE2 = representatives.at(e2);
          value.insert({repE1, repE2});
        }
      }
      assert(validate({relation, Annotation<ExprValue>::newLeaf(value)}));
      return Annotation<ExprValue>::newLeaf(value);
    }
    case RelationOperation::setIdentity: {
      auto left = annotateLiteral(model, relation->set);
      auto leftValue = std::get<SetValue>(left->getValue().value());

      RelationValue value;
      for (const auto event : leftValue) {
        value.insert({event, event});
      }
      assert(validate({relation, Annotation<ExprValue>::newAnnotation(value, left)}));
      return Annotation<ExprValue>::newAnnotation(value, left);
    }
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}
CanonicalAnnotation<ExprValue> annotateLiteral(const Cube &model, const CanonicalSet set) {
  const auto [representatives, equivalenceClasses] = getEquivalenceClasses(model);

  switch (set->operation) {
    case SetOperation::event: {
      const auto value = SetValue{representatives.at(set->label.value())};
      assert(validate({set, Annotation<ExprValue>::newLeaf(value)}));
      return Annotation<ExprValue>::newLeaf(value);
    }
    case SetOperation::image: {
      const auto left = annotateLiteral(model, set->leftOperand);
      const auto right = annotateLiteral(model, set->relation);
      const auto leftValue = std::get<SetValue>(left->getValue().value());
      const auto rightValue = std::get<RelationValue>(right->getValue().value());

      SetValue image;
      for (const auto &r : rightValue) {
        if (leftValue.contains(r.first)) {
          image.insert({r.second});
        }
      }
      assert(validate({set, Annotation<ExprValue>::newAnnotation(image, left, right)}));
      return Annotation<ExprValue>::newAnnotation(image, left, right);
    }
    case SetOperation::domain: {
      const auto left = annotateLiteral(model, set->leftOperand);
      const auto right = annotateLiteral(model, set->relation);
      const auto leftValue = std::get<SetValue>(left->getValue().value());
      const auto rightValue = std::get<RelationValue>(right->getValue().value());

      SetValue domain;
      for (const auto &r : rightValue) {
        if (leftValue.contains(r.second)) {
          domain.insert({r.first});
        }
      }
      assert(validate({set, Annotation<ExprValue>::newAnnotation(domain, left, right)}));
      return Annotation<ExprValue>::newAnnotation(domain, left, right);
    }
    case SetOperation::baseSet: {
      SetValue value;
      for (const auto &setMembership :
           model | std::views::filter(&Literal::isPositiveSetPredicate)) {
        const auto left = representatives.at(setMembership.leftEvent->label.value());
        auto baseSet = setMembership.identifier.value();

        if (baseSet == set->identifier.value()) {
          value.insert(left);
        }
      }
      assert(validate({set, Annotation<ExprValue>::newLeaf(value)}));
      return Annotation<ExprValue>::newLeaf(value);
    }
    case SetOperation::emptySet:
      assert(validate({set, Annotation<ExprValue>::newLeaf(SetValue{})}));
      return Annotation<ExprValue>::newLeaf(SetValue{});
    case SetOperation::fullSet: {
      SetValue value;
      const auto events = gatherActiveEvents(model);
      for (const auto event : events) {
        value.insert(representatives.at(event));
      }
      assert(validate({set, Annotation<ExprValue>::newLeaf(value)}));
      return Annotation<ExprValue>::newLeaf(value);
    }
    case SetOperation::setIntersection: {
      const auto left = annotateLiteral(model, set->leftOperand);
      const auto right = annotateLiteral(model, set->rightOperand);
      const auto leftValue = std::get<SetValue>(left->getValue().value());
      const auto rightValue = std::get<SetValue>(right->getValue().value());

      SetValue intersect;
      std::ranges::set_intersection(leftValue, rightValue,
                                    std::inserter(intersect, intersect.begin()));
      assert(validate({set, Annotation<ExprValue>::newAnnotation(intersect, left, right)}));
      return Annotation<ExprValue>::newAnnotation(intersect, left, right);
    }
    case SetOperation::setUnion: {
      const auto left = annotateLiteral(model, set->leftOperand);
      const auto right = annotateLiteral(model, set->rightOperand);
      const auto leftValue = std::get<SetValue>(left->getValue().value());
      const auto rightValue = std::get<SetValue>(right->getValue().value());

      SetValue setunion;
      std::ranges::set_union(leftValue, rightValue, std::inserter(setunion, setunion.begin()));
      assert(validate({set, Annotation<ExprValue>::newAnnotation(setunion, left, right)}));
      return Annotation<ExprValue>::newAnnotation(setunion, left, right);
    }
    default:
      throw std::logic_error("unreachable");
  }
}
// TODO: remove
// bool annotateLiteral(const Cube &model, const Literal &literal) {
//   switch (literal.operation) {
//     case PredicateOperation::constant:
//       return !literal.negated;
//     case PredicateOperation::edge:
//     case PredicateOperation::set:
//       if (literal.negated) {
//         return std::ranges::none_of(
//             model, [&](const Literal &modelLit) { return modelLit.isNegatedOf(literal); });
//       }
//       return contains(model, literal);
//     case PredicateOperation::equality:
//       throw std::logic_error("not implemented");
//     case PredicateOperation::setNonEmptiness:
//       return annotateLiteral(model, literal.set).empty() == literal.negated;
//     default:
//       throw std::logic_error("unreachable");
//   }
// }
CanonicalAnnotation<SaturationAnnotation> makeSaturationAnnotationHelper(
    const AnnotatedSet<ExprValue> &annotatedSet, const Event &tracedValue);
CanonicalAnnotation<SaturationAnnotation> makeSaturationAnnotationHelper(
    const AnnotatedRelation<ExprValue> &annotatedRelation, const EventPair &tracedValue) {
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
      const auto left = std::get<RelationValue>(annotation->getLeft()->getValue().value());
      const auto right = std::get<RelationValue>(annotation->getRight()->getValue().value());
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
      const auto left = std::get<RelationValue>(annotation->getLeft()->getValue().value());
      const auto right = std::get<RelationValue>(annotation->getRight()->getValue().value());
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
    case RelationOperation::baseRelation:
      // TODO: assert that base relation contains value
      return Annotation<SaturationAnnotation>::newLeaf({1, 1});
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
    const AnnotatedSet<ExprValue> &annotatedSet, const Event &tracedValue) {
  const auto &[set, annotation] = annotatedSet;

  switch (set->operation) {
    case SetOperation::image: {
      const auto left = std::get<SetValue>(annotation->getLeft()->getValue().value());
      const auto right = std::get<RelationValue>(annotation->getRight()->getValue().value());
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
      const auto left = std::get<SetValue>(annotation->getLeft()->getValue().value());
      const auto right = std::get<RelationValue>(annotation->getRight()->getValue().value());
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
      return Annotation<SaturationAnnotation>::newLeaf({1, 1});
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
      const auto left = std::get<SetValue>(annotation->getLeft()->getValue().value());
      const auto right = std::get<SetValue>(annotation->getRight()->getValue().value());
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
    const AnnotatedSet<ExprValue> &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;
  const auto value = std::get<SetValue>(annotation->getValue().value());
  if (value.empty()) {
    return Annotation<SaturationAnnotation>::newLeaf({0, 0});
  }
  const auto someEvent = *value.begin();
  const auto saturationAnnotation = makeSaturationAnnotationHelper(annotatedSet, someEvent);
  assert(Annotated::validate({set, saturationAnnotation}));
  return saturationAnnotation;
}

// TODO: not used but reduces models size
// // IMPORTANT annotateLiteral assume that model has no equalities anymore
// auto modelCopy = model;
// removeEqualitiesFromModel(modelCopy);
// void removeEqualitiesFromModel(Cube &model) {
//   print(model);
//   const auto [representatives, equivalenceClasses] = getEquivalenceClasses(model);
//   auto renaming = Renaming::empty();
//   for (const auto [event, representive] : representatives) {
//     renaming = renaming.totalCompose(Renaming::simple(event, representive));
//   }
//
//   for (auto &literal : model) {
//     literal.rename(renaming);
//   }
//
//   std::erase_if(model, std::mem_fn(&Literal::isPositiveEqualityPredicate));
//   removeDuplicates(model);
//
//   print(model);
// }

// return std::nullopt if not spurious (== evaluated negated set non emptiness literal is empty)
// return Literal with annotated saturations otherwise
std::optional<Literal> checkAndMarkSaturation(const Cube &model, const Literal &negatedLiteral) {
  assert(negatedLiteral.negated);

  switch (negatedLiteral.operation) {
    case PredicateOperation::setNonEmptiness: {
      const auto annotation = annotateLiteral(model, negatedLiteral.set);
      const auto result = std::get<SetValue>(annotation->getValue().value());
      if (result.empty()) {
        return std::nullopt;
      }

      assert(validate({negatedLiteral.set, annotation}));
      const auto saturationAnnotation = makeSaturationAnnotation({negatedLiteral.set, annotation});
      auto litCopy = negatedLiteral;
      litCopy.annotation = saturationAnnotation;
      assert(Annotated::validate(litCopy.annotatedSet()));
      return litCopy;
    }
    case PredicateOperation::set:
    case PredicateOperation::edge: {
      if (std::ranges::any_of(model, [&](const Literal &modelLit) {
            return modelLit.isNegatedOf(negatedLiteral);
          })) {
        auto litCopy = negatedLiteral;
        litCopy.annotation = Annotation<SaturationAnnotation>::newLeaf({1, 1});
        return litCopy;
      }
      return std::nullopt;
    }
    case PredicateOperation::equality: {
      const auto e1 = negatedLiteral.leftEvent->label.value();
      const auto e2 = negatedLiteral.rightEvent->label.value();
      const auto [representatives, equivalenceClasses] = getEquivalenceClasses(model);
      if (representatives.at(e1) == representatives.at(e2)) {
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
    while (curNode != nullptr) {
      assert(validateReachabilityTree());

      // check
      const auto &nodeRenaming = getRootRenaming(curNode);
      for (const auto &cubeLiteral : curNode->cube | std::views::filter(&Literal::negated)) {
        // rename cubeLiteral to root + saturationRenaming
        auto renamedLiteral = cubeLiteral;
        renamedLiteral.rename(nodeRenaming);
        Cube checkedCube = model;
        checkedCube.push_back(renamedLiteral);

        const auto result = checkAndMarkSaturation(model, renamedLiteral);

        renamedLiteral.rename(saturationRenaming);
        Cube checkedCubeSaturated = saturatedNodeModel;
        checkedCubeSaturated.push_back(renamedLiteral);

        const auto resultSaturated = checkAndMarkSaturation(saturatedNodeModel, renamedLiteral);

        if (!result && resultSaturated) {
          const auto &annotatedLiteral = resultSaturated.value();
          // std::cout << Annotated::toString<false>(annotatedLiteral.annotatedSet()) << "\n";
          //  std::cout << Annotated::toString<true>(annotatedLiteral.annotatedSet()) <<
          //  std::endl;

          // old check:
          // Tableau finiteTableau(checkedCube);
          // Tableau finiteTableauSaturated(checkedCubeSaturated);
          // const auto spurious = finiteTableau.computeDnf().empty();
          // const auto saturatedSpurious = finiteTableauSaturated.computeDnf().empty();
          //
          // if (!spurious && saturatedSpurious) {

          // remove old children (if there are any)
          removeChildren(curNode);
          // IMPORTANT: invariant in validation of tableau is temporally violated
          // after removing all children we may have an open leaf that is not on unreduced nodes

          // literal is only contradicting if we saturate
          // mark cubeLiterals to saturate it during normalization
          auto cubeCopy = curNode->getCube();
          for (auto &copyLiteral : cubeCopy) {
            if (copyLiteral == cubeLiteral) {
              copyLiteral.annotation = annotatedLiteral.annotation;
              assert(Annotated::validate(copyLiteral.annotatedSet()));
            }
          }
          Tableau tableau(cubeCopy);
          tableau.exportDebug("debug");

          const auto &dnf =
              tableau.computeDnf();  // TODO: remove :false  // dont weakening, do saturate
          tableau.exportDebug("debug");

          if (dnf.empty()) {
            curNode->closed = true;
          } else {
            newChildren(curNode, dnf);
          }

          pathNeedsSaturation = true;
        }
      }
      curNode = curNode->reachabilityTreeParent;
    }
    if (!pathNeedsSaturation) {
      return false;
    }
    exportDebug("debug");
  }
  Stats::counter("#saturations (lazy)")++;
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
    removeDuplicates(model);  // TODO:

    if (cur->reachabilityTreeParent != nullptr) {
      auto renaming = cur->reachabilityTreeParent->getLabelForChild(cur).inverted();
      renameCube(renaming, model);
    }

    cur = cur->reachabilityTreeParent;
  }
  assert(validateCube(model));
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
          checkedCube.emplace_back(AnnotatedSet<SaturationAnnotation>{
              fromR_and_to, Annotated::makeWithValue(fromR_and_to, {0, 0})});
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
        checkedCube.emplace_back(AnnotatedSet<SaturationAnnotation>{
            fromR_and_to, Annotated::makeWithValue(fromR_and_to, {0, 0})});
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
      checkedCube.emplace_back(
          AnnotatedSet<SaturationAnnotation>{fS, Annotated::makeWithValue(fS, {0, 0})});
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
    spdlog::info("[Solver] Counterexample:");
    print(model);
    finiteTableau.exportProof("spurious-check");
    exportModel("counterexample", getModel(openLeaf));
    exportModel("counterexample-saturated", model);
    exportCounterexamplePath(currentNode);
  }
  return spurious;
}

void RegularTableau::exportModel(const std::string &filename, const Cube &model) {
  assert(validateCube(model));

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
  for (const auto event : representatives | std::views::values) {
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
  std::unordered_set<Literal> insertedEdges;
  auto edges = model | std::views::filter(&Literal::isPositiveEdgePredicate);
  for (const auto &edge : edges) {
    const int left = edge.leftEvent->label.value();
    const int right = edge.rightEvent->label.value();
    auto baseRelation = edge.identifier.value();

    const auto insertedEdge = Literal(Set::newEvent(representatives.at(left)),
                                      Set::newEvent(representatives.at(right)), baseRelation);
    const auto [_, inserted] = insertedEdges.insert(insertedEdge);
    if (!inserted) {
      continue;
    }

    counterexamleModel << "N" << representatives.at(left) << " -> N" << representatives.at(right);
    counterexamleModel << "[label = \"" << baseRelation << "\"];\n";
  }

  counterexamleModel << "}" << '\n';
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
    output << "N" << node << " [color=blue, fontcolor=blue]";
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
