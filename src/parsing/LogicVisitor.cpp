#include "LogicVisitor.h"

#include "../RegularTableau.h"

/*DNF*/ std::any Logic::visitProof(LogicParser::ProofContext *context) {
  DNF assertionCubes;

  for (auto statementContext : context->statement()) {
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
      CanonicalSet fullSet = Set::newSet(SetOperation::full);
      CanonicalSet rT = Set::newSet(SetOperation::domain, fullSet, assumption.relation);
      CanonicalSet TrT = Set::newSet(SetOperation::intersection, fullSet, rT);
      Literal l(true, TrT);
      cube.push_back(l);
    }
  }

  return assertionCubes;
}
/*void*/ std::any Logic::visitInclusion(LogicParser::InclusionContext *context) {
  auto _ = Logic::parseMemoryModel(context->FILEPATH()->getText());

  return 0;
}
/*Cube*/ std::any Logic::visitAssertion(LogicParser::AssertionContext *context) {
  if (context->INEQUAL()) {
    // currently only support relations on each side of assertion
    CanonicalRelation lhs = parseRelation(context->e1->getText());
    CanonicalRelation rhs = parseRelation(context->e2->getText());
    CanonicalSet e1 = Set::newEvent(Set::maxSingletonLabel++);
    CanonicalSet e2 = Set::newEvent(Set::maxSingletonLabel++);
    CanonicalSet e1LHS = Set::newSet(SetOperation::image, e1, lhs);
    CanonicalSet e1RHS = Set::newSet(SetOperation::image, e1, rhs);

    CanonicalSet e1LHS_and_e2 = Set::newSet(SetOperation::intersection, e1LHS, e2);
    CanonicalSet e1RHS_and_e2 = Set::newSet(SetOperation::intersection, e1RHS, e2);

    Cube cube = {Literal(false, e1LHS_and_e2), Literal(true, e1RHS_and_e2)};
    return cube;
  }
  spdlog::error("[Parser] Unsupported assertion format.");
  exit(0);
}

/*void*/ std::any Logic::visitHypothesis(LogicParser::HypothesisContext *context) {
  CanonicalRelation lhs = parseRelation(context->lhs->getText());
  CanonicalRelation rhs = parseRelation(context->rhs->getText());
  switch (rhs->operation) {
    case RelationOperation::base: {
      Assumption assumption(lhs, *rhs->identifier);
      Assumption::baseAssumptions.emplace(*assumption.baseRelation, assumption);
      return 0;
    }
    case RelationOperation::empty: {
      Assumption::emptinessAssumptions.emplace_back(lhs);
      return 0;
    }
    case RelationOperation::identity: {
      Assumption::idAssumptions.emplace_back(lhs);
      return 0;
    }
    default: {
      std::cout << "[Parser] Ignoring unsupported hypothesis:" << context->lhs->getText()
                << " <= " << context->rhs->getText() << std::endl;
      return 0;
    }
  }
}

