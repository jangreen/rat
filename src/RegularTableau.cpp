#include "RegularTableau.h"

#include <algorithm>
#include <boost/pending/disjoint_sets.hpp>
#include <iostream>
#include <map>
#include <tuple>
#include <utility>

std::vector<Assumption> RegularTableau::emptinessAssumptions;
std::vector<Assumption> RegularTableau::idAssumptions;
std::map<std::string, Assumption> RegularTableau::baseAssumptions;

RegularTableau::RegularTableau(std::initializer_list<Formula> initalFormulas)
    : RegularTableau(std::vector(initalFormulas)) {}
RegularTableau::RegularTableau(FormulaSet initalFormulas) {
  Tableau t{initalFormulas};
  expandNode(nullptr, &t);
}

// assumptions:
// clause is in normal form
// parent == nullptr -> rootNode
RegularTableau::Node *RegularTableau::addNode(FormulaSet clause) {
  // create node, add to nodes (returns pointer to existing node if already exists)
  auto newNode = std::make_unique<Node>(clause);
  auto insertion = nodes.insert(std::move(newNode));
  if (insertion.second) {
    // new node has been added added (no isomorphic node existed)
    unreducedNodes.push(insertion.first->get());
  }
  return insertion.first->get();
}

// TODO: use these functions ... access modifier!

void RegularTableau::updateRootParents(Node *node) {
  if (std::find(rootNodes.begin(), rootNodes.end(), node) == rootNodes.end() &&
      node->rootParents.empty()) {
    for (auto childNode : node->childNodes) {
      auto foundNode =
          std::find(childNode->rootParents.begin(), childNode->rootParents.end(), node);
      if (foundNode != childNode->rootParents.end()) {
        childNode->rootParents.erase(foundNode);
        updateRootParents(childNode);
      }
    }
  }
}

// TODO: what are root parents needed for? -> description
void RegularTableau::removeEdge(Node *parent, Node *child) {
  auto childIt = std::find(parent->childNodes.begin(), parent->childNodes.end(), child);
  if (childIt != parent->childNodes.end()) {
    parent->childNodes.erase(childIt);
  }
  child->parentNodes.erase(parent);
  auto rootParentIt = std::find(child->rootParents.begin(), child->rootParents.end(), parent);
  if (rootParentIt != child->rootParents.end()) {
    child->rootParents.erase(rootParentIt);
  }
  updateRootParents(child);
}

// return fixed node as set, otherwise nullopt if consistent
std::optional<FormulaSet> RegularTableau::checkInconsistency(Node *parent,
                                                             const FormulaSet &newFormulas) {
  if (parent == nullptr) {
    return std::nullopt;
  }
  // 1) gather parent active labels
  std::vector<int> parentActiveLabels;
  for (const auto &formula : parent->formulas) {
    if (!formula.literal->negated) {
      auto formulaLabels = formula.literal->predicate->labels();
      parentActiveLabels.insert(std::end(parentActiveLabels), std::begin(formulaLabels),
                                std::end(formulaLabels));
    }
  }
  // 2) filter newFormulas for parent
  FormulaSet filteredNewFormulas;
  for (const auto &formula : newFormulas) {
    auto formulaLabels = formula.literal->predicate->labels();
    if (isSubset(formulaLabels, parentActiveLabels)) {
      filteredNewFormulas.push_back(formula);
    }
  }
  // 3) check if new derived literal exists
  if (!isSubset(filteredNewFormulas, parent->formulas)) {
    for (const auto &formula : parent->formulas) {
      if (std::find(filteredNewFormulas.begin(), filteredNewFormulas.end(), formula) ==
          filteredNewFormulas.end()) {
        filteredNewFormulas.push_back(formula);
      }
    }
    // at this point filteredNewFormulas is the alternative child
    return filteredNewFormulas;
  }
  return std::nullopt;
}

