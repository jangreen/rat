#pragma once
#include <map>
#include <unordered_set>
#include <valarray>

#include "Preprocessing.h"
#include "Stats.h"
#include "basic/Literal.h"
#include "utility.h"

typedef std::map<CanonicalRelation, std::unordered_set<CanonicalRelation>> CanonicalParents;
typedef std::map<std::string, std::vector<CanonicalRelation>> ReplaceMap;

inline void updateParentMap(const CanonicalRelation relation, CanonicalParents& parentMap) {
  switch (relation->operation) {
    case RelationOperation::relationIntersection:
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

inline void updateParentMap(const CanonicalSet set, CanonicalParents& parentMap) {
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

inline ReplaceMap greatestCommonConjunctiveContext(const Cube& goal) {
  auto positiveGoal =
      goal | std::views::filter([](const Literal& literal) { return !literal.negated; });

  // parentMap: maps expressions to their contexts
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
      while (parentMap.contains(curRelation) && parentMap.at(curRelation).size() == 1) {
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

inline void eleminateRedundantConjunctiveContexts(Cube& goal) {
  const auto commonContexts = greatestCommonConjunctiveContext(goal);

  auto negatedGoal = goal | std::views::filter(&Literal::negated);
  for (auto& literal : negatedGoal) {
    if (literal.operation == PredicateOperation::setNonEmptiness) {
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
  }
}

inline void preprocessing(Cube& goal) {
  eleminateRedundantConjunctiveContexts(goal);
  spdlog::info("[Status] Preprocesing done.");
}