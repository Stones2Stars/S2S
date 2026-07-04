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

class CvCity;
class CvPlayer;
class CvGame;

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
	CPK_FRONTIER = 2048, // the ENABLER frontier sets (buildable/trainable/creatable/maintainable) -- the flip's serving cache
	CPK_RATES   = CPK_YPCT | CPK_YSPEC | CPK_YEXTRA | CPK_CSPEC | CPK_CPCT | CPK_CBASE,   // = 63
	CPK_ALL     = 4095
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
	int scMaintModCity;                  // maintenance percents: city buildings + civic city/empire/area + techs (city ctx)
	int scTradeCity;                     // tradeRoutes flats: city buildings + civic empire + techs (city ctx)
	int scTradeCoastalCiv;               // civic coastal flats (city ctx; × the coastal gate at read)
	int brSrUnitProd, brSrBuildingProd;  // stateReligion.empire.{unit|building}Production (civics, city ctx; SR gate live)
	// -- buildRate: the city keyed LEDGER (key = (memberSeg<<20)|keySeg, both compiled ints) + city members --
	std::map<long, int> brCityKeyed;
	int brCityMilitary, brCitySpace;     // buildRate.city.{military|space} percents
	// -- the ENABLER frontier (#430 THE FLIP, owner 2026-07-04): the harness-proven availability sets,
	// served by the flipped can* gates via ensure-on-read (the FACTS idiom, deliberately NOT the rates'
	// bare fetch: gate reads are decision-time and legacy chains builds within a turn) --
	std::set<int> enBuildable, enTrainable, enCreatable, enMaintainable;

	CvDerivedCacheSet<CvCity> set;       // the ONE dirty protocol (bind in CvCity's ctor)

	CascadeCityPackages()
	{
		for (int y = 0; y < NUM_YIELD_TYPES; ++y) { yPctCity[y] = 0; ySpec[y] = 0; yExtra100[y] = 0; }
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
			{ cSpec100[c] = 0; cPct[c] = 0; cBaseOwn100[c] = 0; cKeyed100[c] = 0; aiWbCommercePer[c] = 0; }
		iCSrMatch = 0;
		for (int w = 0; w < 4; ++w) aWbVerdict[w] = 0;
		scGpBaseBld = 0; scGpBaseSpec = 0; scGpModCity = 0; scGpModSr = 0; scDefense = 0;
		scMaintModCity = 0; scTradeCity = 0; scTradeCoastalCiv = 0;
		brSrUnitProd = 0; brSrBuildingProd = 0;
		brCityMilitary = 0; brCitySpace = 0;
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
	PSC_FRONTIER = 32, // the ENABLER player frontier (researchable/civics/hurries + the canBuild rem-set + promo tech halves)
	PSC_ALL    = 63
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
	int maintPlayerAll;                  // maintenance.empire building pcts
	int maintConnPct;                    // connectedCity pcts (× connected-and-not-capital gate at read)
	std::map<int, int> maintAreaPct;     // areaId -> own-area pcts
	std::map<int, int> maintOtherAreaPct;// areaId -> otherArea pcts CONTRIBUTED BY that area's cities
	int maintOtherAreaTotal;
	int tradeEmpireAll;                  // building empire flats
	int tradeCoastalAll;                 // building coastal flats (× coastal gate at read)
	int tradeWorldMine;                  // THIS player's world-scope flats (the world package sums these)
	// -- buildRate (building-sourced halves only) --
	std::map<long, int> brEmpKeyed;      // (memberSeg<<20)|keySeg -> Σ pcts (all cities' active buildings)
	int brEmpMilitary, brEmpSpace;       // empire member pcts (buildings)
	// -- the ENABLER player frontier (#430 THE FLIP): researchable/civics/hurries sets (the harness-proven
	// bare-player-ctx fills), the canBuild UNLOCK rem-set (obsoletes.builds over held techs), and the
	// promotion frontier's player-wide tech halves (the per-unit composite folds these + the unit's own) --
	std::set<int> enResearchable, enCivicsOk, enHurryOk, enBuildRem;
	std::set<int> enPromoTechCand, enPromoTechRem;

	CvDerivedCacheSet<CvPlayer> set;     // bind in CvPlayer's ctor

	CascadePlayerScope()
	{
		for (int y = 0; y < NUM_YIELD_TYPES; ++y) { yFlatFreeCity[y] = 0; yFlatGoldenAge[y] = 0; }
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
			{ cPlayerExtra100[c] = 0; cGoldenAge[c] = 0; cSrPool[c] = 0; }
		gpModPlayer = 0; maintPlayerAll = 0; maintConnPct = 0; maintOtherAreaTotal = 0;
		tradeEmpireAll = 0; tradeCoastalAll = 0; tradeWorldMine = 0;
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
