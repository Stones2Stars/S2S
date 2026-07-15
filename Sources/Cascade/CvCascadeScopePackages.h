#pragma once
#ifndef CV_CASCADE_SCOPE_PACKAGES_H
#define CV_CASCADE_SCOPE_PACKAGES_H

//
//	The #430 SCOPE PACKAGES -- the modifier substrate as the spec designed it (modifier.md §1 storage
//	semantics + docs/plans/structural-cleanup/scope-packages.md):
//
//	 - ONE package = ONE standing summed number, identity (scope × combine-position × channel × unit-kind).
//	   Flat packages and percent packages are always SEPARATE fields (the unit is part of the slot key,
//	   modifier.md §2) -- a flat-only event never touches a percent package, structurally.
//	 - Each scope object holds ONLY its own scope's deposits: CvCity the *.city.* sums, CvPlayer the
//	   *.empire.* / *.area.* sums (area maps grouped per area -- promoted to CvArea when a second channel
//	   needs it), CvGame the *.world.* sums. A lower scope never stores an upper scope's sums.
//	 - Every struct sits on ONE CvDerivedCacheSet bound to its object: events mark, boundaries ensure,
//	   reads are bare fetches + the channel's combine formula (family positions realized AS the field
//	   layout below -- the family-metadata table's storage half; polarity/floors live in the combine fns).
//	 - Gated sums (state-religion / coastal / connected / golden-age) are SEPARATE fields, stored UNGATED;
//	   their per-city/live gates apply at read -- the gate flipping never invalidates anything.
//	 - Two REALIZED-JOIN packages live city-side by design (documented in scope-packages.md): the
//	   commerce buildingKeyed realization (player grantor-ledger × this city's active set) and the civic
//	   building-keyed percent -- city-scope realizations of cross-scope joins, marked by their derived
//	   masks, never raw upper-scope sums.
//
//	Never serialized; all-dirty from birth/reset; the load warm-up is the same ensure run eagerly.
//

#include "Defines/CvEnums.h"
#include "Infrastructure/CvDerivedCache.h"
#include <map>
#include <vector>

class CvCity;
class CvPlayer;
class CvGame;

// ===== the keyed buildRate LEDGER, DENSE (the precipice-§5 hot-path fix) =====
// The AI's production scoring reads this per (item × city); the former std::map<long,int> paid two
// red-black-tree finds per read PLUS an enum→segment conversion. The dense form obeys generic-code-
// static-storage: ONE array index by the GAME ENUM id per scope, nothing else. The fill routes each
// keyed deposit by its member kind (units/buildings/domains/unitCombats) + its FK-resolved targetFk
// (match-equivalent to the segment matching: both derive from the same INFOTYPE string). Vectors are
// sized to the live info counts at each fill (assign zero-fills -- contract rule 2).
struct CascadeBrLedger
{
	std::vector<int> units;            // by UnitTypes
	std::vector<int> buildings;        // by BuildingTypes
	std::vector<int> domains;          // by DomainTypes
	std::vector<int> unitCombats;      // by UnitCombatTypes
	std::vector<int> specialBuildings; // by SpecialBuildingTypes (the L11 trait keyed leg, 2026-07-05)
	static int at(const std::vector<int>& v, int i) { return (i >= 0 && i < (int)v.size()) ? v[i] : 0; }
};

// ===== the signed-split pair (modifier.md §2b; StoneBase Split) =====
struct WbSplit
{
	int iGood, iBad;
	WbSplit() : iGood(0), iBad(0) {}
	void fold(int v) { if (v >= 0) iGood += v; else iBad += v; }
};

