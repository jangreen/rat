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

shared_ptr<Relation> CatInferVisitor::parseRelation(const string &relationString)
{
    ANTLRInputStream input(relationString);
    CatLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    CatParser parser(&tokens);

    CatParser::ExpressionContext *ctx = parser.expression(); // expect expression
    return any_cast<shared_ptr<Relation>>(this->visit(ctx));
}

antlrcpp::Any CatInferVisitor::visitMcm(CatParser::McmContext *ctx)
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

antlrcpp::Any CatInferVisitor::visitAxiomDefinition(CatParser::AxiomDefinitionContext *ctx)
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
    shared_ptr<Relation> relation = any_cast<shared_ptr<Relation>>(ctx->e->accept(this));
    return Constraint(type, relation, name);
}

antlrcpp::Any CatInferVisitor::visitLetDefinition(CatParser::LetDefinitionContext *ctx)
{
    string name = ctx->NAME()->getText();
    shared_ptr<Relation> derivedRelation = any_cast<shared_ptr<Relation>>(ctx->e->accept(this));
    Relation::relations[name] = derivedRelation;
    return derivedRelation;
}

antlrcpp::Any CatInferVisitor::visitExpr(CatParser::ExprContext *ctx)
{
    // process: (e)
    shared_ptr<Relation> derivedRelation = any_cast<shared_ptr<Relation>>(ctx->e->accept(this));
    return derivedRelation;
}

antlrcpp::Any CatInferVisitor::visitExprCartesian(CatParser::ExprCartesianContext *ctx)
{
    // TODO: currently we consider cartesian product as binary base relation
    string r1 = ctx->e1->getText();
    string r2 = ctx->e2->getText();
    return make_shared<Relation>(Operation::base, r1 + "*" + r2); // Relation(Operation::base, r1 + "*" + r2);
}
antlrcpp::Any CatInferVisitor::visitExprBasic(CatParser::ExprBasicContext *ctx)
{
    string name = ctx->NAME()->getText();
    if (name == "id")
    {
        return make_shared<Relation>(Operation::identity);
    }
    if (name == "0")
    {
        return make_shared<Relation>(Operation::empty);
    }
    return make_shared<Relation>(Operation::base, name); // TODO: remove old: Relation::get(name);
}
antlrcpp::Any CatInferVisitor::visitExprMinus(CatParser::ExprMinusContext *ctx)
{
    /* TODO: shared_ptr<Relation> r1 = any_cast<shared_ptr<Relation>>(ctx->e1->accept(this));
    shared_ptr<Relation> r2 = any_cast<shared_ptr<Relation>>(ctx->e2->accept(this));
    return make_shared<Relation>(Operation::setminus, r1, r2); */
}
antlrcpp::Any CatInferVisitor::visitExprUnion(CatParser::ExprUnionContext *ctx)
{
    shared_ptr<Relation> r1 = any_cast<shared_ptr<Relation>>(ctx->e1->accept(this));
    shared_ptr<Relation> r2 = any_cast<shared_ptr<Relation>>(ctx->e2->accept(this));
    return make_shared<Relation>(Operation::choice, r1, r2);
}
antlrcpp::Any CatInferVisitor::visitExprComposition(CatParser::ExprCompositionContext *ctx)
{
    shared_ptr<Relation> r1 = any_cast<shared_ptr<Relation>>(ctx->e1->accept(this));
    shared_ptr<Relation> r2 = any_cast<shared_ptr<Relation>>(ctx->e2->accept(this));
    return make_shared<Relation>(Operation::composition, r1, r2);
}
antlrcpp::Any CatInferVisitor::visitExprIntersection(CatParser::ExprIntersectionContext *ctx)
{
    shared_ptr<Relation> r1 = any_cast<shared_ptr<Relation>>(ctx->e1->accept(this));
    shared_ptr<Relation> r2 = any_cast<shared_ptr<Relation>>(ctx->e2->accept(this));
    return make_shared<Relation>(Operation::intersection, r1, r2);
}
antlrcpp::Any CatInferVisitor::visitExprTransitive(CatParser::ExprTransitiveContext *ctx)
{
    /* TODO: shared_ptr<Relation> r1 = any_cast<shared_ptr<Relation>>(ctx->e->accept(this));
    return make_shared<Relation>(Operation::transitiveClosure, r1);*/
}
antlrcpp::Any CatInferVisitor::visitExprComplement(CatParser::ExprComplementContext *ctx)
{
    /* shared_ptr<Relation> r1 = any_cast<shared_ptr<Relation>>(ctx->e->accept(this));
    return make_shared<Relation>(Operation::complement, r1); */
}
antlrcpp::Any CatInferVisitor::visitExprInverse(CatParser::ExprInverseContext *ctx)
{
    shared_ptr<Relation> r1 = any_cast<shared_ptr<Relation>>(ctx->e->accept(this));
    return make_shared<Relation>(Operation::converse, r1);
}
antlrcpp::Any CatInferVisitor::visitExprDomainIdentity(CatParser::ExprDomainIdentityContext *ctx)
{
    cout << "TODO: visitExprDomainIdentity: " << ctx->getText() << endl;
    return nullptr;
}
antlrcpp::Any CatInferVisitor::visitExprIdentity(CatParser::ExprIdentityContext *ctx)
{
    if (ctx->TOID() == nullptr)
    {
        // TODO: dont use basic realtion intersection id
        // use text intersection id
        string set = ctx->e->getText();
        // TODO: fix return make_shared<Relation>(Operation::intersection, Relation::get(set + "*" + set), Relation::ID);
        return nullptr;
    }
    else
    {
        cout << "TODO: visitExprIdentity TOID: " << ctx->getText() << endl;
        return nullptr;
    }
}
antlrcpp::Any CatInferVisitor::visitExprRangeIdentity(CatParser::ExprRangeIdentityContext *ctx)
{
    cout << "TODO: visitExprRangeIdentity: " << ctx->getText() << endl;
    return nullptr;
}
antlrcpp::Any CatInferVisitor::visitExprTransRef(CatParser::ExprTransRefContext *ctx)
{
    shared_ptr<Relation> r1 = any_cast<shared_ptr<Relation>>(ctx->e->accept(this));
    return make_shared<Relation>(Operation::transitiveClosure, r1);
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