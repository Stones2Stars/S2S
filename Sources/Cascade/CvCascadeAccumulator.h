#pragma once
#ifndef CV_CASCADE_ACCUMULATOR_H
#define CV_CASCADE_ACCUMULATOR_H

//
//	CascadeAccumulator -- the #430 modifier SCOPE ACCUMULATOR: the modifier.md §1 substrate
//	(docs/plans/structural-cleanup/modifier-substrate.md). Standing per-city component slots over the calculator
//	packages, event-driven freshness, O(1) clean reads -- the machine the spec designed. The calculator's fresh
//	full compute is the verification ORACLE ([SLOT] shadow).
//
//	STATE HOME (2026-07-03): the slots live ON CvCity (`m_cascadeRateSlots`, a mutable cache member -- the same
//	idiom as the CvPlot yield cache) driven by a CvDerivedCacheSet (state-repositories.md, the partial-dirty
//	form's first consumer). No side map, no lookup on reads; city lifetime owns the cache. The cascade MATH
//	stays module-side (refreshComponents); CvCity carries one thin delegate (cascadeRefreshRates).
//

#include "Defines/CvEnums.h"
#include "Infrastructure/CvDerivedCache.h"

class CvCity;

// Every package is a PLUGIN NUMBER (owner ruling 2026-07-03): each isolated per-channel package is a standing
// value that plugs into the combine; only the package whose inputs changed recomputes -- "the rest of the pipe
// stays the same". The channels are independent (culture never impacts gold); the SLIDER only re-splits the
// base commerce yield and is read LIVE at combine; the WORKED-PLOT base is PULLED live from the CvPlot cache.
enum AccDirty
{
	ACCD_PCT     = 1,    // yield percent stacks (building/civic/trait/project deposits)
	ACCD_SPEC    = 4,    // yield specialist totals
	ACCD_EXTRA   = 8,    // yield building flats + perPopulation
	ACCD_EMPFLAT = 16,   // free-city + golden-age trait flats (epoch-volatile only; no city hook)
	ACCD_CSPEC   = 32,   // commerce specialist terms (hot: specialist churn touches ONLY this on the commerce side)
	ACCD_CBASE   = 64,   // commerce baseExtra (religion/corporation/goldenAge/building block/playerExtra)
	ACCD_CPCT    = 128,  // commerce percent stacks
	ACCD_WB      = 256,  // the §2b wellbeing verdicts (happy/unhappy/goodHealth/badHealth -- CascadeWellbeing)
	ACCD_ALL     = 509
};

// The per-city standing slots -- a MUTABLE CACHE member on CvCity (never serialized; rebuilt on load via the
// eager warm-up + dirty-on-construct). Public fields by the shadow-phase convention (CvJsonInfo precedent).
struct CascadeRateSlots
{
	long aPct[NUM_YIELD_TYPES];         // stored modifier = max(0, 100 + Σpercent) -- the whole §2a stack
	long aSpec[NUM_YIELD_TYPES];        // specialist totals (human units; own sub-stack inside)
	long aExtra100[NUM_YIELD_TYPES];    // building flats + perPop (×100)
	long aEmpFlat[NUM_YIELD_TYPES];     // free-city + golden-age trait flats (human units)
	long aCSpec100[NUM_COMMERCE_TYPES]; // commerce specialist terms (×100) -- the hot commerce-side plugin
	long aCBase100[NUM_COMMERCE_TYPES]; // commerce baseExtra (religion/corp/GA/building block/playerExtra, ×100)
	long aCPct[NUM_COMMERCE_TYPES];     // commerce percent stacks (max(0, 100 + Σ))
	int aWb[4];                         // the §2b wellbeing verdicts (MILITARY-FREE): happy / unhappy / goodHealth / badHealth
	int iWbMilPerUnit;                  // the epoch-stable per-military-unit happiness VALUE (×live count at read -- rides on top, never invalidates)
	int iEpoch;                         // combined global+owner epoch stamp
	int iTurn;                          // the §3 turn-roll re-check stamp
	CvDerivedCacheSet<CvCity> set;      // the dirty protocol (bind in CvCity's ctor)
	CascadeRateSlots() : iEpoch(-1), iTurn(-1)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; ++i) { aPct[i] = 100; aSpec[i] = 0; aExtra100[i] = 0; aEmpFlat[i] = 0; }
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c) { aCSpec100[c] = 0; aCBase100[c] = 0; aCPct[c] = 100; }
		for (int w = 0; w < 4; ++w) aWb[w] = 0;
		iWbMilPerUnit = 0;
	}
};

class CascadeAccumulator
{
public:
	// The §2a combine from standing components -- O(1) when clean; recomputes only dirty components.
	static long yieldRate100(const CvCity* pCity, YieldTypes eY);
	// The §2 CombineSplit over the standing plugin numbers + the LIVE slider/disorder -- O(1) when clean.
	static long commerceRate100(const CvCity* pCity, CommerceTypes eC);
	// The §2b wellbeing verdict slot (0=happy 1=unhappy 2=goodHealth 3=badHealth) -- O(1) when clean.
	static int wellbeing(const CvCity* pCity, int iVerdict);

	// DOMAIN dirty hooks (modifier-substrate.md): a mutation in THIS city marks the affected components.
	static void dirtyCity(const CvCity* pCity, int iMask);
	// PLAYER-level events (civics / golden age / techs via the team): that player's cities re-check on next
	// read. ⛔ NOT for building completions -- measured 5x regression (the highest-frequency player event);
	// sibling-empire freshness belongs to the turn-end unified rebuild (state-repositories.md end-state).
	static void bumpPlayerEpoch(PlayerTypes ePlayer);
	// The global fallback (game reset / anything not player-attributable): everything re-checks.
	static void bumpEpoch();

	// The combined global+owner epoch stamp a per-city derived cache compares against (the acc_ensure semantic,
	// exposed single-source -- the facts cache stamps with the SAME epoch, so tech/civic/GA events re-check both).
	static int epochFor(PlayerTypes ePlayer);

	// The masked component recompute -- CvCity::cascadeRefreshRates delegates here (the CvDerivedCacheSet's
	// refresh target). Writes the city's m_cascadeRateSlots arrays from CURRENT state.
	static void refreshComponents(const CvCity* pCity, int iMask);
};

#endif // CV_CASCADE_ACCUMULATOR_H
