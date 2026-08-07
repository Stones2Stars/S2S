#pragma once
#ifndef CV_FOLD_TARGET_INFO_H
#define CV_FOLD_TARGET_INFO_H

//
//	CvFoldTargetInfo -- what a GENERALIZED PLOT PREDICATE MEANS, as the concrete substrate entities a deposit
//	can land on (json.md par.3.5: we never fold onto a boolean, we need a target to fold onto).
//
//	A deposit lands on a real substrate ENTITY. A predicate naming a CATEGORY rather than an entity -- IS_WATER,
//	HAS_PEAK and their kin -- therefore has nothing to attach to, and a `plots`-target deposit gated on one
//	delivers NOTHING while erroring nowhere. This info supplies the missing target: the predicate resolves
//	against the terrains named here.
//
//	It exists as DATA rather than an engine table for the reason the whole JSON model exists (json.md par.1: the
//	data reads cold) -- what IS_WATER means is readable in a file, and a new water terrain joins by being named
//	there, with no engine change.
//
//	JSON-fed (Assets/Data/foldtargets/*.json via mapFrom); no XML shell, no legacy field set.
//

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include
#include <string>
#include <vector>

namespace picojson { class value; }

class CvFoldTargetInfo : public CvInfo
{
public:
	CvFoldTargetInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= INTRINSIC -- bare typed reads =======================
	// The authored predicate spelling this set defines ("IS_WATER"). Resolved to its CvCascPredKind by the
	// index that consumes it, through the ONE public speller (cascadeSpellPredKind) rather than a second
	// string->enum map ([DEC-single-implementation]).
	const std::string& getPredicate() const { return m_szPredicate; }

	// The TERRAIN ids this predicate means, in authored order. FK-resolved at mapFrom, so the read is a bare
	// member ([DEC-materialize-at-mapfrom]).
	const std::vector<int>& getTerrains() const { return m_aiTerrains; }

private:
	std::string m_szPredicate;
	std::vector<int> m_aiTerrains;
};

//
//	FoldTargets -- the load-derived index the EVALUATOR asks: does this plot's terrain fall in this predicate's
//	fold set? Purely-organizational static-methods class (patterns.md static-class law): no data members, never
//	instantiated, game-thread only.
//
//	⛔ It answers from TERRAIN, and that is the whole point. The plot-TYPE axis these predicates used to read is
//	not a reliable carrier -- its fact announces for a fraction of the map and never for water -- while terrain
//	announces for every plot. So the fold set is what makes IS_WATER answerable at all.
//	⚑ A predicate NO file defines answers false and is simply unset, never an error: the registry is open
//	(json.md par.8), so a fold set appears when data authors one.
//
class FoldTargets
{
public:
	// Is iTerrain in ePredicate's authored fold set? False when the predicate has no set.
	static bool terrainMatches(CvCascPredKind ePredicate, int iTerrain);
	// Does any authored file define a fold set for this predicate? (The evaluator uses this to tell "not in the
	// set" from "this predicate does not fold through terrain at all", so a non-fold predicate keeps its own read.)
	static bool defines(CvCascPredKind ePredicate);
};

#endif // CV_FOLD_TARGET_INFO_H