void RegularTableau::addEdge(Node *parent, Node *child, EdgeLabel label) {
  if (parent != nullptr) {
    // not root node
    parent->childNodes.push_back(child);
    child->parentNodes[parent] = label;

    // set first parent node if not already set
    if (child->firstParentNode == nullptr) {
      child->firstParentNode = parent;
    }

    // update rootParents of child node
    if (std::find(rootNodes.begin(), rootNodes.end(), parent) != rootNodes.end() ||
        !parent->rootParents.empty()) {
      child->rootParents.push_back(parent);
    }

    if (label.size() == 0) {
      // epsilon edge is added
      // add shortcuts
      for (const auto &[grandparentNode, parentLabel] : parent->parentNodes) {
        addEdge(grandparentNode, child, parentLabel);

        // check recursively
        if (isInconsistent(grandparentNode, child)) {
          // remove edge
          removeEdge(grandparentNode, child);
          // alternativ child are already added in isInconsistent
          // TODO: make functions more clear:
          // checkInconsistency = getInconsistentLiterals
          // isInconsistent = fixInconsistencies
        }
      }
    }
  } else {
    rootNodes.push_back(child);
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
    exportProof("regular-debug");
    if (!checkAndExpandNode(currentNode)) {
      extractCounterexample(currentNode);
      std::cout << "[Solver] False." << std::endl;
      return false;
    }
  }
  std::cout << "[Solver] True." << std::endl;
  return true;
}

bool RegularTableau::checkAndExpandNode(Node *node) {
  Tableau tableau{node->formulas};
  auto atomicFormula = tableau.applyRuleA();  // TODO: make return value boolean

  if (atomicFormula) {
    expandNode(node, &tableau);
    return true;
  }
  return false;
}
// assumptions:
// node has only normal terms
void RegularTableau::expandNode(Node *node, Tableau *tableau) {
  // node is expandable
  auto dnf = tableau->dnf();
  saturate(dnf);

  // for each clause: calculate potential child
  for (const auto &clause : dnf) {
    FormulaSet droppedFormulas;
    EdgeLabel edgeLabel;
    auto newClause = purge(clause, droppedFormulas, edgeLabel);

    // check if intial inconsistency exists
    auto alternativeNode = checkInconsistency(node, droppedFormulas);
    if (alternativeNode) {
      // create new fixed Node
      Node *fixedNode = addNode(*alternativeNode);
      addEdge(node, fixedNode, {});
      return;
    }

    // add child
    // checks if renaming exists when adding and returns already existing node if possible
    Node *newNode = addNode(newClause);
    addEdge(node, newNode, edgeLabel);
  }

  if (dnf.empty() && node != nullptr) {
    node->closed = true;
  }
}

