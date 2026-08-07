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
	ENE_DOMAIN_CENSUS = 1,   // what the reseed ACTUALLY BUILT in a player's domains, once the load gate pass has run
	// What the PLOT-ATOM index actually compiled. It exists because the failure mode of this index is SILENT: an
	// empty one re-gates nobody, which is indistinguishable from "no candidate needed re-gating" at every other
	// observation point -- a reverse walk that found nothing already shipped once looking verified.
	ENE_PLOTATOM_CENSUS = 2,
	// How many candidates each COARSE gate class holds. A class holding most of the registry makes every fact
	// routed through it a whole-registry re-gate -- the shape par.7.1's "small load-compiled set" rules out -- and
	// without this line that widening is invisible.
	ENE_GATECLASS_CENSUS = 3
};

enum EnFld
{
	ENF_OWNER = 1,   // the player
	ENF_DOMAIN,      // which domain, by name
	ENF_INTREE,      // members at >= GREYED -- the edges APPLIED
	ENF_LISTED,      // members the gate PASSED -- offerable now
	ENF_TOTAL,       // the registry size, so an empty domain is legible without a second lookup
	ENF_ATOMKEYS,    // distinct (kind, id) plot atoms any candidate names -- 0 means the index built EMPTY
	ENF_ATOMENTRIES, // total candidate entries across those keys (the re-gate work one atom can cost)
	ENF_CLASS,       // the gate class, by name
	ENF_MEMBERS      // candidates in it -- read against `of` (the registry size) to see how coarse it is
};

