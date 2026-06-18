#pragma once
#ifndef CV_CASCADE_CONDITION_H
#define CV_CASCADE_CONDITION_H

#include <vector>
#include "CvCascadeTally.h"  // CountScope, CountDomain, cascadeTally()

//
//	The shared, UNIFORM CONDITION vocabulary (data-model-spec §2.4) -- the in-memory shape readJson materializes a
//	`requires` / `enabled` / `disabled` object into, evaluated by BOTH the enabler gate and (later) the modifier
//	conditioning. ONE serialization, one evaluator. This is a NEW, independent implementation (the #430 shadow
//	surface): it reads ground-truth STATE primitives (does the city have building X, does the plot have a river)
//	but reimplements the COMBINATION/availability logic from the JSON -- it never threads through the legacy
//	canConstruct/canTrain/... gate internals (those are only the shadow's comparison oracle).
//
//	A leaf is EITHER a COUNT ATOM over a domain/scope, OR a system PREDICATE. The boolean tree is bounded:
//	all (AND) · any (AND-of-OR-groups) · noneOf (none present).
//

// The JSON type-prefix category an atom counts. The tally backs BUILDING/UNIT at cross-city scopes; the rest are
// read live (city/player/team/plot) at the clause's scope. Decoupled from the tally's CountDomain so the atom
// vocabulary can be broad without the tally needing a bucket for every kind.
enum AtomDomain
{
	ATOMDOMAIN_BUILDING = 0,
	ATOMDOMAIN_UNIT,
	ATOMDOMAIN_TECH,
	ATOMDOMAIN_BONUS,
	ATOMDOMAIN_CIVIC,
	ATOMDOMAIN_RELIGION,
	ATOMDOMAIN_CORPORATION,
	ATOMDOMAIN_HERITAGE,   // per-player acquired heritage (CvPlayer::hasHeritage); empire scope
	ATOMDOMAIN_POPULATION, // catch-all token: the context city's population
	ATOMDOMAIN_CITYCOUNT,  // catch-all token: number of cities (empire/team)
	NUM_ATOM_DOMAINS
};

// Resource connection requirement (BONUS atoms only). Mirrors the JSON `connection` axis.
enum ConnReq { CONN_NONE = 0, CONN_TRADE, CONN_VICINITY, CONN_TRADE_OR_VICINITY };

// count(domain,type,scope) satisfied while iMin <= count <= iMax. iMax < 0 == no upper bound. iMin default 1
// (presence). POPULATION/CITYCOUNT ignore iType (token domains).
struct CvCountAtom
{
	AtomDomain eDomain;
	int        iType;
	CountScope eScope;
	int        iMin;
	int        iMax;
	ConnReq    eConn;

	CvCountAtom()
		: eDomain(ATOMDOMAIN_BUILDING), iType(-1), eScope(COUNTSCOPE_CITY), iMin(1), iMax(-1), eConn(CONN_NONE) {}
	CvCountAtom(AtomDomain d, int t, CountScope s, int lo, int hi, ConnReq c = CONN_NONE)
		: eDomain(d), iType(t), eScope(s), iMin(lo), iMax(hi), eConn(c) {}
};

// A system's runtime-state query (object-evaluated, enabler-spec §3). Each system owns its predicate(s); an
// unknown predicate is DROPPED at parse (never reaches here), so the engine never has to special-case "false".
enum PredicateKind
{
	PRED_IS_CAPITAL = 0,
	PRED_HAS_POWER,
	PRED_HAS_STATE_RELIGION,
	PRED_STATE_RELIGION_IN_CITY,    // player has a state religion AND this city has it (legacy needStateReligionInCity)
	PRED_IS_COASTAL,
	PRED_HAS_RIVER,
	PRED_IS_WATER,
	PRED_IS_HILLS,
	PRED_IS_PEAK,
	PRED_IS_FLATLANDS,
	PRED_IS_FRESHWATER,
	PRED_HAS_IRRIGATION,
	PRED_HAS_FEATURE_ANY,           // "has ANY feature"
	PRED_HAS_FEATURE,               // iParam = FeatureTypes
	PRED_HAS_TERRAIN,               // iParam = TerrainTypes
	PRED_HAS_BONUS,                 // iParam = BonusTypes (plot has the bonus)
	PRED_HAS_RELIGION,              // iParam = ReligionTypes (city)
	PRED_STATE_RELIGION,            // iParam = ReligionTypes (player's state religion)
	PRED_HOLY_CITY,                 // iParam = ReligionTypes (city is its holy city)
	PRED_HAS_CORPORATION,           // iParam = CorporationTypes (city)
	NUM_PREDICATE_KINDS
};

struct CvPredicate
{
	PredicateKind eKind;
	int           iParam; // type index for parameterized predicates, else -1
	CvPredicate() : eKind(PRED_IS_CAPITAL), iParam(-1) {}
	CvPredicate(PredicateKind k, int p) : eKind(k), iParam(p) {}
};

// A condition leaf: a count atom OR a predicate.
struct CvCascadeLeaf
{
	bool        bPredicate;
	CvCountAtom atom;
	CvPredicate pred;
	CvCascadeLeaf() : bPredicate(false) {}
};

// A BOUNDED boolean tree (no arbitrary nesting): all (AND), any (AND of OR-groups), noneOf (none present).
struct CvCascadeCondition
{
	std::vector<CvCascadeLeaf>               all;
	std::vector<std::vector<CvCascadeLeaf> > any;
	std::vector<CvCascadeLeaf>               noneOf;

	bool isEmpty() const { return all.empty() && any.empty() && noneOf.empty(); }
};

// The evaluation context: the player being gated, and (for city/plot scope + predicates) a specific city.
struct CvCascadeContext
{
	int iPlayer; // the player being gated; -1 if none
	int iCity;   // city id within iPlayer (city/plot-scope reads + predicates); -1 = no specific city
	CvCascadeContext(int iPlayer_ = -1, int iCity_ = -1) : iPlayer(iPlayer_), iCity(iCity_) {}
	int contextFor(CountScope eScope) const; // the iContext the tally wants at this (cross-city) scope
};

// Evaluators (forward state reads; never an upward cascade walk, never a legacy-gate call).
int  cascadeAtomCount(const CvCountAtom& kAtom, const CvCascadeContext& kCtx); // the raw count/presence
bool cascadeEvalAtom(const CvCountAtom& kAtom, const CvCascadeContext& kCtx);
bool cascadeEvalPredicate(const CvPredicate& kPred, const CvCascadeContext& kCtx);
bool cascadeEvalLeaf(const CvCascadeLeaf& kLeaf, const CvCascadeContext& kCtx);
bool cascadeEvalCondition(const CvCascadeCondition& kCond, const CvCascadeContext& kCtx);

// The non-boolean count-vocabulary the modifier/enabler also need (tally-backed cross-city counts):
int  cascadePerValue(CountDomain eDomain, int iType, CountScope eScope, int iEach, const CvCascadeContext& kCtx);
bool cascadeWithinAllowed(CountDomain eDomain, int iSelfType, CountScope eScope, int iCap, const CvCascadeContext& kCtx);

#endif // CV_CASCADE_CONDITION_H