// The per-family CITY-scope wellbeing term set (the §2b deposit-derived terms this city's sources produce;
// the area/empire building splits live PLAYER-side; the raw-state anger/timer inputs are LIVE at read).
struct CascadeWbTerms
{
	WbSplit bld;           // building city base (per-building NET fold) + the event ledger
	WbSplit bonus;         // bonus empire flats (city presence) + BONUS-gated building/civic/trait entries
	WbSplit extraB;        // civic/trait buildings.{B} keyed × this city's active set
	WbSplit featMember;    // civic/trait features.{F} keyed × radius feature counts
	WbSplit featSubstrate; // feature-info plot.percent per radius feature (÷100 per-feature fold)
	WbSplit corp;          // corporation city flats (per-corp fold)
	WbSplit project;       // project empire + world flats (per-project fold)
	WbSplit spec;          // specialist city flats × count (×100 pools folded ÷100 per type)
	int iCivicNet;         // civic plain empire flats, NET (city-conditioned joins -- realized here)
	int iTraitNet;         // trait plain empire flats, NET
	int iTechNet;          // tech empire flats, NET
	int iSrNet;            // building {STATE_RELIGION:X}-gated, NET
	int iTechGatedNet;     // building {TECH_X}-gated, NET
	int iMilitary;         // the per-military-unit VALUE (× the LIVE count at read -- rides on top)
	int iLargest;          // the ranked `cities` member (rank gate evaluated at fill)
	int iPpPct;            // perPopulation percent pool (× live pop ÷100 at read)
	CascadeWbTerms() : iCivicNet(0), iTraitNet(0), iTechNet(0), iSrNet(0), iTechGatedNet(0),
		iMilitary(0), iLargest(0), iPpPct(0) {}
	void reset() { *this = CascadeWbTerms(); }
private:
	// (plain value struct -- copy is fine; the reset() above rides the default copy)
};

// ===== the CITY packages (dirty bits) =====
// Flat-kind and percent-kind packages carry SEPARATE bits so the derived masks can split percent-vs-flat.
enum CascadeCityPkg
{
	CPK_YPCT    = 1,     // yield percent packages, city scope (+ the civic building-keyed realization)
	CPK_YSPEC   = 2,     // yield specialist flat packages (BASE tier)
	CPK_YEXTRA  = 4,     // yield building flat packages ×100 (EXTRA tier, incl. perPopulation)
	CPK_CSPEC   = 8,     // commerce specialist flat packages ×100 (BASE side)
	CPK_CPCT    = 16,    // commerce percent packages, city scope
	CPK_CBASE   = 32,    // commerce city base-term packages (religion/corp/own/shrine/corpHQ/double/keyed/srMatch)
	CPK_WB      = 64,    // the wellbeing city term packages (both families + the commerce-happiness pools)
	CPK_SCFLAT  = 128,   // scalar flat packages: gpBase buildings, tradeRoutes city
	CPK_SCPCT   = 256,   // scalar percent packages: gpMod city, defense amount, maintenance city
	CPK_SCSPEC  = 512,   // the gpBase specialist flat package (governor churn touches ONLY this)
	CPK_BR      = 1024,  // the buildRate city ledgers + city member percents
	// (the enabler frontiers -- buildable/trainable/creatable/maintainable -- live on the standardized enabler
	// domains, CvCity::m_enabler / CvPlayer::m_enabler, event-maintained; enabler.md par.7/8. No CPK bit.)
	CPK_RATES   = CPK_YPCT | CPK_YSPEC | CPK_YEXTRA | CPK_CSPEC | CPK_CPCT | CPK_CBASE,   // = 63
	CPK_ALL     = 2047,
	CPK_EAGER   = CPK_ALL
};

