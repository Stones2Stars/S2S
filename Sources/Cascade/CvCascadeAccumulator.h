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
//	the package layout); the CALCULATORS (CommerceCalc/CascadeWellbeing/CascadeScalarChannels) serve the
//	/computed decomposition endpoints (from-scratch recomputes attributed to NAMED terms) -- they never
//	read a package.
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
	// The city-local HAVE-atom kinds a flip can TARGET (enabler-frontier-perf.md Part C): the atoms whose
	// footprint is a clean requires-only, per-city re-check (pop / religion / corporation / power). tech / civic /
	// golden-age deliberately stay the broad markPlayerScopeAndCities (their footprint spans obsoletes/enables/
	// waiver edges the requires-scan index does not carry -- keeping them broad is the correctness floor, rule 3).
	enum CascadeHaveKind { CASC_HAVE_POP = 0, CASC_HAVE_RELIGION, CASC_HAVE_CORP, CASC_HAVE_POWER };

	// ===== the realized reads (bare fetches + the combine; O(1) integer arithmetic) =====
	static long yieldRate100(const CvCity* pCity, YieldTypes eY);          // §2a: (plots+trade+BASE)×pct + EXTRA
	static long commerceRate100(const CvCity* pCity, CommerceTypes eC);    // the §2 CombineSplit over the packages
	static int wellbeing(const CvCity* pCity, int iVerdict);               // §2b: 0=happy 1=unhappy 2=good 3=bad
	static int scGpBase(const CvCity* pCity);                              // gp building + specialist flats
	static int scGpNational(const CvPlayer* pPlayer);                      // max(0, trait national GP flats) -- the L6 fold
	static int scGpModifier(const CvCity* pCity);                          // max(0, 100 + city + player + gated SR/GA)
	static int scDefense(const CvCity* pCity);                             // the building defense stack (city)
	static int scDefenseBombard(const CvCity* pCity);                      // L13: bombard pcts (city buildings)
	static int scDefenseMin(const CvCity* pCity);                          // L13: the min FLOOR flats (city buildings)
	static int scDefensePlayer(const CvPlayer* pPlayer);                   // L13: bldg+civic+trait empire defense pcts
	static int scBuildingBombardDefense(const CvCity* pCity);             // L13: composed getBuildingBombardDefense (bldg+national, capped)
	static int fsAmountAny(const CvCity* pCity);                           // the freeSpecialists AMOUNT (any bucket)
	static int fsAmountByType(const CvCity* pCity, int eSpecialist);       // the freeSpecialists AMOUNT (per type)
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
	// dictate; the operating buildings always re-check (operate conditions).
	static void buildingProcessed(const CvCity* pCity, BuildingTypes eBuilding);
	// A player-level event (civic swap / golden-age flip / tech researched): marks the player scope + ALL
	// the player's cities' packages + operating buildings (conditions on city-scope deposits reference these).
	static void markPlayerScopeAndCities(PlayerTypes ePlayer);

	// Part A: build the building + unit frontier reverse indices at the load-end warm-up (idempotent). Off the
	// lazy first-onBuildingChanged trigger -- the indices stand ready before turn 1's targeted re-checks.
	static void buildFrontierIndices();
	// Part C: a city-local HAVE atom (CascadeHaveKind) flipped -> TARGETED frontier re-check (buildable + trainable)
	// over that atom's reverse-index bucket, in place. Replaces the broad CPK_FRONTIER dirty at pop/religion/corp/
	// power; a box with a full rebuild pending is left to that rebuild.
	static void cityHaveChanged(const CvCity* pCity, int eHaveKind);
	// Part B: a unit's EMPIRE count changed (trained / lost) -> TARGETED trainable re-check across the player's
	// cities (unit caps are empire-scoped). A no-op for an uncapped, unreferenced unit (the combat common case).
	static void unitCountChanged(const CvPlayer& kPlayer, int eUnit);

	// ===== the ENABLER frontier reads (#430 THE FLIP -- ensure-on-read, the operating buildings idiom) =====
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
	// verdict-equivalent to the legacy triple by construction; enBuildUnlocked above is the FULL
	// requires.build eval (the plot-aware form)
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
