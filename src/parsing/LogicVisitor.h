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
#include "Assumptions.h"
#include "LogicVisitor.h"

class Logic : LogicBaseVisitor {
  std::unordered_map<std::string, CanonicalRelation> derivedRelations;
  std::unordered_map<std::string, CanonicalSet> derivedSets;
  std::unordered_map<std::string, CanonicalSet> definedSingletons;

  Assumptions assumptions;

  /*DNF*/ std::any visitProof(LogicParser::ProofContext *context) override;
  /*void*/ std::any visitInclusion(LogicParser::InclusionContext *ctx) override;
  /*Cube*/ std::any visitAssertion(LogicParser::AssertionContext *context) override;
  /*void*/ std::any visitHypothesis(LogicParser::HypothesisContext *ctx) override;
  /*std::vector<Constraint>*/ std::any visitMcm(LogicParser::McmContext *context) override;
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
  /*CanonicalExpression*/ std::any visitRelationDomain(
      LogicParser::RelationDomainContext *context) override;
  /*CanonicalExpression*/ std::any visitRelationRange(
      LogicParser::RelationRangeContext *context) override;
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
  /*CanonicalExpression*/ std::any visitRelationComplement(
      LogicParser::RelationComplementContext *context) override;

 public:
  DNF parse(const std::string &filePath) {
    spdlog::info(fmt::format("[Parser] File: {}", filePath));
    std::ifstream stream;
    stream.open(filePath);
    if (!stream.good()) {
      throw std::runtime_error(fmt::format("[Parser] Could not open file {}", filePath));
    }
    antlr4::ANTLRInputStream input(stream);

    LogicLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    LogicParser parser(&tokens);

    LogicParser::ProofContext *ctx = parser.proof();
    return std::any_cast<DNF>(visitProof(ctx));
  }

    [[nodiscard]] const Assumptions& getAssumptions() const;

  // CanonicalExpression parseExpression(const std::string &exprString) {
  //   antlr4::ANTLRInputStream input(exprString);
  //   LogicLexer lexer(&input);
  //   antlr4::CommonTokenStream tokens(&lexer);
  //   LogicParser parser(&tokens);
  //
  //   LogicParser::ExpressionContext *context = parser.expression();  // expect expression
  //   return std::any_cast<CanonicalExpression>(visit(context));
  // }
};
