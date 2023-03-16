#pragma once
#include <CatBaseVisitor.h>
#include "Constraint.h"
#include "Relation.h"

class CatInferVisitor : CatBaseVisitor
{
public:
    vector<Constraint> parseMemoryModel(const string &filePath);
    Relation parseRelation(const string &relationString);
    /*vector<Constraint>*/ antlrcpp::Any visitMcm(CatParser::McmContext *ctx);
    /*void*/ antlrcpp::Any visitLetDefinition(CatParser::LetDefinitionContext *ctx);
    /*Constraint*/ antlrcpp::Any visitAxiomDefinition(CatParser::AxiomDefinitionContext *ctx);
    /*Relation*/ antlrcpp::Any visitExpr(CatParser::ExprContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprCartesian(CatParser::ExprCartesianContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprRangeIdentity(CatParser::ExprRangeIdentityContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprBasic(CatParser::ExprBasicContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprMinus(CatParser::ExprMinusContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprUnion(CatParser::ExprUnionContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprComposition(CatParser::ExprCompositionContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprIntersection(CatParser::ExprIntersectionContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprTransitive(CatParser::ExprTransitiveContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprComplement(CatParser::ExprComplementContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprInverse(CatParser::ExprInverseContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprDomainIdentity(CatParser::ExprDomainIdentityContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprIdentity(CatParser::ExprIdentityContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprTransRef(CatParser::ExprTransRefContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprFencerel(CatParser::ExprFencerelContext *ctx);
    /*Relation*/ antlrcpp::Any visitExprOptional(CatParser::ExprOptionalContext *ctx);
};
