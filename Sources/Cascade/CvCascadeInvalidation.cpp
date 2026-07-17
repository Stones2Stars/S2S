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
		case SEVT_RELIGION_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const int iDirtyPackages = CPK_YPCT | CPK_CBASE | CPK_CPCT | CPK_WB | CPK_SCPCT;
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
				const int iDirtyPackages = CPK_YPCT | CPK_CBASE | CPK_CPCT | CPK_WB | CPK_SCPCT | CPK_BR;
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
				const int iDirtyPackages = CPK_YEXTRA | CPK_CBASE;
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				CascadeAccumulator::cityHaveChanged(pCity, CascadeAccumulator::CASC_HAVE_POP);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}
		case SEVT_POWER_CHANGED:   // R4 gap #5: the rate/wellbeing mask is PROVISIONAL (self-heal-backstopped)
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const int iDirtyPackages = CPK_YPCT | CPK_WB;
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				CascadeAccumulator::cityHaveChanged(pCity, CascadeAccumulator::CASC_HAVE_POWER);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}
		case SEVT_BONUS_CHANGED:   // R4 gap #1: PROVISIONAL presence mask (self-heal-backstopped)
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const int iDirtyPackages = CPK_YPCT | CPK_CBASE | CPK_CPCT | CPK_WB | CPK_SCPCT;
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
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
		// frontier) is the CASC_HAVE_BONUS enabler follow-on, self-heal-backstopped. Wired ADDITIVELY alongside the
		// legacy per-city SEVT_BONUS_CHANGED (retired at the crutch-removal pass). The connection:vicinity (RADIUS)
		// axis rides a CITY plot-gain/loss hook -- reshape pending (membership is city/plot STATE, not a cascade
		// fat-cross recompute); until then vicinity stays self-heal-backstopped.
		case SEVT_PLOTGROUP_BONUS_CHANGED:   // NETWORK RESOURCE: a plot-group gained/lost a resource -> its member cities re-eval connection:trade
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)kEvent.iC);
				const int iDirtyPackages = CPK_YPCT | CPK_CBASE | CPK_CPCT | CPK_WB | CPK_SCPCT;
				int iLoop;
				for (const CvCity* pc = kPlayer.firstCity(&iLoop); pc != NULL; pc = kPlayer.nextCity(&iLoop))
					if (pc->plot()->getPlotGroupId((PlayerTypes)kEvent.iC) == kEvent.iSrcLoc)
					{
						// (the per-city enabler's bonus axis needs nothing here: the network recount flows through
						// CvCity::processBonus, whose SEVT_BONUS_CHANGED count-deltas the domain already consumes)
						CascadeAccumulator::dirtyCity(pc, iDirtyPackages);
						emitCacheInvalidate(0, kEvent.iC, pc->getID(), iDirtyPackages, szSource);
					}
			}
			break;
		case SEVT_CITY_NETWORK_CHANGED:   // NETWORK MEMBERSHIP: a city's own center plot moved plot-group (merge/split) -> its whole network set changed
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const int iDirtyPackages = CPK_YPCT | CPK_CBASE | CPK_CPCT | CPK_WB | CPK_SCPCT;
				CascadeAccumulator::dirtyCity(pCity, iDirtyPackages);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, iDirtyPackages, szSource);
			}
			break;
		}

		default: break;   // count / name / grant-trigger / lifecycle / plot-substrate events are not package sources (plot yield is pull-computed; the connection:vicinity axis is pending the city plot-gain/loss hook)
		}
	}
};

static CvCacheInvalidationConsumer s_cacheInvalidationConsumer;

void cascadeRegisterInvalidation()
{
	eventSpine().registerConsumer(&s_cacheInvalidationConsumer);
}
