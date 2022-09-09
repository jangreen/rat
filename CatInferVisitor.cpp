#include <unordered_map>
#include <iostream>
#include <antlr4-runtime.h>
#include <CatParser.h>
#include <CatLexer.h>
#include "CatInferVisitor.h"
#include "Relation.h"
#include "Constraint.h"

using namespace std;
using namespace antlr4;

ConstraintSet CatInferVisitor::parse(string filePath)
{
    ifstream stream;
    stream.open(filePath);
    ANTLRInputStream input(stream);

    CatLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    CatParser parser(&tokens);

    CatParser::McmContext* scContext = parser.mcm();
    return any_cast<ConstraintSet>(this->visitMcm(scContext));
}

antlrcpp::Any CatInferVisitor::visitMcm(CatParser::McmContext *ctx)
{
    cout << "Parse " << ctx->NAME()->getText() << endl;
    unordered_map<string, Constraint> constraints;

    for (auto definitionContext : ctx->definition())
    {
        if (definitionContext->letDefinition())
        {
            definitionContext->letDefinition()->accept(this);
        }
        else if (definitionContext->letRecDefinition())
        {
            cout << "TODO: recursive definitions" << endl;
        }
        else if (definitionContext->axiomDefinition())
        {
            Constraint axiom = any_cast<Constraint>(definitionContext->axiomDefinition()->accept(this));
            constraints[axiom.name] = axiom;
        }
    }
    return constraints;
}

antlrcpp::Any CatInferVisitor::visitAxiomDefinition(CatParser::AxiomDefinitionContext *ctx)
{
    string name;
    if (ctx->NAME()) {
        name = ctx->NAME()->getText();
    } else {
        name = ctx->getText();
    }
    ConstraintType type;
    if (ctx->EMPTY())
    {
        type = ConstraintType::empty;
    }
    else if (ctx->IRREFLEXIVE())
    {
        type = ConstraintType::irreflexive;
    }
    else if (ctx->ACYCLIC())
    {
        type = ConstraintType::acyclic;
    }
    Relation relation = any_cast<Relation>(ctx->e->accept(this));
    return Constraint(type, &relation, name);
}

antlrcpp::Any CatInferVisitor::visitLetDefinition(CatParser::LetDefinitionContext *ctx)
{
    string name = ctx->NAME()->getText();
    Relation derivedRelation = any_cast<Relation>(ctx->e->accept(this));
    Relation::add(name, derivedRelation);
    return derivedRelation;
}

antlrcpp::Any CatInferVisitor::visitExpr(CatParser::ExprContext *ctx)
{
    // process: (e)
    return ctx->e->accept(this);
}

antlrcpp::Any CatInferVisitor::visitExprCartesian(CatParser::ExprCartesianContext *ctx)
{
    cout << "TODO: visitExprCartesian: " << ctx->getText() << endl;
    return nullptr;
}
antlrcpp::Any CatInferVisitor::visitExprRangeIdentity(CatParser::ExprRangeIdentityContext *ctx)
{
    cout << "TODO: visitExprRangeIdentity: " << ctx->getText() << endl;
    return nullptr;
}
antlrcpp::Any CatInferVisitor::visitExprBasic(CatParser::ExprBasicContext *ctx)
{
    string name = ctx->NAME()->getText();
    Relation baseRelation = Relation(name);
    Relation::add(name, baseRelation);
    return baseRelation;
}
antlrcpp::Any CatInferVisitor::visitExprMinus(CatParser::ExprMinusContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
    Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
    return Relation("", Operator::setminus, &r1, &r2);
}
antlrcpp::Any CatInferVisitor::visitExprUnion(CatParser::ExprUnionContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
    Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
    return Relation("", Operator::cup, &r1, &r2);
}
antlrcpp::Any CatInferVisitor::visitExprComposition(CatParser::ExprCompositionContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
    Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
    return Relation("", Operator::composition, &r1, &r2);
}
antlrcpp::Any CatInferVisitor::visitExprIntersection(CatParser::ExprIntersectionContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
    Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
    return Relation("", Operator::cap, &r1, &r2);
}
antlrcpp::Any CatInferVisitor::visitExprTransitive(CatParser::ExprTransitiveContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e->accept(this));
    return Relation("", Operator::transitive, &r1);
}
antlrcpp::Any CatInferVisitor::visitExprComplement(CatParser::ExprComplementContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e->accept(this));
    return Relation("", Operator::complement, &r1);
}
antlrcpp::Any CatInferVisitor::visitExprInverse(CatParser::ExprInverseContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e->accept(this));
    return Relation("", Operator::inverse, &r1);
}
antlrcpp::Any CatInferVisitor::visitExprDomainIdentity(CatParser::ExprDomainIdentityContext *ctx)
{
    cout << "TODO: visitExprDomainIdentity: " << ctx->getText() << endl;
    return nullptr;
}
antlrcpp::Any CatInferVisitor::visitExprIdentity(CatParser::ExprIdentityContext *ctx)
{
    cout << "TODO: visitExprIdentity: " << ctx->getText() << endl;
    return nullptr;
}
antlrcpp::Any CatInferVisitor::visitExprTransRef(CatParser::ExprTransRefContext *ctx)
{
    cout << "TODO: visitExprTransRef: " << ctx->getText() << endl;
    return nullptr;
}
antlrcpp::Any CatInferVisitor::visitExprFencerel(CatParser::ExprFencerelContext *ctx)
{
    cout << "TODO: visitExprOptional: " << ctx->getText() << endl;
    return nullptr;
}
antlrcpp::Any CatInferVisitor::visitExprOptional(CatParser::ExprOptionalContext *ctx)
{
    cout << "TODO: visitExprOptional: " << ctx->getText() << endl;
    return nullptr;
}