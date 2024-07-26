#include "LogicVisitor.h"

#include <any>
#include <iostream>

#include "../Assumption.h"
#include "../basic/Annotated.h"
#include "../regularTableau/RegularTableau.h"

/*DNF*/ std::any Logic::visitProof(LogicParser::ProofContext *context) {
  DNF assertionCubes;

  for (const auto statementContext : context->statement()) {
    if (statementContext->letDefinition()) {
      visitLetDefinition(statementContext->letDefinition());
    } else if (statementContext->inclusion()) {
      visitInclusion(statementContext->inclusion());
    } else if (statementContext->hypothesis()) {
      visitHypothesis(statementContext->hypothesis());
    } else if (statementContext->assertion()) {
      auto untypedCube = visitAssertion(statementContext->assertion());
      auto cube = std::any_cast<Cube>(untypedCube);
      assertionCubes.push_back(cube);
    }
  }

  // process assumptions
  // emptiness r = 0 |- r1 <= r2 iff |- r1 <= r2 + T.r.T
  for (auto &cube : assertionCubes) {
    for (const auto &assumption : Assumption::emptinessAssumptions) {
      const CanonicalSet fullSet = Set::fullSet();
      const CanonicalSet rT = Set::newSet(SetOperation::domain, fullSet, assumption.relation);
      const CanonicalSet TrT = Set::newSet(SetOperation::setIntersection, fullSet, rT);
      cube.emplace_back(Annotated::makeWithValue(TrT, {0, 0}));  // T & r.T
    }
  }

  return assertionCubes;
}
/*void*/ std::any Logic::visitInclusion(LogicParser::InclusionContext *ctx) {
  std::ignore = parseMemoryModel(ctx->FILEPATH()->getText());
  return 0;
}
/*Cube*/ std::any Logic::visitAssertion(LogicParser::AssertionContext *context) {
  if (!context->INEQUAL()) {
    spdlog::error("[Parser] Unsupported assertion format.");
    exit(0);
  }

  const auto lhs = parseExpression(context->e1->getText());
  const auto rhs = parseExpression(context->e2->getText());
  const bool sameType =
      std::holds_alternative<CanonicalSet>(lhs) == std::holds_alternative<CanonicalSet>(rhs);
  if (!sameType) {
    spdlog::error("[Parser] Type mismatch in assertion.");
    exit(0);
  }

  const bool isSetAssertion = std::holds_alternative<CanonicalSet>(lhs);
  if (isSetAssertion) {
    const auto lhSet = std::get<CanonicalSet>(lhs);
    const auto rhSet = std::get<CanonicalSet>(rhs);
    const auto rhSetAnnotated = Annotated::makeWithValue(rhSet, {0, 0});
    return Cube{Literal(lhSet), Literal(rhSetAnnotated)};
  }

  // relation assertion
  const auto lhRelation = std::get<CanonicalRelation>(lhs);
  const auto rhRelation = std::get<CanonicalRelation>(rhs);
  const CanonicalSet e1 = Set::freshEvent();
  const CanonicalSet e2 = Set::freshEvent();
  const CanonicalSet e1LHS = Set::newSet(SetOperation::image, e1, lhRelation);
  const CanonicalSet e1RHS = Set::newSet(SetOperation::image, e1, rhRelation);

  const CanonicalSet e1LHS_and_e2 = Set::newSet(SetOperation::setIntersection, e1LHS, e2);
  const CanonicalSet e1RHS_and_e2 = Set::newSet(SetOperation::setIntersection, e1RHS, e2);
  const auto e1RHS_and_e2_annotated = Annotated::makeWithValue(e1RHS_and_e2, {0, 0});
  return Cube{Literal(e1LHS_and_e2), Literal(e1RHS_and_e2_annotated)};
}

/*void*/ std::any Logic::visitHypothesis(LogicParser::HypothesisContext *ctx) {
  const CanonicalRelation lhs = parseRelation(ctx->lhs->getText());
  const CanonicalRelation rhs = parseRelation(ctx->rhs->getText());
  switch (rhs->operation) {
    case RelationOperation::baseRelation: {
      Assumption assumption(lhs, *rhs->identifier);
      Assumption::baseAssumptions.emplace(*assumption.baseRelation, assumption);
      return 0;
    }
    case RelationOperation::emptyRelation: {
      Assumption::emptinessAssumptions.emplace_back(lhs);
      return 0;
    }
    case RelationOperation::idRelation: {
      Assumption::idAssumptions.emplace_back(lhs);
      return 0;
    }
    default:
      std::cout << "[Parser] Unsupported hypothesis:" << ctx->lhs->getText()
                << " <= " << ctx->rhs->getText() << std::endl;
      throw std::runtime_error("");
      // throw std::format_error(""); Unsupported in some compilers
  }
}

