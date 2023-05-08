#include "Tableau.h"

#include <algorithm>
#include <iostream>
#include <tuple>
#include <utility>

#include "ProofRule.h"

// templates
/* RULES */
template <>
std::optional<std::tuple<Relation, Metastatement>> Tableau::Node::applyRule<ProofRule::a>() {
  if (relation && !relation->negated) {
    auto result = relation->applyRuleDeep<ProofRule::a, std::tuple<Relation, Metastatement>>();
    if (result) {
      auto &[newRelation, newMetastatement] = *result;
      appendBranches(newRelation);
      appendBranches(newMetastatement);
      return result;
    }
  }
  return std::nullopt;
}
template <>
std::optional<bool> Tableau::Node::applyRule<ProofRule::id>() {
  if (relation && !relation->negated) {
    auto rId = relation->applyRuleDeep<ProofRule::id, Relation>();
    if (rId) {
      appendBranches(*rId);
      return true;
    }
  }
  return std::nullopt;
}
template <>
std::optional<bool> Tableau::Node::applyRule<ProofRule::at>() {
  if (relation && !relation->negated) {
    auto atResult = relation->applyRuleDeep<ProofRule::at, std::tuple<Relation, Metastatement>>();
    if (atResult) {
      auto &[newRelation, newMetastatement] = *atResult;
      appendBranches(newRelation);
      appendBranches(newMetastatement);
    }
  }
  return std::nullopt;
}
// TODO: meta statemetn adaption :       node->metastatement->label2 =
// std::min(metastatement.label1, metastatement.label2);
template <>
std::optional<bool> Tableau::Node::applyRule<ProofRule::propagation>() {
  bool appliedRule = false;
  if (metastatement && metastatement->type == MetastatementType::labelEquality) {
    Tableau::Node *currentNode = parentNode;
    while (currentNode != nullptr) {
      if (currentNode->relation && currentNode->relation->negated) {
        auto rProp = currentNode->relation->applyRuleDeep<ProofRule::propagation, Relation>(
            &(*metastatement));
        if (rProp) {
          appliedRule = true;
          rProp->negated = currentNode->relation->negated;
          appendBranches(*rProp);
        }
      }
      currentNode = currentNode->parentNode;
    }
  }
  return appliedRule ? std::make_optional(true) : std::nullopt;
}
template <>
std::optional<bool> Tableau::Node::applyRule<ProofRule::negA>() {
  bool appliedRule = false;
  if (metastatement && metastatement->type == MetastatementType::labelRelation) {
    Tableau::Node *currentNode = parentNode;
    while (currentNode != nullptr) {
      if (currentNode->relation && currentNode->relation->negated) {
        auto rNegA =
            currentNode->relation->applyRuleDeep<ProofRule::negA, Relation>(&(*metastatement));
        if (rNegA) {
          appliedRule = true;
          rNegA->negated = currentNode->relation->negated;
          appendBranches(*rNegA);
        }
      }
      currentNode = currentNode->parentNode;
    }
  } else {
    Tableau::Node *currentMetaNode = parentMetastatement;
    while (currentMetaNode != nullptr) {
      auto rNegA =
          relation->applyRuleDeep<ProofRule::negA, Relation>(&(*currentMetaNode->metastatement));
      if (rNegA) {
        appliedRule = true;
        rNegA->negated = relation->negated;
        appendBranches(*rNegA);
      }
      currentMetaNode = currentMetaNode->parentMetastatement;
    }
  }
  return appliedRule ? std::make_optional(true) : std::nullopt;
}

Tableau::Tableau(std::initializer_list<Relation> initalRelations)
    : Tableau(std::vector(initalRelations)) {}
Tableau::Tableau(Clause initalRelations) {
  Node *currentNode = nullptr;
  for (auto relation : initalRelations) {
    auto newNode = std::make_unique<Node>(this, Relation(relation));
    newNode->parentNode = currentNode;
    Node *temp = newNode.get();
    if (rootNode == nullptr) {
      rootNode = std::move(newNode);
    } else if (currentNode != nullptr) {
      currentNode->leftNode = std::move(newNode);
    }

    currentNode = temp;
    unreducedNodes.push(temp);
  }
}

// TODO: move in Node class

bool Tableau::solve(int bound) {
  while (!unreducedNodes.empty() && bound > 0) {
    bound--;
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    // exportProof("infinite"); // TODO: remove
    currentNode->applyAnyRule();
  }
}

// pops unreduced nodes from stack until rule can be applied
bool Tableau::apply(const std::initializer_list<ProofRule> rules) {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (currentNode->apply(rules)) {
      return true;
    }
  }
  return false;
}

std::optional<Metastatement> Tableau::applyRuleA() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    auto result = currentNode->applyRule<ProofRule::a, std::tuple<Relation, Metastatement>>();
    if (result) {
      // TODO: apply at greedy
      while (currentNode->leftNode != nullptr) {
        currentNode = currentNode->leftNode.get();

        if (currentNode->leftNode == nullptr) {
          Node *nodeNeedsCheck = currentNode->parentNode;
          Node *newRelMetastatement = currentNode;
          auto atResult = nodeNeedsCheck->relation
                              ->applyRuleDeep<ProofRule::at, std::tuple<Relation, Metastatement>>();
          if (atResult) {
            auto &[newRelation, newEqMetastatement] = *atResult;
            nodeNeedsCheck->appendBranches(newRelation);
            nodeNeedsCheck->appendBranches(newEqMetastatement);

            newRelMetastatement->metastatement->label2 =
                std::min(newEqMetastatement.label1, newEqMetastatement.label2);
          }
        }
      }
      return std::get<Metastatement>(*result);
    }
  }
  return std::nullopt;
}

