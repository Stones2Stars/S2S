#pragma once
#ifndef CV_CASCADE_PROPERTY_BRIDGE_H
#define CV_CASCADE_PROPERTY_BRIDGE_H

//
//	CascadePropertyBridge -- the JSON->legacy-expression translator for the KEEP-legacy property engine
//	(property-audit.md increment 4). The poco load bridges (CvBuildingInfo/CvUnitInfo/CvPropertyInfo mapFrom) feed
//	curated PROPERTY_* deposits into CvPropertyManipulators as per-turn sources; a CONDITIONED deposit (`enabled`/
//	`disabled`) and a per-POPULATION amount need the legacy solver's own expression trees (BoolExpr / IntExpr),
//	which only an XML read() could build before this. Scoped, NOT a general bridge: the known predicate set
//	(tech/bonus/building/civic/religion/corporation presence + the four diffuse tag-gates) translates; anything
//	else returns NULL and the caller SKIPS the source -- fail closed, never over-apply.
//	One implementation per translation (docs/architecture/patterns.md §DRY (single implementation)); pure static functions, no state.
//

#include <string>

class BoolExpr;
class IntExpr;
class CvCondition;
class CvModEntry;
class CvModifiers;
class CvTriggers;
class CvPropertyManipulators;

class CascadePropertyBridge
{
public:
	// A typed condition tree -> a legacy BoolExpr the solver evaluates via CvGameObject::hasGOM. Presence atoms
	// map by type prefix (TECH_/BONUS_/BUILDING_/CIVIC_/RELIGION_/CORPORATION_ -> BoolExprHas(GOM_*, id), plain
	// presence only); groups fold (all -> And, anyOf -> Or, noneOf -> Not(Or)). NULL = untranslatable (a count
	// threshold, a connection, an unmapped kind) -- the caller skips the source. Caller owns the returned tree.
	static const BoolExpr* condToBoolExpr(const CvCondition* pCond);

	// An entry's ACTIVE gate: enabled AND NOT disabled, composed from the entry's condition fields. Returns NULL
	// for an unconditioned entry (apply always) AND for an untranslatable one (skip) -- *pbUntranslatable tells
	// them apart. Caller owns the returned tree.
	static const BoolExpr* entryActiveExpr(const CvModEntry* pEntry, bool* pbUntranslatable);

	// A bare predicate STRING (the `properties.diffuse[].enabled` gate -- these strings deliberately do not parse
	// into CvCondition) -> BoolExprIs on the matching legacy tag. NULL = unknown string (caller skips).
	static const BoolExpr* predStringToBoolExpr(const std::string& szPred);

	// A per-POPULATION amount: value x POPULATION / each (the each==1 divide is elided). Caller owns the tree.
	static const IntExpr* perPopulationAmount(int iValue, int iEach);

	// Free an unconsumed tree from a caller that only sees the forward declaration (deleting an incomplete
	// type is UB; the complete type lives here).
	static void discard(const BoolExpr* pExpr);

	// THE ONE per-poco family walk (docs/architecture/patterns.md §DRY (single implementation)): every PROPERTY_X.{city|plot}.flat entry of
	// pMods becomes a per-turn Constant/AttributeConstant source in kTarget, carrying eRelation/iRelationData
	// (the category's legacy delivery shape: buildings NO_RELATION, units/promotions/specialists
	// RELATION_SAME_PLOT, civics/traits/heritages/handicaps RELATION_ASSOCIATED — player-gathered, fanned to
	// every associated city). PROPERTY_X.empire.flat routes to pEmpireTarget when given (the buildings'
	// all-cities container), else skips. Conditioned entries ride entryActiveExpr; untranslatable ones skip
	// (fail closed). EVERY skip announces on the readJson census (jsonNoteUnconsumed, attributed to
	// szSourceType) — fail-closed AND silent is the invisible-on-both-axes state triggers.md bans.
	// Clears kTarget (and pEmpireTarget) first — the CvInfo.h mapFrom idempotency contract.
	static void bridgeFamilies(const CvModifiers* pMods, CvPropertyManipulators& kTarget,
		RelationTypes eRelation, int iRelationData = 0, CvPropertyManipulators* pEmpireTarget = 0,
		const char* szSourceType = 0);

	// The `triggers` PROPERTY pulse walk (features/improvements — json §5 property-delta actions, curated from
	// the legacy plot manipulators): every plain per-turn onTurn pulse becomes a Constant source with its
	// authored spatial intent (on/relation/distance). Chance-rolled, interval>1, or non-turn entries skip
	// (fail closed — none authored), each skip announced on the same census. Clears kTarget first (idempotency).
	static void bridgePulses(const CvTriggers* pTriggers, CvPropertyManipulators& kTarget,
		const char* szSourceType = 0);

private:
	CascadePropertyBridge();   // purely-organizational static-methods class -- never instantiated
};

#endif // CV_CASCADE_PROPERTY_BRIDGE_H
