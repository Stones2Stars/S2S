//
//	CvEnablerConsumer -- the enabler's spine consumer (see the header). The per-domain appliers
//	(enabler.md par.7.1) consume EVERY DOMAIN emit, play-time and the load reseed alike; the order contracts
//	(pre-flip held-flag guards) live here.
//

#include "CvGameCoreDLL.h"
#include "CvEnablerConsumer.h"
#include "Spine/CvEventSpine.h"
#include "CvTechEnabler.h"
#include "CvBuildingEnabler.h"
#include "CvUnitEnabler.h"
#include "CvCivicEnabler.h"
#include "CvProjectEnabler.h"
#include "CvProcessEnabler.h"
#include "CvBuildEnabler.h"
#include "CvPromotionEnabler.h"
#include "AI/CvPlayerAI.h"

// Resolve a per-city event's (owner, cityId) to the live CvCity. A negative owner/id => NULL (an empire/world
// event). Works during the load reseed too: the two-phase city stream read (FFreeListTrashArray.h) registers a
// city in its owner's m_cities off readIdentity, BEFORE its body streams its in-read emits.
static const CvCity* cityForEvent(int iOwner, int iCityId)
{
	if (iOwner < 0 || iOwner >= MAX_PLAYERS || iCityId < 0) return NULL;
	return GET_PLAYER((PlayerTypes)iOwner).getCity(iCityId);
}

class CvEnablerSpineConsumer : public IEventConsumer
{
public:
	int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }

	// LOAD-ACTIVE: no spineGameLoadInProgress() suppression -- the in-read emits BUILD the domains.
	void onEvent(const CvSpineEvent& kEvent) { routeEnablerDeltas(kEvent); }

private:
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
		// ---- a GAME OPTION flipped: re-gate EVERY city ----
		// The entity-level gate is read at gate time and an option is its ONE axis ([DEC-entity-gate]), so a flip
		// can move any candidate's verdict at once -- and the tri-state is a BARE FETCH, so nothing would ever
		// re-derive it ([DEC-no-self-heal]). Wholesale is the honest derivation here rather than a blanket hiding
		// a missed route: the fact names no source to route from (the SEVT_AREAS_RECALCULATED shape), and a flip
		// is WorldBuilder-rare, so it costs nothing at its real frequency.
		// ⚠ The GAME space ONLY. Authored data references no MODDERGAMEOPTION_ at all -- every gate and every
		// condition in the tree names a real GAMEOPTION_ -- so a modder-option flip (a BUG-menu slider like the
		// leader-promotion culture threshold) moves no verdict, and re-gating every city for it would be a blanket
		// bought with nothing. Emit liberally, GATE precisely.
		case SEVT_GAME_OPTION_CHANGED:
			if (kEvent.iB == GAMEOPTSPACE_GAME)
			{
				BuildingEnabler::gateAllCities();
				UnitEnabler::gateAllCities();
			}
			break;
		default: break;
		}
	}
};

static CvEnablerSpineConsumer s_enablerConsumer;

void enablerRegisterConsumer()
{
	eventSpine().registerConsumer(&s_enablerConsumer);
}
