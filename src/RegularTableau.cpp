#include "RegularTableau.h"

#include <algorithm>
#include <boost/pending/disjoint_sets.hpp>
#include <iostream>
#include <map>
#include <tuple>
#include <utility>

// helper
void printGDNF(const GDNF &gdnf) {
  std::cout << "Clauses:";
  for (auto &clause : gdnf) {
    std::cout << "\n";
    for (auto &literal : clause) {
      std::cout << literal.toString() << " , ";
    }
  }
  std::cout << std::endl;
}

void printFormulaSet(const FormulaSet &formulas) {
  for (auto &literal : formulas) {
    std::cout << literal.toString() << " , ";
  }
  std::cout << std::endl;
}

// assumption: input is dnf clause
// purges useless literals
FormulaSet purge(const FormulaSet &clause, FormulaSet &dropped, EdgeLabel &label) {
  FormulaSet newClause;

  // remove edge predicates and put them in the edge label
  for (const auto &formula : clause) {
    if (formula.isEdgePredicate() || formula.isPositiveEqualityPredicate()) {
      std::get<0>(label).push_back(formula);
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
  for (const auto &formula : std::get<0>(label)) {
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

// return fixed node as set, otherwise nullopt if consistent
std::optional<FormulaSet> getInconsistentLiterals(RegularTableau::Node *parent,
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
    std::cout << "[Solver] Inconsistent Node " << std::hash<RegularTableau::Node>()(*parent)
              << std::endl;
    // TODO: remove:
    // printFormulaSet(filteredNewFormulas);
    // at this point filteredNewFormulas is the alternative child
    return filteredNewFormulas;
  }
  return std::nullopt;
}
// helper end

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
RegularTableau::Node *RegularTableau::addNode(FormulaSet clause, EdgeLabel &label) {
  // rename clause and update edgeLabel // TODO: do sorting inside renaming
  std::sort(clause.begin(), clause.end());
  auto renaming = rename(clause);
  std::get<1>(label) = renaming;
  // create node, add to nodes (returns pointer to existing node if already exists)
  auto newNode = std::make_unique<Node>(clause);
  auto insertion = nodes.insert(std::move(newNode));
  if (insertion.second) {
    // new node has been added added (no isomorphic node existed)
    unreducedNodes.push(insertion.first->get());

    std::cout << "[Solver] Add node " << std::hash<Node>()(*insertion.first->get()) << std::endl;
  }
  return insertion.first->get();
}

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
    std::cout << "[Solver] Add edge " << std::hash<Node>()(*parent) << " -> "
              << std::hash<Node>()(*child) << ", label: ";
    printFormulaSet(std::get<0>(label));
    // adding edge
    // only add each child once to child nodes
    auto childIt = std::find(parent->childNodes.begin(), parent->childNodes.end(), child);
    if (childIt == parent->childNodes.end()) {
      parent->childNodes.push_back(child);
    }
    child->parentNodes[parent].push_back(label);

    // set first parent node if not already set
    if (child->firstParentNode == nullptr) {
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
    exportProof("regular-debug");
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if ((std::find(rootNodes.begin(), rootNodes.end(), currentNode) == rootNodes.end() &&
         currentNode->rootParents.empty()) ||
        currentNode->closed) {
      // skip already closed nodes and nodes that cannot be reached by a root node
      continue;
    }

    Tableau tableau{currentNode->formulas};
    auto atomicFormula = tableau.applyRuleA();  // TODO: make return value boolean

    if (atomicFormula) {
      expandNode(currentNode, &tableau);

    } else {
      extractCounterexample(currentNode);
      std::cout << "[Solver] False." << std::endl;
      return false;
    }
  }
  std::cout << "[Solver] True." << std::endl;
  return true;
}

// assumptions:
// node has only normal terms
void RegularTableau::expandNode(Node *node, Tableau *tableau) {
  // node is expandable
  // calculate normal form
  auto dnf = tableau->dnf();
  saturate(dnf);
  if (dnf.empty() && node != nullptr) {
    node->closed = true;
    return;
  }

  std::cout << "[Solver] Expand node " << (node == nullptr ? 0 : std::hash<Node>()(*node))
            << std::endl;

  // for each clause: calculate potential child
  for (const auto &clause : dnf) {
    FormulaSet droppedFormulas;
    EdgeLabel edgeLabel;
    auto newClause = purge(clause, droppedFormulas, edgeLabel);

    // check if immediate inconsistency exists (inconsistent literals that are immdediatly
    // dropped)
    auto alternativeNode = getInconsistentLiterals(node, droppedFormulas);
    if (alternativeNode) {
      // create new fixed Node
      Node *fixedNode = addNode(*alternativeNode, edgeLabel);
      addEdge(node, fixedNode, {});
      return;
    }

    // add child
    // checks if renaming exists when adding and returns already existing node if possible
    // updates edge Label with renaming
    Node *newNode = addNode(newClause, edgeLabel);
    addEdge(node, newNode, edgeLabel);
  }
}

// input is edge (parent,label,child)
// must not be part of the proof graph
bool RegularTableau::isInconsistent(Node *parent, Node *child, EdgeLabel label) {
  if (std::get<0>(label).empty()) {  // is already fixed node to its parent (by def inconsistent)
    return false;                    // should return true?
  }

  std::cout << "[Solver] Check consistency " << std::hash<Node>()(*parent) << " -> "
            << std::hash<Node>()(*child) << std::endl;
  if (child->formulas.empty()) {
    // empty child not incsonistent
    return false;
  }
  // calculate converse request
  FormulaSet converseRequest = child->formulas;
  // use parent naming: label has already parent naming, rename child formulas
  for (auto &formula : converseRequest) {
    formula.literal->predicate->rename(std::get<1>(label), true);
  }

  Tableau calcConverseReq(converseRequest);
  // add formulas of label (adding them to converseRequest is incomplete, because inital
  // contradiction are not checked)
  for (const auto &formula : std::get<0>(label)) {
    calcConverseReq.rootNode->appendBranch(formula);
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
  GDNF dnf = calcConverseReq.rootNode->extractDNF();
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
  /* TODO:
  for (const auto &assumption : RegularTableau::emptinessAssumptions) {
    Set s(SetOperation::domain, Set(SetOperation::full), Relation(assumption.relation));
    Predicate p(PredicateOperation::intersectionNonEmptiness, Set(SetOperation::full),
                std::move(s));
    Formula f(FormulaOperation::literal, Literal(true, std::move(p)));
    formulas.push_back(std::move(f));
  }
  */

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
    for (const auto &edge : std::get<0>(node->firstParentLabel)) {
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
