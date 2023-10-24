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

GDNF RegularTableau::calclateDNF(const FormulaSet &conjunction) {
  // TODO: more direct computation of DNF
  GDNF disjunction = {conjunction};
  Formula dummyRoot(FormulaOperation::top);
  Tableau tableau{std::move(dummyRoot)};
  tableau.rootNode->appendBranch(disjunction);
  tableau.solve();  // TODO: use dedicated function
  tableau.exportProof("infinite-debug");
  auto dnf = tableau.rootNode->extractDNF();
  GDNF saturatedDNF;
  for (auto &clause : dnf) {
    // saturation phase
    // saturate(clause) inlined
    for (Formula &formula : clause) {
      if (formula.operation == FormulaOperation::literal) {
        formula.literal->saturate();
      }
    }

    // saturate(clause) inlined end
    Tableau tableau2{clause};
    tableau2.solve();
    if (tableau2.rootNode->isClosed()) {
      continue;
    }

    // normal procedure after saturation:
    auto dnf2 = tableau2.rootNode->extractDNF();
    for (const auto &clause : dnf2) {
      saturatedDNF.push_back(clause);
    }
  }
  return saturatedDNF;
}

RegularTableau::RegularTableau(std::initializer_list<Formula> initalFormulas)
    : RegularTableau(std::vector(initalFormulas)) {}
RegularTableau::RegularTableau(FormulaSet initalFormulas) {
  auto dnf = calclateDNF(initalFormulas);
  for (const auto &clause : dnf) {
    auto newNode = addNode(clause);
    rootNodes.push_back(newNode);
  }
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
    if (!expandNode(currentNode)) {
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
bool RegularTableau::expandNode(Node *node) {
  Tableau tableau{node->formulas};
  auto atomicFormula = tableau.applyRuleA();  // TODO: make return value boolean

  if (atomicFormula) {  // node is expandable
    // request is calculated within normal form here
    tableau.solve();
    if (tableau.rootNode->isClosed()) {
      // can happen with emptiness hypotheses
      node->closed = true;
      return true;
    }
    // extract DNF
    auto dnf = tableau.rootNode->extractDNF();

    // saturation phase
    for (auto &clause : dnf) {
      // saturation phase
      saturate(clause);
      Tableau tableau2{clause};
      tableau.solve();
      if (tableau2.rootNode->isClosed()) {
        continue;
      }

      // normal procedure after saturation:
      auto dnf2 = tableau2.rootNode->extractDNF();
      // for each clause: calculate potential child
      for (auto &clause : dnf2) {
        EdgeLabel edgeLabel;
        FormulaSet newClause;

        // remove edge predicates and put them in the edge label
        for (const auto &formula : clause) {
          if (formula.isEdgePredicate()) {
            edgeLabel.push_back(formula);
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
        // 2) filtering
        FormulaSet copy = std::move(newClause);
        newClause = {};
        FormulaSet droppedFormulas;
        for (const auto &formula : copy) {
          auto formulaLabels = formula.literal->predicate->labels();
          if (isSubset(formulaLabels, activeLabels)) {
            newClause.push_back(formula);
          } else {
            // check for immediate inconsistencies
            droppedFormulas.push_back(formula);
          }
        }

        // check if intial inconsistency exists
        auto alternativeNode = checkInconsistency(node, droppedFormulas);
        if (alternativeNode) {
          // create new fixed Node
          Node *fixedNode = addNode(*alternativeNode);
          addEdge(node, fixedNode, {});
          return true;
        }

        // add child
        // checks if renaming exists when adding and returns already existing node if possible
        Node *newNode = addNode(newClause);
        addEdge(node, newNode, edgeLabel);
      }
    }

    // TODO?: check if any clause has been added
    /*otherwise:
    if (dnf.empty()) {
      node->closed = true;
    }*/

    return true;
  }
  return false;
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

void RegularTableau::saturate(FormulaSet &formulas) {
  // regular assumptions
  for (Formula &formula : formulas) {
    if (formula.operation == FormulaOperation::literal) {
      formula.literal->saturate();
    }
  }
  /*TODO
  case AssumptionType::empty:
        for (const auto &literal : clause) {
          for (auto label : literal.labels()) {
            if (find(nodeLabels.begin(), nodeLabels.end(), label) == nodeLabels.end()) {
              nodeLabels.push_back(label);
            }
          }
        }

        for (auto label : nodeLabels) {
          Relation leftSide(assumption.relation);
          Relation full(RelationOperation::full);
          Relation r(RelationOperation::composition, std::move(leftSide), std::move(full));
          r.label = label;
          r.negated = true;
          // TODO: currently: saturate emoty hyptohesis with regular -> simple formulation of
          // emptiness hypos better: preprocess all hypos ?
          std::optional<Relation> saturated = saturateRelation(r);
          if (saturated) {
            saturated->negated = true;
            saturated->label = label;
            saturatedRelations.push_back(std::move(*saturated));
          }
          //----
          saturatedRelations.push_back(std::move(r));
        }
        break;
        case AssumptionType::identity:
        for (const auto &literal : clause) {
          if (literal.negated) {
            auto saturated = saturateIdRelation(assumption, literal);
            if (saturated) {
              saturated->negated = true;
              saturatedRelations.push_back(*saturated);
            }
          }
        }*/
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