static const char* en_prefix(int evt)
{
	switch (evt)
	{
	case ENE_DOMAIN_CENSUS: return "[ENABLER/census]";
	case ENE_PLOTATOM_CENSUS: return "[ENABLER/plotatoms]";
	case ENE_GATECLASS_CENSUS: return "[ENABLER/gateclass]";
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
	case ENF_ATOMKEYS:    return "atomKeys";
	case ENF_ATOMENTRIES: return "atomEntries";
	case ENF_CLASS:   *peType = SFT_STR; return "class";
	case ENF_MEMBERS: return "members";
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
//	THE PLOT-ATOM CENSUS -- load-compiled static data, so it is emitted ONCE and not per player.
//	⚑ Read it against the authored data: every candidate whose `requires` names a terrain / feature / improvement
//	/ route / mapcategory / plot bit is one entry, so `atomKeys=0` means the index compiled empty and the whole
//	plot plane re-gates nobody.
static void en_emitPlotAtomCensus()
{
	int iBuildingKeys = 0;
	int iBuildingEntries = 0;
	int iUnitKeys = 0;
	int iUnitEntries = 0;
	BuildingEnabler::plotAtomCensus(iBuildingKeys, iBuildingEntries);
	UnitEnabler::plotAtomCensus(iUnitKeys, iUnitEntries);
	CvSpineEvent eBuildings(EVENTKIND_DIAGNOSTIC, SD_ENABLER, ENE_PLOTATOM_CENSUS, 0);
	eBuildings.addStr(ENF_DOMAIN, "buildings").addI(ENF_ATOMKEYS, iBuildingKeys).addI(ENF_ATOMENTRIES, iBuildingEntries);
	eventSpine().emit(eBuildings);
	CvSpineEvent eUnits(EVENTKIND_DIAGNOSTIC, SD_ENABLER, ENE_PLOTATOM_CENSUS, 0);
	eUnits.addStr(ENF_DOMAIN, "units").addI(ENF_ATOMKEYS, iUnitKeys).addI(ENF_ATOMENTRIES, iUnitEntries);
	eventSpine().emit(eUnits);
}

//	THE GATE-CLASS CENSUS -- load-compiled static data like the plot atoms, so once, not per player.
//	⚑ Read `members` against `of`: a class approaching the registry size is NOT a bounded re-gate set, and every
//	fact routed through it re-gates nearly everything. That is the number the coarse-class routing lives or dies
//	on, and it is what makes an axis quietly keeping the catch-all (one that gained a precise route but still
//	marks `dynamic`) visible instead of merely suspected.
static void en_emitGateClassCensus()
{
	static const char* const szClassNames[] = { "pop", "power", "goldenAge", "stateReligion", "dynamic" };
	int aiBuildings[BuildingEnabler::NUM_GATE_CLASSES];
	int aiUnits[UnitEnabler::NUM_GATE_CLASSES];
	int iBuildingTotal = 0;
	int iUnitTotal = 0;
	BuildingEnabler::gateClassCensus(aiBuildings, iBuildingTotal);
	UnitEnabler::gateClassCensus(aiUnits, iUnitTotal);
	for (int iClass = 0; iClass < BuildingEnabler::NUM_GATE_CLASSES; ++iClass)
	{
		CvSpineEvent e(EVENTKIND_DIAGNOSTIC, SD_ENABLER, ENE_GATECLASS_CENSUS, 0);
		e.addStr(ENF_DOMAIN, "buildings").addStr(ENF_CLASS, szClassNames[iClass])
		 .addI(ENF_MEMBERS, aiBuildings[iClass]).addI(ENF_TOTAL, iBuildingTotal);
		eventSpine().emit(e);
	}
	for (int iClass = 0; iClass < UnitEnabler::NUM_GATE_CLASSES; ++iClass)
	{
		CvSpineEvent e(EVENTKIND_DIAGNOSTIC, SD_ENABLER, ENE_GATECLASS_CENSUS, 0);
		e.addStr(ENF_DOMAIN, "units").addStr(ENF_CLASS, szClassNames[iClass])
		 .addI(ENF_MEMBERS, aiUnits[iClass]).addI(ENF_TOTAL, iUnitTotal);
		eventSpine().emit(e);
	}
}

static void en_emitDomainCensus()
{
	en_emitPlotAtomCensus();
	en_emitGateClassCensus();
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
				// ⛔ THE OPERATE SET IS THE OTHER HALF, AND IT IS NOT THE FRONTIER'S. The lines above move what the
				// city may BUILD; this moves what it is RUNNING. A building arriving must resolve ITS OWN dormancy --
				// added and dormant is an ordinary outcome, not a contradiction -- and must re-check the buildings
				// whose operate references it and those it dorm-triggers. Without it a new building never enters the
				// active set, so it never deposits and never supplies its `provides.bonuses`, and a manufactured
				// chain cannot light its next tier (enabler.md §3.2 / §7: the set is seeded once and maintained
				// thereafter by exactly this targeted propagation).
				EnablerKernel::onBuildingChangedActive(pCity, kEvent.iType);
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
				EnablerKernel::onHaveChangedActive(pCity, CASC_HAVE_RELIGION);   // the operate half of the same axis
			}
			break;
		}
		case SEVT_CITY_CORPORATION_ADDED:
		case SEVT_CITY_CORPORATION_REMOVED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityCorporationChanged(*pCity, kEvent.iType, kEvent.iEventId == SEVT_CITY_CORPORATION_ADDED);
				EnablerKernel::onHaveChangedActive(pCity, CASC_HAVE_CORP);   // the operate half of the same HAVE axis
			}
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
				// The OPERATE half: this bonus becoming (un)available re-checks exactly the buildings whose
				// requires.operate consumes it, and the provides-ripple inside carries the cascading flips -- which
				// is how a manufactured chain lights tier by tier instead of stopping at whatever the load resolved.
				EnablerKernel::onBonusAccessChangedActive(pCity, kEvent.iType);
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
				// The LOCAL half of the same supply (json §5a: a vicinity provider satisfies the same atom as a
				// traded one), so the operate consumers re-check on it too.
				EnablerKernel::onBonusAccessChangedActive(pCity, kEvent.iType);
			}
			break;
		}
		// ---- THE PROPERTY BAND CROSSING -- the operate half of the PROPERTY_ axis (enabler.md §3). ----
		// A band is a `requires.operate` {PROPERTY_X, min/max} clause, and it is what decides active-vs-dormant for
		// the whole system-placed band class (crime / disease / education / pollution): the building is placed once
		// and never removed, so this crossing is the ONLY thing that ever moves its verdict.
		// ⚑ The re-check is gated on a BOUNDARY crossing rather than fired on every property move, and that is
		// what makes it affordable: the solver runs propagators -> interactions -> sources every doTurn, so a
		// property value moves constantly in every city while a band verdict moves only when the value passes a
		// threshold some clause actually declares. s_operatePropertyBandThresholds holds exactly those boundaries,
		// which is why it is built beside the consumer index.
		// ⚑ The consumer does NO arithmetic: the HOLDER announces the crossing (CvProperties, beside the value
		// fact), so this routes the happening and nothing more. The raw SEVT_PROPERTY_ADDED / _REMOVED value fact
		// is deliberately NOT handled here -- it fires for nearly every property of every city every turn.
		case SEVT_CITY_PROPERTY_BAND_ADDED:
		case SEVT_CITY_PROPERTY_BAND_REMOVED:
		{
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			// Direction-less by design: ek_classifyBuilding re-reads the live value against each band, so which
			// way the boundary was crossed is redundant once the fact says one WAS.
			if (pCity != NULL) EnablerKernel::onPropertyBandHitActive(pCity, kEvent.iType);
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
				// The OPERATE half. A tech moves two things this set depends on: an operate condition that reads a
				// tech, and OBSOLESCENCE -- and a tech is the only thing that can obsolete (enabler.md §3.2), so
				// this is where a building's obsolete verdict is re-derived at all.
				foreach_(const CvCity* pLoopCity, GET_PLAYER((PlayerTypes)kEvent.iC).cities())
				{
					EnablerKernel::onPlayerScopeChangedActive(pLoopCity);
				}
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
				// The OPERATE half: a civic swap moves what an operate condition reading a civic resolves to.
				foreach_(const CvCity* pLoopCity, GET_PLAYER((PlayerTypes)kEvent.iC).cities())
				{
					EnablerKernel::onPlayerScopeChangedActive(pLoopCity);
				}
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
				EnablerKernel::onHaveChangedActive(pCity, CASC_HAVE_POP);   // the operate half of the same axis
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
				EnablerKernel::onHaveChangedActive(pCity, CASC_HAVE_POWER);   // the operate half of the same axis
			}
			break;
		}
		case SEVT_EMPIRE_GOLDEN_AGE_ADDED:
		case SEVT_EMPIRE_GOLDEN_AGE_REMOVED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				BuildingEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, BuildingEnabler::GATE_GOLDEN_AGE);
				UnitEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, UnitEnabler::GATE_GOLDEN_AGE);
				// The OPERATE half: a golden age is a player-scope axis an operate condition may read, so every
				// city of that player re-checks the buildings whose operate references one.
				foreach_(const CvCity* pLoopCity, GET_PLAYER((PlayerTypes)kEvent.iC).cities())
				{
					EnablerKernel::onPlayerScopeChangedActive(pLoopCity);
				}
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
		// ---- the BUILDING-count crossing: the `allowed` self-cap's own re-check (par.7.1 step 3) ----
		// The twin of the unit case above. A world/team/national wonder's cap is CROSS-CITY, so the per-city
		// presence fact cannot serve it: the city that BUILT it re-gates through onCityBuildingChanged, and every
		// other city of every affected player re-gates here.
		case SEVT_EMPIRE_BUILDING_COUNT_ADDED:
		case SEVT_EMPIRE_BUILDING_COUNT_REMOVED:
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
				BuildingEnabler::onBuildingCountChanged((PlayerTypes)kEvent.iC, kEvent.iType);
			break;
		// ---- THE LIVE-STATE (GATE_DYNAMIC) AXES, on the facts that actually move them ----
		// These atoms carry no reverse FK -- IS_CAPITAL, IS_HOLY_CITY, IS_GOVERNMENT_CENTER, a HERITAGE_ presence,
		// an ERA threshold -- so the load-compiled DYNAMIC class list is the re-gate set, exactly as the game-option
		// and nukes-ban flips already use it.
		// ⚑ WHY THESE FACTS AND NOT THE PLOT SUBSTRATE: a whole-CLASS re-gate is only affordable on a RARE
		// happening, and each fact below is rare -- a capital moves, an era advances, a religion founds, a city is
		// conquered. ⚠ It is affordable only while the CLASS stays bounded, which is a property of what
		// `scanCondDeps` marks rather than of this switch: read `[ENABLER/gateclass]` at load before adding a fact
		// here, because an axis that quietly kept the catch-all turns every one of these into a whole-registry
		// pass. ⛔ The
		// plot substrate is NOT rare and must NOT be routed here: it wants the narrower `EDGEF_REQUIRED_BY` re-gate
		// over the specific terrain / feature / improvement id ([enabler.md §7.1](../../docs/specs/enabler.md) step 2),
		// or a worked-plot flip would re-gate every building in the city.
		case SEVT_EMPIRE_ERA_ADDED:
		case SEVT_EMPIRE_ERA_REMOVED:
		case SEVT_EMPIRE_CAPITAL_ADDED:
		case SEVT_EMPIRE_CAPITAL_REMOVED:
		case SEVT_EMPIRE_REBEL_ADDED:
		case SEVT_EMPIRE_REBEL_REMOVED:
		case SEVT_EMPIRE_HERITAGE_ADDED:
		case SEVT_EMPIRE_HERITAGE_REMOVED:
			// A heritage is a HAVE axis the player holds but NOT an enabler DOMAIN (nothing offers heritages through
			// the frontier -- they arrive by mission), so it re-gates its dependents and applies no edges.
			// ⛔ SKIPPED INSIDE THE LOAD BRACKET, and this is not optional. Every fact here fires from
			// CvPlayer::read / CvCity::read, where the city's other slots have NOT landed yet -- and unlike the
			// small gate classes beside it, GATE_DYNAMIC contains the CAPPED buildings, so gating one mid-read
			// reaches the wonder-category cap and reads GC.getCultureLevelInfo(NO_CULTURELEVEL): a fail-loud
			// info-plane read that kills the load ([DEC-info-plane-read-only]). The load-end pass gates every city
			// once after the stream ends, which is the par.7.1 order rule's second option and covers all of this.
			if (spineGameLoadInProgress()) break;
			if (kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
			{
				BuildingEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, BuildingEnabler::GATE_DYNAMIC);
				UnitEnabler::onPlayerGateClass((PlayerTypes)kEvent.iC, UnitEnabler::GATE_DYNAMIC);
			}
			break;
		case SEVT_CITY_HOLY_CITY_ADDED:
		case SEVT_CITY_HOLY_CITY_REMOVED:
		case SEVT_CITY_GOVERNMENT_CENTER_ADDED:
		case SEVT_CITY_GOVERNMENT_CENTER_REMOVED:
		case SEVT_CITY_HEADQUARTERS_ADDED:
		case SEVT_CITY_HEADQUARTERS_REMOVED:
		case SEVT_CITY_FRESH_WATER_ADDED:
		case SEVT_CITY_FRESH_WATER_REMOVED:
		{
			// The city-scope twins of the above: one city's designation moved, so only that city re-gates.
			// Same load-bracket skip, same reason (the capped buildings' culture-level read).
			if (spineGameLoadInProgress()) break;
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::onCityGateClass(*pCity, BuildingEnabler::GATE_DYNAMIC);
				UnitEnabler::onCityGateClass(*pCity, UnitEnabler::GATE_DYNAMIC);
			}
			break;
		}
		// ---- THE PLOT PLANE -- par.7.1 step 2, the narrow per-atom re-gate ----
		// The single largest gate axis in the authored data (MAPCATEGORY_ / TERRAIN_ / FEATURE_ / IMPROVEMENT_ /
		// HAS_COAST / HAS_RIVER across thousands of entities): a terraform, a chop, an improvement built or
		// pillaged must move the verdicts that named it, and a tri-state read is a bare fetch that nothing
		// recomputes ([DEC-no-self-heal]), so a missed route leaves the stale verdict standing for the session.
		// ⚑ TWO things keep it affordable at plot-fact frequency, and both are required:
		//   - the CANDIDATES come from the enabler's own (kind, id) plot-atom index -- only what actually names
		//     the atom. ⛔ NOT EDGEF_REQUIRED_BY: the reverse pass deliberately routes no plot-substrate prefix
		//     (CvReversePass::rp_requiredByRefInfo returns NULL for every one), so that walk finds nothing and
		//     re-gates nobody -- silently, since an empty result is indistinguishable from "nothing to do".
		//     ⛔ And NOT the GATE_DYNAMIC class, which is effectively the whole registry.
		//   - the CITIES come from the plot's own workableBy list, so a plot no city can work re-gates nobody.
		case SEVT_PLOT_TERRAIN_ADDED:
		case SEVT_PLOT_TERRAIN_REMOVED:
		case SEVT_PLOT_FEATURE_ADDED:
		case SEVT_PLOT_FEATURE_REMOVED:
		case SEVT_PLOT_IMPROVEMENT_ADDED:
		case SEVT_PLOT_IMPROVEMENT_REMOVED:
		case SEVT_PLOT_ROUTE_ADDED:
		case SEVT_PLOT_ROUTE_REMOVED:
		// The plot's own derived VERDICT crossed (PlotContext's per-bit table). It is a different question from a
		// substrate fact -- that says what the tile now CARRIES, this says what it MEANS -- and it is the only
		// route the bare bits have: HAS_RIVER / HAS_FRESHWATER / IS_WATER / HAS_COAST and their kin name no
		// entity, so no substrate id could ever reach them.
		case SEVT_PLOT_PREDICATE_ADDED:
		case SEVT_PLOT_PREDICATE_REMOVED:
		{
			if (spineGameLoadInProgress()) break;   // the load-end pass gates every city once
			const CvPlot* pPlot = GC.getMap().plotByIndex(kEvent.iSrcLoc);
			if (pPlot == NULL || kEvent.iType < 0) break;
			int eAtomKind = PLOTATOM_PREDICATE;
			switch (kEvent.iEventId)
			{
			case SEVT_PLOT_TERRAIN_ADDED:
			case SEVT_PLOT_TERRAIN_REMOVED:     eAtomKind = PLOTATOM_TERRAIN; break;
			case SEVT_PLOT_FEATURE_ADDED:
			case SEVT_PLOT_FEATURE_REMOVED:     eAtomKind = PLOTATOM_FEATURE; break;
			case SEVT_PLOT_IMPROVEMENT_ADDED:
			case SEVT_PLOT_IMPROVEMENT_REMOVED: eAtomKind = PLOTATOM_IMPROVEMENT; break;
			case SEVT_PLOT_ROUTE_ADDED:
			case SEVT_PLOT_ROUTE_REMOVED:       eAtomKind = PLOTATOM_ROUTE; break;
			default:                            eAtomKind = PLOTATOM_PREDICATE; break;
			}
			const std::vector<IDInfo>& kWorkableBy = pPlot->workableByCities();
			for (size_t iCity = 0; iCity < kWorkableBy.size(); ++iCity)
			{
				const CvCity* pCity = ::getCity(kWorkableBy[iCity]);
				if (pCity == NULL) continue;
				BuildingEnabler::onPlotAtomChanged(*pCity, eAtomKind, kEvent.iType);
				UnitEnabler::onPlotAtomChanged(*pCity, eAtomKind, kEvent.iType);
			}
			break;
		}
		// ---- THE VICINITY SUPPLY'S **MAP** HALF -- the re-gate the plot facts carry ----
		// SEVT_CITY_VICINITY_BONUS_* carries the BUILDING half ALONE, by construction; the MAP half -- a resource
		// appearing or vanishing on a radius tile, and that tile beginning or ceasing to SERVE it -- is announced by
		// the PLOT facts ([event-spine.md], that fact's own contract). So this is where a `connection:"vicinity"`
		// atom's map half re-gates, and without it the verdict stands stale for the session: a tri-state read is a
		// bare fetch and nothing recomputes it ([DEC-no-self-heal]).
		// ⛔ NOT reachable through the plot-atom index: that is keyed by (PlotAtomKind, id) over the SUBSTRATE
		// prefixes, and a bonus atom names a BONUS -- so the improvement fact re-gates whoever named that
		// IMPROVEMENT and never the mine that named the ORE. The candidates here come from the bonus's own
		// EDGEF_REQUIRED_BY, which the reverse pass DOES route for the bonus prefix.
		// ⚑ TWO facts, two questions, and neither substitutes for the other: PLOT_BONUS says the tile CARRIES the
		// resource (the `owned` / default / `crossBorder` tiers), PLOT_SERVED_BONUS says it MAKES IT AVAILABLE (the
		// `onSite` tier). An improvement being built moves only the second, and moves no bonus fact at all.
		case SEVT_PLOT_BONUS_ADDED:
		case SEVT_PLOT_BONUS_REMOVED:
		case SEVT_PLOT_SERVED_BONUS_ADDED:
		case SEVT_PLOT_SERVED_BONUS_REMOVED:
		{
			// ⛔ NO LOAD GUARD, deliberately. `spineGameLoadInProgress` is the RESULT-PRODUCER suppression -- it
			// stops the trigger/grant machinery handing things out for a load that is not an acquisition
			// ([event-spine.md], [DEC-spine-reseed]) -- and the enabler is a LOAD-ACTIVE consumer that BUILDS from
			// the reseed's own facts. Borrowing it here would assert that the load cannot be trusted to build this,
			// which is the claim the reseed exists to falsify.
			// ⚑ Nothing is needed in its place: the map streams BEFORE the players, so while these facts fire no
			// city has established a work area yet and `workableByCities` is empty -- the fan reaches nobody by
			// construction rather than by a guard. The domain's own `isSeeded()` covers a candidate arriving early.
			const CvPlot* pPlot = (kEvent.iSrcLoc >= 0) ? GC.getMap().plotByIndex(kEvent.iSrcLoc) : NULL;
			if (pPlot == NULL || kEvent.iType < 0) break;
			// The CITIES come from the plot's own workableBy list, so a tile no city can work re-gates nobody.
			const std::vector<IDInfo>& kWorkableBy = pPlot->workableByCities();
			for (size_t iCity = 0; iCity < kWorkableBy.size(); ++iCity)
			{
				const CvCity* pCity = ::getCity(kWorkableBy[iCity]);
				if (pCity == NULL) continue;
				BuildingEnabler::onCityVicinityBonusChanged(*pCity, kEvent.iType);
				UnitEnabler::onCityVicinityBonusChanged(*pCity, kEvent.iType);
				// The OPERATE half of the same supply -- the provides-ripple inside is what lets a manufactured
				// chain light tier by tier rather than stopping where the load resolved.
				EnablerKernel::onBonusAccessChangedActive(pCity, kEvent.iType);
			}
			break;
		}
		// ---- CONQUEST: the city's whole HAVE basis is a different player's ----
		// Every axis the gate reads moves at once (techs, civics, traits, the empire's counts and caps), and the
		// tri-state is a bare fetch that nothing re-derives -- so a conquered city would otherwise serve the PREVIOUS
		// owner's verdicts for the rest of the game. The full per-city pass is the honest derivation, and a capture is
		// rare enough to pay for it.
		// ⚠ The ADDED end only: on _REMOVED the city has already left the old owner's list, so there is nothing to
		// resolve and nothing that still needs gating -- the ADDED twin re-gates it under whoever now owns it.
		case SEVT_CITY_OWNER_ADDED:
		{
			// ⛔ SKIPPED INSIDE THE LOAD BRACKET. This is the FIRST fact CvCity::read emits -- ownership is
			// established before population, buildings, religion and culture level -- so a full gate pass here runs
			// against a city that is barely deserialized, and the capped buildings' culture-level read fails loud.
			// A conquest in PLAY is what this route is for; the loaded state is the load-end pass's job.
			if (spineGameLoadInProgress()) break;
			const CvCity* pCity = cityForEvent(kEvent.iC, kEvent.iSrcLoc);
			if (pCity != NULL)
			{
				BuildingEnabler::gateCity(*pCity);
				UnitEnabler::gateCity(*pCity);
			}
			break;
		}
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
