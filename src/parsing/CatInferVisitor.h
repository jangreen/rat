#pragma once
#include <CatBaseVisitor.h>
#include <CatLexer.h>
#include <CatParser.h>
#include <antlr4-runtime.h>

#include <string>
#include <vector>

#include "../Relation.h"
#include "../Set.h"
#include "../cat/Constraint.h"

class CatInferVisitor : CatBaseVisitor {
 public:
  std::vector<Constraint> parseMemoryModel(const std::string &filePath);
  Relation parseRelation(const std::string &relationString);
  /*std::vector<Constraint>*/ antlrcpp::Any visitMcm(CatParser::McmContext *ctx);
  /*void*/ antlrcpp::Any visitLetDefinition(CatParser::LetDefinitionContext *ctx);
  /*Constraint*/ antlrcpp::Any visitAxiomDefinition(CatParser::AxiomDefinitionContext *ctx);
  /*Set*/ antlrcpp::Any visitSetBasic(CatParser::SetBasicContext *ctx);
  /*Set*/ antlrcpp::Any visitSetIntersection(CatParser::SetBasicContext *ctx);
  /*Set*/ antlrcpp::Any visitSetUnion(CatParser::SetBasicContext *ctx);

  /*Relation*/ antlrcpp::Any visitRelation(CatParser::RelationContext *ctx);
  /*Relation*/ antlrcpp::Any visitCartesianProduct(CatParser::CartesianProductContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationRangeIdentity(
      CatParser::RelationRangeIdentityContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationBasic(CatParser::RelationBasicContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationMinus(CatParser::RelationMinusContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationUnion(CatParser::RelationUnionContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationComposition(CatParser::RelationCompositionContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationIntersection(CatParser::RelationIntersectionContext *ctx);
  /*Relation*/ antlrcpp::Any visitTransitiveClosure(CatParser::TransitiveClosureContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationComplement(CatParser::RelationComplementContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationInverse(CatParser::RelationInverseContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationDomainIdentity(
      CatParser::RelationDomainIdentityContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationIdentity(CatParser::RelationIdentityContext *ctx);
  /*Relation*/ antlrcpp::Any visitTransReflexiveClosure(
      CatParser::TransReflexiveClosureContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationFencerel(CatParser::RelationFencerelContext *ctx);
  /*Relation*/ antlrcpp::Any visitRelationOptional(CatParser::RelationOptionalContext *ctx);
};
