#pragma once
#include <LogicBaseVisitor.h>
#include <LogicLexer.h>
#include <LogicParser.h>
#include <antlr4-runtime.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../Formula.h"
#include "../cat/Constraint.h"
#include "LogicVisitor.h"

/*/ helper
Relation loadModel(const std::string &file) {
  CatInferVisitor visitor;
  auto sc = visitor.parseMemoryModel(file);
  std::optional<Relation> unionR;
  for (auto &constraint : sc) {
    constraint.toEmptyNormalForm();
    Relation r = constraint.relation;
    if (unionR) {
      unionR = Relation(RelationOperation::choice, std::move(*unionR), std::move(r));
    } else {
      unionR = r;
    }
  }
  return *unionR;
}
*/

class Logic : LogicBaseVisitor {  // TODO: should inherit from CatInferVisitor
 public:
  std::vector<Constraint> parseMemoryModel(const std::string &filePath);
  Relation parseRelation(const std::string &relationString);
  /*std::vector<Constraint>*/ antlrcpp::Any visitMcm(LogicParser::McmContext *ctx) override;
  /*void*/ antlrcpp::Any visitLetDefinition(LogicParser::LetDefinitionContext *ctx) override;
  /*Constraint*/ antlrcpp::Any visitAxiomDefinition(
      LogicParser::AxiomDefinitionContext *ctx) override;
  /*Set*/ antlrcpp::Any visitSetBasic(LogicParser::SetBasicContext *ctx) override;
  /*Set*/ antlrcpp::Any visitSingleton(LogicParser::SingletonContext *ctx) override;
  /*Set*/ antlrcpp::Any visitSetIntersection(LogicParser::SetIntersectionContext *ctx) override;
  /*Set*/ antlrcpp::Any visitSetUnion(LogicParser::SetUnionContext *ctx) override;
  /*Set*/ antlrcpp::Any visitSet(LogicParser::SetContext *ctx) override;