/*std::vector<Constraint>*/ std::any Logic::visitMcm(LogicParser::McmContext *context) {
  std::vector<Constraint> constraints;

  for (const auto definitionContext : context->definition()) {
    if (definitionContext->letDefinition()) {
      visitLetDefinition(definitionContext->letDefinition());
    } else if (definitionContext->letRecDefinition()) {
      std::cout << "[Parser] Recursive definitions are not supported." << std::endl;
      exit(0);
    } else if (definitionContext->axiomDefinition()) {
      auto untypedAxiom = visitAxiomDefinition(definitionContext->axiomDefinition());
      auto axiom = std::any_cast<Constraint>(untypedAxiom);
      constraints.push_back(axiom);
    }
  }
  return constraints;
}
// use default implementation of visitDefinition(LogicParser::DefinitionContext*context)
/*Constraint*/ std::any Logic::visitAxiomDefinition(LogicParser::AxiomDefinitionContext *context) {
  std::string name;
  if (context->RELNAME()) {
    name = context->RELNAME()->getText();
  } else {
    name = context->getText();
  }
  ConstraintType type;
  if (context->EMPTY()) {
    type = ConstraintType::empty;
  } else if (context->IRREFLEXIVE()) {
    type = ConstraintType::irreflexive;
  } else if (context->ACYCLIC()) {
    type = ConstraintType::acyclic;
  } else {
    throw std::logic_error("unreachable");
    assert(false);  // Exhaustive cases
  }
  const auto relationVariant = std::any_cast<CanonicalExpression>(context->e->accept(this));
  const auto relation = std::get<CanonicalRelation>(relationVariant);
  return Constraint(type, relation, name);
}
/*void*/ std::any Logic::visitLetDefinition(LogicParser::LetDefinitionContext *context) {
  const auto expr = std::any_cast<CanonicalExpression>(context->e->accept(this));
  if (context->RELNAME()) {
    const std::string name = context->RELNAME()->getText();
    const auto derivedRelation = std::get<CanonicalRelation>(expr);
    // overwrite existing definition
    derivedRelations.insert_or_assign(name, derivedRelation);
  } else if (context->SETNAME()) {
    const std::string name = context->SETNAME()->getText();
    const auto derivedSet = std::get<CanonicalSet>(expr);
    // overwrite existing definition
    derivedSets.insert_or_assign(name, derivedSet);
  }
  return 0;
}
/*void*/ std::any Logic::visitLetRecDefinition(LogicParser::LetRecDefinitionContext *context) {
  std::cout << "[Parser] Recursive definitions are currently not supported." << std::endl;
  exit(0);
}
/*void*/ std::any Logic::visitLetRecAndDefinition(
    LogicParser::LetRecAndDefinitionContext *context) {
  std::cout << "[Parser] Recursive definitions are currently not supported." << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitParentheses(LogicParser::ParenthesesContext *context) {
  // process: (e)
  auto expression = std::any_cast<CanonicalExpression>(context->e1->accept(this));
  return expression;
}
/*CanonicalExpression*/ std::any Logic::visitTransitiveClosure(
    LogicParser::TransitiveClosureContext *context) {
  const auto relationExpression = std::any_cast<CanonicalExpression>(context->e->accept(this));

  if (std::holds_alternative<CanonicalRelation>(relationExpression)) {
    const auto r = std::get<CanonicalRelation>(relationExpression);
    const CanonicalRelation rStar = Relation::newRelation(RelationOperation::transitiveClosure, r);
    const CanonicalRelation rrStar =
        Relation::newRelation(RelationOperation::composition, r, rStar);
    CanonicalExpression result = rrStar;
    return result;
  }
  std::cout << "[Parser] Type mismatch of the operand of the relation transitive closure."
            << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitRelationFencerel(
    LogicParser::RelationFencerelContext *context) {
  const auto relationName = context->n->getText();
  if (derivedRelations.contains(relationName)) {
    const auto r = derivedRelations.at(relationName);
    const CanonicalRelation po = Relation::newBaseRelation("po");
    const CanonicalRelation po_r = Relation::newRelation(RelationOperation::composition, po, r);
    const CanonicalRelation po_r_po =
        Relation::newRelation(RelationOperation::composition, po_r, po);
    CanonicalExpression result = po_r_po;
    return result;
  }
  std::cout << "[Parser] Error: fencerel() of unknown relation." << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitSetSingleton(
    LogicParser::SetSingletonContext *context) {
  std::string name = context->SETNAME()->getText();
  if (!definedSingletons.contains(name)) {
    definedSingletons.insert({name, Set::freshEvent()});
  }
  return definedSingletons.at(name);
}
/*CanonicalExpression*/ std::any Logic::visitRelationBasic(
    LogicParser::RelationBasicContext *context) {
  const std::string name = context->RELNAME()->getText();
  if (name == "id") {
    CanonicalExpression result = Relation::idRelation();
    return result;
  }
  if (name == "0") {
    CanonicalExpression result = Relation::emptyRelation();
    return result;
  }
  if (derivedRelations.contains(name)) {
    CanonicalExpression result = derivedRelations.at(name);
    return result;
  }
  CanonicalExpression result = Relation::newBaseRelation(name);
  return result;
}
/*CanonicalExpression*/ std::any Logic::visitRelationMinus(
    LogicParser::RelationMinusContext *context) {
  std::cout << "[Parser] Setminus operation is not supported." << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitRelationDomainIdentity(
    LogicParser::RelationDomainIdentityContext *context) {
  std::cout << "[Parser] Domain identity expressions are not supported." << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitRelationRangeIdentity(
    LogicParser::RelationRangeIdentityContext *context) {
  std::cout << "[Parser] Range identity expressions are not supported." << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitUnion(LogicParser::UnionContext *context) {
  const auto e1 = std::any_cast<CanonicalExpression>(context->e1->accept(this));
  const auto e2 = std::any_cast<CanonicalExpression>(context->e2->accept(this));
  if (std::holds_alternative<CanonicalRelation>(e1) &&
      std::holds_alternative<CanonicalRelation>(e2)) {
    const auto &r1 = std::get<CanonicalRelation>(e1);
    const auto &r2 = std::get<CanonicalRelation>(e2);
    const CanonicalRelation r1_or_r2 =
        Relation::newRelation(RelationOperation::relationUnion, r1, r2);
    CanonicalExpression result = r1_or_r2;
    return result;
  }

  if (std::holds_alternative<CanonicalSet>(e1) && std::holds_alternative<CanonicalSet>(e2)) {
    const auto &s1 = std::get<CanonicalSet>(e1);
    const auto &s2 = std::get<CanonicalSet>(e2);
    const auto s1_or_s2 = Set::newSet(SetOperation::setUnion, s1, s2);
    CanonicalExpression result = s1_or_s2;
    return result;
  }
  std::cout << "[Parser] Type mismatch of two operands of the union operator." << std::endl;
  exit(0);
}

/*CanonicalExpression*/ std::any Logic::visitEmptyset(LogicParser::EmptysetContext *ctx) {
  const CanonicalRelation r = Relation::emptyRelation();
  CanonicalExpression result = r;
  return result;
}

/*CanonicalExpression*/ std::any Logic::visitRelationInverse(
    LogicParser::RelationInverseContext *context) {
  auto relationExpression = std::any_cast<CanonicalExpression>(context->e->accept(this));
  if (std::holds_alternative<CanonicalRelation>(relationExpression)) {
    const auto r = std::get<CanonicalRelation>(relationExpression);
    const auto rInv = Relation::newRelation(RelationOperation::converse, r);
    CanonicalExpression result = rInv;
    return result;
  }
  std::cout << "[Parser] Type mismatch of the operand of the relation inverse." << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitRelationOptional(
    LogicParser::RelationOptionalContext *context) {
  const auto relationExpression = std::any_cast<CanonicalExpression>(context->e->accept(this));
  if (std::holds_alternative<CanonicalRelation>(relationExpression)) {
    const auto r = std::get<CanonicalRelation>(relationExpression);
    const auto id = Relation::idRelation();
    const auto r_or_id = Relation::newRelation(RelationOperation::relationUnion, r, id);
    CanonicalExpression result = r_or_id;
    return result;
  }
  std::cout << "[Parser] Type mismatch of the operand of the relation optional." << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitRelationIdentity(
    LogicParser::RelationIdentityContext *context) {
  if (context->TOID() == nullptr) {
    const auto setExpression = std::any_cast<CanonicalExpression>(context->e->accept(this));
    const auto set = std::get<CanonicalSet>(setExpression);
    CanonicalExpression result = Relation::setIdentity(set);
    return result;
  }

  std::cout << "[Parser] 'visitExprIdentity TOID' expressions are not supported." << std::endl;
  exit(1);
}
/*CanonicalExpression*/ std::any Logic::visitCartesianProduct(
    LogicParser::CartesianProductContext *context) {
  // treat cartesian product as binary base relation
  const std::string r1 = context->e1->getText();
  const std::string r2 = context->e2->getText();
  const CanonicalRelation r = Relation::newBaseRelation(r1 + "*" + r2);
  CanonicalExpression result = r;
  return result;
}
/*CanonicalExpression*/ std::any Logic::visitSetBasic(LogicParser::SetBasicContext *context) {
  const std::string name = context->SETNAME()->getText();
  if (name == "E") {
    CanonicalExpression result = Set::fullSet();
    return result;
  }
  if (name == "0") {
    CanonicalExpression result = Set::emptySet();
    return result;
  }
  if (derivedSets.contains(name)) {
    CanonicalExpression result = derivedSets.at(name);
    return result;
  }
  CanonicalExpression result = Set::newBaseSet(name);
  return result;
}
/*CanonicalExpression*/ std::any Logic::visitTransitiveReflexiveClosure(
    LogicParser::TransitiveReflexiveClosureContext *context) {
  const auto e = std::any_cast<CanonicalExpression>(context->e->accept(this));
  if (std::holds_alternative<CanonicalRelation>(e)) {
    const CanonicalRelation r =
        Relation::newRelation(RelationOperation::transitiveClosure, std::get<CanonicalRelation>(e));
    CanonicalExpression result = r;
    return result;
  }
  std::cout << "[Parser] Type mismatch of operand of the Kleene operator: " << context->getText()
            << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitComposition(LogicParser::CompositionContext *context) {
  const auto e1 = std::any_cast<CanonicalExpression>(context->e1->accept(this));
  const auto e2 = std::any_cast<CanonicalExpression>(context->e2->accept(this));

  if (std::holds_alternative<CanonicalRelation>(e1) &&
      std::holds_alternative<CanonicalRelation>(e2)) {
    const auto r1 = std::get<CanonicalRelation>(e1);
    const auto r2 = std::get<CanonicalRelation>(e2);
    const auto r1_r2 = Relation::newRelation(RelationOperation::composition, r1, r2);
    CanonicalExpression result = r1_r2;
    return result;
  }
  if (std::holds_alternative<CanonicalSet>(e1) && std::holds_alternative<CanonicalRelation>(e2)) {
    const auto s = std::get<CanonicalSet>(e1);
    const auto r = std::get<CanonicalRelation>(e2);
    const auto sr = Set::newSet(SetOperation::image, s, r);
    CanonicalExpression result = sr;
    return result;
  }
  if (std::holds_alternative<CanonicalRelation>(e1) && std::holds_alternative<CanonicalSet>(e2)) {
    const CanonicalRelation r = std::get<CanonicalRelation>(e1);
    const CanonicalSet s = std::get<CanonicalSet>(e2);
    const CanonicalSet rs = Set::newSet(SetOperation::domain, s, r);
    CanonicalExpression result = rs;
    return result;
  }
  std::cout << "[Parser] Type mismatch of two operands of the composition operator: "
            << context->getText() << std::endl;
  exit(0);
}
/*CanonicalExpression*/ std::any Logic::visitIntersection(
    LogicParser::IntersectionContext *context) {
  const auto e1 = std::any_cast<CanonicalExpression>(context->e1->accept(this));
  const auto e2 = std::any_cast<CanonicalExpression>(context->e2->accept(this));
  if (std::holds_alternative<CanonicalRelation>(e1) &&
      std::holds_alternative<CanonicalRelation>(e2)) {
    const auto r1 = std::get<CanonicalRelation>(e1);
    const auto r2 = std::get<CanonicalRelation>(e2);
    const auto r1_and_r2 = Relation::newRelation(RelationOperation::relationIntersection, r1, r2);
    CanonicalExpression result = r1_and_r2;
    return result;
  }

  if (std::holds_alternative<CanonicalSet>(e1) && std::holds_alternative<CanonicalSet>(e2)) {
    const CanonicalSet s1 = std::get<CanonicalSet>(e1);
    const CanonicalSet s2 = std::get<CanonicalSet>(e2);
    const CanonicalSet s1_and_s2 = Set::newSet(SetOperation::setIntersection, s1, s2);
    CanonicalExpression result = s1_and_s2;
    return result;
  }
  std::cout << "[Parser] Type mismatch of two operands of the intersection operator." << std::endl;
  exit(0);
}
// /*CanonicalExpression*/ std::any
// Logic::visitCanonicalRelationComplement(
//     LogicParser::CanonicalRelationComplementContext *context) {
//   std::cout << "[Parser] Complement operation is not supported." << std::endl;
//   exit(0);
// }

std::unordered_map<std::string, CanonicalRelation> Logic::derivedRelations;
std::unordered_map<std::string, CanonicalSet> Logic::derivedSets;
std::unordered_map<std::string, CanonicalSet> Logic::definedSingletons;