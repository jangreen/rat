#include "RegularTableau.h"

#include <algorithm>
#include <boost/pending/disjoint_sets.hpp>
#include <iostream>
#include <map>
#include <tuple>
#include <utility>

GDNF RegularTableau::calclateDNF(const FormulaSet &conjunction) {
  // TODO: more direct computation of DNF
  GDNF disjunction = {conjunction};
  Formula dummyRoot(FormulaOperation::literal, Literal(false, Predicate(PredicateOperation::top)));
  Tableau tableau{std::move(dummyRoot)};
  tableau.rootNode->appendBranch(disjunction);
  tableau.solve();  // TODO: use dedicated function
  return tableau.rootNode->extractDNF();
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
  // calculate renaming & rename
  // TODO: do for more than one positive literal
  Renaming renaming;
  for (auto &formula : clause) {
    if (!formula.literal->negated) {
      renaming = formula.literal->predicate->calculateRenaming();
      break;
    }
  }

  for (auto &formula : clause) {
    formula.rename(renaming);
  }

  // create node, check if it is duplicate
  auto newNode = std::make_unique<Node>(clause);
  auto existingNode = nodes.find(newNode);
  if (existingNode != nodes.end()) {
    // equivalent node exists
    auto oldNode = existingNode->get();
    return oldNode;
  } else {
    unreducedNodes.push(newNode.get());
    nodes.emplace(std::move(newNode));
    return newNode.get();
  }

  // saturation phase
  /*saturate(clause);
  auto saturatedDNF = DNF(clause);
  for (auto saturatedClause : saturatedDNF) {
    // filter clause
    Clause filteredClause;
    for (auto &literal : saturatedClause) {
      auto relation = std::get_if<Relation>(&literal);
      if (relation) {
        filteredClause.push_back(*relation);
      }
    }

    // PREVIOUSLY HERE WAS THE ACTUAL CREATION
    // create node, edges
    auto newNode = std::make_unique<Node>(filteredClause);
    ....
    // ENDOF PREVIOUSLY HERE WAS THE ACTUAL CREATION

    // check if consistent
    bool inconsistent =
        parent && isInconsistent(parent, newNode.get());  // parent null if root node
    if (!inconsistent) {
      // push to unreduced nodes, add node to proof

    }
  }*/
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
  auto atomicFormula = tableau.applyRuleA();

  if (atomicFormula) {
    // tableau.calculateRequest();
    tableau.solve();
    if (tableau.rootNode->isClosed()) {
      // can happen with emptiness hypotheses
      node->closed = true;
      return true;
    }

    // extract DNF and remove atomicFormula
    auto dnf = tableau.rootNode->extractDNF();
    GDNF newDNF;
    for (auto &clause : dnf) {
      FormulaSet newClause;
      for (auto &formula : clause) {
        if (formula != *atomicFormula) {
          newClause.emplace_back(formula);
        }
      }
      newDNF.emplace_back(newClause);
    }

    /*for (const auto &clause : dnf) {
      // TODO: refactor DRY
      // calculate equivalence class to rename metaststatement
      std::vector<std::vector<int>> equivalenceClasses;
      for (auto &literal : clause) {
        auto metastatement = std::get_if<Metastatement>(&literal);
        if (metastatement) {
          bool alreadyExists = false;
          for (auto &EquivClass : equivalenceClasses) {
            bool l1found = std::find(EquivClass.begin(), EquivClass.end(), metastatement->label1) !=
                           EquivClass.end();
            bool l2found = std::find(EquivClass.begin(), EquivClass.end(), metastatement->label2) !=
                           EquivClass.end();
            if (l1found && !l2found) {
              EquivClass.push_back(metastatement->label2);
              alreadyExists = true;
            }
            if (!l1found && l2found) {
              EquivClass.push_back(metastatement->label1);
              alreadyExists = true;
            }
          }
          if (!alreadyExists) {
            equivalenceClasses.push_back({metastatement->label1, metastatement->label2});
          }
        }
      }
      // fix metatstatement
      for (auto &equivClass : equivalenceClasses) {
        if (std::find(equivClass.begin(), equivClass.end(), metastatement->label1) !=
            equivClass.end()) {
          auto minLabel = std::min_element(equivClass.begin(), equivClass.end());
          metastatement->label1 = *minLabel;
        }
        if (std::find(equivClass.begin(), equivClass.end(), metastatement->label2) !=
            equivClass.end()) {
          auto minLabel = std::min_element(equivClass.begin(), equivClass.end());
          metastatement->label2 = *minLabel;
        }
      }
      // TODO: -------
      addNode(node, clause, metastatement);
    }*/
    if (dnf.empty()) {
      node->closed = true;
    } else {
      for (auto &clause : dnf) {
        addNode(clause);
        // TODO: connect edges, renaming, inconsistency check
      }
    }

    return true;
  }
  return false;
}

