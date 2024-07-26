#pragma once
#include <LogicBaseVisitor.h>
#include <LogicLexer.h>
#include <LogicParser.h>
#include <antlr4-runtime.h>
#include <spdlog/spdlog.h>

#include <string>
#include <vector>

#include "../basic/Literal.h"
#include "../cat/Constraint.h"
#include "LogicVisitor.h"

typedef std::variant<CanonicalSet, CanonicalRelation> CanonicalExpression;

class Logic : LogicBaseVisitor {
 public:
  /*DNF*/ std::any visitProof(LogicParser::ProofContext *context) override;
  // /*void*/ std::any visitStatement(LogicParser::StatementContext *ctx) override;
  /*void*/ std::any visitInclusion(LogicParser::InclusionContext *ctx) override;
  /*Cube*/ std::any visitAssertion(LogicParser::AssertionContext *context) override;
  /*void*/ std::any visitHypothesis(LogicParser::HypothesisContext *ctx) override;
  /*std::vector<Constraint>*/ std::any visitMcm(LogicParser::McmContext *context) override;
  // /*void*/ std::any visitDefinition(LogicParser::DefinitionContext *context) override;
  /*Constraint*/ std::any visitAxiomDefinition(
      LogicParser::AxiomDefinitionContext *context) override;
  /*void*/ std::any visitLetDefinition(LogicParser::LetDefinitionContext *context) override;
  /*void*/ std::any visitLetRecDefinition(LogicParser::LetRecDefinitionContext *context) override;
  /*void*/ std::any visitLetRecAndDefinition(
      LogicParser::LetRecAndDefinitionContext *context) override;
  /*CanonicalExpression*/ std::any visitParentheses(
      LogicParser::ParenthesesContext *context) override;
  /*CanonicalExpression*/ std::any visitTransitiveClosure(
      LogicParser::TransitiveClosureContext *context) override;
  /*CanonicalExpression*/ std::any visitRelationFencerel(
      LogicParser::RelationFencerelContext *context) override;
  /*CanonicalExpression*/ std::any visitSetSingleton(
      LogicParser::SetSingletonContext *context) override;
  /*CanonicalExpression*/ std::any visitRelationBasic(
      LogicParser::RelationBasicContext *context) override;
  /*CanonicalExpression*/ std::any visitRelationMinus(
      LogicParser::RelationMinusContext *context) override;
  /*CanonicalExpression*/ std::any visitRelationDomainIdentity(
      LogicParser::RelationDomainIdentityContext *context) override;
  /*CanonicalExpression*/ std::any visitRelationRangeIdentity(
      LogicParser::RelationRangeIdentityContext *context) override;
  /*CanonicalExpression*/ std::any visitUnion(LogicParser::UnionContext *context) override;
  /*CanonicalExpression*/ std::any visitEmptyset(LogicParser::EmptysetContext *ctx) override;
  /*CanonicalExpression*/ std::any visitRelationInverse(
      LogicParser::RelationInverseContext *context) override;
  /*CanonicalExpression*/ std::any visitRelationOptional(
      LogicParser::RelationOptionalContext *context) override;
  /*CanonicalExpression*/ std::any visitRelationIdentity(
      LogicParser::RelationIdentityContext *context) override;
  /*CanonicalExpression*/ std::any visitCartesianProduct(
      LogicParser::CartesianProductContext *context) override;
  /*CanonicalExpression*/ std::any visitSetBasic(LogicParser::SetBasicContext *context) override;
  /*CanonicalExpression*/ std::any visitTransitiveReflexiveClosure(
      LogicParser::TransitiveReflexiveClosureContext *context) override;
  /*CanonicalExpression*/ std::any visitComposition(
      LogicParser::CompositionContext *context) override;
  /*CanonicalExpression*/ std::any visitIntersection(
      LogicParser::IntersectionContext *context) override;
  // /*CanonicalExpression*/ std::any visitRelationComplement(
  //    LogicParser::RelationComplementContext *context) override;

  static std::unordered_map<std::string, CanonicalRelation> derivedRelations;
  static std::unordered_map<std::string, CanonicalSet> derivedSets;
  static std::unordered_map<std::string, CanonicalSet> definedSingletons;
  static DNF parse(const std::string &filePath) {
    spdlog::info(fmt::format("[Parser] File: {}", filePath));
    std::ifstream stream;
    stream.open(filePath);
    antlr4::ANTLRInputStream input(stream);

    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ProofContext *ctx = parser.proof();
    Logic visitor;
    return std::any_cast<DNF>(visitor.visitProof(ctx));
  }
  static std::vector<Constraint> parseMemoryModel(const std::string &filePath) {
    spdlog::info(fmt::format("[Parser] File: {}", filePath));
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

  static CanonicalRelation parseRelation(const std::string &relationString) {
    antlr4::ANTLRInputStream input(relationString);
    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ExpressionContext *context = parser.expression();  // expect expression
    Logic visitor;
    auto parsedRelation = std::any_cast<CanonicalExpression>(visitor.visit(context));
    return std::get<CanonicalRelation>(parsedRelation);
    // FIXME: clang-tidy claims address of <parsedRelation> can escape... why?
  }

  static CanonicalExpression parseExpression(const std::string &exprString) {
    antlr4::ANTLRInputStream input(exprString);
    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ExpressionContext *context = parser.expression();  // expect expression
    Logic visitor;
    return std::any_cast<CanonicalExpression>(visitor.visit(context));
  }
};
