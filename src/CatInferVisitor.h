#pragma once
#include <CatBaseVisitor.h>
#include "Constraint.h"
#include "Relation.h"

class CatInferVisitor : CatBaseVisitor
{
public:
    vector<Constraint> parseMemoryModel(const string &filePath);
    shared_ptr<Relation> parseRelation(const string &relationString);
    antlrcpp::Any visitMcm(CatParser::McmContext *ctx);
    antlrcpp::Any visitLetDefinition(CatParser::LetDefinitionContext *ctx);
    antlrcpp::Any visitAxiomDefinition(CatParser::AxiomDefinitionContext *ctx);
    antlrcpp::Any visitExpr(CatParser::ExprContext *ctx);
    antlrcpp::Any visitExprCartesian(CatParser::ExprCartesianContext *ctx);
    antlrcpp::Any visitExprRangeIdentity(CatParser::ExprRangeIdentityContext *ctx);
    antlrcpp::Any visitExprBasic(CatParser::ExprBasicContext *ctx);
    antlrcpp::Any visitExprMinus(CatParser::ExprMinusContext *ctx);
    antlrcpp::Any visitExprUnion(CatParser::ExprUnionContext *ctx);
    antlrcpp::Any visitExprComposition(CatParser::ExprCompositionContext *ctx);
    antlrcpp::Any visitExprIntersection(CatParser::ExprIntersectionContext *ctx);
    antlrcpp::Any visitExprTransitive(CatParser::ExprTransitiveContext *ctx);
    antlrcpp::Any visitExprComplement(CatParser::ExprComplementContext *ctx);
    antlrcpp::Any visitExprInverse(CatParser::ExprInverseContext *ctx);
    antlrcpp::Any visitExprDomainIdentity(CatParser::ExprDomainIdentityContext *ctx);
    antlrcpp::Any visitExprIdentity(CatParser::ExprIdentityContext *ctx);
    antlrcpp::Any visitExprTransRef(CatParser::ExprTransRefContext *ctx);
    antlrcpp::Any visitExprFencerel(CatParser::ExprFencerelContext *ctx);
    antlrcpp::Any visitExprOptional(CatParser::ExprOptionalContext *ctx);
};
