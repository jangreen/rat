#pragma once
#include <LogicBaseVisitor.h>
#include <LogicLexer.h>
#include <LogicParser.h>
#include <antlr4-runtime.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../Formula.h"
#include "../cat/Constraint.h"
#include "LogicVisitor.h"

/*/ helper
Relation loadModel(const std::string &file) {
  auto sc = Logic::parseMemoryModel(file);
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
// TODO: implement precedence rules
class Logic : LogicBaseVisitor {  // TODO: should inherit from CatInferVisitor
 public:
  /*std::vector<Formula>*/ std::any visitProof(LogicParser::ProofContext *context) override;
  // /*void*/ std::any visitStatement(LogicParser::StatementContext *ctx) override;
  /*void*/ std::any visitInclusion(LogicParser::InclusionContext *ctx) override;
  /*Formula*/ std::any visitAssertion(LogicParser::AssertionContext *context) override;
  /*Formula*/ std::any visitFormula(LogicParser::FormulaContext *context) override;
  /*Predicate*/ std::any visitPredicate(LogicParser::PredicateContext *context) override;
  /*void*/ std::any visitHypothesis(LogicParser::HypothesisContext *ctx) override;
  /*std::vector<Constraint>*/ std::any visitMcm(LogicParser::McmContext *context) override;
  // /*void*/ std::any visitDefinition(LogicParser::DefinitionContext *context) override;
  /*Constraint*/ std::any visitAxiomDefinition(
      LogicParser::AxiomDefinitionContext *context) override;
  /*void*/ std::any visitLetDefinition(LogicParser::LetDefinitionContext *context) override;
  /*void*/ std::any visitLetRecDefinition(LogicParser::LetRecDefinitionContext *context) override;
  /*void*/ std::any visitLetRecAndDefinition(
      LogicParser::LetRecAndDefinitionContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitParentheses(
      LogicParser::ParenthesesContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitTransitiveClosure(
      LogicParser::TransitiveClosureContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationFencerel(
      LogicParser::RelationFencerelContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitSetSingleton(
      LogicParser::SetSingletonContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationBasic(
      LogicParser::RelationBasicContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationMinus(
      LogicParser::RelationMinusContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationDomainIdentity(
      LogicParser::RelationDomainIdentityContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationRangeIdentity(
      LogicParser::RelationRangeIdentityContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitUnion(LogicParser::UnionContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitEmptyset(
      LogicParser::EmptysetContext *ctx) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationInverse(
      LogicParser::RelationInverseContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationOptional(
      LogicParser::RelationOptionalContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitRelationIdentity(
      LogicParser::RelationIdentityContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitCartesianProduct(
      LogicParser::CartesianProductContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitSetBasic(
      LogicParser::SetBasicContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitTransitiveReflexiveClosure(
      LogicParser::TransitiveReflexiveClosureContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitComposition(
      LogicParser::CompositionContext *context) override;
  /*std::variant<Set, Relation>*/ std::any visitIntersection(
      LogicParser::IntersectionContext *context) override;
  // /*std::variant<Set, Relation>*/ std::any visitRelationComplement(
  //    LogicParser::RelationComplementContext *context) override;

  static std::unordered_map<std::string, Relation> definedRelations;
  static std::unordered_map<std::string, int> definedSingletons;
  static std::vector<Formula> parse(const std::string &filePath) {
    spdlog::info(fmt::format("[Parser] Parse file: {}", filePath));
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
  static std::vector<Constraint> parseMemoryModel(const std::string &filePath) {
    spdlog::info(fmt::format("[Parser] Parse file: {}", filePath));
    std::ifstream stream;
    stream.open(filePath);
    antlr4::ANTLRInputStream input(stream);

    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::McmContext *context = parser.mcm();
    Logic visitor;
    return std::any_cast<std::vector<Constraint>>(visitor.visitMcm(context));
  }

  static Relation parseRelation(const std::string &relationString) {
    antlr4::ANTLRInputStream input(relationString);
    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ExpressionContext *context = parser.expression();  // expect expression
    Logic visitor;
    std::variant<Set, Relation> parsedRelation =
        std::any_cast<std::variant<Set, Relation>>(visitor.visit(context));
    return std::get<Relation>(parsedRelation);
  }

  static Formula parseFormula(const std::string &formulaString) {
    antlr4::ANTLRInputStream input(formulaString);
    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::FormulaContext *ctx = parser.formula();
    Logic visitor;
    Formula formula = std::any_cast<Formula>(visitor.visitFormula(ctx));
    return formula;
  }

  static Predicate parsePredicate(const std::string &predicateString) {
    antlr4::ANTLRInputStream input(predicateString);
    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::PredicateContext *ctx = parser.predicate();
    Logic visitor;
    Predicate predicate = std::any_cast<Predicate>(visitor.visitPredicate(ctx));
    return predicate;
  }

  static Set parseSet(const std::string &setString) {
    antlr4::ANTLRInputStream input(setString);
    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ExpressionContext *ctx = parser.expression();
    Logic visitor;
    std::variant<Set, Relation> set =
        std::any_cast<std::variant<Set, Relation>>(visitor.visit(ctx));
    return std::get<Set>(set);
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
  */
};
