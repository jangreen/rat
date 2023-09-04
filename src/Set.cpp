#include "Set.h"

std::string Set::toString() const {
  std::string output;
  switch (operation) {
    case SetOperation::singleton:
      output += "[" + std::to_string(*label) + "]";
      break;
    case SetOperation::image:
      output += "(" + leftOperand->toString() + ";" + relation->toString() + ")";
      break;
    case SetOperation::domain:
      output += "(" + relation->toString() + ";" + leftOperand->toString() + ")";
      break;
    case SetOperation::base:
      output += *identifier;
      break;
    case SetOperation::empty:
      output += "0";
      break;
    case SetOperation::full:
      output += "E";
      break;
    case SetOperation::intersection:
      output += "(" + leftOperand->toString() + " & " + rightOperand->toString() + ")";
      break;
    case SetOperation::choice:
      output += "(" + leftOperand->toString() + " | " + rightOperand->toString() + ")";
      break;
  }
  return output;
}