#pragma once
#ifndef CV_CASCADE_WELLBEING_H
#define CV_CASCADE_WELLBEING_H

//
//	CascadeWellbeing -- the #430 modifier machine WELLBEING channel (modifier.md §2b): the city's four realized
//	health/happiness verdicts computed from the curated deposit data + the raw-state inputs, transcribed from the
//	StoneBase assembler that reached attributed parity (WellbeingLevels.cs -- the source-completeness proof; the
//	remaining legacy divergence classes are the improvement BALANCE-CUT and the STORED-ACCUMULATOR DRIFT, both
//	documented in modifier.md §2b as engine-wrong/attributed-accepted).
//
//	The wellbeing slots are LIVE in the accumulator (CvCascadeAccumulator's standing scope packages feed the
//	engine getters); compute() is the fresh full recompute serving the /computed/cities/wellbeing decomposition
//	endpoint (the attribution surface).
//

#include "CvCascadeConditionEval.h"
#include "CvCascadeScopePackages.h"   // CascadeWbTerms + WbSplit -- the shared §2b term types
#include <map>

class CvCity;
class CvPlayer;

struct CascadeWellbeingVerdicts
{
	// ⚠ MILITARY-FREE verdicts (owner ruling 2026-07-03): the unit-count happiness NEVER enters the cached
	// computation -- it rides ALONE on top at read time (perUnit × the live O(1) engine counter), so unit
	// moves invalidate nothing. iMilPerUnit is the refresh-stable civic/trait per-military-unit VALUE.
	int iHappy;      // happyLevel WITHOUT the military term
	int iUnhappy;    // unhappyLevel WITHOUT the military term
	int iGood;       // goodHealth (no military term exists)
	int iBad;        // badHealth (no military term exists)
	int iMilPerUnit; // Σ civic/trait `unit: IS_MILITARY`-qualified cities values (× the live count at read, json §3.7)
	CascadeWellbeingVerdicts() : iHappy(0), iUnhappy(0), iGood(0), iBad(0), iMilPerUnit(0) {}
};

class CascadeWellbeing
{
public:
	// The CITY-scope §2b gather (both families + the commerce-happiness pools) -- the scope-package fill AND
	// the calculator's city half (single-source, patterns.md). No area/empire building terms (player-scope).
	static void gatherCityTerms(const CvCity* pCity, const CvCascadeEvalCtx& ec,
		CascadeWbTerms& hap, CascadeWbTerms& hea, int aiCommercePer[/*NUM_COMMERCE_TYPES*/]);

	// Per-source §2b decomposition terms for the legacy sub-getters (live gather; the VERDICT stays the cached
	// path). Lets getBonus*/getBuilding* stand on the cascade instead of the retired stored accumulators.
	// iGood = Σ positive contributions, iBad = Σ negative contributions (the WbSplit convention == the legacy split).
	static void bonusWellbeing(const CvCity* pCity, int& iHapGood, int& iHapBad, int& iHeaGood, int& iHeaBad);
	// The CITY-scope building term (hap.bld/hea.bld): active buildings' city flat/perPop deposits + the
	// per-building event ledger (getBuildingHappy/HealthChange). The area/empire building rollups are the
	// SEPARATE playerAreaEmpire fold (CvArea/CvPlayer getters), not this term.
	static void buildingWellbeing(const CvCity* pCity, int& iHapGood, int& iHapBad, int& iHeaGood, int& iHeaBad);

	// The PLAYER-scope area/empire building fold maps (famSeg -> areaId -> split; famSeg -> empire split)
	// + the building-KEYED ledger (famSeg -> targetFk -> Σ flats: the Royal-Tomb class, a BUILDING granting
	// happiness/health to every city holding the KEYED building -- legacy player extraBuilding* accumulators),
	// both families in one player-city walk -- the CvPlayer package fill AND the calculator's fresh walk.
	static void playerAreaEmpire(const CvPlayer& player,
		std::map<int, std::map<int, WbSplit> >& areaByFam, std::map<int, WbSplit>& empireByFam,
		std::map<int, std::map<int, int> >& keyedByFam);

	// The per-city realization of the building-keyed ledger: an entry pays where its KEYED building is
	// ACTIVE, folded into the same extraB term the civic/trait keyed leg uses (ONE term, one attribution).
	static void foldBuildingKeyed(const std::map<int, std::map<int, int> >& keyedByFam,
		const CvCascadeEvalCtx& ec, CascadeWbTerms& hap, CascadeWbTerms& hea);

	// The VERDICT ASSEMBLY -- the four engine bodies (happyLevel/unhappyLevel/goodHealth/badHealth),
	// term-substituted, PURE over its inputs + the live raw-state reads (anger percents, timers, gate flags).
	// ONE implementation fed its inputs (patterns.md): the package combine feeds packaged terms; compute()
	// feeds fresh ones.
	static CascadeWellbeingVerdicts assemble(const CvCity* pCity,
		const CascadeWbTerms& hap, const CascadeWbTerms& hea, const int aiCommercePer[],
		const WbSplit& hapArea, const WbSplit& hapEmp, const WbSplit& heaArea, const WbSplit& heaEmp);

	// The four verdicts from CURRENT state (the /computed decomposition recompute): fresh city gather + fresh
	// player area/empire walk + the assembly.
	static CascadeWellbeingVerdicts compute(const CvCity* pCity, const CvCascadeEvalCtx& ec);
};
// compute() is the /computed/cities/wellbeing decomposition path -- the fresh recompute the endpoint emits so a
// wellbeing value attributes to NAMED terms (the live reads ride the accumulator's standing packages).

#endif // CV_CASCADE_WELLBEING_H
