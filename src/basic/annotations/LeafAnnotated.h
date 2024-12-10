#pragma once

#include "../Relation.h"
#include "../Set.h"
#include "../model/InterpretationTree.h"
#include "LeafAnnotation.h"

// This file defines helper functions for expressions that have leaf annotations
// defines LeafAnnotatedSets/LeafAnnotatedRelations
// lifts getLeft/getRight to expressions(relations/sets)

template <typename AnnotationType>
using LeafAnnotatedSet = std::pair<CanonicalSet, CanonicalLeafAnnotation<AnnotationType>>;
template <typename AnnotationType>
using LeafAnnotatedRelation = std::pair<CanonicalRelation, CanonicalLeafAnnotation<AnnotationType>>;

namespace Annotated {
// getLeft/getRight
template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> getLeft(const LeafAnnotatedSet<AnnotationType> &LeafAnnotatedSet);
template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> getRightSet(
    const LeafAnnotatedSet<AnnotationType> &LeafAnnotatedSet);
template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> getRightRelation(
    const LeafAnnotatedSet<AnnotationType> &LeafAnnotatedSet);
template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> getLeftSet(
    const LeafAnnotatedRelation<AnnotationType> &LeafAnnotatedRelation);
template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> getLeftRelation(
    const LeafAnnotatedRelation<AnnotationType> &LeafAnnotatedRelation);
template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> getRight(
    const LeafAnnotatedRelation<AnnotationType> &LeafAnnotatedRelation);

// newSet
template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> newSet(SetOperation operation,
                                        const LeafAnnotatedSet<AnnotationType> &left,
                                        const LeafAnnotatedSet<AnnotationType> &right);
template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> newSet(SetOperation operation,
                                        const LeafAnnotatedSet<AnnotationType> &left,
                                        const LeafAnnotatedRelation<AnnotationType> &relation);
template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> newEvent(int label);
template <typename AnnotationType>
LeafAnnotatedSet<AnnotationType> newBaseSet(const std::string &identifier);

// wrapped newRelation
template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> newSetIdentity(const LeafAnnotatedSet<AnnotationType> &left);
template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> newRelation(
    RelationOperation operation, const LeafAnnotatedRelation<AnnotationType> &left);
template <typename AnnotationType>
LeafAnnotatedRelation<AnnotationType> newRelation(
    RelationOperation operation, const LeafAnnotatedRelation<AnnotationType> &left,
    const LeafAnnotatedRelation<AnnotationType> &right);

// requires two annotations for the same structure
template <typename AnnotationType>
CanonicalLeafAnnotation<AnnotationType> join(CanonicalLeafAnnotation<AnnotationType> a,
                                             CanonicalLeafAnnotation<AnnotationType> b);
}  // namespace Annotated

#include "LeafAnnotated.tpp"