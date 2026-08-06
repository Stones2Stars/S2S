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

// ===================== [ENABLER] spine domain (logging.md: logging is a spine CONSUMER) =====================
//	⛔ ON THE SPINE, NOT gDLL->logMsg. A legacy sink is HELD OPEN by the process, so it cannot be read while the
//	game runs -- which is worthless for a load-time census whose whole job is to be read after a load -- and those
//	call sites are being retired wholesale ([observability.md], todo.md).
enum EnEvt
{
	ENE_DOMAIN_CENSUS = 1   // what the reseed ACTUALLY BUILT in a player's domains, once the load gate pass has run
};

enum EnFld
{
	ENF_OWNER = 1,   // the player
	ENF_DOMAIN,      // which domain, by name
	ENF_INTREE,      // members at >= GREYED -- the edges APPLIED
	ENF_LISTED,      // members the gate PASSED -- offerable now
	ENF_TOTAL        // the registry size, so an empty domain is legible without a second lookup
};

static const char* en_prefix(int evt)
{
	switch (evt)
	{
	case ENE_DOMAIN_CENSUS: return "[ENABLER/census]";
	default:                return "[ENABLER]";
	}
}

static const char* en_field(int tag, SpineFieldType* peType)
{
	switch (tag)
	{
	case ENF_OWNER:  return "owner";
	case ENF_DOMAIN: *peType = SFT_STR; return "domain";
	case ENF_INTREE: return "inTree";
	case ENF_LISTED: return "listed";
	case ENF_TOTAL:  return "of";
	default:         return NULL;
	}
}

static void en_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_ENABLER, en_prefix, "Cascade.log", en_field); s_reg = true; }
}

