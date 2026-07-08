#pragma once
#ifndef CV_CASCADE_SCALAR_CHANNELS_H
#define CV_CASCADE_SCALAR_CHANNELS_H

//
//	CascadeScalarChannels -- the #430 city SCALAR channels (legacy-value-calc-map §4/§5/§9.5): greatPeopleRate,
//	defense, maintenance -- each computed from the curated deposits (buildings via the operating buildings cache + civics +
//	traits). The slots are FLIPPED (the accumulator's packages feed the engine getters); these calculators serve
//	the /computed decomposition endpoints (fresh recomputes attributed to NAMED terms). DEC-unit-modifiers-on-top
//	holds: nothing unit-sourced enters these (no unit-carried deposits exist in these families).
//

#include "CvCascadeConditionEval.h"
#include <map>

class CvCity;
class CvPlayer;
struct CascadePlayerScope;
struct CascadeBrLedger;

class CascadeScalarChannels
{
public:
	// ===== the SCOPED HALVES (the scope-package fills ride these) =====
	// CITY-REALIZED sums (buildings + civics + traits + techs, THIS city's ctx -- conditioned sums are
	// city-realized joins; only the per-source-city building walks stay player-side).
	static int gpModifierCity(const CvCity* pCity, const CvCascadeEvalCtx& ec);          // gp pcts: bldgs + civic/trait
	static int gpModifierSrCity(const CvCity* pCity, const CvCascadeEvalCtx& ec);        // the SR grouped family (gate live)
	static int maintenanceModifierCity(const CvCity* pCity, const CvCascadeEvalCtx& ec); // maint pcts: bldgs + civic + techs
	static int defenseBombardCity(const CvCity* pCity, const CvCascadeEvalCtx& ec);      // L13: bombard pcts (bldgs)
	static int defenseMinCity(const CvCity* pCity, const CvCascadeEvalCtx& ec);          // L13: the min FLOOR flats (bldgs)
	// the freeSpecialists AMOUNT city half (the ruled two-part seam): this city's active buildings' counts
	static void fillFreeSpecialistsCity(const CvCity* pCity, const CvCascadeEvalCtx& ec,
		int& outAny, std::vector<int>& outByType);
	static int tradeRoutesCity(const CvCity* pCity, const CvCascadeEvalCtx& ec);         // trade flats: bldgs + civic + techs
	static int tradeRoutesCoastalCivCity(const CvCity* pCity, const CvCascadeEvalCtx& ec); // civic coastal flats (gate live)
	// The PLAYER scalar fill: the player-BUILDING sums only (per-source-city ctx).
	static void fillPlayerScalars(const CvPlayer& player, CascadePlayerScope& out);
	// The buildRate LEDGER fills: DENSE per-kind tables indexed by targetFk -> Σ percents (scope-packages
	// CascadeBrLedger). City = this city's active buildings + civics/traits (city ctx) + members + the SR
	// fields; player = all cities' active buildings only.
	static void fillBuildRateCity(const CvCity* pCity, const CvCascadeEvalCtx& ec,
		CascadeBrLedger& outKeyed, int& outMilitary, int& outSpace, int& outSrUnit, int& outSrBuilding,
		int& outWorldWonder, int& outTeamWonder, int& outNationalWonder);
	static void fillBuildRatePlayer(const CvPlayer& player, CascadePlayerScope& out);

	// greatPeopleRate: the city BASE (building + specialist flats; the player national rate is a live input)
	// and the MODIFIER percent stack (city + empire percents incl. state-religion/golden-age-gated entries).
	static int gpRateBase(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// the increment-F component split of gpRateBase (gpRateBase == their sum -- single-source): the building
	// half rides ACCD_SCALAR; the specialist half is its own ACCD_SCALARSPEC component so governor churn
	// never pays the building walks (the CSPEC analogy).
	static int gpBaseBuildings(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	static int gpBaseSpecialists(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	static int gpRateModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// the gpMod parts for attribution (building city+empire / civic+trait city+empire / the SR term)
	static void gpModParts(const CvCity* pCity, const CvCascadeEvalCtx& ec, int& iBld, int& iCivTrait, int& iSr);
	// defense: the building city amount stack (legacy m_iBuildingDefense).
	static int defenseAmount(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// maintenance: the effective modifier percent stack (city + empire + area scopes; building/civic/trait).
	static int maintenanceModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// tradeRoutes: the COUNT sources (§9.5) -- this city's extra + the player-wide global + the coastal
	// half (× this city being coastal) + the project world grants + the live raw inputs (below). The max
	// clamp stays at the getter (the legacy contract).
	static int tradeRouteCount(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// the LIVE raw-input folds (the game vote/WB store + the INITIAL define) -- ONE source shared by the
	// /computed decomposition recompute and the slot combine, so the two never diverge by construction
	static int tradeRouteLiveInputs(const CvCity* pCity);
	// PROJECT world routes (the Internet class): raw team project counts × compiled world flats -- shared
	// by the /computed decomposition recompute and the world-scope package fill
	static int tradeRoutesWorldProjects();
	// buildRate (§9.5): the summed signed-% production modifier for the city's HEAD ORDER item (unit/building/
	// project) -- the target's own bonus-gated buildRate.self + the keyed source mods (units/unitCombats/
	// domains/buildings members, city + empire scopes) + military/space members + the SR grouped family.
	// Returns 0 with no order (bHasOrder=false).
	// The parts split for attribution (the gpModParts pattern) -- the modifier IS the sum of these.
	struct BuildRateParts
	{
		int iSelf;          // buildRate.self on the target (bonus-gated own mods)
		int iKeyed;         // units.{U} / buildings.{B} keyed source mods
		int iDomain;        // domains.{D} keyed
		int iCombatMain;    // unitCombats.{main combat}
		int iCombatSubs;    // Σ unitCombats.{each sub combat}
		int iMember;        // the military | space flat member
		int iStateReligion; // stateReligion.empire.{unit|building}Production
		BuildRateParts() : iSelf(0), iKeyed(0), iDomain(0), iCombatMain(0), iCombatSubs(0), iMember(0), iStateReligion(0) {}
		int total() const { return iSelf + iKeyed + iDomain + iCombatMain + iCombatSubs + iMember + iStateReligion; }
	};
	static int productionModifier(const CvCity* pCity, const CvCascadeEvalCtx& ec, bool& bHasOrder, BuildRateParts* pParts = NULL);
};

#endif // CV_CASCADE_SCALAR_CHANNELS_H
