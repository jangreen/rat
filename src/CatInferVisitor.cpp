#include "CatInferVisitor.h"
#include <antlr4-runtime.h>
#include <CatParser.h>
#include <CatLexer.h>
#include <memory>

using namespace std;
using namespace antlr4;

vector<Constraint> CatInferVisitor::parseMemoryModel(const string &filePath)
{
    ifstream stream;
    stream.open(filePath);
    ANTLRInputStream input(stream);

    CatLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    CatParser parser(&tokens);

    CatParser::McmContext *ctx = parser.mcm();
    return any_cast<vector<Constraint>>(this->visitMcm(ctx));
}

Relation CatInferVisitor::parseRelation(const string &relationString)
{
    ANTLRInputStream input(relationString);
    CatLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    CatParser parser(&tokens);

    CatParser::ExpressionContext *ctx = parser.expression(); // expect expression
    Relation parsedRelation = any_cast<Relation>(this->visit(ctx));
    return parsedRelation;
}

/*vector<Constraint>*/ antlrcpp::Any CatInferVisitor::visitMcm(CatParser::McmContext *ctx)
{
    vector<Constraint> constraints;

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
            constraints.push_back(axiom);
        }
    }
    return constraints;
}

/*Constraint*/ antlrcpp::Any CatInferVisitor::visitAxiomDefinition(CatParser::AxiomDefinitionContext *ctx)
{
    string name;
    if (ctx->NAME())
    {
        name = ctx->NAME()->getText();
    }
    else
    {
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
    return Constraint(type, move(relation), name);
}

/*void*/ antlrcpp::Any CatInferVisitor::visitLetDefinition(CatParser::LetDefinitionContext *ctx)
{
    string name = ctx->NAME()->getText();
    Relation derivedRelation = any_cast<Relation>(ctx->e->accept(this));
    // Relation::relations[name] = derivedRelation; // TODO: why error?
    Relation::relations.insert({name, derivedRelation});
    return antlrcpp::Any();
}

/*Relation*/ antlrcpp::Any CatInferVisitor::visitExpr(CatParser::ExprContext *ctx)
{
    // process: (e)
    Relation derivedRelation = any_cast<Relation>(ctx->e->accept(this));
    return derivedRelation;
}

/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprCartesian(CatParser::ExprCartesianContext *ctx)
{
    // TODO: currently we consider cartesian product as binary base relation
    string r1 = ctx->e1->getText();
    string r2 = ctx->e2->getText();
    return Relation(Operation::base, r1 + "*" + r2); // Relation(Operation::base, r1 + "*" + r2);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprBasic(CatParser::ExprBasicContext *ctx)
{
    string name = ctx->NAME()->getText();
    if (name == "id")
    {
        return Relation(Operation::identity);
    }
    if (name == "0")
    {
        return Relation(Operation::empty);
    }
    return Relation(Operation::base, name); // TODO: remove old: Relation::get(name);
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprMinus(CatParser::ExprMinusContext *ctx)
{
    /* TODO: Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
    Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
    return Relation(Operation::setminus, r1, r2); */
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprUnion(CatParser::ExprUnionContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
    Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
    return Relation(Operation::choice, move(r1), move(r2));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprComposition(CatParser::ExprCompositionContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
    Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
    return Relation(Operation::composition, move(r1), move(r2));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprIntersection(CatParser::ExprIntersectionContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e1->accept(this));
    Relation r2 = any_cast<Relation>(ctx->e2->accept(this));
    return Relation(Operation::intersection, move(r1), move(r2));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprTransitive(CatParser::ExprTransitiveContext *ctx)
{
    /* TODO: Relation r1 = any_cast<Relation>(ctx->e->accept(this));
    return Relation(Operation::transitiveClosure, r1);*/
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprComplement(CatParser::ExprComplementContext *ctx)
{
    /* Relation r1 = any_cast<Relation>(ctx->e->accept(this));
    return Relation(Operation::complement, r1); */
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprInverse(CatParser::ExprInverseContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e->accept(this));
    return Relation(Operation::converse, move(r1));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprDomainIdentity(CatParser::ExprDomainIdentityContext *ctx)
{
    cout << "TODO: visitExprDomainIdentity: " << ctx->getText() << endl;
    return nullptr;
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprIdentity(CatParser::ExprIdentityContext *ctx)
{
    if (ctx->TOID() == nullptr)
    {
        // TODO: dont use basic realtion intersection id
        // use text intersection id
        string set = ctx->e->getText();
        // TODO: fix return Relation(Operation::intersection, Relation::get(set + "*" + set), Relation::ID);
        return nullptr;
    }
    else
    {
        cout << "TODO: visitExprIdentity TOID: " << ctx->getText() << endl;
        return nullptr;
    }
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprRangeIdentity(CatParser::ExprRangeIdentityContext *ctx)
{
    cout << "TODO: visitExprRangeIdentity: " << ctx->getText() << endl;
    return nullptr;
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprTransRef(CatParser::ExprTransRefContext *ctx)
{
    Relation r1 = any_cast<Relation>(ctx->e->accept(this));
    return Relation(Operation::transitiveClosure, move(r1));
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprFencerel(CatParser::ExprFencerelContext *ctx)
{
    cout << "TODO: visitExprOptional: " << ctx->getText() << endl;
    return nullptr;
}
/*Relation*/ antlrcpp::Any CatInferVisitor::visitExprOptional(CatParser::ExprOptionalContext *ctx)
{
    cout << "TODO: visitExprOptional: " << ctx->getText() << endl;
    return nullptr;
}