//	THE DOMAIN CENSUS -- what the reseed BUILT, read rather than inferred.
//	⛔ It exists because "the domain is empty" was concluded three times from a vanishing UI list and never once
//	from the domain itself. It separates the two failures that are indistinguishable from outside: NOTHING IN THE
//	TREE (the edges never applied) versus IN THE TREE BUT GATED OUT (membership landed, the gate rejected it) --
//	which have completely different fixes.
static void en_emitDomainCensus()
{
	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (!kPlayer.isAlive())
		{
			continue;
		}
		//	EVERY player-held domain, not just the one under suspicion -- that is what makes the reading
		//	DISCRIMINATING rather than merely confirming. A promotions-only census cannot tell "the promotion
		//	applier is broken" from "no player domain builds at all", and those have opposite fixes.
		for (int iDomain = 0; iDomain < 3; ++iDomain)
		{
			const char* szDomain = "promotions";
			int iTotal = GC.getNumPromotionInfos();
			if (iDomain == 1) { szDomain = "techs";  iTotal = GC.getNumTechInfos(); }
			if (iDomain == 2) { szDomain = "civics"; iTotal = GC.getNumCivicInfos(); }

			int iInTree = 0;
			int iListed = 0;
			for (int iId = 0; iId < iTotal; iId++)
			{
				EnablerDomain::State eState = EnablerDomain::STATE_HIDDEN;
				if (iDomain == 0) eState = kPlayer.getPromotionUnlocked((PromotionTypes)iId);
				else if (iDomain == 1) eState = kPlayer.getTechAvailability((TechTypes)iId);
				else eState = kPlayer.getCivicAvailability((CivicTypes)iId);

				if (eState >= EnablerDomain::STATE_GREYED) ++iInTree;
				if (eState == EnablerDomain::STATE_LISTED) ++iListed;
			}
			CvSpineEvent e(EVENTKIND_DIAGNOSTIC, SD_ENABLER, ENE_DOMAIN_CENSUS, 0);
			e.addI(ENF_OWNER, iPlayer).addStr(ENF_DOMAIN, szDomain)
			 .addI(ENF_INTREE, iInTree).addI(ENF_LISTED, iListed).addI(ENF_TOTAL, iTotal);
			eventSpine().emit(e);
		}
	}
}

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
		// The two PRESENCE happenings. The direction is the EVENT ID, so nothing here reads a delta int
		// ([DEC-facts-name-happenings]); the two cases share a body only because the domain appliers are
		// parameterized on held-ness, never because the fact was.
		case SEVT_CITY_BUILDING_ADDED:
		case SEVT_CITY_BUILDING_REMOVED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const bool bHeld = (kEvent.iEventId == SEVT_CITY_BUILDING_ADDED);
				// held flip + the building's own enables contribution.
				// ORDER: UnitEnabler first (its flip guard reads the buildings domain's held flag PRE-flip).
				UnitEnabler::onCityBuildingChanged(*pCity, kEvent.iType, bHeld);
				BuildingEnabler::onCityBuildingChanged(*pCity, kEvent.iType, bHeld);   // idempotency = the domain's held guard
			}
			break;
		}
		case SEVT_CITY_RELIGION_ADDED:
		case SEVT_CITY_RELIGION_REMOVED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const bool bHeld = (kEvent.iEventId == SEVT_CITY_RELIGION_ADDED);
				BuildingEnabler::onCityReligionChanged(*pCity, kEvent.iType, bHeld);
				UnitEnabler::onCityReligionChanged(*pCity, kEvent.iType, bHeld);
			}
			break;
		}
		case SEVT_CITY_CORPORATION_ADDED:
		case SEVT_CITY_CORPORATION_REMOVED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL) BuildingEnabler::onCityCorporationChanged(*pCity, kEvent.iType, kEvent.iEventId == SEVT_CITY_CORPORATION_ADDED);
			break;
		}
		// The NETWORK supply presence crossing. The direction is the event id ([DEC-facts-name-happenings]); the
		// two cases share a body because the domain appliers take the crossing as a signed argument.
		case SEVT_CITY_BONUS_ADDED:
		case SEVT_CITY_BONUS_REMOVED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				const int iCrossing = (kEvent.iEventId == SEVT_CITY_BONUS_ADDED) ? 1 : -1;
				BuildingEnabler::onCityBonusChanged(*pCity, kEvent.iType, iCrossing);
				UnitEnabler::onCityBonusChanged(*pCity, kEvent.iType, iCrossing);
			}
			break;
		}
		case SEVT_CITY_VICINITY_BONUS_ADDED:
		case SEVT_CITY_VICINITY_BONUS_REMOVED:   // LOCAL presence flip: re-gate the bonus's connection:vicinity dependents
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityVicinityBonusChanged(*pCity, kEvent.iType);
				UnitEnabler::onCityVicinityBonusChanged(*pCity, kEvent.iType);
			}
			break;
		}
		case SEVT_CITY_CULTURE_LEVEL_ADDED:
		case SEVT_CITY_CULTURE_LEVEL_REMOVED:   // the culture-level HAVE axis
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL) BuildingEnabler::onCityCultureLevelChanged(*pCity, kEvent.iA,
				(kEvent.iEventId == SEVT_CITY_CULTURE_LEVEL_ADDED) ? +1 : -1);   // iA = the level this fact names
			break;
		}
		case SEVT_CITY_ORDER_ADDED:
		case SEVT_CITY_ORDER_REMOVED:   // queue push/pop: the buildings gate's queued-exclusion re-gate (par.7.1 step 3)
		{
			if (kEvent.iA == ORDER_CONSTRUCT)
			{
				const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
				if (pCity != NULL) BuildingEnabler::onCityOrderChanged(*pCity, kEvent.iType);
			}
			break;
		}
		case SEVT_EMPIRE_TECH_ADDED:
		case SEVT_EMPIRE_TECH_REMOVED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				// A tech moves the per-bonus TechCityTrade GATE, and that gate lives inside CvCity::getNumBonuses
				// -- ABOVE the network-count compare the ordinary crossing path runs -- so a tech flip changes
				// what a city HOLDS without any network count moving. There is nothing to refresh (the plot group
				// owns the number and the city only relays it, [enabler.md] RESIDENCY); what is owed is the
				// PRESENCE CROSSING, announced here for exactly the bonuses this tech gates.
				//
				// ⛔ THE DIRECTION IS THE EVENT'S IDENTITY, NEVER A PAYLOAD FLAG
				// ([DEC-facts-name-happenings]: "a consumer learns what happened BY WHICH EVENT IT RECEIVED, and
				// reads the payload only for how much"). The emit carries `iA = 0` on BOTH ends -- the fact split
				// into _ADDED / _REMOVED precisely so no discriminator is needed -- so decoding `iA != 0` reads
				// EVERY tech acquisition as a REMOVAL.
				// ⚠ Measured cost of that one expression: `TechEnabler` then hit `isHeld(eTech) == bHas`
				// (false == false) and skipped, so NO tech was ever marked held on a player domain -- and every
				// domain fed off that flag (promotions, civics, projects, processes, builds) built EMPTY, for
				// every player, on every load. Buildings survived only because their own city facts feed them.
				const bool bTechAcquired = (kEvent.iEventId == SEVT_EMPIRE_TECH_ADDED);
				for (int iBonus = 0; iBonus < GC.getNumBonusInfos(); ++iBonus)
				{
					const BonusTypes eBonus = static_cast<BonusTypes>(iBonus);
					if (GC.getBonusInfo(eBonus).getTechCityTrade() != kEvent.iType)
					{
						continue;
					}
					foreach_(CvCity* pLoopCity, GET_PLAYER((PlayerTypes)kEvent.iC).cities())
					{
						// the tech gate is a hard zero, so the verdict crosses iff the city holds any of this
						// bonus once the gate is discounted -- the same quantity getNumBonuses returns behind it.
						const int iHeldBehindGate =
							pLoopCity->getNumBonusesFromBase(eBonus, pLoopCity->getNetworkBonusCount(eBonus))
							+ pLoopCity->getCorpBonusProduction(eBonus);

						if (iHeldBehindGate != 0)
						{
							pLoopCity->processBonus(eBonus, bTechAcquired ? 1 : -1);
						}
					}
				}
				// the tech domain's O(delta) membership update -- the SOLE maintainer of the availability
				// vectors' tech axis. iType=Tech, iA=has, iC=triggering player (the team resolves from it).
				// ORDERING CONTRACT: every domain whose flip guard reads the PLAYER tech domain's held flag
				// MUST run BEFORE TechEnabler::onTechChanged flips that flag.
				BuildingEnabler::onCityTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, bTechAcquired);
				UnitEnabler::onCityTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, bTechAcquired);
				CivicEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, bTechAcquired);
				ProjectEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, bTechAcquired);
				ProcessEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, bTechAcquired);
				BuildEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, bTechAcquired);
				PromotionEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, bTechAcquired);
				TechEnabler::onTechChanged(GET_PLAYER((PlayerTypes)kEvent.iC).getTeam(), (TechTypes)kEvent.iType, bTechAcquired);
			}
			break;
		case SEVT_EMPIRE_TRAIT_ADDED:
		case SEVT_EMPIRE_TRAIT_REMOVED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				// the HELD-TRAIT axis: a rung's own `enables.traits` edge is the developing ladder, so acquiring
				// one is what puts the next rung in the tree (iType=Trait, iC=player).
				// ⛔ The direction is the EVENT ID, like every other case here -- the emit carries iA = 0 on BOTH
				// ends ([DEC-facts-name-happenings]), so reading a payload flag makes every acquisition a removal.
				const CvPlayer& kTraitOwner = GET_PLAYER((PlayerTypes)kEvent.iC);
				EnablerKernel::applyPlayerHave(kTraitOwner, kTraitOwner.m_enabler.traits, EDGEB_TRAITS,
					InfoRepo<CvTraitInfo>::get().get(kEvent.iType),
					kEvent.iEventId == SEVT_EMPIRE_TRAIT_ADDED);
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
		case SEVT_EMPIRE_PROJECT_ADDED:
		case SEVT_EMPIRE_PROJECT_REMOVED:
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
		case SEVT_CITY_POPULATION_ADDED:
		case SEVT_CITY_POPULATION_REMOVED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityGateClass(*pCity, BuildingEnabler::GATE_POP);
				UnitEnabler::onCityGateClass(*pCity, UnitEnabler::GATE_POP);
			}
			break;
		}
		case SEVT_CITY_POWER_ADDED:
		case SEVT_CITY_POWER_REMOVED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityGateClass(*pCity, BuildingEnabler::GATE_POWER);
				UnitEnabler::onCityGateClass(*pCity, UnitEnabler::GATE_POWER);
			}
			break;
		}
		case SEVT_EMPIRE_GOLDEN_AGE_ADDED:
		case SEVT_EMPIRE_GOLDEN_AGE_REMOVED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				BuildingEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, BuildingEnabler::GATE_GOLDEN_AGE);
				UnitEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, UnitEnabler::GATE_GOLDEN_AGE);
			}
			break;
		case SEVT_EMPIRE_STATE_RELIGION_ADDED:
		case SEVT_EMPIRE_STATE_RELIGION_REMOVED:
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
		case SEVT_WORLD_NUKES_BANNED_ADDED:
		case SEVT_WORLD_NUKES_BANNED_REMOVED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				BuildingEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, BuildingEnabler::GATE_DYNAMIC);
				UnitEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, UnitEnabler::GATE_DYNAMIC);
			}
			break;
		// ---- the unit-count crossing (par.7.1 step 3): caps / unit-count requires / upgrade availability ----
		case SEVT_EMPIRE_UNIT_COUNT_ADDED:
		case SEVT_EMPIRE_UNIT_COUNT_REMOVED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
				UnitEnabler::onUnitCountChanged((PlayerTypes)kEvent.iC, kEvent.iType);
			break;
		// ---- the load-end gate pass (the par.7.1 order rule's "gate once after the stream ends" option --
		// fires while the bracket is still open, at the end of onFinalInitialized, state fully final) ----
		case SEVT_GAME_LOAD_FINISHED:
			BuildingEnabler::onLoadFinished();
			UnitEnabler::onLoadFinished();
			en_emitDomainCensus();
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
		case SEVT_GAME_OPTION_ADDED:
		case SEVT_GAME_OPTION_REMOVED:
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
	en_registerDomain();
	eventSpine().registerConsumer(&s_enablerConsumer);
}
