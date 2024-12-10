#include "Preprocessing.h"

#include <spdlog/spdlog.h>

#include "Assumption.h"

void Preprocessing::updateParentMap(const CanonicalRelation relation, CanonicalParents& parentMap) {
  switch (relation->operation) {
    case RelationOperation::relationIntersection:
      parentMap[relation->leftOperand].insert(relation);
      parentMap[relation->rightOperand].insert(relation);

      updateParentMap(relation->leftOperand, parentMap);
      updateParentMap(relation->rightOperand, parentMap);
      return;
    case RelationOperation::composition:
      // only add non binary children
      if (relation->leftOperand->operation == RelationOperation::setIdentity ||
          relation->rightOperand->operation == RelationOperation::setIdentity) {
        parentMap[relation->leftOperand].insert(relation);
        parentMap[relation->rightOperand].insert(relation);
      }

      updateParentMap(relation->leftOperand, parentMap);
      updateParentMap(relation->rightOperand, parentMap);
      return;
    case RelationOperation::relationUnion:
      // only add conjunctive parents
      if (relation->leftOperand->operation == RelationOperation::setIdentity &&
          relation->rightOperand->operation == RelationOperation::setIdentity) {
        parentMap[relation->leftOperand].insert(relation);
        parentMap[relation->rightOperand].insert(relation);
      }

      updateParentMap(relation->leftOperand, parentMap);
      updateParentMap(relation->rightOperand, parentMap);
      return;
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      updateParentMap(relation->leftOperand, parentMap);
      return;
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    case RelationOperation::setIdentity:
      return;
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}
void Preprocessing::updateParentMap(const CanonicalSet set, CanonicalParents& parentMap) {
  switch (set->operation) {
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::fullSet:
    case SetOperation::emptySet:
      return;
    case SetOperation::image:
    case SetOperation::domain:
      updateParentMap(set->relation, parentMap);
      updateParentMap(set->leftOperand, parentMap);
      return;
    case SetOperation::setIntersection:
    case SetOperation::setUnion:
      updateParentMap(set->leftOperand, parentMap);
      updateParentMap(set->rightOperand, parentMap);
      return;
    default:
      throw std::logic_error("unreachable");
  }
}
bool Preprocessing::isPure(CanonicalRelation relation) {
  switch (relation->operation) {
    case RelationOperation::relationUnion:
    case RelationOperation::composition:
    case RelationOperation::relationIntersection:
      return isPure(relation->leftOperand) && isPure(relation->rightOperand);
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      return isPure(relation->leftOperand);
    case RelationOperation::baseRelation:
      return !Assumption::baseAssumptions.contains(relation->identifier.value());
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    case RelationOperation::setIdentity:
      return true;
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}
ReplaceMap Preprocessing::greatestCommonConjunctiveContext(const Cube& goal) {
  auto positiveGoal =
      goal | std::views::filter([](const Literal& literal) { return !literal.negated; });

  // parentMap: maps expressions to their conjunctive contexts
  CanonicalParents parentMap;
  for (const auto& literal : positiveGoal) {
    if (literal.operation == PredicateOperation::setNonEmptiness) {
      updateParentMap(literal.set, parentMap);
    }
  }

  // uncomment for printing
  // print(goal);
  // for (const auto& [relation, parents] : parentMap) {
  //   std::cout << relation->toString() << " -> ";
  //   for (const auto parent : parents) {
  //     std::cout << parent->toString() << "   ";
  //   }
  //   std::cout << std::endl;
  // }

  // for each base relation find greatest common context
  ReplaceMap commonContexts;
  for (const auto& relation : parentMap | std::views::keys) {
    if (relation->operation == RelationOperation::baseRelation) {
      auto curRelation = relation;
      while (parentMap.contains(curRelation) && parentMap.at(curRelation).size() == 1 &&
             isPure(curRelation)) {
        curRelation = *parentMap.at(curRelation).begin();
        commonContexts[relation->identifier.value()].push_back(curRelation);
      }
      // reverse order: i.e. try to replace (([W];co-typed);[W]) before ([W];co-typed)
      if (commonContexts.contains(relation->identifier.value())) {
        std::ranges::reverse(commonContexts.at(relation->identifier.value()));
      }
    }
  }

  // uncomment to print commonContexts
  // for (const auto& [baseRelation, replacedRelations] : commonContexts) {
  //   for (const auto replaceRelation : replacedRelations) {
  //     std::cout << replaceRelation->toString() << std::endl;
  //   }
  // }

  return commonContexts;
}
void Preprocessing::eleminateRedundantConjunctiveContexts(Literal& literal,
                                                          const ReplaceMap& commonContexts) {
  for (const auto& [baseRelation, replacedRelations] : commonContexts) {
    for (const auto replaceRelation : replacedRelations) {
      const auto result =
          literal.substituteAll(replaceRelation, Relation::newBaseRelation(baseRelation));
      if (result) {
        literal = result.value();
      }
      Stats::boolean("#reduced literals - preprocessing").count(result.has_value());
    }
  }
}
void Preprocessing::eleminateRedundantConjunctiveContexts(Cube& goal) {
  const auto commonContexts = greatestCommonConjunctiveContext(goal);

  auto negatedGoal = goal | std::views::filter(&Literal::negated);
  for (auto& literal : negatedGoal) {
    if (literal.operation == PredicateOperation::setNonEmptiness) {
      eleminateRedundantConjunctiveContexts(literal, commonContexts);
    }
  }

  // TODO: reduce also assumptions
  // for (auto& assumption : Assumption::baseAssumptions) {
  //   eleminateRedundantConjunctiveContexts()
  // }
}
void Preprocessing::getBase(CanonicalRelation relation,
                            std::unordered_set<CanonicalExpression>& baseExpresssions) {
  switch (relation->operation) {
    case RelationOperation::relationUnion:
    case RelationOperation::composition:
    case RelationOperation::relationIntersection:
      getBase(relation->leftOperand, baseExpresssions);
      getBase(relation->rightOperand, baseExpresssions);
      return;
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      getBase(relation->leftOperand, baseExpresssions);
      return;
    case RelationOperation::baseRelation:
      baseExpresssions.insert(relation);
      return;
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      return;
    case RelationOperation::setIdentity:
      getBase(relation->set, baseExpresssions);
      return;
    case RelationOperation::cartesianProduct:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}
void Preprocessing::getBase(CanonicalSet set,
                            std::unordered_set<CanonicalExpression>& baseExpresssions) {
  switch (set->operation) {
    case SetOperation::baseSet:
      baseExpresssions.insert(set);
      return;
    case SetOperation::event:
    case SetOperation::fullSet:
    case SetOperation::emptySet:
      return;
    case SetOperation::image:
    case SetOperation::domain:
      getBase(set->leftOperand, baseExpresssions);
      getBase(set->relation, baseExpresssions);
      return;
    case SetOperation::setIntersection:
    case SetOperation::setUnion:
      assert(set->leftOperand != nullptr);
      getBase(set->leftOperand, baseExpresssions);
      getBase(set->rightOperand, baseExpresssions);
      return;
    default:
      throw std::logic_error("unreachable");
  }
}
std::unordered_set<CanonicalExpression> Preprocessing::nonEmptyExpressions(const Cube& goal) {
  auto positiveGoal =
      goal | std::views::filter([](const Literal& literal) { return !literal.negated; });

  // parentMap: maps expressions to their conjunctive contexts
  std::unordered_set<CanonicalExpression> nonEmpty;
  for (const auto& literal : positiveGoal) {
    if (literal.operation == PredicateOperation::setNonEmptiness) {
      assert(literal.validate());
      getBase(literal.set, nonEmpty);
    }
  }

  return nonEmpty;
}
void Preprocessing::replaceEmptyExpressionsInNegatedLiterals(
    Literal& literal, const std::unordered_set<CanonicalExpression>& nonEmpty) {
  std::unordered_set<CanonicalExpression> literalBase;
  getBase(literal.set, literalBase);

  for (const auto& expression : literalBase) {
    if (std::holds_alternative<CanonicalRelation>(expression)) {
      const auto relation = std::get<CanonicalRelation>(expression);
      if (!nonEmpty.contains(relation)) {
        // std::cout << "Replace: " << relation->toString() << std::endl;
        const auto result = literal.substituteAll(relation, Relation::emptyRelation());
        if (result) {
          // std::cout << "       " << literal.toString() << "\n    -> " <<
          // result.value().toString()
          //           << std::endl;
          literal = result.value();
        }
        Stats::boolean("#reduced literals - preprocessing empty").count(result.has_value());
      }
    } else {
      const auto set = std::get<CanonicalSet>(expression);
      if (!nonEmpty.contains(set)) {
        // std::cout << "Replace: " << set->toString() << std::endl;
        const auto result = literal.substituteAll(set, Set::emptySet());
        if (result) {
          // std::cout << "       " << literal.toString() << "\n    -> " <<
          // result.value().toString()
          //           << std::endl;
          literal = result.value();
        }
        Stats::boolean("#reduced literals - preprocessing empty").count(result.has_value());
      }
    }
  }
}
void Preprocessing::replaceEmptyExpressionsInNegatedLiterals(Cube& goal) {
  auto nonEmpty = nonEmptyExpressions(goal);

  // insert all base relations that are on right hand side
  for (const auto& [baseIdentifier, _] : Assumption::baseAssumptions) {
    nonEmpty.insert(Relation::newBaseRelation(baseIdentifier));
  }
  for (const auto& [baseIdentifier, _] : Assumption::baseSetAssumptions) {
    nonEmpty.insert(Set::newBaseSet(baseIdentifier));
  }

  // uncomment for printing
  // for (const auto& expression : nonEmpty) {
  //   if (std::holds_alternative<CanonicalRelation>(expression)) {
  //     const auto relation = std::get<CanonicalRelation>(expression);
  //     std::cout << relation->toString() << std::endl;
  //   } else {
  //     const auto set = std::get<CanonicalSet>(expression);
  //     std::cout << set->toString() << std::endl;
  //   }
  // }

  auto negatedGoal = goal | std::views::filter(&Literal::negated);
  for (auto& literal : negatedGoal) {
    if (literal.operation == PredicateOperation::setNonEmptiness) {
      replaceEmptyExpressionsInNegatedLiterals(literal, nonEmpty);
    }
  }
}
void Preprocessing::preprocessing(Cube& goal) {
  eleminateRedundantConjunctiveContexts(goal);

  replaceEmptyExpressionsInNegatedLiterals(goal);
  spdlog::info("[Status] Preprocesing done.");
}