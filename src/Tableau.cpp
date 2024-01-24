#include "Tableau.h"

#include <algorithm>
#include <iostream>
#include <tuple>
#include <utility>

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

    if (currentNode->formula.isPositiveEqualityPredicate()) {
      Predicate equalityPredicate = *currentNode->formula.literal->predicate;
      std::optional<Set> search, replace;
      if (*equalityPredicate.leftOperand->label < *equalityPredicate.rightOperand->label) {
        // e1 = e2 , e1 < e2
        search = *equalityPredicate.rightOperand;
        replace = *equalityPredicate.leftOperand;
      } else {
        search = *equalityPredicate.leftOperand;
        replace = *equalityPredicate.rightOperand;
      }

      Node *temp = currentNode->parentNode;
      while (temp != nullptr) {
        // TODO: must replace in formulas?
        if (temp->formula.operation == FormulaOperation::literal) {
          // check if inside formula can be something inferred
          auto newLiterals = substitute(*temp->formula.literal, *search, *replace);
          for (auto &literal : newLiterals) {
            currentNode->appendBranch(Formula(FormulaOperation::literal, std::move(literal)));
          }
        }
        temp = temp->parentNode;
      }
    }
#if DEBUG
    exportProof("infinite-debug");
#endif
  }

  // warning if bound is reached
  if (bound == 0 && !unreducedNodes.empty()) {
    std::cout << "[Warning] Configured bound is reached. Answer is imprecise." << std::endl;
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

      // currently remove currentNode by replacing it with dummy
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

GDNF Tableau::dnf() {
  solve();
  return rootNode->extractDNF();
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
