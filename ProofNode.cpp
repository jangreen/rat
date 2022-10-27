#include <sstream>
#include <string>
#include <regex>
#include "ProofNode.h"
#include "Solver.h"

using namespace std;

// helper functions
string getId(ProofNode &node)
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
    case ProofNodeStatus::open:
        return "red";
    }
}

ProofNode::ProofNode() : status(ProofNodeStatus::none), appliedRule(ProofRule::none) {}
ProofNode::~ProofNode() {}

string ProofNode::toDotFormat()
{
    string output = "";
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

    string nodeDesc = "\"" + getId(*this) + "\"[label=\"{{" + regex_replace(leftSide, regex("\\|"), "\\|") + " | " + regex_replace(rightSide, regex("\\|"), "\\|") + "} | " + appliedRule.toString() + "}\", color=" + nodeStatusColor(status) + "];\n";
    output += nodeDesc;
    if (leftNode != nullptr)
    {
        output += leftNode->toDotFormat();
        output += "\"" + getId(*this) + "\" -> \"" + getId(*leftNode) + "\";\n";
    }

    if (rightNode != nullptr)
    {
        output += rightNode->toDotFormat();
        output += "\"" + getId(*this) + "\" -> \"" + getId(*rightNode) + "\";\n";
    }
    return output;
}

bool ProofNode::operator==(const ProofNode &other) const
{
    // TODO
    return left == other.left && right == other.right;
    /*for (auto l : left)
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
    return true;*/
}
