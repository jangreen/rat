#pragma once
#include <cassert>

#include "Annotation.h"
#include "Relation.h"
#include "Set.h"


typedef std::pair<CanonicalSet, CanonicalAnnotation> AnnotatedSet;
typedef std::pair<CanonicalRelation, CanonicalAnnotation> AnnotatedRelation;

namespace Annotated {

AnnotatedSet getLeft(const AnnotatedSet& set);
std::variant<AnnotatedSet, AnnotatedRelation> getRight(const AnnotatedSet& relation);


AnnotatedSet makeWithValue(CanonicalSet set, AnnotationType value);
AnnotatedSet make(SetOperation operation, const AnnotatedSet &left, const AnnotatedSet &right);
AnnotatedSet make(SetOperation operation, const AnnotatedSet &set, const AnnotatedRelation &relation);

AnnotatedRelation makeWithValue(CanonicalRelation relation, AnnotationType value);
// TODO: Add remaining variants for AnnotatedRelation?!

// TODO: Add toString/print method

}