  /*Relation*/ antlrcpp::Any visitRelation(LogicParser::RelationContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitCartesianProduct(
      LogicParser::CartesianProductContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationRangeIdentity(
      LogicParser::RelationRangeIdentityContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationBasic(LogicParser::RelationBasicContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationMinus(LogicParser::RelationMinusContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationUnion(LogicParser::RelationUnionContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationComposition(
      LogicParser::RelationCompositionContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationIntersection(
      LogicParser::RelationIntersectionContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitTransitiveClosure(
      LogicParser::TransitiveClosureContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationComplement(
      LogicParser::RelationComplementContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationInverse(
      LogicParser::RelationInverseContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationDomainIdentity(
      LogicParser::RelationDomainIdentityContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationIdentity(
      LogicParser::RelationIdentityContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitTransReflexiveClosure(
      LogicParser::TransReflexiveClosureContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationFencerel(
      LogicParser::RelationFencerelContext *ctx) override;
  /*Relation*/ antlrcpp::Any visitRelationOptional(
      LogicParser::RelationOptionalContext *ctx) override;

  /* std::vector<Formula> */ std::any visitProof(LogicParser::ProofContext *ctx) override;
  /* Formula */ std::any visitAssertion(LogicParser::AssertionContext *ctx) override;
  /* Formula */ std::any visitFormula(LogicParser::FormulaContext *ctx) override;
  /* Predicate */ std::any visitPredicate(LogicParser::PredicateContext *ctx) override;

  static std::unordered_map<std::string, Relation> definedRelations;
  static std::unordered_map<std::string, int> definedSingletons;
  static std::vector<Formula> parse(const std::string &filePath) {
    std::cout << "[Parsing] Parse file: " << filePath << std::endl;
    std::ifstream stream;
    stream.open(filePath);
    antlr4::ANTLRInputStream input(stream);

    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ProofContext *ctx = parser.proof();
    Logic visitor;
    return std::any_cast<std::vector<Formula>>(visitor.visitProof(ctx));
  }

  Formula parseFormula(const std::string &formulaString) {
    antlr4::ANTLRInputStream input(formulaString);
    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::FormulaContext *ctx = parser.formula();
    Formula formula = std::any_cast<Formula>(this->visitFormula(ctx));
    return formula;
  }

  /* LEGACY
    std::tuple<std::vector<Assumption>, std::vector<Formula>> std::any visitStatement(
        LogicParser::StatementContext *ctx) {
      for (const auto &letDefinition : ctx->letDefinition()) {
        std::string name = letDefinition->NAME()->getText();
        Relation derivedRelation(letDefinition->e->getText());
        Logic::definedRelations.insert({name, derivedRelation});
      }

      std::vector<Formula> formulas;
      std::vector<Assumption> hypotheses;

      for (const auto &assertionContext : ctx->assertion()) {
        auto formula = std::any_cast<Formula>(assertionContext->accept(this));
        formulas.push_back(std::move(formula));
      }
      for (const auto &assertionMmContext : ctx->mmAssertion()) {
        auto mm = std::any_cast<std::tuple<Assumption,
  Formula>>(assertionMmContext->accept(this)); auto assumption = std::get<Assumption>(mm); auto
  formula = std::get<Formula>(mm); formulas.push_back(std::move(formula));
        hypotheses.push_back(std::move(assumption));
      }

      for (const auto &hypothesisContext : ctx->hypothesis()) {
        auto hypothesis = std::any_cast<Assumption>(hypothesisContext->accept(this));
        hypotheses.push_back(std::move(hypothesis));
      }
      std::tuple<std::vector<Assumption>, std::vector<Formula>> response{std::move(hypotheses),
                                                                         std::move(formulas)};
      return response;
    }

    / *Assumption
  std::any visitHypothesis(LogicParser::HypothesisContext *ctx) {
    Relation lhs(ctx->lhs->getText());
    Relation rhs(ctx->rhs->getText());
    switch (rhs.operation) {
      case RelationOperation::base:
        return Assumption(AssumptionType::regular, std::move(lhs), *rhs.identifier);
      case RelationOperation::empty:
        return Assumption(AssumptionType::empty, std::move(lhs));
      case RelationOperation::identity:
        return Assumption(AssumptionType::identity, std::move(lhs));
      default:
        std::cout << "[Parser] Unsupported hypothesis:" << ctx->lhs->getText()
                  << " <= " << ctx->rhs->getText() << std::endl;
        break;
    }
  }

  / *Formula
  std::any visitAssertion(LogicParser::AssertionContext *ctx) {
    Relation r1(ctx->lhs->getText());
    Relation r2(ctx->rhs->getText());
    Set startEvent1(SetOperation::singleton, Set::maxSingletonLabel++);
    Set endEvent1(SetOperation::singleton, Set::maxSingletonLabel++);
    Set startEvent2(startEvent1);
    Set endEvent2(endEvent1);
    Set image1(SetOperation::image, std::move(startEvent1), std::move(r1));
    Set image2(SetOperation::image, std::move(startEvent2), std::move(r2));
    Predicate p1(PredicateOperation::intersectionNonEmptiness, std::move(image1),
                 std::move(endEvent1));
    Predicate p2(PredicateOperation::intersectionNonEmptiness, std::move(image2),
                 std::move(endEvent2));
    Literal l1(false, std::move(p1));
    Literal l2(true, std::move(p2));
    Formula f1(FormulaOperation::literal, std::move(l1));
    Formula f2(FormulaOperation::literal, std::move(l2));
    Formula response(FormulaOperation::logicalAnd, std::move(f1), std::move(f2));
    return response;
  }

  / *std::Assumption, Formula>
  std::any visitMmAssertion(LogicParser::MmAssertionContext *ctx) {
    std::string lhsPath(ctx->lhs->getText());
    std::string rhsPath(ctx->rhs->getText());
    auto lhsModel = loadModel(lhsPath);
    auto rhsModel = loadModel(rhsPath);
    // TODO: remove std::cout << rhsModel.toString() << " <= " << lhsModel.toString() <<
  std::endl; Assumption lhsEmpty(AssumptionType::empty, std::move(lhsModel)); Relation
  emptyR(RelationOperation::empty); Set startLabel(SetOperation::singleton,
  Set::maxSingletonLabel++); Set endLabel(SetOperation::singleton, Set::maxSingletonLabel++); Set
  image(SetOperation::image, std::move(startLabel), std::move(rhsModel)); Predicate
  p(PredicateOperation::intersectionNonEmptiness, std::move(image), std::move(endLabel)); Literal
  l(false, std::move(p)); Formula f(FormulaOperation::literal, std::move(l));
    std::tuple<Assumption, Formula> response{std::move(lhsEmpty), std::move(f)};
    return response;
  }
  */
};
