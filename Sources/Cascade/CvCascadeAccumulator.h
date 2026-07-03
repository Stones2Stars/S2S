#pragma once
#ifndef CV_CASCADE_ACCUMULATOR_H
#define CV_CASCADE_ACCUMULATOR_H

//
//	CascadeAccumulator -- the #430 modifier SCOPE ACCUMULATOR: the modifier.md §1 substrate
//	(docs/plans/structural-cleanup/modifier-substrate.md). The machine the spec designed and the drycalc/port
//	phase lost (the 2026-07-02 retro finding): per-city STANDING component slots over the calculator packages,
//	with event-driven freshness -- a clean read is pure arithmetic on stored components (§2a combine), and a
//	component recomputes ONLY when its inputs changed (a DOMAIN dirty hook, the global epoch, or the turn roll --
//	the §3 dormancy re-check cadence AND the self-heal for any unhooked mutation).
//
//	The calculator packages (PercentStack / YieldBasePackages / BuildingPackage) are the per-component recompute
//	functions -- the leaf math stays single-sourced there; this layer only decides WHEN they run and stores their
//	results. The calculator's fresh full compute is the verification ORACLE ([SLOT] shadow).
//
//	Increment A (yield plane): PCT / PLOTS / SPEC / EXTRA per city + FREECITY / GOLDENAGE per player.
//

#include "Defines/CvEnums.h"

class CvCity;

// Per-city component dirty bits (modifier-substrate.md event->dirty mapping).
// Every package is a PLUGIN NUMBER (owner ruling 2026-07-03): each isolated per-channel package is a standing
// value that plugs into the combine; only the package whose inputs changed recomputes -- "the rest of the pipe
// stays the same". The channels are independent (culture never impacts gold); the SLIDER only re-splits the
// base commerce yield and is read LIVE at combine -- a slider move invalidates NOTHING.
// NB no PLOTS component: the worked-plot base is PULLED live from the engine's plot-yield cache
// (CvCity::getPlotYield -- Σ worked plots × O(1) clean CvPlot caches, state-repositories.md's pull model;
// owner 2026-07-03: "pure base yield calcs ... should be a cached number" -- it already IS, at the source).
// Worker/juggle churn therefore costs the accumulator NOTHING; the plot cache's own dirty triggers govern it.
enum AccDirty
{
	ACCD_PCT     = 1,    // yield percent stacks (building/civic/trait/project deposits)
	ACCD_SPEC    = 4,    // yield specialist totals
	ACCD_EXTRA   = 8,    // yield building flats + perPopulation
	ACCD_EMPFLAT = 16,   // free-city + golden-age trait flats (epoch-volatile only; no city hook)
	ACCD_CSPEC   = 32,   // commerce specialist terms (hot: specialist churn touches ONLY this on the commerce side)
	ACCD_CBASE   = 64,   // commerce baseExtra (religion/corporation/goldenAge/building block/playerExtra)
	ACCD_CPCT    = 128,  // commerce percent stacks
	ACCD_ALL     = 253
};

class CascadeAccumulator
{
public:
	// The §2a combine from standing components -- O(1) when clean; recomputes only dirty components.
	static long yieldRate100(const CvCity* pCity, YieldTypes eY);
	// The §2 CombineSplit over the standing plugin numbers + the LIVE slider/disorder -- O(1) when clean.
	static long commerceRate100(const CvCity* pCity, CommerceTypes eC);

	// DOMAIN dirty hooks (modifier-substrate.md): a mutation in THIS city marks the affected components.
	static void dirtyCity(const CvCity* pCity, int iMask);
	// PLAYER-level events (this player's building COUNT changed / civics / golden age / techs via the team):
	// all of THAT player's cities re-check on next read -- empire-scope deposits reach every sibling city
	// (the owner-named hole: the AI evaluates its next build "live" right after a completion), and scoping the
	// epoch per player kills the cross-player invalidation storm the global epoch caused.
	static void bumpPlayerEpoch(PlayerTypes ePlayer);
	// The global fallback (game reset / anything not player-attributable): everything re-checks.
	static void bumpEpoch();
};

#endif // CV_CASCADE_ACCUMULATOR_H