struct CascadeCityPackages
{
	// -- yields, per channel (positions per modifier.md §2a: SPEC = BASE tier, EXTRA = after-percent tier) --
	long yPctCity[NUM_YIELD_TYPES];      // the WHOLE §2a percent stack, CITY-REALIZED (raw Σ: city+area+empire
	                                     // buildings + civics + traits + projects + civic-keyed, THIS city's ctx --
	                                     // percent conditions reference the city, so the stack is a city-realized join)
	long ySpec[NUM_YIELD_TYPES];         // specialist flats (human; own sub-stack resolved inside)
	long yExtra100[NUM_YIELD_TYPES];     // building flats + perPopulation (×100)
	// -- commerce, per channel --
	long cSpec100[NUM_COMMERCE_TYPES];   // specialist terms ×100
	long cPct[NUM_COMMERCE_TYPES];       // the WHOLE commerce percent stack, CITY-REALIZED (raw Σ, as yPctCity)
	long cBaseOwn100[NUM_COMMERCE_TYPES];// religion + corporation + building-own + shrine + corpHQ + doubleTime (×100)
	long cKeyed100[NUM_COMMERCE_TYPES];  // the buildingKeyed REALIZATION (player grantor-ledger × city active, ×100)
	int  iCSrMatch;                      // active buildings matching the state religion (× the player SR pool at read)
	// -- wellbeing (city terms + the STORED verdicts -- assembled at FILL, the ruled end-turn cadence;
	// reads are bare fetches + the live military fold on top) --
	CascadeWbTerms wbHap, wbHea;
	int aiWbCommercePer[NUM_COMMERCE_TYPES];   // commerce-happiness pools (folded at fill)
	int aWbVerdict[4];                         // happy/unhappy/good/bad, MILITARY-FREE (military live at read)
	// -- the scalar CITY-REALIZED halves (buildings + civics + traits + techs, THIS city's ctx -- percent/
	// conditioned sums are city-realized joins; only the per-source-city building walks stay player-side) --
	int scGpBaseBld;                     // greatPeopleRate.city building flats
	int scGpBaseSpec;                    // greatPeopleRate specialist flats × counts
	int scGpModCity;                     // gp percents: city buildings + civic/trait city+empire (city ctx)
	int scGpModSr;                       // the SR grouped family (civics, city ctx; × SR-in-city gate at read)
	int scDefense;                       // defense.city.amount percents (buildings)
	// the L13 defense wiring (owner-ruled shape, 2026-07-05): the additive bombard percents + the min FLOOR
	int scDefBombard;                    // defense.city.bombardDefense percents (buildings; legacy m_iBuildingBombardDefense)
	int scDefMin;                        // defense.city.min flats (buildings; legacy m_iExtraMinDefense)
	int scMaintModCity;                  // maintenance percents: city buildings + civic city/empire/area + techs (city ctx)
	int scTradeCity;                     // tradeRoutes flats: city buildings + civic empire + techs (city ctx)
	int scTradeCoastalCiv;               // civic coastal flats (city ctx; × the coastal gate at read)
	int brSrUnitProd, brSrBuildingProd;  // stateReligion.empire.{unit|building}Production (civics, city ctx; SR gate live)
	// -- buildRate: the city keyed LEDGER (dense per-kind tables, read by game enum id) + city members --
	CascadeBrLedger brCityKeyed;
	int brCityMilitary, brCitySpace;     // buildRate.city.{military|space} percents
	// the WONDER-CATEGORY members (the L11 trait walks, 2026-07-05: trait-authored only in data; civic/trait
	// city-realized walks -- the legacy maxGlobal/maxTeam/maxPlayer accumulators' class)
	int brCityWorldWonder, brCityTeamWonder, brCityNationalWonder;
	// -- the freeSpecialists AMOUNT halves (the ruled two-part seam, 2026-07-05: the cascade owns the
	// AMOUNTS -- summed alive-with-source deposits; the engine owns PLACEMENT; consumers read the
	// placement OUTPUT). City-scope: this city's active buildings' freeSpecialists.city.* counts --
	int fsCityAny;                       // freeSpecialists.city.any counts
	std::vector<int> fsCityByType;       // freeSpecialists.city.{SPECIALIST_X} counts, by SpecialistTypes

