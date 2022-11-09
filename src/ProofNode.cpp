#include <sstream>
#include <string>
#include <regex>
#include "ProofNode.h"
#include "Solver.h"

using namespace std;

// helper functions
string ProofNode::getId(ProofNode &node)
{
    // TODO better approach?
    const void *address = static_cast<const void *>(&node);
    stringstream ss;
    ss << address;
    return ss.str();
}

string nodeStatusColor(ProofNodeStatus status)
{
    switch (status)
    {
    case ProofNodeStatus::closed:
        return "green";
    case ProofNodeStatus::none:
        return "gray";
    case ProofNodeStatus::dismiss:
        return "red";
    }
}

ProofNode::ProofNode()
    : status(ProofNodeStatus::none), appliedRule(ProofRule::none), leftNode(nullptr), rightNode(nullptr), parent(nullptr), currentConsDepth(0) {}

ProofNode::ProofNode(const string &leftExpr, const string &rightExpr)
    : status(ProofNodeStatus::none), appliedRule(ProofRule::none), leftNode(nullptr), rightNode(nullptr), parent(nullptr), currentConsDepth(0)
{
    this->left.insert(Relation::parse(leftExpr));
    this->right.insert(Relation::parse(rightExpr));
}
ProofNode::~ProofNode() {}

string ProofNode::toDotFormat(shared_ptr<ProofNode> currentGoal)
{
    string output;
    string leftSide;
    for (auto relation : left)
    {
        leftSide += relation->toString() + ",";
    }

    string rightSide;
    for (auto relation : right)
    {
        rightSide += relation->toString() + ",";
    }

    bool isCurrentGoal = (&*currentGoal == this);
    // non record based layout:
    string table = "<<table border='0' cellborder='1' cellspacing='0'><tr><td>" + regex_replace(leftSide, regex("\\&"), "&amp;") + "</td><td>" + regex_replace(rightSide, regex("\\&"), "&amp;") + "</td></tr><tr><td colspan='2'>" + appliedRule.toString() + "</td></tr></table>>";
    string nodeAttributes = "[label=" + table + ", color=" + nodeStatusColor(status) + ", fontcolor=" + (isCurrentGoal ? "blue" : "black") + "]";
    string nodeDesc = "\"" + getId(*this) + "\" " + nodeAttributes;
    output += nodeDesc;
    if (leftNode != nullptr)
    {
        output += leftNode->toDotFormat(currentGoal);
        output += "\"" + getId(*this) + "\" -> \"" + getId(*leftNode) + "\";\n";
    }

    if (rightNode != nullptr)
    {
        output += rightNode->toDotFormat(currentGoal);
        output += "\"" + getId(*this) + "\" -> \"" + getId(*rightNode) + "\";\n";
    }

    if (parent != nullptr)
    {
        output += "\"" + getId(*this) + "\" -> \"" + getId(*parent) + "\"[color=grey];\n";
    }
    return output;
}

string ProofNode::relationString()
{
    string leftSide = "";
    for (auto relation : left)
    {
        leftSide += relation->toString() + ",";
    }

    string rightSide = "";
    for (auto relation : right)
    {
        rightSide += relation->toString() + ",";
    }
    return leftSide + " <= " + rightSide;
}

bool ProofNode::operator==(const ProofNode &other) const
{
    // TODO improve performance
    // old: return left == other.left && right == other.right;
    for (auto l : left)
    {
        bool found = false;
        for (auto r : other.left)
        {
            if (*r == *l)
            {
                found = true;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    for (auto l : other.left)
    {
        bool found = false;
        for (auto r : left)
        {
            if (*r == *l)
            {
                found = true;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    for (auto l : right)
    {
        bool found = false;
        for (auto r : other.right)
        {
            if (*r == *l)
            {
                found = true;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    for (auto l : other.right)
    {
        bool found = false;
        for (auto r : right)
        {
            if (*r == *l)
            {
                found = true;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}
