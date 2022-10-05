#include "ProofNode.h"
#include <sstream>
#include <string>
#include <regex>

using namespace std;

string getId(ProofNode &node) {
    // TODO better approach?
    const void *address = static_cast<const void *>(&node);
    stringstream ss;
    ss << address;
    return ss.str();
}

ProofNode::ProofNode() : closed(false) {}
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

    string nodeDesc = "\"" + getId(*this) + "\"[label=\"{{" + regex_replace(leftSide, regex("\\|"), "\\|") + " | " + regex_replace(rightSide, regex("\\|"), "\\|") + "} | " + appliedRule + "}\", color=" + (closed ? "green" : "red") + "];\n";
    output += nodeDesc;
    if (leftNode != nullptr) {
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