	// -- the ENABLER frontier (#430 THE FLIP, owner 2026-07-04): the harness-proven availability sets,
	CvDerivedCacheSet<CvCity> set;       // the ONE dirty protocol (bind in CvCity's ctor)

	CascadeCityPackages()
	{
		for (int y = 0; y < NUM_YIELD_TYPES; ++y) { yPctCity[y] = 0; ySpec[y] = 0; yExtra100[y] = 0; }
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
			{ cSpec100[c] = 0; cPct[c] = 0; cBaseOwn100[c] = 0; cKeyed100[c] = 0; aiWbCommercePer[c] = 0; }
		iCSrMatch = 0;
		for (int w = 0; w < 4; ++w) aWbVerdict[w] = 0;
		scGpBaseBld = 0; scGpBaseSpec = 0; scGpModCity = 0; scGpModSr = 0; scDefense = 0;
		scDefBombard = 0; scDefMin = 0;
		scMaintModCity = 0; scTradeCity = 0; scTradeCoastalCiv = 0;
		brSrUnitProd = 0; brSrBuildingProd = 0;
		brCityMilitary = 0; brCitySpace = 0;
		brCityWorldWonder = 0; brCityTeamWonder = 0; brCityNationalWonder = 0;
		fsCityAny = 0;
	}
};

// ===== the PLAYER packages (dirty bits) =====
// PERCENT stacks live CITY-REALIZED (their conditions reference the city -- the Burdigala class); the player
// scope holds only the genuinely city-agnostic sums: the per-source-city building walks, the flats, the
// pools, the ledgers, the gated fields.
enum CascadePlayerPkg
{
	PSC_YFLAT  = 1,    // yield free-city flats + golden-age flats (UNGATED -- the GA gate is live at read)
	PSC_CFLAT  = 2,    // commerce player-extra + GA (ungated) + the SR pools + the buildingKeyed grantor ledgers
	PSC_WB     = 4,    // the wellbeing area/empire building fold maps
	PSC_SC     = 8,    // the scalar player-building sums (gp/maint/conn/area maps/trade empire+coastal+world)
	PSC_BR     = 16,   // the buildRate empire building ledgers + building member pcts
	// the ENABLER player frontier -- SPLIT (the perf surgery): the promo tech halves are a 700-tech
	// accumHave walk only promotion picks need; researchable/civics/hurries/buildRem fill together
	PSC_FRONT_P     = 32,  // the hurries gate set (the one remaining box frontier slice)
	PSC_FRONTIER = PSC_FRONT_P,
	PSC_ALL    = 63,
	PSC_EAGER  = PSC_ALL & ~PSC_FRONTIER   // the frontier is LAZY (ensure-on-read only)
};

