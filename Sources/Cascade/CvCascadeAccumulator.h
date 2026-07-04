#pragma once
#ifndef CV_CASCADE_ACCUMULATOR_H
#define CV_CASCADE_ACCUMULATOR_H

//
//	CascadeAccumulator -- the #430 modifier machine's QUERY + FRESHNESS surface over the scope packages
//	(CvCascadeScopePackages.h; the design: docs/plans/structural-cleanup/scope-packages.md).
//
//	ONE freshness philosophy: events MARK the packages at the scopes their deposits touch (masks derived
//	from the compiled deposit index); the boundaries ENSURE (each player's slice start + the load warm-up,
//	which is the same ensure run eagerly); reads are BARE FETCHES composed by the channel's combine formula
//	with the live gates (SR-in-city / coastal / connected / golden-age / slider / disorder / military count)
//	applied at read. No epochs, no stamps, no version polling, no read-side ensure protocol.
//
//	The generic combine lives in the accessors below (channel-parameterized, family positions realized as
//	the package layout); the CALCULATORS (YieldRate/CommerceCalc/CascadeWellbeing/CascadeScalarChannels)
//	stay as the from-scratch verification ORACLES the nets diff against -- they never read a package.
//

#include "Defines/CvEnums.h"
#include "CvCascadeScopePackages.h"

class CvCity;
class CvPlayer;
class CvGame;
class CvPlot;
class CvUnit;

class CascadeAccumulator
{
public:
	// ===== the realized reads (bare fetches + the combine; O(1) integer arithmetic) =====
	static long yieldRate100(const CvCity* pCity, YieldTypes eY);          // §2a: (plots+trade+BASE)×pct + EXTRA
	static long commerceRate100(const CvCity* pCity, CommerceTypes eC);    // the §2 CombineSplit over the packages
	static int wellbeing(const CvCity* pCity, int iVerdict);               // §2b: 0=happy 1=unhappy 2=good 3=bad
	static int scGpBase(const CvCity* pCity);                              // gp building + specialist flats
	static int scGpModifier(const CvCity* pCity);                          // max(0, 100 + city + player + gated SR/GA)
	static int scDefense(const CvCity* pCity);                             // the building defense stack (city)
	static int scMaintenanceModifier(const CvCity* pCity);                 // city + player + area pick + conn gate
	static int scTradeRoutes(const CvCity* pCity);                         // city + empire + world + coastal gate
	// buildRate: the head-order production modifier for a specific item (ledger lookups + item self mods).
	static int buildRateUnit(const CvCity* pCity, UnitTypes eUnit);
	static int buildRateBuilding(const CvCity* pCity, BuildingTypes eBuilding);
	static int buildRateProject(const CvCity* pCity, ProjectTypes eProject);

	// ===== the event marks (write-side freshness) =====
	// A city-local mutation marks the given city packages.
	static void dirtyCity(const CvCity* pCity, int iMask);
	// A building completed/lost in pCity: the DERIVED masks (percent-vs-flat × package-group per the
	// compiled deposits) land on the city, the owner scope, and the world scope as the building's deposits
	// dictate; the facts always re-check (operate conditions).
	static void buildingProcessed(const CvCity* pCity, BuildingTypes eBuilding);
	// A player-level event (civic swap / golden-age flip / tech researched): marks the player scope + ALL
	// the player's cities' packages + facts (conditions on city-scope deposits reference these).
	static void markPlayerScopeAndCities(PlayerTypes ePlayer);

	// ===== the ENABLER frontier reads (#430 THE FLIP -- ensure-on-read, the facts idiom) =====
	static bool enConstruct(const CvCity* pCity, int eBuilding);
	static bool enTrain(const CvCity* pCity, int eUnit);
	static bool enCreate(const CvCity* pCity, int eProject);
	static bool enMaintain(const CvCity* pCity, int eProcess);
	static bool enResearch(const CvPlayer* pPlayer, int eTech);
	static bool enCivic(const CvPlayer* pPlayer, int eCivic);
	static bool enHurry(const CvPlayer* pPlayer, int eHurry);
	static bool enFoundReligion(const CvPlayer* pPlayer);
	// the canBuild UNLOCK half only (the plot-validity half stays engine -- the scope ruling)
	static bool enBuildUnlocked(const CvPlayer* pPlayer, int eBuild, const CvPlot* pPlot);
	// the SERVING canBuild unlock (the worker hot path -- per (plot × build) at planning scale): rem-set +
	// target-side obsolescence + the CONFIG techPrereq compare (static Info, the sanctioned class) --
	// verdict-equivalent to the legacy triple by construction; the FULL requires.build eval stays the
	// harness's net side (enBuildUnlocked, plot-sampled) proving the data equivalence continuously
	static bool enBuildUnlockedFast(const CvPlayer* pPlayer, int eBuild);
	// the promotion composite: the cascade frontier half over the bespoke legacy half (isPromotionValidLegacy(...,true))
	static bool enPromotionValid(const CvUnit* pUnit, int ePromo);

	// ===== the boundaries =====
	// The slice-start rebuild ("Cascade.RebuildCache(myPlayerId)"): the self-heal re-mark for the unhooked
	// classes + the eager ensure of this player's packages and his cities'. Runs at CvPlayer::doTurn top;
	// the load warm-up calls the same thing per player (the one mechanism, run eagerly).
	static void playerSliceRebuild(PlayerTypes ePlayer);
	// The world boundary: re-mark + ensure the world packages (CvGame::doTurn + the load warm-up).
	static void worldRebuild();
	// The city-creation boundary (owner ruling 2026-07-04): a founded/acquired city's yields stand
	// IMMEDIATELY (time-to-build is visible at once) -- one eager ensure after the founding/acquisition
	// setup completes; from there the city rides the ordinary marks + slice boundaries.
	static void cityCreated(const CvCity* pCity);

	// ===== the refresh delegate targets (the CacheSets forward here; math module-side) =====
	static void refreshCityPackages(const CvCity* pCity, int iMask);
	static void refreshPlayerScope(const CvPlayer* pPlayer, int iMask);
	static void refreshWorldScope(const CvGame* pGame, int iMask);
};

#endif // CV_CASCADE_ACCUMULATOR_H
