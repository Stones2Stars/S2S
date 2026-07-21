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
	// The FEATURE happiness/health terms. ASYMMETRIC to match
	// legacy: HAPPINESS = featMember + featSubstrate (civic-per-feature + civic-per-improvement + intrinsic
	// improvement, the legacy getFeatureGoodHappiness bundle); HEALTH = featSubstrate only (feature's own
	// plot.percent -- legacy calculateFeatureHealthPercent is feature-own, civic-per-feature health rides civic).
	static void featureWellbeing(const CvCity* pCity, int& iHapGood, int& iHapBad, int& iHeaGood, int& iHeaBad);
	// The SPECIALIST terms (hap.spec/hea.spec). ×100 sign-split (iGood = positives, iBad = negatives) matching the legacy signs;
	// these getters stay ×100 (their consumers ÷100 at use).
	static void specialistWellbeing(const CvCity* pCity, int& iHapGood, int& iHapBad, int& iHeaGood, int& iHeaBad);
	// The extraBuilding terms (hap.extraB/hea.extraB): civic/trait per-building + the building-keyed "Royal Tomb" leg.
	// Needs the FULL path (gather + playerAreaEmpire + foldBuildingKeyed). ×100 sign-split; the human-scale getters ÷100.
	static void extraBuildingWellbeing(const CvCity* pCity, int& iHapGood, int& iHapBad, int& iHeaGood, int& iHeaBad);
	// The CITY religion-happiness split: each present religion
	// contributes the player state/non-state acc, sign-split. Human (÷100 at the accessor). INITIAL + civics + TRAITS.
	static void religionWellbeing(const CvCity* pCity, int& iGood, int& iBad);
	// The player-scope state / non-state religion-happiness values: INITIAL + Σ adopted civics + Σ active traits
	// (PURE-filtered). Human.
	static int playerStateReligionHappiness(const CvPlayer& player);
	static int playerNonStateReligionHappiness(const CvPlayer& player);
	// The player-scope civic / trait / project / world wellbeing feeders. Fresh
	// deposit-derived recomputes reproducing the verdict terms (civic/trait -> iCivicNet/iTraitNet via a
	// bare ctx; project/world mirror the wb_gather empire/world legs). Signed human (÷100); the consumers
	// do their own max(0)/min(0) split.
	static void civicWellbeing(const CvPlayer& player, int& iHap, int& iHea);      // A1,A2
	static int  civilizationHealth(const CvPlayer& player);                        // A3
	static void projectWellbeing(const CvPlayer& player, int& iHap, int& iHea);    // A5,A6 (own-team getProjectCount>0)
	static void worldWellbeing(const CvPlayer& player, int& iHap, int& iHea);      // A7,A8 (game getProjectCreatedCount>0)
	// The rank-gated CITY largest-city happiness (read via CvCity::getLargestCityHappiness): hap.iLargest already
	// applies the population-rank gate at fill.
	static int  largestCityWellbeing(const CvCity* pCity);                         // A4
	// The per-religion CITY building-sourced state-religion happiness:
	// Σ over the city's ACTIVE buildings whose religion == eReligion of the building's stateReligionHappiness. Human
	// (the building info getter is human) -- keyed by an ARBITRARY religion (the civic what-if
	// reads a non-state religion's slot), so NOT the ×100 verdict term (hap.iSrNet is the CURRENT state religion's
	// building happiness, gathered live). eReligion is an int (ReligionTypes) -- NO_RELIGION returns 0.
	static int cityStateReligionHappiness(const CvCity* pCity, int eReligion);
	// The BUILDING tech-gated wellbeing net: the
	// signed iTechGatedNet term (active buildings' TECH-gated happiness/health deposits). Human (÷100). Specialist
	// tech-happiness is NOT here -- it rides the specialist bucket (hap.spec), per the owner "specialist is its own
	// bucket regardless of source" ruling.
	static void techGatedWellbeing(const CvCity* pCity, int& iHapNet, int& iHeaNet);
	// The AREA/EMPIRE building-HEALTH rollups (the playerAreaEmpire fold, health family). iBad is negative (WbSplit convention).
	static void buildingHealthArea(const CvPlayer& player, int iAreaId, int& iGood, int& iBad);
	static void buildingHealthEmpire(const CvPlayer& player, int& iGood, int& iBad);
	// The AREA/EMPIRE building-HAPPINESS rollups (happiness family). Returns the NET signed value (iGood + iBad)
	// -- the legacy CvArea/CvPlayer getBuildingHappiness is a single signed accumulator, split at read.
	static int buildingHappinessArea(const CvPlayer& player, int iAreaId);
	static int buildingHappinessEmpire(const CvPlayer& player);

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