// assumption: input is dnf clause
// purges useless literals
FormulaSet RegularTableau::purge(const FormulaSet &clause, FormulaSet &dropped,
                                 FormulaSet &label) const {
  FormulaSet newClause;

  // remove edge predicates and put them in the edge label
  for (const auto &formula : clause) {
    if (formula.isEdgePredicate() || formula.isPositiveEqualityPredicate()) {
      label.push_back(formula);
    } else {
      newClause.push_back(formula);
    }
  }

  // filter none active labels
  // assume alredy normal
  // 1) gather all active labels
  std::vector<int> activeLabels;
  for (const auto &formula : newClause) {
    if (!formula.literal->negated) {
      auto formulaLabels = formula.literal->predicate->labels();
      activeLabels.insert(std::end(activeLabels), std::begin(formulaLabels),
                          std::end(formulaLabels));
    }
  }
  // calculate equivalence class
  std::map<int, int> minimalRepresentants;
  for (const auto &formula : label) {
    if (formula.isPositiveEqualityPredicate()) {
      auto predicate = *formula.literal->predicate;
      int i = *predicate.leftOperand->label;
      int j = *predicate.rightOperand->label;

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
  // 2) filtering: non active labels and non minimal labels
  FormulaSet copy = std::move(newClause);
  newClause = {};
  for (const auto &formula : copy) {
    auto formulaLabels = formula.literal->predicate->labels();

    // find intersection
    bool intersectionNonempty = false;
    for (const auto nonMin : nonMinimal) {
      if (std::find(formulaLabels.begin(), formulaLabels.end(), nonMin) != formulaLabels.end()) {
        // contains non Minimal label -> drop it and do not remember
        intersectionNonempty = true;
        break;
      }
    }
    if (intersectionNonempty) {
      continue;
    }

    if (isSubset(formulaLabels, activeLabels)) {
      newClause.push_back(formula);
    } else {
      // are minimal
      // check for immediate inconsistencies
      dropped.push_back(formula);
    }
  }
  return newClause;
}

bool RegularTableau::isInconsistent(Node *parent, Node *child) {
  EdgeLabel label = child->parentNodes[parent];
  if (label.size() == 0) {  // is already alternative node to its parent
                            // TODO: should not return but double check?
    return false;
  }

  // calculate converse request
  FormulaSet converseRequest = child->formulas;
  converseRequest.insert(std::end(converseRequest), std::begin(label), std::end(label));
  // converseRequest is now child node + label
  // try to reduce: all rules but positive modal edge rules
  Tableau calcConverseReq(converseRequest);
  calcConverseReq.solve();

  if (calcConverseReq.rootNode->isClosed()) {
    // TODO: test
    child->closed = true;
    return false;
  }

  GDNF dnf = calcConverseReq.rootNode->extractDNF();
  // each Disjunct must have some inconsistency to have an inconsistency
  // otherwise the proof for one clause without inconsistency would subsume the others (which have
  // more literals) if this is the case: add one epsilon edge for each clause
  for (const auto &clause : dnf) {
    if (!checkInconsistency(parent, clause)) {
      return false;
    }
  }

  for (const auto &clause : dnf) {
    auto alternativeNode = checkInconsistency(parent, clause);
    if (alternativeNode) {
      // create new fixed Node
      Node *fixedNode = addNode(*alternativeNode);
      addEdge(parent, fixedNode, {});
    }
  }
  return true;
}

void RegularTableau::saturate(GDNF &dnf) {
  // saturation phase
  GDNF copy = dnf;
  dnf = {};
  for (auto &clause : copy) {
    // saturation phase
    saturate(clause);
    Tableau saturatedTableau{clause};
    auto saturatedDnf = saturatedTableau.dnf();
    dnf.insert(std::end(dnf), std::begin(saturatedDnf), std::end(saturatedDnf));
  }
}

void RegularTableau::saturate(FormulaSet &formulas) {
  // emptiness assumptions
  std::vector<int> activeLabels;
  for (const auto &formula : formulas) {
    if (!formula.literal->negated) {
      auto formulaLabels = formula.literal->predicate->labels();
      activeLabels.insert(std::end(activeLabels), std::begin(formulaLabels),
                          std::end(formulaLabels));
    }
  }
  std::sort(activeLabels.begin(), activeLabels.end());
  activeLabels.erase(std::unique(activeLabels.begin(), activeLabels.end()), activeLabels.end());
  for (const auto &assumption : RegularTableau::emptinessAssumptions) {
    for (const auto i : activeLabels) {
      for (const auto j : activeLabels) {
        Set s(SetOperation::domain, Set(SetOperation::singleton, j), Relation(assumption.relation));
        Predicate p(PredicateOperation::intersectionNonEmptiness, Set(SetOperation::singleton, i),
                    std::move(s));
        Formula f(FormulaOperation::literal, Literal(true, std::move(p)));
        formulas.push_back(std::move(f));
      }
    }
  }

  // regular assumptions
  for (Formula &formula : formulas) {
    if (formula.operation == FormulaOperation::literal) {
      formula.literal->saturate();
    }
  }

  /* TODO:
  // identity assumptions
  for (const auto &literal : clause) {
    if (literal.negated) {
      auto saturated = saturateIdRelation(assumption, literal);
      if (saturated) {
        saturated->negated = true;
        saturatedRelations.push_back(*saturated);
      }
    }
  }
  */
}

void RegularTableau::extractCounterexample(Node *openNode) {
  std::ofstream counterexample("./output/counterexample.dot");
  counterexample << "digraph { node[shape=\"point\"]" << std::endl;

  Node *node = openNode;
  while (node->firstParentNode != nullptr) {
    auto edgeLabel = node->parentNodes[node->firstParentNode];
    for (const auto &edge : edgeLabel) {
      int left = *edge.literal->predicate->leftOperand->label;
      int right = *edge.literal->predicate->rightOperand->label;
      std::string relation = *edge.literal->predicate->identifier;

      counterexample << "N" << left << " -> "
                     << "N" << right << "[label = \"" << relation << "\"];" << std::endl;
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
    output << "root -> "
           << "N" << rootNode << ";" << std::endl;
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
