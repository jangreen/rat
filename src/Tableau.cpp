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
bool Tableau::applyRule(Node *node) {
  if (node->metastatement) {
    if (node->metastatement->type == MetastatementType::labelRelation) {
      node->applyRule<ProofRule::negA>();
      // no rule applicable anymore (metastatement)
      return false;  // TODO: this value may be true
    } else {
      node->applyRule<ProofRule::propagation>();
      return true;
    }
  } else if (node->relation) {
    if (node->relation->isNormal()) {
      if (node->relation->negated) {
        if (node->applyRule<ProofRule::negA>()) {
          return true;
        }
      } else {
        auto rA =
            node->relation->applyRuleRecursive<ProofRule::a, std::tuple<Relation, Metastatement>>();
        if (rA) {
          auto &[r1, metastatement] = *rA;
          r1.negated = node->relation->negated;
          node->appendBranches(r1);
          node->appendBranches(metastatement);
          return true;
        }
      }
    } else if (node->applyDNFRule()) {
      return true;
    }
  }
  return false;
}

bool Tableau::solve(int bound) {
  while (!unreducedNodes.empty() && bound > 0) {
    bound--;
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    // exportProof("infinite"); // TODO: remove
    applyRule(currentNode);
  }
}

bool Tableau::applyModalRule() {
  // assuming that all terms are reduced, s.t. only possible next rule is modal
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (applyRule(currentNode)) {
      // if at rule can be applied do it directly
      // TODO: check this is this the best way to do this? ----
      Node *node = currentNode;
      while (node->leftNode != nullptr) {
        node = &(*node->leftNode);
      }
      Node *newNodeAtCheck = node->parentNode;
      auto atResult =
          newNodeAtCheck->relation
              ->applyRuleRecursive<ProofRule::at, std::tuple<Relation, Metastatement>>();
      if (atResult) {
        const auto &[relation, metastatement] = *atResult;
        node->metastatement->label2 = std::min(metastatement.label1, metastatement.label2);
        node->appendBranches(relation);
        // TODO dont needed?!: node->appendBranches(metastatement);
      }
      // ----
      return true;
    }
  }
  return false;
}

std::tuple<Clause, Clause> Tableau::extractRequest() {
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

std::tuple<Clause, Clause> Tableau::calcReuqest() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (currentNode->metastatement) {
      currentNode->applyRule<ProofRule::negA>();
    } else if (currentNode->relation && currentNode->relation->negated) {
      currentNode->applyRule<ProofRule::negA>();
    }
  }

  exportProof("requestcalc");  // TODO: remove

  return extractRequest();
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
