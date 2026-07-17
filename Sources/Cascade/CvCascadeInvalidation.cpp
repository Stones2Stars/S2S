//
//	CvCascadeInvalidation -- the #430 F0 cache-invalidation consumer (R3). See the header for the summary.
//
//	THE SELF-HEAL BLANKET IS REMOVED ([DEC-no-self-heal], 2026-07-13): playerSliceRebuild/worldRebuild (markAll +
//	ensure ALL packages every turn) no longer exist -- correctness is the eventspine (this consumer) routing each
//	DOMAIN event to ONLY the packages its source feeds, + lazy recalc. The modifier getters are ALREADY on the
//	cascade, so a missed invalidation is a wrong value that IS the /computed oracle -- now it SURFACES as a live
//	divergence instead of being blanket-healed away (the point). The hand-wired mutation-site marks still run
//	ALONGSIDE this consumer (harmless double-mark) until they are removed and the eventspine is the SOLE path.
//	This consumer ANNOUNCES every mark ([CASCADE] invalidate ...) so the real invalidation state is mappable.
//
//	TWO HALVES BY LOAD BEHAVIOUR (DEC-spine-reseed: "the cache-build consumer stays load-active"):
//	-- the ENABLER DELTAS are LOAD-ACTIVE: the reseed's in-read emits are what BUILD the enabler domains (they
//	   apply source-side edges straight off the loaded info objects -- no reverse index needed);
//	-- the MODIFIER MARKS are load-inert: mid-reseed the targeted ripples (operating-buildings) are invalid --
//	   their reverse indices are not built until onFinalInitialized (buildFrontierIndices) -- and marks during
//	   the reseed are pointless (the modifier warm-up builds at load).
//	The per-source CITY masks are lifted VERBATIM from the CvCity.cpp mutation sites (so this is mask-equivalent
//	to the hand-wiring); the derived-from-deposit-index refinement (R2) is a follow-up.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeInvalidation.h"
#include "CvEventSpine.h"          // IEventConsumer, the SEVT_* ids, emitCacheInvalidate / spineEventName, spineGameLoadInProgress
#include "CvCascadeAccumulator.h"  // CascadeAccumulator (the mark methods) + the CPK_*/PSC_* package-bit masks
#include "CvTechEnabler.h"         // TechEnabler::onTechChanged -- the standardized enabler's tech-domain delta
#include "CvBuildingEnabler.h"     // BuildingEnabler::onCity* -- the standardized per-city building-domain deltas
#include "CvUnitEnabler.h"         // UnitEnabler::onCity* -- the standardized per-city unit-domain deltas
#include "CvCivicEnabler.h"        // CivicEnabler::onTechChanged -- the civics domain's tech delta
#include "CvProjectEnabler.h"      // ProjectEnabler::onTechChanged/onProjectChanged -- the projects domain's deltas
#include "CvProcessEnabler.h"      // ProcessEnabler::onTechChanged -- the processes domain's tech delta
#include "CvBuildEnabler.h"        // BuildEnabler::onTechChanged -- the worker-builds domain's tech delta
#include "CvPromotionEnabler.h"    // PromotionEnabler::onTechChanged -- the promotions domain's tech delta
#include "AI/CvPlayerAI.h"         // GET_PLAYER

// Resolve a per-city event's (owner, cityId) to the live CvCity. A negative owner/id => NULL (an empire/world
// event). Works during the load reseed too: the two-phase city stream read (FFreeListTrashArray.h) registers a
// city in its owner's m_cities off readIdentity, BEFORE its body streams its in-read emits.
static const CvCity* cityForEvent(int iOwner, int iCityId)
{
	if (iOwner < 0 || iOwner >= MAX_PLAYERS || iCityId < 0) return NULL;
	return GET_PLAYER((PlayerTypes)iOwner).getCity(iCityId);
}

class CvCacheInvalidationConsumer : public IEventConsumer
{
public:
	int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }

	void onEvent(const CvSpineEvent& kEvent)
	{
		// THE ENABLER DELTAS -- LOAD-ACTIVE (the cache-BUILD half): the reseed's in-read emits build the enabler
		// domains through the same appliers as play (DEC-spine-reseed).
		routeEnablerDeltas(kEvent);
		// THE MODIFIER MARKS -- load-inert: mid-reseed the targeted ripples are invalid (reverse indices unbuilt
		// until buildFrontierIndices) and marks are pointless (the modifier warm-up builds at load).
		if (spineGameLoadInProgress()) return;
		routeModifierMarks(kEvent);
	}