struct CascadePlayerScope
{
	// -- yields --
	long yFlatFreeCity[NUM_YIELD_TYPES]; // trait {ch}.empire flats
	long yFlatGoldenAge[NUM_YIELD_TYPES];// trait {ch}.empire.goldenAge flats, UNGATED (max(0,·)+gate at read)
	// -- commerce --
	long cPlayerExtra100[NUM_COMMERCE_TYPES];  // trait + heritage empire flats ×100
	long cGoldenAge[NUM_COMMERCE_TYPES];       // trait GA member flats (human), UNGATED
	long cSrPool[NUM_COMMERCE_TYPES];          // the state-religion commerce POOL (× the city's match count at read)
	std::map<int, long> cKeyedLedger[NUM_COMMERCE_TYPES];  // targetFk -> Σ count(grantor)×value100
	// -- wellbeing: the player-wide building fold maps (famSeg -> areaId -> split; famSeg -> empire split) --
	std::map<int, std::map<int, WbSplit> > wbAreaByFam;
	std::map<int, WbSplit> wbEmpireByFam;
	std::map<int, std::map<int, int> > wbBuildingKeyedByFam; // famSeg -> targetFk -> Σ flats (the Royal-Tomb class)
	// -- the scalar player-BUILDING sums (each walked per source city with that city's own ctx) --
	int gpModPlayer;                     // greatPeopleRate.empire building pcts
	int gpNationalFlat;                  // trait national GP flats (greatPeopleRate.empire.units.{GP}.flat, Σ all
	                                     // keys -- the m_iNationalGreatPeopleRate class; the L6 census fold 2026-07-05)
	int defPlayerAll;                    // defense.empire.amount percents: buildings + civics + traits (the L13
	                                     // wiring -- the legacy getCityDefenseModifier three-accumulator class)
	int defPlayerBombard;                // defense.empire.bombardDefense percents (traits) -- legacy
	                                     // m_iNationalBombardDefenseModifier; composes into getBuildingBombardDefense
	int maintPlayerAll;                  // maintenance.empire building pcts
	int maintConnPct;                    // connectedCity pcts (× connected-and-not-capital gate at read)
	std::map<int, int> maintAreaPct;     // areaId -> own-area pcts
	std::map<int, int> maintOtherAreaPct;// areaId -> otherArea pcts CONTRIBUTED BY that area's cities
	int maintOtherAreaTotal;
	int tradeEmpireAll;                  // building empire flats
	int tradeCoastalAll;                 // building coastal flats (× coastal gate at read)
	int tradeWorldMine;                  // THIS player's world-scope flats (the world package sums these)
	// -- the freeSpecialists AMOUNT empire/area halves (the ruled seam; civics + traits + empire buildings;
	// the area.any buildings fold per SOURCE-city area, the maintAreaPct precedent) --
	int fsEmpireAny;
	std::vector<int> fsEmpireByType;     // by SpecialistTypes
	std::map<int, int> fsAreaAny;        // areaId -> any counts (2 authorings; area BY-TYPE has none -- census guard)
	// -- buildRate (building-sourced halves only) --
	CascadeBrLedger brEmpKeyed;          // dense per-kind tables -> Σ pcts (all cities' active buildings)
	int brEmpMilitary, brEmpSpace;       // empire member pcts (buildings)
	// -- the ENABLER player frontier (#430 THE FLIP): the hurries set (the harness-proven bare-player-ctx
	// fill -- hurries are NOT an enabler domain, enabler.md par.7.1; this box slice serves canHurry until the
	// civic-ability model consumes it). TECHS + CIVICS + BUILDS + PROMOTIONS left this package: their lists
	// are the STANDARDIZED enabler's maintained vectors (CvPlayer::m_enabler.*) --
	std::set<int> enHurryOk;

	CvDerivedCacheSet<CvPlayer> set;     // bind in CvPlayer's ctor

	CascadePlayerScope()
	{
		for (int y = 0; y < NUM_YIELD_TYPES; ++y) { yFlatFreeCity[y] = 0; yFlatGoldenAge[y] = 0; }
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
			{ cPlayerExtra100[c] = 0; cGoldenAge[c] = 0; cSrPool[c] = 0; }
		gpModPlayer = 0; gpNationalFlat = 0; defPlayerAll = 0; defPlayerBombard = 0; maintPlayerAll = 0; maintConnPct = 0; maintOtherAreaTotal = 0;
		tradeEmpireAll = 0; tradeCoastalAll = 0; tradeWorldMine = 0;
		fsEmpireAny = 0;
		brEmpMilitary = 0; brEmpSpace = 0;
	}
};

// ===== the WORLD packages =====
enum CascadeWorldPkg { WSC_ALL = 1 };

struct CascadeWorldScope
{
	int tradeWorldFlat;                  // Σ living players' tradeWorldMine (world wonders grant EVERY player)
	CvDerivedCacheSet<CvGame> set;       // bind in CvGame's reset (idempotent)
	CascadeWorldScope() : tradeWorldFlat(0) {}
};

#endif // CV_CASCADE_SCOPE_PACKAGES_H
