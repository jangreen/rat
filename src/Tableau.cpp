#include "Tableau.h"

#include <algorithm>
#include <iostream>
#include <tuple>
#include <utility>

/* LEGACY
// templates
// RULES
template <>
std::optional<std::tuple<Relation, Metastatement>> Tableau::Node::applyRule<ProofRule::a>() {
  if (formula && formula->operation == FormulaOperation::literal && !formula->literal->negated) {
    auto result = formula->literal->predicate
                      ->applyRuleDeep<ProofRule::a, std::tuple<Relation, Metastatement>>();
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
*/

Tableau::Tableau(std::initializer_list<Formula> initalFormulas)
    : Tableau(std::vector(initalFormulas)) {}
Tableau::Tableau(FormulaSet initalFormulas) {
  Node *currentNode = nullptr;
  for (const auto &formula : initalFormulas) {
    auto newNode = std::make_unique<Node>(this, std::move(formula));
    newNode->parentNode = currentNode;

    Node *temp = newNode.get();
    if (rootNode == nullptr) {
      rootNode = std::move(newNode);
    } else if (currentNode != nullptr) {
      newNode->parentNode->leftNode = std::move(newNode);
    }

    currentNode = temp;
    unreducedNodes.push(temp);
  }
}

bool Tableau::solve(int bound) {
  while (!unreducedNodes.empty() && bound > 0) {
    bound--;
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    // TODO: remove:
    // std::cout << "[Tableau]: Current Node. " << currentNode->formula.toString() << std::endl;
    auto result = currentNode->applyRule();

    // next two methods try to apply negated modal rules
    if (currentNode->formula.isNormal()) {  // (!result) { does not work in case: ~0(b1) -> ~((0b)1)
                                            // but dont considers replacing b1
      currentNode->inferModal();
    }
    if (currentNode->formula.isEdgePredicate()) {
      currentNode->inferModalAtomic();
    }
    exportProof("infinite-debug");
  }

  // warning if bound is reached
  if (bound == 0 && !unreducedNodes.empty()) {
    std::cout << "[Warning] Bonfigured bound is reached. Answer is imprecise." << std::endl;
  }

  return rootNode->isClosed();
}

std::vector<std::vector<Formula>> Tableau::Node::extractDNF() const {
  if (isClosed()) {
    return {};
  }
  if (isLeaf()) {
    if (formula.isNormal()) {
      return {{formula}};
    }
    return {};
  } else {
    std::vector<std::vector<Formula>> result;
    if (leftNode != nullptr) {
      auto left = leftNode->extractDNF();
      result.insert(result.end(), left.begin(), left.end());
    }
    if (rightNode != nullptr) {
      auto right = rightNode->extractDNF();
      result.insert(result.end(), right.begin(), right.end());
    }
    if (formula.isNormal()) {
      if (result.empty()) {
        result.push_back({});
      }
      for (auto &clause : result) {
        clause.push_back(formula);
      }
    }
    return result;
  }
}

std::optional<Formula> Tableau::applyRuleA() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();

    auto result = currentNode->applyRule(true);
    if (result) {
      auto modalResult = *result;

      // currently remove node by replacing it with dummy
      // this is needed for expandNode
      currentNode->formula = Formula(FormulaOperation::top);

      // find atomic
      for (const auto &clause : modalResult) {
        // should be only one clause
        for (const auto &formula : clause) {
          if (formula.operation == FormulaOperation::literal && formula.isEdgePredicate()) {
            return formula;
          }
        }
      }
    }
  }
  return std::nullopt;
}

void Tableau::toDotFormat(std::ofstream &output) const {
  output << "graph {" << std::endl << "node[shape=\"plaintext\"]" << std::endl;
  if (rootNode != nullptr) {
    rootNode->toDotFormat(output);
  }
  output << "}" << std::endl;
}

void Tableau::exportProof(std::string filename) const {
  // TODO: std::cout << "[Status] Export infinite proof: " << filename << ".dot" << std::endl;
  std::ofstream file("./output/" + filename + ".dot");
  toDotFormat(file);
  file.close();
}

/* LEGACY
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

std::tuple<ExtendedClause, Clause> Tableau::extractRequest() const {
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

  ExtendedClause request{*newPositive};
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
    } else if (node->metastatement &&
               node->metastatement->type == MetastatementType::labelEquality) {
      request.push_back(*node->metastatement);
    }
    node = node->leftNode.get();
  }
  std::tuple<ExtendedClause, Clause> result{request, converseRequest};
  return result;
}

void Tableau::calcReuqest() {
  while (unreducedNodes.top()->metastatement ||
         unreducedNodes.top()->relation->negated) {  //! unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (currentNode->metastatement) {
      currentNode->applyRule<ProofRule::negA, bool>();
    } else if (currentNode->relation && currentNode->relation->negated) {
      auto rNegAt = currentNode->relation->applyRuleDeep<ProofRule::negAt, Relation>();
      if (rNegAt) {
        auto r1 = *rNegAt;
        r1.negated = currentNode->relation->negated;
        currentNode->appendBranches(r1);
      } else {
        currentNode->applyRule<ProofRule::negA, bool>();
      }
    }
  }

  // TODO: remove exportProof("requestcalc");  // TODO: remove
}

// DNF
DNF Tableau::calcDNF() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (currentNode->formula->leftOperand->operation == FormulaOperation::literal &&
        !currentNode->formula->leftOperand->literal->negated &&
        currentNode->formula->leftOperand->literal->predicate->leftOperand->operation ==
            SetOperation::singleton &&
        currentNode->formula->leftOperand->literal->predicate->rightOperand->operation ==
            SetOperation::singleton) {
      // case e1.e2
      currentNode->applyRule<ProofRule::propagation, bool>();
    } else {
      currentNode->applyDNFRule();
    }
  }

  // TODO: remove exportProof("dnfcalc");  // TODO: remove

  auto dnf = rootNode->extractDNF();

  // keep only terms with active labels
  for (auto &clause : dnf) {
    // determine active labels & filter terms with non active labels
    auto clauseCopy = clause;
    clause.clear();
    std::vector<int> activeLabels;
    for (auto &literal : clauseCopy) {
      auto relation = std::get_if<Relation>(&literal);
      if (relation && !relation->negated) {
        activeLabels = relation->labels();
        clause.push_back(literal);
        std::sort(activeLabels.begin(), activeLabels.end());
        break;
      }
    }

    for (auto &literal : clauseCopy) {
      auto relation = std::get_if<Relation>(&literal);
      if (relation && relation->negated) {
        bool include = true;
        for (auto label : relation->labels()) {
          if (std::find(activeLabels.begin(), activeLabels.end(), label) == activeLabels.end()) {
            include = false;
            break;
          }
        }
        if (include) {
          clause.push_back(literal);
        }
      } else {
        auto metastatement = std::get_if<Metastatement>(&literal);
        if (metastatement && metastatement->type == MetastatementType::labelEquality) {
          clause.push_back(literal);
        }
      }
    }
  }
  return dnf;
};
*/