private:
	// The standardized per-domain enabler appliers (enabler.md par.7.1) -- consume EVERY DOMAIN emit, play-time
	// and the load reseed alike. Order contracts live here (pre-flip held-flag guards).
	void routeEnablerDeltas(const CvSpineEvent& kEvent)
	{
		switch (kEvent.iEventId)
		{
		case SEVT_BUILDING_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				// held flip + the building's own enables contribution.
				// ORDER: UnitEnabler first (its flip guard reads the buildings domain's held flag PRE-flip).
				UnitEnabler::onCityBuildingChanged(*pCity, kEvent.iType, kEvent.iB > 0);
				BuildingEnabler::onCityBuildingChanged(*pCity, kEvent.iType, kEvent.iB > 0);   // idempotency = the domain's held guard
			}
			break;
		}
		case SEVT_RELIGION_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityReligionChanged(*pCity, kEvent.iType, kEvent.iA != 0);   // flip-guarded emit
				UnitEnabler::onCityReligionChanged(*pCity, kEvent.iType, kEvent.iA != 0);
			}
			break;
		}
		case SEVT_CORPORATION_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL) BuildingEnabler::onCityCorporationChanged(*pCity, kEvent.iType, kEvent.iA != 0);   // flip-guarded emit
			break;
		}
		case SEVT_BONUS_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityBonusChanged(*pCity, kEvent.iType, kEvent.iB);   // count-delta crossing
				UnitEnabler::onCityBonusChanged(*pCity, kEvent.iType, kEvent.iB);
			}
			break;
		}
		case SEVT_VICINITY_BONUS_CHANGED:   // LOCAL presence flip: re-gate the bonus's connection:vicinity dependents
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityVicinityBonusChanged(*pCity, kEvent.iType);
				UnitEnabler::onCityVicinityBonusChanged(*pCity, kEvent.iType);
			}
			break;
		}
		case SEVT_CITY_CULTURE_LEVEL_CHANGED:   // the culture-level HAVE axis
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL) BuildingEnabler::onCityCultureLevelChanged(*pCity, kEvent.iB, kEvent.iA);   // old (iB) -> new (iA)
			break;
		}
		case SEVT_CITY_ORDER_CHANGED:   // queue push/pop: the buildings gate's queued-exclusion re-gate (par.7.1 step 3)
		{
			if (kEvent.iA == ORDER_CONSTRUCT)
			{
				const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
				if (pCity != NULL) BuildingEnabler::onCityOrderChanged(*pCity, kEvent.iType);
			}
			break;
		}
		case SEVT_TECH_CHANGED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				// the MAINTAINED city bonus counts: a tech can move the TechCityTrade gate, so the gated
				// answers refresh from the stored totals (no group walk) for the player's cities.
				foreach_(CvCity* pLoopCity, GET_PLAYER((PlayerTypes)kEvent.iC).cities())
				{
					pLoopCity->refreshAllEffectiveBonuses();
				}
				// the tech domain's O(delta) membership update -- the SOLE maintainer of the availability
				// vectors' tech axis. iType=Tech, iA=has, iC=triggering player (the team resolves from it).
				// ORDERING CONTRACT: every domain whose flip guard reads the PLAYER tech domain's held flag
				// MUST run BEFORE TechEnabler::onTechChanged flips that flag.
				BuildingEnabler::onCityTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, kEvent.iA != 0);
				UnitEnabler::onCityTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, kEvent.iA != 0);
				CivicEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, kEvent.iA != 0);
				ProjectEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, kEvent.iA != 0);
				ProcessEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, kEvent.iA != 0);
				BuildEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, kEvent.iA != 0);
				PromotionEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, kEvent.iA != 0);
				TechEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, kEvent.iA != 0);
			}
			break;
		case SEVT_CIVIC_ADOPTED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				// the civic HAVE axis -- the emit carries the swap fact (adopted iType, swapped-out iB)
				BuildingEnabler::onPlayerCivicsChanged((PlayerTypes)kEvent.iC, kEvent.iB, kEvent.iType);
				UnitEnabler::onPlayerCivicsChanged((PlayerTypes)kEvent.iC, kEvent.iB, kEvent.iType);
			}
			break;
		case SEVT_PROJECT_CHANGED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				// the project->project HAVE axis (iType=Project, iB=team count delta). PER-MEMBER emits (one per
				// alive team member -- play and the load reseed alike), so the applier scopes to the emitting
				// player: exactly-once per player, no team-wide double-apply.
				ProjectEnabler::onProjectChanged((PlayerTypes)kEvent.iC, (ProjectTypes)kEvent.iType, kEvent.iB);
			}
			break;
		// ---- the requires-gate CLASS re-gates (no FK reverse edge exists for these -- the load-compiled
		// class lists re-gate; the enablers skip them inside the load bracket) ----
		case SEVT_POPULATION_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityGateClass(*pCity, BuildingEnabler::GATE_POP);
				UnitEnabler::onCityGateClass(*pCity, UnitEnabler::GATE_POP);
			}
			break;
		}
		case SEVT_POWER_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityGateClass(*pCity, BuildingEnabler::GATE_POWER);
				UnitEnabler::onCityGateClass(*pCity, UnitEnabler::GATE_POWER);
			}
			break;
		}
		case SEVT_GOLDEN_AGE_CHANGED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				BuildingEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, BuildingEnabler::GATE_GOLDEN_AGE);
				UnitEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, UnitEnabler::GATE_GOLDEN_AGE);
			}
			break;
		case SEVT_STATE_RELIGION_CHANGED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				BuildingEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, BuildingEnabler::GATE_STATE_RELIGION);
				UnitEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, UnitEnabler::GATE_STATE_RELIGION);
			}
			break;
		// #430 nukes: the world NO_NUKES ban flips a build gate (Manhattan-type buildings carry
		// requires.build.disabled: NO_NUKES). NO_NUKES is an unrecognized predicate -> the GATE_DYNAMIC bucket, so the
		// frontier re-gate is onPlayerGateClass(GATE_DYNAMIC). Emitted per-player (the ban fans out), rare (once/game),
		// so re-gating the player's dynamic buildings is cheap. No modifier mark (no NO_NUKES-conditioned deposits).
		case SEVT_NUKES_CHANGED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				BuildingEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, BuildingEnabler::GATE_DYNAMIC);
				UnitEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, UnitEnabler::GATE_DYNAMIC);
			}
			break;
		// ---- the unit-count crossing (par.7.1 step 3): caps / unit-count requires / upgrade availability ----
		case SEVT_UNIT_COUNT:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
				UnitEnabler::onUnitCountChanged((PlayerTypes)kEvent.iC, kEvent.iType);
			break;
		// ---- the load-end gate pass (the par.7.1 order rule's "gate once after the stream ends" option --
		// fires while the bracket is still open, at the end of onFinalInitialized, state fully final) ----
		case SEVT_GAME_LOAD_FINISHED:
			BuildingEnabler::onLoadFinished();
			UnitEnabler::onLoadFinished();
			break;
		default: break;
		}
	}

	// The modifier-package marks (the invalidation half) -- play-time only.
	void routeModifierMarks(const CvSpineEvent& kEvent)
	{
		const char* szSource = spineEventName(kEvent.iEventId);   // the source event's name, for the observability line

		switch (kEvent.iEventId)
		{
		// ---- per-CITY sources (iC = owner, iSrcLoc = cityId); masks lifted verbatim from CvCity.cpp ----
		// The modifier marks key on BOTH building events: the PRESENCE fact (SEVT_BUILDING_CHANGED -- the load
		// reseed's in-read emits AND play-time build/raze; processBuilding never runs during a load, so the
		// package feed MUST ride the presence emit or the reseed leaves the percent packages empty -- the
		// London food-collapse regression) AND the PROCESSED flip (dormancy disable/enable -- a disabled
		// building stops depositing with no presence change). Idempotent marks; both route to the same body.
		case SEVT_BUILDING_CHANGED:
		case SEVT_BUILDING_PROCESSED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				CascadeAccumulator::buildingProcessed(pCity, (BuildingTypes)kEvent.iType);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC), szSource);
			}
			break;
		}
		// RELIGION / CORPORATION / POPULATION / POWER all fire the operate-dormancy ripple (cityHaveChanged ->
		// ek_recheckActiveSet), which flips buildings active<->dormant. A flipped building deposits the FULL active
		// footprint (YEXTRA/SCFLAT/BR beyond the source-conditioned packages), so the mask MUST be the building-active
		// footprint (buildingProcessed's mask), exactly as the G3 bonus events. (The narrow source-conditioned mask
		// left YEXTRA/SCFLAT/BR stale on an operate flip -- the adversarial mask audit's confirmed gap.)
		case SEVT_RELIGION_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const int iDirtyPackages = CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC);
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				CascadeAccumulator::cityHaveChanged(pCity, CascadeAccumulator::CASC_HAVE_RELIGION);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}
		case SEVT_CORPORATION_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const int iDirtyPackages = CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC);
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				CascadeAccumulator::cityHaveChanged(pCity, CascadeAccumulator::CASC_HAVE_CORP);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}
		case SEVT_SPECIALIST_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				// #430 G2: specialists ALSO deposit §2b happiness/health (wbTerms.spec, folded at fill) -> CPK_WB
				const int iDirtyPackages = CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC | CPK_WB;
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}
		case SEVT_POPULATION_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				// perPopulation deposits scale directly with pop (YEXTRA/CBASE) AND CASC_HAVE_POP flips pop-thresholded
				// operate buildings -> the building-active footprint (the mask audit's #1 gap: SCFLAT/WB/CPCT/BR/SCPCT/YPCT
				// went stale on every growth/starve tick).
				const int iDirtyPackages = CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC);
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				CascadeAccumulator::cityHaveChanged(pCity, CascadeAccumulator::CASC_HAVE_POP);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}
		case SEVT_POWER_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				// CASC_HAVE_POWER flips power-operate buildings (factory-class) -> the building-active footprint. (The
				// old CPK_YPCT|CPK_WB mask + the doc's "Cleared (NOT a gap)" were BOTH wrong: 934 power-operate buildings
				// leak CBASE/YEXTRA/SCFLAT/CPCT/BR/SCPCT on a power flip -- the mask audit's #2 gap.)
				const int iDirtyPackages = CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC);
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				CascadeAccumulator::cityHaveChanged(pCity, CascadeAccumulator::CASC_HAVE_POWER);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}
		case SEVT_BONUS_CHANGED:   // LEGACY per-city network-count crossing (processBonus); being RETIRED -- the forward
		{                          // vicinity/plotgroup/network events (broadened + operate-rippled, G3) own the bonus axis.
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				// Kept narrow ON PURPOSE: the operate-flip footprint + ripple ride the forward bonus events; this legacy
				// mark just double-covers the deposit-conditioning until it retires. (No self-heal backstop exists.)
				const int iDirtyPackages = CPK_YPCT | CPK_CBASE | CPK_CPCT | CPK_WB | CPK_SCPCT;
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}
		// #430 the connection:VICINITY axis (owner model -- supersedes the pending "city plot-gain/loss hook" + the
		// ring-walk): a bonus flipped in THIS city's workable radius (an improved tile via doVicinityBonus, or an active
		// provider via processBuilding). Vicinity is a CITY-LOCAL presence fact (enabler.md par.8 -- "VICINITY belongs to
		// the CITY"), so it marks ONLY the emitting city's bonus-conditioned packages. The NETWORK half -- a provider
		// that makes the bonus TRADEABLE across the group -- is a SEPARATE fact: processBuilding injects it into the
		// city's plot group (CvCity.cpp:4792 changeNumBonuses), which fires SEVT_PLOTGROUP_BONUS_CHANGED on the presence
		// transition (CvPlotGroup.cpp:525) -> the member-city fan-out above. The city already carries its plot-group id
		// and the group its member cities (the reverse-mapped membership) -- so there is NO plot->cities ring walk.
		// Same mask as SEVT_BONUS_CHANGED (both gate the same deposit channels). The operate-dormancy ripple
		// (a building whose requires.operate needs a connection:vicinity bonus) is the CASC_HAVE_BONUS enabler
		// follow-on (G3), sequenced next; the enabler's own buildable re-gate already rides routeEnablerDeltas.
		case SEVT_VICINITY_BONUS_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				// #430 G3: a bonus's vicinity presence can flip an operate-gated building dormant/active, so the mask is
				// the BUILDING-ACTIVE footprint (buildingProcessed's mask) -- the flipped building's YEXTRA/SCFLAT/BR
				// deposits re-sum against the ripple-updated active set, not just the bonus-conditioned packages.
				const int iDirtyPackages = CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC);
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				CascadeAccumulator::cityBonusAccessChanged(pCity, kEvent.iType);   // the targeted operate-dormancy ripple (reverse-FK)
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}
		// ---- per-EMPIRE sources / conditioner fan-out (iC = player): the broad mark (conditioner fan-out spans
		// obsoletes/enables/waiver edges the deposit index does not reverse-map -- the deliberate correctness floor;
		// trait / state-religion / project ride it as R4 gaps, self-heal-backstopped) ----
		case SEVT_TECH_CHANGED:
		case SEVT_CIVIC_ADOPTED:
		case SEVT_PROJECT_CHANGED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				CascadeAccumulator::markPlayerScopeAndCities((PlayerTypes)kEvent.iC);   // the broad conditioner mark (unchanged)
				emitCacheInvalidate(1, kEvent.iC, kEvent.iC, PSC_ALL, szSource);
			}
			break;

		case SEVT_GOLDEN_AGE_CHANGED:
		case SEVT_TRAIT_CHANGED:
		case SEVT_STATE_RELIGION_CHANGED:
		// #430 era: a per-player broad input -- every ERA-counter-threshold deposit (era-band flats accumulate as the
		// counter passes each min:, modifier.md §6) must re-sum. The heritage era-stacked commerce (PSC_CFLAT) is
		// applied IN setCurrentEra; this mark covers the in-package era-threshold deposits. Frontier ERA build atoms
		// ride the GATE_DYNAMIC per-turn re-check (ERA is a count token, not an FK), so no enabler route is needed here.
		case SEVT_ERA_CHANGED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				CascadeAccumulator::markPlayerScopeAndCities((PlayerTypes)kEvent.iC);
				emitCacheInvalidate(1, kEvent.iC, kEvent.iC, PSC_ALL, szSource);
			}
			break;

		case SEVT_HERITAGE_CHANGED:   // #430 G1: heritage empire commerce flats -> PSC_CFLAT (the boundary ensure rolls it to the cities next slice -- the ruled player-scope mid-turn cadence, matching the plain empire-commerce-flat case in buildingProcessed)
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				GET_PLAYER((PlayerTypes)kEvent.iC).m_cascadePlayerScope.set.markDirty(PSC_CFLAT);
				emitCacheInvalidate(1, kEvent.iC, kEvent.iC, PSC_CFLAT, szSource);
			}
			break;

		// #430 BONUS NETWORK ACCESS -- the connection:trade axis, keyed on the plot-group identity the city already
		// carries. TWO triggers: a plot-group's resource SET changed (resource event), and a city MOVED to a different
		// group (membership event). Mask = the bonus-CONDITIONED packages; the operate-dormancy ripple (YEXTRA/BR +
		// frontier) is the CASC_HAVE_BONUS enabler follow-on (G3), sequenced next. Wired ADDITIVELY alongside the
		// legacy per-city SEVT_BONUS_CHANGED (retired at the crutch-removal pass). The connection:VICINITY (RADIUS)
		// axis is the SEVT_VICINITY_BONUS_CHANGED case above -- a city-local presence mark; a provider's network half
		// rides THIS plot-group fan-out via the provides->group injection (no ring walk, no plot->cities map).
		case SEVT_PLOTGROUP_BONUS_CHANGED:   // NETWORK RESOURCE: a plot-group gained/lost a resource -> its member cities re-eval connection:trade
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)kEvent.iC);
				// #430 G3: the network crossing can flip an operate-gated building -> the BUILDING-ACTIVE footprint mask.
				const int iDirtyPackages = CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC);
				int iLoop;
				for (const CvCity* pc = kPlayer.firstCity(&iLoop); pc != NULL; pc = kPlayer.nextCity(&iLoop))
					if (pc->plot()->getPlotGroupId((PlayerTypes)kEvent.iC) == kEvent.iSrcLoc)
					{
						// (the per-city enabler's bonus axis needs nothing here: the network recount flows through
						// CvCity::processBonus, whose SEVT_BONUS_CHANGED count-deltas the domain already consumes)
						CascadeAccumulator::dirtyCity(pc, iDirtyPackages);
						CascadeAccumulator::cityBonusAccessChanged(pc, kEvent.iType);   // #430 G3: the targeted operate-dormancy ripple for THIS bonus, per member city
						emitCacheInvalidate(0, kEvent.iC, pc->getID(), iDirtyPackages, szSource);
					}
			}
			break;
		case SEVT_CITY_NETWORK_CHANGED:   // NETWORK MEMBERSHIP: a city's own center plot moved plot-group (merge/split) -> its whole network set changed
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				// #430 G3: the city's ENTIRE connected-resource set may have shifted -> the building-active footprint
				// mask + the WHOLE-SET operate ripple (CASC_HAVE_BONUS re-checks every bonus-operate building here).
				const int iDirtyPackages = CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC);
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				CascadeAccumulator::cityHaveChanged(pCity, CascadeAccumulator::CASC_HAVE_BONUS);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}

		// #430 holy-city: a per-city predicate flip (IS_HOLY_CITY / IS_STATE_RELIGION_HOLY_CITY) on a holy-city
		// RELOCATION. Mark the city's building-active footprint (holy-city-conditioned deposits -- shrine commerce
		// etc.) + the operate ripple: IS_HOLY_CITY buckets under `religion` in the operate reverse-index (s_opReligion),
		// so CASC_HAVE_RELIGION re-checks the holy-city-operate buildings. NOTE: at religion FOUNDING the co-firing
		// SEVT_RELIGION_CHANGED already invalidates this city; the standalone event matters for a relocation without a
		// religion-presence change. The bare-IS_HOLY_CITY requires.BUILD frontier gate (predicate, not FK-bucketed) is
		// a known enabler follow-on -- surfaced for the verification pass, not silently skipped.
		case SEVT_HOLY_CITY_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const int iDirtyPackages = CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC);
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				CascadeAccumulator::cityHaveChanged(pCity, CascadeAccumulator::CASC_HAVE_RELIGION);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}

		// #430 FEATURE change (a PLOT event) -> the CACHED wellbeing term. Feature presence in a city's workable radius
		// folds each feature's health.plot.percent (+ the civic/trait feature-keyed WB) into CPK_WB, so a worker-clear /
		// feature spread / popDestroys stales the WB of EVERY city whose radius includes the plot. Fan out to those
		// cities: enumerate the plot's 3-ring neighborhood (plotCity ring) and mark every city on it -- a bounded
		// SUPERSET of the true radius cities (an over-mark is harmless -- the package re-sums and finds no change; an
		// under-mark would be the bug, so the superset is provably safe and sidesteps the culture-radius membership
		// subtlety). Feature YIELD is pull-computed (the plot-yield cache self-dirties on setFeatureType), so ONLY CPK_WB.
		case SEVT_FEATURE_CHANGED:
		{
			const CvPlot* pPlot = GC.getMap().plotByIndex(kEvent.iSrcLoc);
			if (pPlot != NULL)
			{
				for (int i = 0; i < NUM_CITY_PLOTS; i++)
				{
					const CvPlot* pRing = plotCity(pPlot->getX(), pPlot->getY(), i);
					if (pRing == NULL) continue;
					const CvCity* pRadiusCity = pRing->getPlotCity();
					if (pRadiusCity == NULL) continue;
					CascadeAccumulator::dirtyCity(pRadiusCity, CPK_WB);
					emitCacheInvalidate(0, pRadiusCity->getOwner(), pRadiusCity->getID(), CPK_WB, szSource);
				}
			}
			break;
		}

		default: break;   // count / name / grant-trigger / lifecycle / plot-substrate events are not package sources (plot yield is pull-computed; connection:vicinity is handled above via SEVT_VICINITY_BONUS_CHANGED)
		}
	}
};

static CvCacheInvalidationConsumer s_cacheInvalidationConsumer;

void cascadeRegisterInvalidation()
{
	eventSpine().registerConsumer(&s_cacheInvalidationConsumer);
}