// LEGACY
/*

std::vector<Assumption> RegularTableau::assumptions;

// helper
void resetUnreducedNodes(Tableau *tableau, Tableau::Node *node) {
  tableau->unreducedNodes.push(node);
  if (node->leftNode != nullptr) {
    resetUnreducedNodes(tableau, node->leftNode.get());
  }
  if (node->rightNode != nullptr) {
    resetUnreducedNodes(tableau, node->rightNode.get());
  }
}

// helper
std::vector<int> getActiveLabels(RegularTableau::Node *node) {
  for (const auto &relation : node->formulas) {
    if (!relation.negated) {
      return relation.labels();
    }
  }
  return {};
}

bool RegularTableau::isInconsistent(Node *node, Node *newNode) {
  if (!newNode->parentNodeExpansionMeta) {  // is already alternative node to its parent
    return false;
  }
  // calculate converse request: remove positive, inverse rename
  Clause converseRequest = newNode->formulas;
  converseRequest.erase(std::remove_if(converseRequest.begin(), converseRequest.end(),
                                       [](const Relation &r) { return !r.negated; }),
                        converseRequest.end());
  for (auto &relation : converseRequest) {
    relation.inverseRename(newNode->parentNodeRenaming);
  }
  // add parent terms to converse request (see test6)
  converseRequest.insert(converseRequest.end(), node->formulas.begin(), node->formulas.end());
  Tableau calcConverseReq(converseRequest);
  auto edgeLabel = newNode->parentNodes[node];
  calcConverseReq.rootNode->appendBranches(std::get<Metastatement>(*edgeLabel));
  calcConverseReq.calcReuqest();
  if (calcConverseReq.rootNode->isClosed()) {
    newNode->closed = true;
    return false;
  }
  calcConverseReq.exportProof("converserequestcalc");
  // extract coverse request
  Tableau::Node *temp = &(*calcConverseReq.rootNode);
  converseRequest.clear();
  while (temp != nullptr) {
    if (temp->formulas && temp->formulas->isNormal()) {
      converseRequest.push_back(*temp->formulas);
    }
    temp = &(*temp->leftNode);  // exploit only alpha rules applied
  }
  // filter active labels
  Clause filtered;
  auto activeLabels = getActiveLabels(node);
  std::copy_if(
      converseRequest.begin(), converseRequest.end(), std::back_inserter(filtered),
      [activeLabels](Relation &r) {
        for (auto l : r.labels()) {
          if (std::find(activeLabels.begin(), activeLabels.end(), l) == activeLabels.end()) {
            return false;
          }
        }
        return true;
      });
  // compare if new
  bool inconsistent = false;
  for (const auto &relation : filtered) {
    if (std::find(node->formulas.begin(), node->formulas.end(), relation) == node->formulas.end()) {
      inconsistent = true;
    }
  }
  if (!inconsistent) {
    return false;
  }

  // calculate alternative
  // calculate epsilon child
  Clause nodeCopy = node->formulas;
  Clause missingRelations;
  for (const auto &relation : filtered) {
    if (std::find(nodeCopy.begin(), nodeCopy.end(), relation) == nodeCopy.end()) {
      nodeCopy.push_back(relation);
      missingRelations.push_back(relation);
    }
  }
  auto dnf = DNF(nodeCopy);
  for (const auto &clause : dnf) {
    addNode(node, clause);  // epsilon edge label
    // TODO: remove exportProof("regular");
  }
  // change incoming edges from parents
  auto parentNodesCopy = node->parentNodes;
  for (const auto &[parentNode, parentLabel] : parentNodesCopy) {
    for (const auto childNode : node->childNodes) {
      if (!childNode->parentNodes[node]) {  // here are all children epsilon
        // counterexample
        childNode->parentNodeExpansionMeta = node->parentNodeExpansionMeta;
        childNode->parentNode = node->parentNode;
        childNode->parentNodeRenaming = node->parentNodeRenaming;
        childNode->parentEquivalences = node->parentEquivalences;
        // add edge
        addEdge(parentNode, childNode, parentLabel);
      }
    }

    // remove edge from parent to insoncsitent node
    removeEdge(parentNode, node);
    // TODO: remove exportProof("regular");
  }
  if (std::find(rootNodes.begin(), rootNodes.end(), node) != rootNodes.end()) {
    for (auto childNode : node->childNodes) {
      if (!childNode->parentNodes[node]) {
        rootNodes.push_back(childNode);
      }
    }
  }

  // check recursively
  for (auto const &[parentNode, parentLabel] : parentNodesCopy) {
    for (const auto childNode : node->childNodes) {
      if (!childNode->parentNodes[node]) {
        if (isInconsistent(parentNode, childNode)) {
          // remove edge
          for (auto child :
               node->childNodes) {  // new epsilon childs (are directly connected to parentNode)
            removeEdge(parentNode, child);
          }
        }
      }
    }
  }
  return true;
}

void RegularTableau::addEdge(Node *parent, Node *child, EdgeLabel label) {
  parent->childNodes.push_back(child);
  child->parentNodes[parent] = label;
  if (std::find(rootNodes.begin(), rootNodes.end(), parent) != rootNodes.end() ||
      !parent->rootParents.empty()) {
    child->rootParents.push_back(parent);
  }
  // TODO: remove exportProof("regular");
  / *
  newNode->parentNodes.insert({
      parent,
  });
  newNode->parentNodeRenaming = renaming;
  newNode->parentEquivalences = equalityStatements;
  newNode->parentNode = parent;
  if (parent != nullptr) {
    if (metastatement) {
      std::tuple<Metastatement, Renaming> edgelabel{*metastatement, renaming};
      newNode->parentNodes[parent] = edgelabel;
    } else {
      newNode->parentNodes[parent] = std::nullopt;
    }
    if (std::find(rootNodes.begin(), rootNodes.end(), parent) != rootNodes.end() ||
        !parent->rootParents.empty()) {
      newNode->rootParents.push_back(parent);
    }
  }
}

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
  // TODO: remove exportProof("regular");
}

std::optional<Relation> RegularTableau::saturateRelation(const Relation &relation) {
  if (relation.saturated || relation.saturatedId) {
    return std::nullopt;
  }

  std::optional<Relation> leftSaturated;
  std::optional<Relation> rightSaturated;

  switch (relation.operation) {
    case RelationOperation::none:
    case RelationOperation::identity:
    case RelationOperation::empty:
      return std::nullopt;
    case RelationOperation::base:
      for (const auto &assumption : assumptions) {
        // TODO fast get regular assumption
        if (assumption.type == AssumptionType::regular &&
            *assumption.baseRelation == *relation.identifier) {
          Relation leftSide(assumption.relation);
          leftSide.label = relation.label;
          return leftSide;
        }
      }
      return std::nullopt;
    case RelationOperation::choice:
    case RelationOperation::intersection:
      leftSaturated = saturateRelation(*relation.leftOperand);
      rightSaturated = saturateRelation(*relation.rightOperand);
      break;
    case RelationOperation::composition:
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      leftSaturated = saturateRelation(*relation.leftOperand);
      break;
    default:
      break;
  }
  if (!leftSaturated && !rightSaturated) {
    return std::nullopt;
  }
  if (!leftSaturated) {
    leftSaturated = Relation(*relation.leftOperand);
  }
  if (!rightSaturated && relation.rightOperand != nullptr) {
    rightSaturated = Relation(*relation.rightOperand);
  }
  if (rightSaturated) {
    Relation saturated(relation.operation, std::move(*leftSaturated), std::move(*rightSaturated));
    return saturated;
  }
  Relation saturated(relation.operation, std::move(*leftSaturated));
  return saturated;
}

std::optional<Relation> RegularTableau::saturateIdRelation(const Assumption &assumption,
                                                           const Relation &relation) {
  if (relation.saturatedId || relation.operation == RelationOperation::identity ||
      relation.operation == RelationOperation::empty ||
      (relation.operation == RelationOperation::converse && relation.leftOperand->saturatedId)) {
    return std::nullopt;
  }
  if (relation.label) {
    Relation copy(relation);
    copy.label = std::nullopt;
    copy.negated = false;
    Relation assumptionR(assumption.relation);
    assumptionR.label = relation.label;
    Relation r(RelationOperation::composition, std::move(assumptionR), std::move(copy));
    return r;
  }

  std::optional<Relation> leftSaturated;
  std::optional<Relation> rightSaturated;

  switch (relation.operation) {
    case RelationOperation::none:
    case RelationOperation::identity:
    case RelationOperation::empty:
    case RelationOperation::base:
      return std::nullopt;
    case RelationOperation::choice:
    case RelationOperation::intersection:
      leftSaturated = saturateIdRelation(assumption, *relation.leftOperand);
      rightSaturated = saturateIdRelation(assumption, *relation.rightOperand);
      break;
    case RelationOperation::composition:
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      leftSaturated = saturateIdRelation(assumption, *relation.leftOperand);
      break;
    default:
      break;
  }

  // TODO_ support intersections -> different identity or no saturations
  if (!leftSaturated && !rightSaturated) {
    return std::nullopt;
  }
  if (!leftSaturated) {
    leftSaturated = Relation(*relation.leftOperand);
  }
  if (!rightSaturated) {
    rightSaturated = Relation(*relation.rightOperand);
  }
  Relation saturated(relation.operation, std::move(*leftSaturated), std::move(*rightSaturated));
  return saturated;
}

void RegularTableau::saturate(Clause &clause) {
  Clause saturatedRelations;
  // regular assumptions
  for (const Relation &literal : clause) {
    if (literal.negated) {
      std::optional<Relation> saturated = saturateRelation(literal);
      if (saturated) {
        saturated->negated = true;
        saturatedRelations.push_back(std::move(*saturated));
      }
    }
  }

  for (const auto &assumption : assumptions) {
    std::vector<int> nodeLabels;
    switch (assumption.type) {
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
        }
      default:
        break;
    }
  }

  clause.insert(clause.end(), saturatedRelations.begin(), saturatedRelations.end());
  // remove duplicates since it has not been check when inserting
  sort(clause.begin(), clause.end());
  clause.erase(unique(clause.begin(), clause.end()), clause.end());
}

void RegularTableau::extractCounterexample(Node *openNode) {
  Node *node = openNode;

  // calculate equivlence classes
  std::vector<std::string> elements;
  typedef std::map<std::string, int> rank_t;
  typedef std::map<std::string, std::string> parent_t;
  rank_t ranks;
  parent_t parents;
  boost::disjoint_sets<boost::associative_property_map<rank_t>,
                       boost::associative_property_map<parent_t>>
      dsets(boost::make_assoc_property_map(ranks), boost::make_assoc_property_map(parents));

  // TODO: refactor
  while (node->parentNode != nullptr) {
    for (int i = 0; i < node->parentNodeRenaming.size(); i++) {
      std::stringstream ss1;
      ss1 << node->parentNode << "_" << node->parentNodeRenaming[i];
      if (std::find(elements.begin(), elements.end(), ss1.str()) == elements.end()) {
        dsets.make_set(ss1.str());
        elements.push_back(ss1.str());
      }
      std::stringstream ss2;
      ss2 << node << "_" << i;
      if (std::find(elements.begin(), elements.end(), ss2.str()) == elements.end()) {
        dsets.make_set(ss2.str());
        elements.push_back(ss2.str());
      }
      dsets.union_set(ss1.str(), ss2.str());
    }
    for (auto equivalence : node->parentEquivalences) {
      std::stringstream ss1;
      ss1 << node->parentNode << "_" << equivalence.label1;
      if (std::find(elements.begin(), elements.end(), ss1.str()) == elements.end()) {
        dsets.make_set(ss1.str());
        elements.push_back(ss1.str());
      }
      std::stringstream ss2;
      ss2 << node->parentNode << "_" << equivalence.label2;
      if (std::find(elements.begin(), elements.end(), ss2.str()) == elements.end()) {
        dsets.make_set(ss2.str());
        elements.push_back(ss2.str());
      }
      dsets.union_set(ss1.str(), ss2.str());
    }
    node = node->parentNode;
  }

  std::ofstream counterexample("./output/counterexample.dot");
  counterexample << "digraph { node[shape=\"point\"]" << std::endl;
  node = openNode;
  while (node->parentNode != nullptr) {
    std::stringstream ss1;
    ss1 << node->parentNode << "_" << node->parentNodeExpansionMeta->label1;
    std::stringstream ss2;
    ss2 << node->parentNode << "_" << node->parentNodeExpansionMeta->label2;

    counterexample << "N" << dsets.find_set(ss1.str()) << " -> "
                   << "N" << dsets.find_set(ss2.str()) << "[label = \""
                   << *node->parentNodeExpansionMeta->baseRelation << "\"];" << std::endl;
    node = node->parentNode;
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
  std::ofstream file("./../output/" + filename + ".dot");
  toDotFormat(file);
  file.close();
}
*/
