#include "Tableau.h"

#include <algorithm>
#include <iostream>
#include <tuple>
#include <utility>

#include "Metastatement.h"
#include "ProofRule.h"

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