/*std::vector<Constraint>*/ std::any Logic::visitMcm(LogicParser::McmContext *context) {
  std::vector<Constraint> constraints;

  for (auto definitionContext : context->definition()) {
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
    assert(false);  // Exhaustive cases
  }
  auto relationVariant =
      std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e->accept(this));
  CanonicalRelation relation = std::get<CanonicalRelation>(relationVariant);
  return Constraint(type, relation, name);
}
/*void*/ std::any Logic::visitLetDefinition(LogicParser::LetDefinitionContext *context) {
  if (context->RELNAME()) {
    std::string name = context->RELNAME()->getText();
    auto derivedRelationVariant =
        std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e->accept(this));
    CanonicalRelation derivedRelation = std::get<CanonicalRelation>(derivedRelationVariant);
    // overwrite existing definition
    Logic::definedRelations.insert_or_assign(name, derivedRelation);
  } else if (context->SETNAME()) {
    std::string name = context->SETNAME()->getText();
    // TODO: implement
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
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitParentheses(
    LogicParser::ParenthesesContext *context) {
  // process: (e)
  auto expression =
      std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e1->accept(this));
  return expression;
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitTransitiveClosure(
    LogicParser::TransitiveClosureContext *context) {
  auto relationExpression =
      std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e->accept(this));

  if (std::holds_alternative<CanonicalRelation>(relationExpression)) {
    CanonicalRelation r = std::get<CanonicalRelation>(relationExpression);
    CanonicalRelation rStar = Relation::newRelation(RelationOperation::transitiveClosure, r);
    CanonicalRelation rrStar = Relation::newRelation(RelationOperation::composition, r, rStar);
    std::variant<CanonicalSet, CanonicalRelation> result = rrStar;
    return result;
  }
  std::cout << "[Parser] Type mismatch of the operand of the relation transitive closure."
            << std::endl;
  exit(0);
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitRelationFencerel(
    LogicParser::RelationFencerelContext *context) {
  auto relationName = context->n->getText();
  if (Logic::definedRelations.contains(relationName)) {
    auto r = Logic::definedRelations.at(relationName);
    CanonicalRelation po = Relation::newBaseRelation("po");
    CanonicalRelation po_r = Relation::newRelation(RelationOperation::composition, po, r);
    CanonicalRelation po_r_po = Relation::newRelation(RelationOperation::composition, po_r, po);
    std::variant<CanonicalSet, CanonicalRelation> result = po_r_po;
    return result;
  }
  std::cout << "[Parser] Error: fencerel() of unknown relation." << std::endl;
  exit(0);
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitSetSingleton(
    LogicParser::SetSingletonContext *context) {
  std::string name = context->SETNAME()->getText();
  int label;
  if (definedSingletons.contains(name)) {
    label = definedSingletons.at(name);
  } else {
    label = Set::maxSingletonLabel++;
    definedSingletons.insert({name, label});
  }
  CanonicalSet s = Set::newEvent(label);
  std::variant<CanonicalSet, CanonicalRelation> result = s;
  return result;
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitRelationBasic(
    LogicParser::RelationBasicContext *context) {
  std::string name = context->RELNAME()->getText();
  if (name == "id") {
    std::variant<CanonicalSet, CanonicalRelation> result =
        Relation::newRelation(RelationOperation::identity);
    return result;
  }
  if (name == "0") {
    std::variant<CanonicalSet, CanonicalRelation> result =
        Relation::newRelation(RelationOperation::empty);
    return result;
  }
  if (Logic::definedRelations.contains(name)) {
    std::variant<CanonicalSet, CanonicalRelation> result = Logic::definedRelations.at(name);
    return result;
  }
  CanonicalRelation r = Relation::newBaseRelation(name);
  std::variant<CanonicalSet, CanonicalRelation> result = r;
  return result;
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitRelationMinus(
    LogicParser::RelationMinusContext *context) {
  std::cout << "[Parser] Setminus operation is not supported." << std::endl;
  exit(0);
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitRelationDomainIdentity(
    LogicParser::RelationDomainIdentityContext *context) {
  std::cout << "[Parser] Domain identity expressions are not supported." << std::endl;
  exit(0);
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitRelationRangeIdentity(
    LogicParser::RelationRangeIdentityContext *context) {
  std::cout << "[Parser] Range identity expressions are not supported." << std::endl;
  exit(0);
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitUnion(
    LogicParser::UnionContext *context) {
  auto e1 = std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e1->accept(this));
  auto e2 = std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e2->accept(this));
  if (std::holds_alternative<CanonicalRelation>(e1) &&
      std::holds_alternative<CanonicalRelation>(e2)) {
    CanonicalRelation r1 = std::get<CanonicalRelation>(e1);
    CanonicalRelation r2 = std::get<CanonicalRelation>(e2);
    CanonicalRelation r1_or_r2 = Relation::newRelation(RelationOperation::choice, r1, r2);
    std::variant<CanonicalSet, CanonicalRelation> result = r1_or_r2;
    return result;
  }

  if (std::holds_alternative<CanonicalSet>(e1) && std::holds_alternative<CanonicalSet>(e2)) {
    CanonicalSet s1 = std::get<CanonicalSet>(e1);
    CanonicalSet s2 = std::get<CanonicalSet>(e2);
    CanonicalSet s1_or_s2 = Set::newSet(SetOperation::choice, s1, s2);
    std::variant<CanonicalSet, CanonicalRelation> result = s1_or_s2;
    return result;
  }
  std::cout << "[Parser] Type mismatch of two operands of the union operator." << std::endl;
  exit(0);
}

// TODO: emptyset as set type is not supported
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitEmptyset(
    LogicParser::EmptysetContext *ctx) {
  CanonicalRelation r = Relation::newRelation(RelationOperation::empty);
  std::variant<CanonicalSet, CanonicalRelation> result = r;
  return result;
}

/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitRelationInverse(
    LogicParser::RelationInverseContext *context) {
  auto relationExpression =
      std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e->accept(this));
  if (std::holds_alternative<CanonicalRelation>(relationExpression)) {
    CanonicalRelation r = std::get<CanonicalRelation>(relationExpression);
    CanonicalRelation rInv = Relation::newRelation(RelationOperation::converse, r);
    std::variant<CanonicalSet, CanonicalRelation> result = rInv;
    return result;
  }
  std::cout << "[Parser] Type mismatch of the operand of the relation inverse." << std::endl;
  exit(0);
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitRelationOptional(
    LogicParser::RelationOptionalContext *context) {
  auto relationExpression =
      std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e->accept(this));
  if (std::holds_alternative<CanonicalRelation>(relationExpression)) {
    CanonicalRelation r = std::get<CanonicalRelation>(relationExpression);
    CanonicalRelation id = Relation::newRelation(RelationOperation::identity);
    CanonicalRelation r_or_id = Relation::newRelation(RelationOperation::choice, r, id);
    std::variant<CanonicalSet, CanonicalRelation> result = r_or_id;
    return result;
  }
  std::cout << "[Parser] Type mismatch of the operand of the relation optional." << std::endl;
  exit(0);
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitRelationIdentity(
    LogicParser::RelationIdentityContext *context) {
  if (context->TOID() == nullptr) {
    std::string set = context->e->getText();
    CanonicalRelation r = Relation::newBaseRelation(set + "*" + set);
    CanonicalRelation id = Relation::newRelation(RelationOperation::identity);
    CanonicalRelation r_and_id = Relation::newRelation(RelationOperation::intersection, r, id);
    std::variant<CanonicalSet, CanonicalRelation> result = r_and_id;
    return result;
  } else {
    std::cout << "[Parser] 'visitExprIdentity TOID' expressions are not supported." << std::endl;
    exit(0);
  }
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitCartesianProduct(
    LogicParser::CartesianProductContext *context) {
  // treat cartesian product as binary base relation
  std::string r1 = context->e1->getText();
  std::string r2 = context->e2->getText();
  CanonicalRelation r = Relation::newBaseRelation(r1 + "*" + r2);
  std::variant<CanonicalSet, CanonicalRelation> result = r;
  return result;
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitSetBasic(
    LogicParser::SetBasicContext *context) {
  // TODO: lookup let definitions
  std::string name = context->SETNAME()->getText();
  if (name == "E") {
    std::variant<CanonicalSet, CanonicalRelation> result = Set::newSet(SetOperation::full);
    return result;
  }
  if (name == "0") {
    std::variant<CanonicalSet, CanonicalRelation> result = Set::newSet(SetOperation::empty);
    return result;
  }
  CanonicalSet s = Set::newBaseSet(name);
  std::variant<CanonicalSet, CanonicalRelation> result = s;
  return result;
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitTransitiveReflexiveClosure(
    LogicParser::TransitiveReflexiveClosureContext *context) {
  auto e = std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e->accept(this));
  if (std::holds_alternative<CanonicalRelation>(e)) {
    CanonicalRelation r =
        Relation::newRelation(RelationOperation::transitiveClosure, std::get<CanonicalRelation>(e));
    std::variant<CanonicalSet, CanonicalRelation> result = r;
    return result;
  }
  std::cout << "[Parser] Type mismatch of operand of the Kleene operator: " << context->getText()
            << std::endl;
  exit(0);
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitComposition(
    LogicParser::CompositionContext *context) {
  auto e1 = std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e1->accept(this));
  auto e2 = std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e2->accept(this));

  if (std::holds_alternative<CanonicalRelation>(e1) &&
      std::holds_alternative<CanonicalRelation>(e2)) {
    CanonicalRelation r1 = std::get<CanonicalRelation>(e1);
    CanonicalRelation r2 = std::get<CanonicalRelation>(e2);
    CanonicalRelation r1_r2 = Relation::newRelation(RelationOperation::composition, r1, r2);
    std::variant<CanonicalSet, CanonicalRelation> result = r1_r2;
    return result;
  }
  if (std::holds_alternative<CanonicalSet>(e1) && std::holds_alternative<CanonicalRelation>(e2)) {
    CanonicalSet s = std::get<CanonicalSet>(e1);
    CanonicalRelation r = std::get<CanonicalRelation>(e2);
    CanonicalSet sr = Set::newSet(SetOperation::image, s, r);
    std::variant<CanonicalSet, CanonicalRelation> result = sr;
    return result;
  }
  if (std::holds_alternative<CanonicalRelation>(e1) && std::holds_alternative<CanonicalSet>(e2)) {
    CanonicalRelation r = std::get<CanonicalRelation>(e1);
    CanonicalSet s = std::get<CanonicalSet>(e2);
    CanonicalSet rs = Set::newSet(SetOperation::domain, s, r);
    std::variant<CanonicalSet, CanonicalRelation> result = rs;
    return result;
  }
  std::cout << "[Parser] Type mismatch of two operands of the composition operator: "
            << context->getText() << std::endl;
  exit(0);
}
/*std::variant<CanonicalSet, CanonicalRelation>*/ std::any Logic::visitIntersection(
    LogicParser::IntersectionContext *context) {
  auto e1 = std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e1->accept(this));
  auto e2 = std::any_cast<std::variant<CanonicalSet, CanonicalRelation>>(context->e2->accept(this));
  if (std::holds_alternative<CanonicalRelation>(e1) &&
      std::holds_alternative<CanonicalRelation>(e2)) {
    CanonicalRelation r1 = std::get<CanonicalRelation>(e1);
    CanonicalRelation r2 = std::get<CanonicalRelation>(e2);
    CanonicalRelation r1_and_r2 = Relation::newRelation(RelationOperation::intersection, r1, r2);
    std::variant<CanonicalSet, CanonicalRelation> result = r1_and_r2;
    return result;
  }

  if (std::holds_alternative<CanonicalSet>(e1) && std::holds_alternative<CanonicalSet>(e2)) {
    CanonicalSet s1 = std::get<CanonicalSet>(e1);
    CanonicalSet s2 = std::get<CanonicalSet>(e2);
    CanonicalSet s1_and_s2 = Set::newSet(SetOperation::intersection, s1, s2);
    std::variant<CanonicalSet, CanonicalRelation> result = s1_and_s2;
    return result;
  }
  std::cout << "[Parser] Type mismatch of two operands of the intersection operator." << std::endl;
  exit(0);
}
// /*std::variant<CanonicalSet, CanonicalRelation>*/ std::any
// Logic::visitCanonicalRelationComplement(
//     LogicParser::CanonicalRelationComplementContext *context) {
//   std::cout << "[Parser] Complement operation is not supported." << std::endl;
//   exit(0);
// }

std::unordered_map<std::string, CanonicalRelation> Logic::definedRelations;
std::unordered_map<std::string, int> Logic::definedSingletons;