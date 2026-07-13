//
//	CvCascadeInvalidation -- the #430 F0 cache-invalidation consumer (R3). See the header for the summary.
//
//	⚠ STAGED, ADDITIVE (deliberate): the hand-wired mutation-site marks AND the per-turn self-heal (playerSliceRebuild)
//	remain the correctness CRUTCH while this routing is verified live. The modifier getters are ALREADY on the cascade,
//	so a missed invalidation is a wrong value that IS the /computed oracle -- UNDETECTABLE without a playtest. So the
//	crutch stays until the invalidation is proven complete (f0-eventspine-invalidation.md); this consumer's job now is
//	to route correctly + ANNOUNCE every mark ([CASCADE] invalidate ...) so each piece can be verified, not assumed.
//
//	Load-INERT: mid-reseed the targeted ripples (operating-buildings / frontier) are invalid -- their reverse indices
//	are not built until onFinalInitialized (buildFrontierIndices). The load warm-up builds the cascade; this is a
//	PLAY-TIME consumer. The per-source CITY masks are lifted VERBATIM from the CvCity.cpp mutation sites (so this is
//	mask-equivalent to the hand-wiring); the derived-from-deposit-index refinement (R2) is a follow-up.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeInvalidation.h"
#include "CvEventSpine.h"          // IEventConsumer, the SEVT_* ids, emitCacheInvalidate / spineEventName, spineGameLoadInProgress
#include "CvCascadeAccumulator.h"  // CascadeAccumulator (the mark methods) + the CPK_*/PSC_* package-bit masks
#include "AI/CvPlayerAI.h"         // GET_PLAYER

// Resolve a per-city event's (owner, cityId) to the live CvCity. A negative owner/id => NULL (an empire/world event).
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
		if (spineGameLoadInProgress()) return;   // load-inert: ripples invalid pre-buildFrontierIndices
		const char* szSource = spineEventName(kEvent.iEventId);   // the source event's name, for the observability line

		switch (kEvent.iEventId)
		{
		// ---- per-CITY sources (iC = owner, iSrcLoc = cityId); masks lifted verbatim from CvCity.cpp ----
		case SEVT_BUILDING_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				CascadeAccumulator::buildingProcessed(pCity, (BuildingTypes)kEvent.iType);
				emitCacheInvalidate(0, kEvent.iC, kEvent.iSrcLoc, CPK_ALL & ~(CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC | CPK_FRONT_B | CPK_FRONT_U), szSource);
			}
			break;
		}
		case SEVT_RELIGION_CHANGED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const int iDirtyPackages = CPK_YPCT | CPK_CBASE | CPK_CPCT | CPK_WB | CPK_SCPCT | CPK_FRONT_PP;
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
				const int iDirtyPackages = CPK_YPCT | CPK_CBASE | CPK_CPCT | CPK_WB | CPK_SCPCT | CPK_BR | CPK_FRONTIER;
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
				const int iDirtyPackages = CPK_YSPEC | CPK_CSPEC | CPK_SCSPEC;
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
				const int iDirtyPackages = CPK_YEXTRA | CPK_CBASE | CPK_FRONT_PP;
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
		case SEVT_GOLDEN_AGE_CHANGED:
		case SEVT_TRAIT_CHANGED:
		case SEVT_STATE_RELIGION_CHANGED:
		case SEVT_PROJECT_CHANGED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				CascadeAccumulator::markPlayerScopeAndCities((PlayerTypes)kEvent.iC);
				emitCacheInvalidate(1, kEvent.iC, kEvent.iC, PSC_ALL, szSource);
			}
			break;

		default: break;   // count / name / grant-trigger / lifecycle / plot-substrate events are not package sources
		}
	}
};

static CvCacheInvalidationConsumer s_cacheInvalidationConsumer;

void cascadeRegisterInvalidation()
{
	eventSpine().registerConsumer(&s_cacheInvalidationConsumer);
}