std::tuple<Clause, Clause> Tableau::extractRequest() const {
  // exrtact terms for new node
  Node *node = rootNode.get();
  std::optional<Relation> oldPositive;
  std::optional<Relation> newPositive;
  std::vector<int> activeLabels;
  std::vector<int> oldActiveLabels;
  while (node != nullptr) {  // exploit that only alpha rules applied
    if (node->relation && !node->relation->negated) {
      if (!oldPositive) {
        oldPositive = node->relation;
        oldActiveLabels = node->relation->labels();
      } else {
        newPositive = node->relation;
        activeLabels = node->relation->labels();
      }
    }
    node = node->leftNode.get();
  }

  Clause request{*newPositive};
  Clause converseRequest;
  node = rootNode.get();
  while (node != nullptr) {  // exploit that only alpha rules applied
    if (node->relation && node->relation->negated) {
      std::vector<int> relationLabels = node->relation->labels();
      bool allLabelsActive = true;
      bool allLabelsActiveInOld = true;
      for (auto label : relationLabels) {
        if (std::find(activeLabels.begin(), activeLabels.end(), label) == activeLabels.end()) {
          allLabelsActive = false;
          break;
        }
      }
      for (auto label : relationLabels) {
        if (std::find(oldActiveLabels.begin(), oldActiveLabels.end(), label) ==
            oldActiveLabels.end()) {
          allLabelsActiveInOld = false;
          break;
        }
      }
      if (allLabelsActive) {
        request.push_back(*node->relation);
      }
      if (allLabelsActiveInOld) {
        converseRequest.push_back(*node->relation);
      }
    }
    node = node->leftNode.get();
  }
  std::tuple<Clause, Clause> result{request, converseRequest};
  return result;
}

void Tableau::calcReuqest() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (currentNode->metastatement) {
      currentNode->applyRule<ProofRule::negA, bool>();
    } else if (currentNode->relation && currentNode->relation->negated) {
      currentNode->applyRule<ProofRule::negA, bool>();
    }
  }

  exportProof("requestcalc");  // TODO: remove
}

// DNF
std::vector<ExtendedClause> Tableau::DNF() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (currentNode->metastatement) {
      // only equality meatstatement possible
      currentNode->applyRule<ProofRule::propagation, bool>();
    } else {
      currentNode->applyDNFRule();
    }
  }

  exportProof("dnfcalc");  // TODO: remove

  auto dnf = rootNode->extractDNF();
  // filter redundant terms due to equality statements
  for (auto &clause : dnf) {
    // calc eqquivalence classes
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
    // remove minimal element in each class
    std::vector<int> forbidden;
    for (auto &equivClass : equivalenceClasses) {
      auto minLabel = std::min_element(equivClass.begin(), equivClass.end());
      equivClass.erase(minLabel);
      forbidden.insert(forbidden.end(), equivClass.begin(), equivClass.end());
    }
    std::sort(forbidden.begin(), forbidden.end());
    // filter clause: erease terms with non minimal labels
    auto clauseCopy = clause;
    clause.clear();
    for (auto &literal : clauseCopy) {
      auto relation = std::get_if<Relation>(&literal);
      if (relation) {
        auto labels = relation->labels();
        std::vector<int> intersection;
        std::sort(labels.begin(), labels.end());
        std::set_intersection(labels.begin(), labels.end(), forbidden.begin(), forbidden.end(),
                              back_inserter(intersection));
        if (intersection.empty()) {
          clause.push_back(literal);
        }
      } else {
        clause.push_back(literal);
      }
    }
  }
  return dnf;
};

std::vector<ExtendedClause> Tableau::Node::extractDNF() {
  if (isClosed()) {
    return {};
  }
  if (isLeaf()) {
    if (relation && relation->isNormal()) {
      return {{*relation}};
    }
    if (metastatement && metastatement->type == MetastatementType::labelEquality) {
      return {{*metastatement}};
    }
    return {};
  } else {
    std::vector<ExtendedClause> result;
    if (leftNode != nullptr) {
      auto left = leftNode->extractDNF();
      result.insert(result.end(), left.begin(), left.end());
    }
    if (rightNode != nullptr) {
      auto right = rightNode->extractDNF();
      result.insert(result.end(), right.begin(), right.end());
    }
    if (relation && relation->isNormal()) {
      if (result.empty()) {
        result.push_back({});
      }
      for (auto &clause : result) {
        clause.push_back(*relation);
      }
    } else if (metastatement && metastatement->type == MetastatementType::labelEquality) {
      if (result.empty()) {
        result.push_back({});
      }
      for (auto &clause : result) {
        clause.push_back(*metastatement);
      }
    }
    return result;
  }
}

void Tableau::toDotFormat(std::ofstream &output) const {
  output << "graph {" << std::endl << "node[shape=\"plaintext\"]" << std::endl;
  rootNode->toDotFormat(output);
  output << "}" << std::endl;
}

void Tableau::exportProof(std::string filename) const {
  std::ofstream file(filename + ".dot");
  toDotFormat(file);
  file.close();
}
