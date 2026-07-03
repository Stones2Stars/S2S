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
enum AccDirty
{
	ACCD_PCT     = 1,    // the percent stack (building/civic/trait/project deposits)
	ACCD_PLOTS   = 2,    // the worked-plot base packages
	ACCD_SPEC    = 4,    // specialist totals
	ACCD_EXTRA   = 8,    // building flats + perPopulation
	ACCD_EMPFLAT = 16,   // free-city + golden-age trait flats (epoch-volatile only; no city hook)
	ACCD_CRATE   = 32,   // the §2 commerce rates (auto-ORed by dirtyCity: commerce RIDES the yield components)
	ACCD_ALL     = 63
};

class CascadeAccumulator
{
public:
	// The §2a combine from standing components -- O(1) when clean; recomputes only dirty components.
	static long yieldRate100(const CvCity* pCity, YieldTypes eY);
	// The §2 commerce rate from the standing C_RATE component (slider folded at ITS recompute -- see the slider hook).
	static long commerceRate100(const CvCity* pCity, CommerceTypes eC);

	// DOMAIN dirty hooks (modifier-substrate.md): a mutation in THIS city marks the affected components.
	// The yields->commerce dependency is encoded HERE (any yield-feeding bit auto-ORs ACCD_CRATE), never at hook sites.
	static void dirtyCity(const CvCity* pCity, int iMask);
	// Player/team-level events (tech / civic / trait / golden-age / project): everything re-checks on next read.
	static void bumpEpoch();
	// The slider moved: the player's cities' C_RATE components are stale (the slider folds at recompute).
	static void dirtyPlayerCommerce(PlayerTypes ePlayer);
};

#endif // CV_CASCADE_ACCUMULATOR_H
