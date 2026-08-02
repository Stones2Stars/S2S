#pragma once
#ifndef CV_CONDITION_QUERY_H
#define CV_CONDITION_QUERY_H

//
//	CvConditionQuery -- the ONE static read over a parsed condition tree: "what does this `requires` NAME?".
//	The structural sibling of CvConditionEval, and the distinction between them is the whole point: the
//	EVALUATOR asks whether a tree HOLDS right now (it needs the live contexts); this asks what the tree
//	MENTIONS, which is a pure function of the compiled data and needs no game state at all.
//
//	⛔ WHY IT EXISTS AS A SHARED SURFACE ([DEC-single-implementation]). The tree is already reachable --
//	CvRequires exposes `build`/`operate` as public CvCondition* and the node is a plain tagged struct -- so
//	every consumer CAN write its own recursion, and that is exactly the failure: the building river/coastal
//	counts, the corp prereq gate and the requires block composer would each grow one. A file-static walk is
//	the same hazard one level down (patterns.md: "the next consumer can't see it, so it reimplements it"),
//	which is why this is a declared surface rather than a helper inside one call site.
//
//	⛔ IT IS NOT A BOOLEAN-EXPRESSION API ([pedia-read-map.md] finding 3). No consumer wants to walk nodes,
//	discover combinators or reconstruct structure; they want the ID LIST or a membership answer. Nothing here
//	exposes a node, and nothing here reports HOW the tree combines -- an `all`, an `anyOf` and a `noneOf` all
//	report their atoms alike. ⚠ That is a deliberate limit with teeth: a consumer needing "is this REQUIRED"
//	as opposed to "is this MENTIONED" must not use these reads -- a `noneOf` names the very thing it forbids.
//	The gate verdict is the enabler's, and it is already maintained ([enabler.md] par.7).
//
//	THE TWO AXES, which mirror how the vocabulary itself splits ([json.md] par.3.4/3.5):
//	  - a PRESENCE atom is a HAVE-axis entity (BUILDING_/TECH_/BONUS_/CIVIC_/...), keyed by its edge BUCKET;
//	  - the PLOT SUBSTRATE and every state question are PREDICATES (HAS_TERRAIN carries its type in `param`,
//	    HAS_RIVER carries nothing), keyed by predicate KIND.
//	A consumer asking the wrong axis gets an empty answer rather than a wrong one.
//

#include "CvCondition.h"
#include "CvEdges.h"
#include <vector>
#include <string>

class CvConditionQuery
{
public:
	// --- the HAVE axis: PRESENCE atoms, keyed by edge bucket ---

	// Every FK-resolved presence-atom id of that bucket named anywhere in the tree. Appends; ids may repeat
	// across branches, so a caller wanting a set de-duplicates (the trees are small and most name one atom).
	static void collectIds(const CvCondition* pRoot, EnEdgeBucket eBucket, std::vector<int>& aIdsOut);

	// Membership without the allocation -- the common shape ("does this entity's requires name THIS corp?").
	static bool namesId(const CvCondition* pRoot, EnEdgeBucket eBucket, int iId);

	// --- the PREDICATE axis: plot substrate + state questions, keyed by predicate kind ---

	// Does the tree name this predicate anywhere? The bare form (HAS_RIVER, HAS_COAST, IS_CAPITAL, ...).
	static bool hasPredicate(const CvCondition* pRoot, CvCascPredKind ePredicate);

	// The FK-resolved parameter ids of a PARAMETERIZED predicate ({HAS_TERRAIN: TERRAIN_X} and its kin) --
	// the plot substrate's answer to collectIds, which cannot serve it: terrain and feature have no edge
	// bucket, because they are not a HAVE axis.
	static void collectPredicateIds(const CvCondition* pRoot, CvCascPredKind ePredicate, std::vector<int>& aIdsOut);

	// --- the axis router ---

	// The edge bucket an INFOTYPE id routes to, by its prefix ([naming.md]: the prefix identifies the kind and
	// routes the reference). NO_EDGEB for anything that is not a HAVE-axis entity -- the engine tokens
	// (TURN / POPULATION / ERA / ...), the plot substrate, and the PROPERTY_ bands, each of which has its own
	// axis and its own event routing.
	static EnEdgeBucket bucketForType(const std::string& szType);

private:
	CvConditionQuery();   // a purely-organizational static-methods holder: never instantiated, holds no state
};

#endif // CV_CONDITION_QUERY_H
