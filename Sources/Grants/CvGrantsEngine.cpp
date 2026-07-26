//
//	CvCascadeGrants -- the #430 GRANTS machine consumer + the [GRANTS] spine domain. See the header + grants-machine.md.
//	Slice-1: on a building-built / unit-created DOMAIN event, resolve the source entity's GENUINE grants off its mapped
//	CvJson<X>Info (the composed CvGrants unit in the InfoRepo, minus the deferred mission-keys) and emit a [GRANTS]
//	diagnostic. Resolution only -- it does NOT apply (legacy applies); un-run parity (owner: no live parity until everything is in).
//

#include "CvGameCoreDLL.h"          // PCH umbrella
#include "Grants/CvGrantsEngine.h"
#include "Spine/CvEventSpine.h"
#include "CvInfo.h"             // CvInfo::grantList / grantPulse / grantFlag (the CvGrants unit's read-throughs)
#include "Repos/InfoRepo.h"        // InfoRepo<CvXInfo>::get().get(id) -> the mapped CvInfo*
#include "CvBuildingInfo.h"        // InfoRepo<CvBuildingInfo>
#include "CvUnitInfo.h"            // InfoRepo<CvUnitInfo>
#include "CvTechInfo.h"           // InfoRepo<CvTechInfo> (tech first-discover grants)
#include "CvReligionInfo.h" // InfoRepo<CvReligionInfo> (religion founder grants)
#include "CvCivicInfo.h"    // InfoRepo<CvCivicInfo> (civic revolution grant)
#include "Infos/CvCivilizationInfo.h" // InfoRepo<CvCivilizationInfo> (game-start civ grants)
#include "Infos/CvEraInfo.h"      // InfoRepo<CvEraInfo> (game-start era grants)
#include "Infos/CvHandicapInfo.h" // InfoRepo<CvHandicapInfo> (game-start handicap grants)
#include "AI/CvPlayerAI.h"        // GET_PLAYER -- the player's civ/era/handicap for the game-start resolve
#include "Engine/CvGame.h"        // GC.getGame().getStartEra() -- the era the game-start grants key on
#include "Engine/CvCity.h"        // the per-turn apply walks the player's cities
#include "Engine/CityContext.h"   // fillEvalCtx (city/plot) -- the contexts fill the eval state (contexts.md)
#include "Engine/EmpireContext.h" // fillEvalCtx (player/team)
#include "Engine/CvUnit.h"        // the spawned unit + the full-heal targets
#include "Engine/CvPlot.h"        // the city plot's units (full heal) + the criminal count (crime spawn odds)
#include "Infos/CvPropertyInfo.h" // getAIWeight -- the positive/negative property split (spawn owner)
#include "Infos/CvTriggers.h"     // CvTriggerEntry -- the COMPOSED `triggers` entries (trigger/chance/action)
#include "Enabler/CvEnablerKernel.h"      // operatingBuildings/wireOperatingBuildings -- a dormant building grants nothing
#include "Conditions/CvConditionEval.h" // cascadeEvalCondition -- the ONE evaluator for a trigger entry's condition
#include "AI/BetterBTSAI.h"        // gPlayerLogLevel -- the slice-1 observe gate
#include <map>
#include <string>
#include <vector>

// ===================== [GRANTS] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
enum GrEvt { GRE_BUILDING = 1, GRE_UNIT, GRE_TECH, GRE_RELIGION, GRE_CIVIC, GRE_GAMESTART, GRE_REPEAT, GRE_FOUND };
enum GrFld
{
	GF_PLAYER = 1, GF_BUILDING, GF_UNIT, GF_TECH, GF_RELIGION, GF_CIVIC,
	GF_PROMOTIONS, GF_GRANTBUILDINGS,                        // unit genuine grants (promotions + settle-time buildings)
	GF_TRIGGERENTRIES, GF_FREEPROMOS, GF_FREETECHS,          // building triggers + genuine grants
	GF_GOLDENAGE, GF_POPULATION,                             // building flag + scoped-pulse grants (increment 2)
	GF_FIRSTUNIT, GF_FIRSTPROPHET,                           // tech first-discover grants (increment 3a)
	GF_NUMFREEUNITS, GF_FREEUNIT, GF_REVOLUTION,             // religion + civic grants (increment 3b)
	GF_CIVICS, GF_TECHS, GF_BUILDINGS, GF_STARTINGGOLD,      // game-start civ + era/handicap grants (increment 3c)
	GF_SUPPRESSED,                                           // 1 = resolved but WITHHELD
	GF_FIRSTACQUIRE,                                         // buildings: 1 = genuine first build, 0 = conquest/restore
	GF_CITY, GF_SPAWNED, GF_HEALED,                          // the per-turn apply (increment 5): what actually LANDED
	GF_APPLIED,                                              // 1 = the machine ran the FIRST-BUILD apply (NOT a claim about every grant on the line)
	GF_MATMISMATCH                                           // 1 = a mapFrom-materialized getter disagrees with its composed grants read
};
static const char* gr_prefix(int evt)
{
	switch (evt)
	{
	case GRE_BUILDING: return "[GRANTS/building]";
	case GRE_UNIT:     return "[GRANTS/unit]";
	case GRE_TECH:     return "[GRANTS/tech]";
	case GRE_RELIGION: return "[GRANTS/religion]";
	case GRE_CIVIC:    return "[GRANTS/civic]";
	case GRE_GAMESTART: return "[GRANTS/gameStart]";
	case GRE_REPEAT:   return "[GRANTS/repeat]";
	case GRE_FOUND:    return "[GRANTS/cityFounded]";
	default:           return "[GRANTS]";
	}
}
static const char* gr_field(int tag, SpineFieldType* peType)
{
	*peType = SFT_INT;
	switch (tag)
	{
	case GF_PLAYER:         *peType = SFT_PLAYER;   return "player";
	case GF_BUILDING:       *peType = SFT_BUILDING; return "building";
	case GF_UNIT:           *peType = SFT_UNIT;     return "unit";
	case GF_TECH:           *peType = SFT_TECH;     return "tech";
	case GF_RELIGION:       *peType = SFT_RELIGION; return "religion";
	case GF_CIVIC:          *peType = SFT_CIVIC;    return "civic";
	case GF_FIRSTUNIT:      *peType = SFT_UNIT;     return "firstFreeUnit";
	case GF_FIRSTPROPHET:   *peType = SFT_UNIT;     return "firstFreeProphet";
	case GF_FREEUNIT:       *peType = SFT_UNIT;     return "freeUnit";
	case GF_NUMFREEUNITS:   return "numFreeUnits";
	case GF_REVOLUTION:     return "revolution";
	case GF_CIVICS:         return "civics";
	case GF_TECHS:          return "techs";
	case GF_BUILDINGS:      return "buildings";
	case GF_STARTINGGOLD:   return "startingGold";
	case GF_SUPPRESSED:     return "suppressed";
	case GF_FIRSTACQUIRE:   return "firstAcquire";
	case GF_PROMOTIONS:     return "promotions";
	case GF_GRANTBUILDINGS: return "grantBuildings";
	case GF_TRIGGERENTRIES: return "triggerEntries";
	case GF_FREEPROMOS:     return "freePromotions";
	case GF_FREETECHS:      return "freeTechs";
	case GF_GOLDENAGE:      return "goldenAge";
	case GF_POPULATION:     return "population";
	case GF_CITY:           *peType = SFT_INT;      return "city";
	case GF_SPAWNED:        *peType = SFT_UNIT;     return "spawned";
	case GF_HEALED:         return "healed";
	case GF_APPLIED:        return "appliedFirstBuild";
	case GF_MATMISMATCH:    return "matMismatch";
	default:                return NULL;
	}
}
static void gr_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_GRANTS, gr_prefix, "Cascade.log", gr_field); s_reg = true; }
}

// The LOAD-BRACKET flag (event-spine.md / DEC-spine-reseed). A grant is the RESULT of a genuine in-play
// acquisition and a LOAD IS NOT ONE: the save read replays every present fact as a DOMAIN event, so an applying
// machine would re-grant a whole empire on every load. Suppression withholds the APPLY ONLY -- the machine still
// RESOLVES and still emits, carrying `suppressed=1`, so "saw them and withheld" is distinguishable from "saw
// nothing" on /events. A bare early-return makes those two identical.
static bool s_bSuppressed = false;
// Buildings only: whether the acquisition that triggered this resolve was a GENUINE first build. Conquest
// (CvPlayer::acquireCity re-adding the captured buildings with bFirst=false) and the load restore are NOT, and
// the engine's own grant block is gated on exactly this bit -- so acting on them would re-grant a captured
// city's entire first-build set. Emitted beside `suppressed` so the REASON is on the wire: suppressed=1 with
// firstAcquire=0 is "conquest/restore", with firstAcquire=1 is "the load bracket was open".
static bool s_bFirstAcquire = true;

// ===================== the grant-key handles =====================
// Minted ONCE off the CvGrants LOCAL intern table (every runtime grant read is int-keyed; the authored strings
// live on the parse surface only, [DEC-materialize-at-mapfrom]). Mint-on-first-ask makes static-init order safe.
static const int gr_keyPromotions       = CvGrants::key("promotions");
static const int gr_keyBuildings        = CvGrants::key("buildings");
static const int gr_keyUnits            = CvGrants::key("units");
static const int gr_keyTechs            = CvGrants::key("techs");
static const int gr_keyCivics           = CvGrants::key("civics");
static const int gr_keyFreeTechs        = CvGrants::key("freeTechs");
static const int gr_keyGoldenAge        = CvGrants::key("goldenAge");
static const int gr_keyPopulation       = CvGrants::key("population");
static const int gr_keyScopeCity        = CvGrants::key("city");
static const int gr_keyScopeEmpire      = CvGrants::key("empire");
static const int gr_keyFirstFreeUnit    = CvGrants::key("firstFreeUnit");
static const int gr_keyFirstFreeProphet = CvGrants::key("firstFreeProphet");
static const int gr_keyNumFreeUnits     = CvGrants::key("numFreeUnits");
static const int gr_keyFreeUnit         = CvGrants::key("freeUnit");
static const int gr_keyRevolution       = CvGrants::key("revolution");
static const int gr_keyStartingGold     = CvGrants::key("startingGold");

// ===================== resolution off the mapped CvInfo =====================
// A grantList bucket's id-count (0 if absent). GENUINE buckets only -- the deferred mission-keys (unit `buildings`/
// `greatPeople`/`greatPersonAction`/`goldenAge`) are simply not read here (they migrate in the missions pass).
static int gr_listCount(const CvInfo* j, int iBucketKey)
{
	const std::vector<int>* l = j->grantList(iBucketKey);
	return (l != NULL) ? (int)l->size() : 0;
}
static int gr_pulse(const CvInfo* j, int iChannelKey)   // pulses are stored ×100 -> /100 to the human count/amount
{
	return j->grantPulse(iChannelKey) / 100;
}
static int gr_flag(const CvInfo* j, int iFlagKey)   // a bool grant present? (goldenAge)
{
	return j->grantFlag(iFlagKey) ? 1 : 0;
}
static int gr_scopedPulseSum(const CvInfo* j, int iChannelKey)   // sum a scoped pulse over its scopes (×100 -> /100)
{
	const CvGrants* g = j->getGrants();
	return g ? g->scopedPulseSumAllScopes(iChannelKey) / 100 : 0;
}
static int gr_promoteEntryCount(const CvInfo* j)   // `triggers` promote entries (the end-turn free-promotion plane)
{
	const CvTriggers* pTriggers = j->getTriggers();
	if (pTriggers == NULL) return 0;
	int iCount = 0;
	for (size_t i = 0; i < pTriggers->entries().size(); ++i)
	{
		if (!pTriggers->entries()[i]->promotePromotions.empty()) ++iCount;
	}
	return iCount;
}

// ===== FREE PROMOTIONS (json §5) -- TARGETED PROPAGATION, never a per-turn rescan =====
//
// Two triggers cover the whole (active promo building x unit present) relation, each firing only when that
// relation actually CHANGES:
//   (1) a unit ENTERS the city      -> grant it from every active promo building   (SEVT_UNIT_ENTERED_CITY)
//   (2) a promo building GOES ACTIVE -> grant to every own-team unit present       (SEVT_BUILDING_PROCESSED, iB>0)
// (2) rides PROCESSED rather than the first-build apply because processBuilding fires on BOTH a fresh build AND a
// dormancy WAKE -- so one hook covers "the building was just built" and "the building stepped out of dormancy"
// (owner: both must grant). A unit TRAINED here is covered by its own creation path.
// The rejected alternative was rescanning every city's buildings x units every turn: measured at 42,336
// assign calls in ONE turn (one city alone at 1,859), nearly all of them re-checking promotions units already
// held. That is the blanket-recompute shape [DEC-no-self-heal] rejects, and the enabler's targeted-propagation
// model is the house pattern for exactly this.
//
// This also closes a LIVE defect: the legacy on-move path (CvCity::doPromotion) was gated on
// isApplyFreePromotionOnMove(), a hardcoded `false`, so a unit that walked into a city NEVER gained its
// promotions at all -- only units trained there did.
static int gr_promoteOneUnit(CvCity* pCity, CvUnit* pUnit)
{
	if (pCity == NULL || pUnit == NULL) return 0;
	if (pUnit->getTeam() != GET_PLAYER(pCity->getOwner()).getTeam()) return 0;
	const OperatingBuildings& ob = EnablerKernel::operatingBuildings(pCity);
	int n = 0;
	for (std::set<int>::const_iterator it = ob.active.begin(); it != ob.active.end(); ++it)
	{
		const CvBuildingInfo& kB = GC.getBuildingInfo((BuildingTypes)*it);
		if (kB.getFreePromoTypes().empty()) continue;
		n += pCity->assignPromotionsFromBuildingChecked(kB, pUnit);   // GRANTED, not visited
	}
	return n;
}

static int gr_promoteCityUnits(CvCity* pCity, const CvBuildingInfo& kB)
{
	if (pCity == NULL || kB.getFreePromoTypes().empty()) return 0;
	const TeamTypes eTeam = GET_PLAYER(pCity->getOwner()).getTeam();
	int n = 0;
	foreach_(CvUnit* pLoopUnit, pCity->plot()->units())
	{
		if (pLoopUnit->getTeam() != eTeam) continue;
		n += pCity->assignPromotionsFromBuildingChecked(kB, pLoopUnit);   // GRANTED, not visited
	}
	return n;
}

// The FIRST-BUILD provisions apply. Mirrors the legacy CvCity::setupBuilding `bFirst` block, which is DELETED:
// local population, empire/team population, the player-chosen free techs, and the golden age. Only these FOUR are
// live -- `grants.techs` (the legacy getFreeSpecialTech branch) has ZERO authorings across all 5,180 buildings, so
// that branch was dead code and is not ported.
//
// Ordering note: the legacy applied these MID-setup; the machine applies at SEVT_BUILDING_CHANGED, which fires at
// the END of setHasBuilding (CvCity.cpp:13486, after setupBuilding at :13446) -- so the building is fully set up
// before its provisions land, which is strictly the safer order.
static void gr_applyBuildingFirstBuild(const CvInfo* j, int iBuilding, int iPlayer, int iCity)
{
	CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
	CvCity* pCity = player.getCity(iCity);
	if (pCity == NULL || j->getGrants() == NULL) return;


	// LOCAL population -- legacy applied this OUTSIDE the isFinalInitialized/WorldBuilder guard, so it does too.
	const int iPopCity = j->getGrants()->scopedPulse(gr_keyPopulation, gr_keyScopeCity) / 100;
	if (iPopCity != 0)
	{
		if (iPopCity > 0)
			for (int i = 0; i < iPopCity; ++i) pCity->changeFood(pCity->growthThreshold(), true);
		else
			for (int i = 0; i < -iPopCity; ++i) pCity->changeFood(-std::max(pCity->growthThreshold(-1), pCity->getFood() + 1), true);
		pCity->AI_updateAssignWork();   // don't starve with the extra citizen working nothing
	}

	// The rest are gated exactly as legacy gated them.
	if (!GC.getGame().isFinalInitialized() || gDLL->GetWorldBuilderMode()) return;

	if (j->getGrants()->flag(gr_keyGoldenAge))
	{
		player.changeGoldenAgeTurns(1 + player.getGoldenAgeLength());
	}

	const int iPopEmpire = j->getGrants()->scopedPulse(gr_keyPopulation, gr_keyScopeEmpire) / 100;
	if (iPopEmpire > 0)
	{
		const CvBuildingInfo& kB = GC.getBuildingInfo((BuildingTypes)iBuilding);
		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			// isTeamShare spreads it across the TEAM; otherwise just the owner's own cities.
			if (!kB.isTeamShare() ? (iI != iPlayer) : !GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(pCity->getTeam())) continue;
			foreach_(CvCity* pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
			{
				for (int i = 0; i < iPopEmpire; ++i) pLoopCity->changeFood(pLoopCity->growthThreshold());
				pLoopCity->AI_updateAssignWork();
			}
		}
	}

	const int iFreeTechs = j->getGrants()->pulse(gr_keyFreeTechs) / 100;
	if (iFreeTechs > 0)
	{
		if (pCity->isHuman())
		{
			player.chooseTech(iFreeTechs,
				gDLL->getText("TXT_KEY_MISC_COMPLETED_WONDER_CHOOSE_TECH", GC.getBuildingInfo((BuildingTypes)iBuilding).getTextKeyWide()));
		}
		else for (int i = 0; i < iFreeTechs; ++i) player.AI_chooseFreeTech();
	}
}

static void gr_resolveBuilding(int iBuilding, int iPlayer, int iCity)
{
	const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(iBuilding);
	if (j == NULL) return;
	const int nRepeat    = (j->getTriggers() != NULL) ? (int)j->getTriggers()->entries().size() : 0;   // the `triggers` entries (per-turn spawn/heal/promote)
	const int nFreePromo = gr_promoteEntryCount(j);   // end-turn promotions to units in the city (triggers promote entries)
	const int nFreeTech  = gr_pulse(j, gr_keyFreeTechs);            // one-shot on first build
	const int nGoldenAge = gr_flag(j, gr_keyGoldenAge);             // one-shot golden age (bool grant, increment 2)
	const int nPop       = gr_scopedPulseSum(j, gr_keyPopulation);  // one-shot population boost (scoped pulse, increment 2)
	if (nRepeat == 0 && nFreePromo == 0 && nFreeTech == 0 && nGoldenAge == 0 && nPop == 0) return;
	// The two population scopes SEPARATELY (nPop is their sum) -- the apply and the tripwire need them apart.
	const int nPopCity   = (j->getGrants() != NULL) ? j->getGrants()->scopedPulse(gr_keyPopulation, gr_keyScopeCity)   / 100 : 0;
	const int nPopEmpire = (j->getGrants() != NULL) ? j->getGrants()->scopedPulse(gr_keyPopulation, gr_keyScopeEmpire) / 100 : 0;
	// The MATERIALIZATION tripwire: the mapFrom-materialized getters must agree with the composed grants read they
	// were materialized FROM. They are otherwise unobservable (grants are not on /state/info), so a silent zeroing
	// -- the real hazard of moving a live read to load time -- would never surface. 1 = they diverged.
	const CvBuildingInfo& kB = GC.getBuildingInfo((BuildingTypes)iBuilding);
	const bool bMatMismatch =
		   kB.getPopulationChange()       != nPopCity
		|| kB.getGlobalPopulationChange() != nPopEmpire
		|| kB.getFreeTechs()              != nFreeTech;

	// APPLY -- the machine hands the provisions over; legacy's bFirst block is deleted. Withheld exactly when the
	// resolution is (load bracket, or a conquest/restore that is not a genuine first acquisition).
	const bool bApplied = !s_bSuppressed && (iCity >= 0);
	if (bApplied) gr_applyBuildingFirstBuild(j, iBuilding, iPlayer, iCity);

	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_BUILDING, 1)
		.addI(GF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(GF_FIRSTACQUIRE, s_bFirstAcquire ? 1 : 0)
		.addI(GF_APPLIED, bApplied ? 1 : 0).addI(GF_MATMISMATCH, bMatMismatch ? 1 : 0)
		.addI(GF_PLAYER, iPlayer).addI(GF_BUILDING, iBuilding)
		.addI(GF_TRIGGERENTRIES, nRepeat).addI(GF_FREEPROMOS, nFreePromo).addI(GF_FREETECHS, nFreeTech)
		.addI(GF_GOLDENAGE, nGoldenAge).addI(GF_POPULATION, nPop));
}

// The unit's OWN `grants.promotions`, handed to the created INSTANCE. This is the ONLY leg of the legacy
// CvUnit::setFreePromotion that is a grant: the player free-promotion registry is written solely by
// CvPlayer::applyEvent (random events -- out of scope) and the trait-derived promotions are refcounted with the
// trait (a MODIFIER, alive-with-source). See grant-apply-sites.md §4.
static void gr_resolveUnit(int iUnit, int iPlayer, int iUnitId)
{
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(iUnit);
	if (j == NULL) return;
	const int nPromos = gr_listCount(j, gr_keyPromotions);   // free promotions on creation
	const int nFound  = gr_listCount(j, gr_keyBuildings);    // settle-time building seeds (grants.buildings on the settler)
	if (nPromos == 0 && nFound == 0) return;

	int nApplied = 0;
	if (!s_bSuppressed && iUnitId >= 0 && iPlayer >= 0 && nPromos > 0)
	{
		CvUnit* pUnit = GET_PLAYER((PlayerTypes)iPlayer).getUnit(iUnitId);
		const std::vector<int>* promos = (j->getGrants() != NULL) ? j->getGrants()->list(gr_keyPromotions) : NULL;
		if (pUnit != NULL && promos != NULL)
		{
			for (size_t i = 0; i < promos->size(); ++i)
			{
				const PromotionTypes eP = (PromotionTypes)(*promos)[i];
				if (!pUnit->isHasPromotion(eP)) { pUnit->setHasPromotion(eP, true, true); ++nApplied; }
			}
		}
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_UNIT, 1)
		.addI(GF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(GF_APPLIED, nApplied)
		.addI(GF_PLAYER, iPlayer).addI(GF_UNIT, iUnit)
		.addI(GF_PROMOTIONS, nPromos).addI(GF_GRANTBUILDINGS, nFound));
}

static int gr_firstId(const CvInfo* j, int iBucketKey)   // a single-id grant bucket's id (-1 if absent)
{
	return (j->getGrants() != NULL) ? j->getGrants()->firstListId(iBucketKey) : -1;
}

// The TECH first-discoverer provisions. Mirrors the CvTeam::setHasTech first-discover block, whose apply legs are
// DELETED -- what stays there is the non-grant residue: the `bClearResearchQueueAI` rider (a free tech invalidates
// the AI's queued research) and the "first to tech" announcements, both keyed off the same data.
// The prophet leg is the tech's own `firstFreeProphet` gated on GAMEOPTION_RELIGION_DIVINE_PROPHETS -- exactly what
// CvPlayer::getTechFreeProphet does (a pure info read + the option), so it moves without changing the resolution.
static void gr_applyTechFirstDiscover(int iTech, int iPlayer, int iFirstUnit, int iFirstProphet, int nFreeTechs)
{
	CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
	CvCity* pCapital = player.getCapitalCity();

	if (iFirstUnit >= 0 && pCapital != NULL) pCapital->createGreatPeople((UnitTypes)iFirstUnit, false, false);
	if (iFirstProphet >= 0 && pCapital != NULL && GC.getGame().isOption(GAMEOPTION_RELIGION_DIVINE_PROPHETS))
		pCapital->createGreatPeople((UnitTypes)iFirstProphet, false, false);

	if (nFreeTechs > 0)
	{
		if (player.isHuman())
			player.chooseTech(nFreeTechs, gDLL->getText("TXT_KEY_MISC_FIRST_TECH_CHOOSE_FREE",
				GC.getTechInfo((TechTypes)iTech).getTextKeyWide()));
		else for (int i = 0; i < nFreeTechs; ++i) player.AI_chooseFreeTech();
	}
}

static void gr_resolveTech(int iTech, int iPlayer)
{
	const CvInfo* j = InfoRepo<CvTechInfo>::get().get(iTech);
	if (j == NULL) return;
	const int iFirstUnit    = gr_firstId(j, gr_keyFirstFreeUnit);     // first-discover free unit id (-1 none)
	const int iFirstProphet = gr_firstId(j, gr_keyFirstFreeProphet);  // first-discover free prophet id (option-gated)
	const int nFreeTechs    = gr_pulse(j, gr_keyFreeTechs);           // first-discover free tech picks (count)
	if (iFirstUnit < 0 && iFirstProphet < 0 && nFreeTechs == 0) return;
	const bool bApplied = !s_bSuppressed && iPlayer >= 0;
	if (bApplied) gr_applyTechFirstDiscover(iTech, iPlayer, iFirstUnit, iFirstProphet, nFreeTechs);
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_TECH, 1)
		.addI(GF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(GF_APPLIED, bApplied ? 1 : 0)
		.addI(GF_PLAYER, iPlayer).addI(GF_TECH, iTech)
		.addI(GF_FIRSTUNIT, iFirstUnit).addI(GF_FIRSTPROPHET, iFirstProphet).addI(GF_FREETECHS, nFreeTechs));
}

// The RELIGION FOUNDER provisions. The two religions are DIFFERENT on purpose (CvPlayer::foundReligion): the SLOT
// being claimed sets the COUNT, the religion the player CHOSE sets the unit TYPE. Legacy's apply is deleted.
// (Under GAMEOPTION_RELIGION_DIVINE_PROPHETS foundReligion never runs -- founding is an OUTCOME there, a separate
// system -- so this path simply does not fire, by design.)
static void gr_resolveReligion(int iReligion, int iSlotReligion, int iPlayer, int iCity, bool bAward)
{
	const CvInfo* jChosen = InfoRepo<CvReligionInfo>::get().get(iReligion);
	const CvInfo* jSlot   = InfoRepo<CvReligionInfo>::get().get(iSlotReligion);
	if (jChosen == NULL || jSlot == NULL) return;
	const int nNumFree  = gr_pulse(jSlot, gr_keyNumFreeUnits);   // count of founder units -- from the SLOT
	const int iFreeUnit = gr_firstId(jChosen, gr_keyFreeUnit);   // the founder unit type -- from the CHOSEN religion
	if (nNumFree == 0 && iFreeUnit < 0) return;

	const bool bApplied = !s_bSuppressed && bAward && iFreeUnit >= 0 && nNumFree > 0 && iPlayer >= 0;
	if (bApplied)
	{
		CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
		CvCity* pCity = player.getCity(iCity);
		if (pCity != NULL)
		{
			for (int i = 0; i < nNumFree; ++i)
				player.initUnit((UnitTypes)iFreeUnit, pCity->getX(), pCity->getY(), NO_UNITAI, NO_DIRECTION,
					GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
		}
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_RELIGION, 1)
		.addI(GF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(GF_APPLIED, bApplied ? 1 : 0)
		.addI(GF_PLAYER, iPlayer).addI(GF_RELIGION, iReligion).addI(GF_CITY, iCity)
		.addI(GF_NUMFREEUNITS, nNumFree).addI(GF_FREEUNIT, iFreeUnit));
}

static void gr_resolveCivic(int iCivic, int iPlayer)
{
	const CvInfo* j = InfoRepo<CvCivicInfo>::get().get(iCivic);
	if (j == NULL) return;
	const int nRev = gr_pulse(j, gr_keyRevolution);   // rev-index pulse on adopt (signed; Python-applied in legacy)
	if (nRev == 0) return;
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_CIVIC, 1)
		.addI(GF_SUPPRESSED, s_bSuppressed ? 1 : 0)
		.addI(GF_PLAYER, iPlayer).addI(GF_CIVIC, iCivic).addI(GF_REVOLUTION, nRev));
}

// Game start: resolve the player's game-start grants off its civilization (civics/techs/buildings), era + handicap
// (startingGold). The apply is spread across legacy init points; the cascade resolves the whole set at ONE trigger.
static void gr_resolvePlayerInit(int iPlayer)
{
	const CvPlayer& p = GET_PLAYER((PlayerTypes)iPlayer);
	const CvInfo* jc = InfoRepo<CvCivilizationInfo>::get().get(p.getCivilizationType());
	// ⛔ The era-sourced game-start grants key on the game's START ERA, never the player's CURRENT era. Every
	// legacy site reads GC.getGame().getStartEra() (CvPlayer::initFreeState:1802, initFreeUnits:1853/1885,
	// CvCity::init:352). m_eCurrentEra starts at 0 and only advances through CvTeam::setHasTech, whose game-start
	// tech loop grants techs of era < startEra -- so at this trigger getCurrentEra() is startEra-1 at best, and 0
	// for a start era with no prior-era techs. Reading it resolves the wrong era's gold/units for every player.
	const CvInfo* je = InfoRepo<CvEraInfo>::get().get((int)GC.getGame().getStartEra());
	const CvInfo* jh = InfoRepo<CvHandicapInfo>::get().get(p.getHandicapType());
	const int nCivics = (jc != NULL) ? gr_listCount(jc, gr_keyCivics)    : 0;
	const int nTechs  = (jc != NULL) ? gr_listCount(jc, gr_keyTechs)     : 0;
	const int nBuild  = (jc != NULL) ? gr_listCount(jc, gr_keyBuildings) : 0;
	const int nGold   = ((je != NULL) ? gr_pulse(je, gr_keyStartingGold) : 0) + ((jh != NULL) ? gr_pulse(jh, gr_keyStartingGold) : 0);
	if (nCivics == 0 && nTechs == 0 && nBuild == 0 && nGold == 0) return;

	// STARTING GOLD is the machine's: (handicap + era startingGold) x gamespeed, replacing CvPlayer::initFreeState's
	// apply. The gamespeed scaling is engine pacing, not data ([mission-outcome-system.md]: Adapt* is pure engine),
	// so it stays here rather than being baked into the grant.
	const bool bApplied = !s_bSuppressed && nGold > 0;
	if (bApplied)
	{
		CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
		player.setGold(0);
		player.changeGold(nGold * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent() / 100);
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_GAMESTART, 1)
		.addI(GF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(GF_APPLIED, bApplied ? 1 : 0)
		.addI(GF_PLAYER, iPlayer).addI(GF_CIVICS, nCivics).addI(GF_TECHS, nTechs)
		.addI(GF_BUILDINGS, nBuild).addI(GF_STARTINGGOLD, nGold));
}

// ===================== THE LOAD-BRACKET SUPPRESSION (event-spine.md; DEC-spine-reseed) =====================
// A grant is the RESULT of a genuine in-play acquisition, and a LOAD IS NOT ONE: the save read replays every
// present fact as a DOMAIN event, so an applying machine would re-grant a whole empire's history on every load.
// The suppression is therefore MANDATORY before the apply lands.
//
// It suppresses the APPLY ONLY -- never the resolution. The machine still resolves each grant during the load and
// records it here, so "the machine saw these and withheld them" is DISTINGUISHABLE from "the machine saw nothing"
// (a bare early-return makes those two identical, which is how a resolution that reads 0 hides as normal quiet --
// exactly the failure that let the game-start resolver no-op unnoticed). Read via /computed/grants: no log file,
// no gate ([DEC-obs-hook-shapes] hook 3).
// ===================== increment 5: the PER-TURN provisions APPLY =====================
//
// The machine's per-turn work arrives on the PLAYER-scoped SEVT_TURN_STARTED. The machine is an IEventConsumer and
// the spine is its ONLY way in: a bespoke CvCity::doTurn entry point would give ONE machine TWO front doors -- the
// scattered-endpoint disease the machine exists to cure ("the eventSpine is the ONLY place any 'happening' lives",
// observability.md). It REPLACES the legacy per-turn sites outright -- CvCity::doPropertyUnitSpawn and CvCity::doHeal
// are DELETED with the serialized ledgers that fed them (m_aPropertySpawns / m_iNumUnitFullHeal: pure sums over the
// city's buildings, so DERIVED -- soft-removed via Assets/savemigration.txt, save.md §3, NOT a save break).
//
// It reads the COMPOSED getTriggers() entries (json.md §5 trigger -> chance -> action), never any legacy collapse
// member. Gated on the enabler's operating-building set: a DORMANT building grants nothing.
//
// A granted unit is an ORDINARY unit (owner ruling): placed through initUnit exactly as a trained one, so it fires
// the ordinary DOMAIN events and nothing downstream can tell the difference -- only the production debit is skipped.

// The full-heal provision -- heal up to iCount damaged own-team units on the city plot, chosen at random.
// Mirrors the legacy CvCity::doHeal body verbatim (DEC-mirror-then-redesign), including its RNG draw.
static int gr_applyFullHeal(CvCity* pCity, int iCount)
{
	UnitVector damagedUnits;
	algo::push_back(damagedUnits,
		pCity->plot()->units() | filtered(CvUnit::fn::getTeam() == pCity->getTeam() && CvUnit::fn::getDamage() > 0));
	algo::random_shuffle(damagedUnits, CvGame::SorenRand("Unit Full Heals"));
	const int iMax = std::min<int>(iCount, (int)damagedUnits.size());
	int iHealed = 0;
	foreach_(CvUnit* pUnit, damagedUnits | sliced(0, iMax))
	{
		pUnit->setDamage(0, pCity->getOwner(), false);
		++iHealed;
	}
	return iHealed;
}

// The property-scaled unit spawn -- mirrors the legacy CvCity::doPropertyUnitSpawn odds EXACTLY: the city's current
// property value, halved, is the per-10000 chance; PROPERTY index 0 (crime) additionally backs off by the plot's
// criminal count and bails once criminals reach half the population. A NEGATIVE-weight property spawns for the
// BARBARIAN player (the crime/disease case), a positive one for the city owner.
static int gr_applySpawn(CvCity* pCity, int iChancePerProperty, int iSpawnUnit)
{
	const PropertyTypes eProperty = (PropertyTypes)iChancePerProperty;
	if (eProperty < 0 || eProperty >= GC.getNumPropertyInfos()) return -1;

	int iCurrentValue = std::max(0, pCity->getPropertiesConst()->getValueByProperty(eProperty));
	if (eProperty == 0)
	{
		const int iNumCriminals = pCity->plot()->getNumCriminals();
		if (iNumCriminals >= pCity->getPopulation() / 2) return -1;
		iCurrentValue -= iNumCriminals * iCurrentValue / 10;
	}
	const bool bPositiveProperty = GC.getPropertyInfo(eProperty).getAIWeight() >= 0;
	iCurrentValue = std::max(0, iCurrentValue) / 2;

	if (GC.getGame().getSorenRandNum(10000, "Property Unit Spawn Check") >= iCurrentValue) return -1;

	const UnitTypes eUnit = (UnitTypes)iSpawnUnit;
	if (!GET_PLAYER(pCity->getOwner()).canTrain(eUnit, false, false, true, true)) return -1;

	const PlayerTypes eSpawnOwner = bPositiveProperty ? pCity->getOwner() : (PlayerTypes)BARBARIAN_PLAYER;
	CvUnit* pUnit = GET_PLAYER(eSpawnOwner).initUnit(eUnit, pCity->getX(), pCity->getY(), UNITAI_BARB_CRIMINAL,
		NO_DIRECTION, GC.getGame().getSorenRandNum(10000, "AI Unit Birthmark"));
	if (pUnit == NULL) return -1;

	if (pUnit->isExcile()) pUnit->jumpToNearestValidPlot(false);
	pUnit->finishMoves();
	pCity->addProductionExperience(pUnit);

	if (!GET_PLAYER(pCity->getOwner()).isModderOption(MODDEROPTION_IGNORE_DISABLED_ALERTS))
	{
		AddDLLMessage(
			pCity->getOwner(), false, GC.getEVENT_MESSAGE_TIME(),
			gDLL->getText(bPositiveProperty ? "TXT_KEY_CITY_PROPERTY_SPAWN_FRIENDLY" : "TXT_KEY_CITY_PROPERTY_SPAWN_BARB",
				GC.getUnitInfo(eUnit).getDescription(), pCity->getNameKey()),
			NULL, MESSAGE_TYPE_MINOR_EVENT, GC.getUnitInfo(eUnit).getButton(),
			bPositiveProperty ? GC.getCOLOR_HIGHLIGHT_TEXT() : GC.getCOLOR_WARNING_TEXT(),
			pCity->getX(), pCity->getY(), true, true
		);
	}
	return (int)eUnit;
}

static void gr_applyCityPerTurn(CvCity* pCity)
{
	const CvInfo* pAny = NULL;
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	// the contexts ARE the eval state (contexts.md): city/plot + player/team through the fill seams, never a
	// hand-assembled raw-pointer ctx beside them
	CvCascadeEvalCtx ec;
	pCity->getCityContext().fillEvalCtx(ec);
	player.getEmpireContext().fillEvalCtx(ec);
	EnablerKernel::wireOperatingBuildings(pCity, ec);
	const CvCascadeEvalFlags kFlags;

	const OperatingBuildings& ob = EnablerKernel::operatingBuildings(pCity);
	const int iTurn = GC.getGame().getGameTurn();
	int iFullHeal = 0, iSpawned = -1, iSpawnCount = 0;

	for (std::set<int>::const_iterator it = ob.active.begin(); it != ob.active.end(); ++it)
	{
		pAny = InfoRepo<CvBuildingInfo>::get().get(*it);
		if (pAny == NULL || pAny->getTriggers() == NULL) continue;
		const std::vector<CvTriggerEntry*>& entries = pAny->getTriggers()->entries();
		for (size_t i = 0; i < entries.size(); ++i)
		{
			const CvTriggerEntry* pEntry = entries[i];
			// This is the onTurn plane only -- the onTurnEnd promote entries ride the targeted-propagation
			// free-promotion path (SEVT_BUILDING_PROCESSED / SEVT_UNIT_ENTERED_CITY), never a per-turn rescan.
			if (pEntry->happening != "onTurn") continue;
			// RECURRENCE (json §3.8 / §5): "onTurn" = every turn; {"onTurn": N} = every Nth.
			if (pEntry->happeningInterval > 1 && (iTurn % pEntry->happeningInterval) != 0) continue;
			// The per-entry state condition, through the ONE evaluator (DEC-single-implementation).
			if (pEntry->condition != NULL && !cascadeEvalCondition(pEntry->condition, ec, kFlags)) continue;

			if (pEntry->healFull) { iFullHeal += (pEntry->healCount > 0 ? pEntry->healCount : 1); continue; }
			if (pEntry->healUnitCombatId >= 0) continue;   // the heal-RATE term -- the modifier plane's, not a grant
			if (pEntry->grant != NULL)
			{
				const std::vector<int>* pSpawnUnits = pEntry->grant->list(gr_keyUnits);
				if (pSpawnUnits != NULL && !pSpawnUnits->empty())
				{
					const int iUnit = gr_applySpawn(pCity, pEntry->chancePerTypeId, (*pSpawnUnits)[0]);
					if (iUnit >= 0) { iSpawned = iUnit; ++iSpawnCount; }
				}
			}
		}

	}

	const int iHealed = (iFullHeal > 0) ? gr_applyFullHeal(pCity, iFullHeal) : 0;
	if (iSpawnCount > 0 || iHealed > 0)
	{
		// The SPAWNED field is added ONLY when a unit actually landed: a sentinel -1 through the SFT_UNIT index
		// formatter renders "spawned=?", which cannot be told apart from "a spawn was attempted and failed" --
		// an ambiguous line defeats the point of a surface you reconstruct state from.
		CvSpineEvent ev(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_REPEAT, 1);
		ev.addI(GF_SUPPRESSED, s_bSuppressed ? 1 : 0)
		  .addI(GF_PLAYER, pCity->getOwner()).addI(GF_CITY, pCity->getID())
		  .addI(GF_HEALED, iHealed);
		if (iSpawnCount > 0) { ev.addI(GF_SPAWNED, iSpawned); }
		eventSpine().emit(ev);
	}
}

// The player's per-turn provisions. Suppressed inside the load bracket like every other apply: a grant is the
// RESULT of a genuine in-play acquisition, and a load is not one ([DEC-spine-reseed]).
static void gr_applyPerTurn(int iPlayer)
{
	if (s_bSuppressed) return;
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS) return;
	CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
	if (!player.isAlive()) return;
	foreach_(CvCity* pLoopCity, player.cities())
	{
		gr_applyCityPerTurn(pLoopCity);
	}
}

// A CITY WAS FOUNDED -- the settle-time provisions. Today this hands over the FOUNDER's `grants.buildings`
// (json §5: the settler's considered action IS founding): a settler seeding buildings into the city it founds.
// That is a NEW mechanic coined for this rework, so there is no legacy apply to mirror -- the data has been
// authored and inert, waiting for a trigger.
// ⛔ The other settle-time provisions (start-era freePopulation, civilization buildings, FreeStartEra, the trait
// settle keys, barbarianInitialDefenders) still apply in CvPlayer::found: several are not authored in a `grants`
// block at all, so the machine cannot resolve them off getGrants() until the curator emits them
// (grant-apply-sites.md §5.4). The TRIGGER now exists; the DATA is the remaining blocker.
static void gr_resolveCityFounded(int iOwner, int iCity, int iFounderType)
{
	if (iOwner < 0 || iFounderType < 0) return;
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(iFounderType);
	if (j == NULL || j->getGrants() == NULL) return;
	const std::vector<int>* pSeeds = j->getGrants()->list(gr_keyBuildings);
	if (pSeeds == NULL || pSeeds->empty()) return;

	CvPlayer& player = GET_PLAYER((PlayerTypes)iOwner);
	CvCity* pCity = player.getCity(iCity);
	int nPlaced = 0;
	if (!s_bSuppressed && pCity != NULL)
	{
		// the contexts ARE the eval state (contexts.md): the fill seams, never a hand-assembled raw ctx
		CvCascadeEvalCtx ec;
		pCity->getCityContext().fillEvalCtx(ec);
		player.getEmpireContext().fillEvalCtx(ec);
		const CvCascadeEvalFlags kFlags;
		for (size_t i = 0; i < pSeeds->size(); ++i)
		{
			const int iBuilding = (*pSeeds)[i];
			if (iBuilding < 0) continue;
			// the entry's own `enabled` condition (the §3.9 conditioned object form), index-parallel to the ids
			const CvCondition* pEnabled = j->getGrants()->listCond(gr_keyBuildings, i);
			if (pEnabled != NULL && !cascadeEvalCondition(pEnabled, ec, kFlags)) continue;
			if (pCity->hasBuilding((BuildingTypes)iBuilding)) continue;
			pCity->changeHasBuilding((BuildingTypes)iBuilding, true);
			++nPlaced;
		}
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_FOUND, 1)
		.addI(GF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(GF_APPLIED, nPlaced)
		.addI(GF_PLAYER, iOwner).addI(GF_CITY, iCity).addI(GF_UNIT, iFounderType)
		.addI(GF_GRANTBUILDINGS, (int)pSeeds->size()));
}

// THE CAPITAL RELOCATED -- re-seed the palace into the new capital. The palace is what MAKES a city the capital
// (setupBuilding's isCapital branch calls setCapitalCity), so without this a captured capital never relocates:
// the settler's `grants.buildings` covers FOUNDING only, and the civilization building list that used to carry the palace on
// relocation no longer does. Same gate as the founding case -- the building on its OWN absence -- so an empire
// that still holds a palace somewhere gets nothing.
static void gr_resolveCapitalChanged(int iOwner, int iCity)
{
	if (iOwner < 0 || iCity < 0) return;   // -1 city = no city left to be a capital
	CvPlayer& player = GET_PLAYER((PlayerTypes)iOwner);
	CvCity* pCity = player.getCity(iCity);
	if (pCity == NULL) return;

	// The capital building is the one flagged isCapital -- the same identity the engine keys setCapitalCity on.
	static int s_iCapitalBuilding = -2;
	if (s_iCapitalBuilding == -2)
	{
		s_iCapitalBuilding = -1;
		for (int i = 0; i < GC.getNumBuildingInfos(); ++i)
		{
			if (GC.getBuildingInfo((BuildingTypes)i).isCapital()) { s_iCapitalBuilding = i; break; }
		}
	}
	if (s_iCapitalBuilding < 0) return;

	const int nHave = player.getBuildingCount((BuildingTypes)s_iCapitalBuilding);
	const bool bApplied = !s_bSuppressed && nHave == 0 && !pCity->hasBuilding((BuildingTypes)s_iCapitalBuilding);
	if (bApplied) pCity->changeHasBuilding((BuildingTypes)s_iCapitalBuilding, true);

	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_FOUND, 1)
		.addI(GF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(GF_APPLIED, bApplied ? 1 : 0)
		.addI(GF_PLAYER, iOwner).addI(GF_CITY, iCity).addI(GF_BUILDING, s_iCapitalBuilding));
}

void CvCascadeGrants::onEvent(const CvSpineEvent& e)
{
	if (e.eKind != EVENTKIND_DOMAIN) return;
	// ⛔ The machine's OPERATION is not gated on a log level. It used to return early below `gPlayerLogLevel < 1`,
	// which was harmless while it only observed -- but the instant it PLACES, that gate would mean grants fire only
	// when logging happens to be on. The gate belongs on the DIAGNOSTIC emit inside each resolver, never here.
	s_bSuppressed = spineGameLoadInProgress();
	switch (e.iEventId)
	{
	// BUILDING grants ride SEVT_BUILDING_CHANGED, not the player COUNT event. Three reasons, all load-bearing:
	// the count event is emitted only from CvPlayer::changeBuildingCount and so NEVER fires during the save read --
	// the machine was blind to every building grant on load (0 lines, indistinguishable from "no grants"); CHANGED
	// fires on the genuine presence flip in play AND per building in the reseed read loop (CvCity.cpp:13501 /
	// :16752), which is exactly one resolve in both worlds; and it is NOT the dormancy signal (that is
	// SEVT_BUILDING_PROCESSED, which processBuilding also fires on every disable/enable flip -- listening there
	// would re-grant a building each time it woke up). iB = delta, iC = owner, iSrcLoc = city.
	case SEVT_BUILDING_CHANGED:
		if (e.iB > 0)
		{
			// iA = bFirst. A conquest transfer / load restore resolves (so it is visible) but is WITHHELD --
			// the engine grants nothing in that case either (CvCity::setupBuilding's bFirst gate).
			s_bFirstAcquire = (e.iA != 0);
			if (!s_bFirstAcquire) s_bSuppressed = true;
			gr_resolveBuilding(e.iType, e.iC, e.iSrcLoc);   // iSrcLoc = the city the building landed in
		}
		break;
	// The per-TYPE tally carries no instance, so it cannot apply -- the instance-aware SEVT_UNIT_CREATED does.
	case SEVT_UNIT_CREATED:   gr_resolveUnit(e.iType, e.iC, e.iA); break;   // iA = the created unit's id
	// iType = founding unit's type, iC = owner, iSrcLoc = the new city
	case SEVT_CITY_FOUNDED:   gr_resolveCityFounded(e.iC, e.iSrcLoc, e.iType); break;
	// iC = owner, iSrcLoc = the new capital (-1 = none left)
	case SEVT_CAPITAL_CHANGED: gr_resolveCapitalChanged(e.iC, e.iSrcLoc); break;
	case SEVT_TECH_ACQUIRED:    gr_resolveTech(e.iType, e.iC);       break;  // first-discover only (iC = discoverer)
	// iType = chosen religion, iA = slot religion, iB = bAward, iC = founding player, iSrcLoc = holy city
	case SEVT_RELIGION_FOUNDED: gr_resolveReligion(e.iType, e.iA, e.iC, e.iSrcLoc, e.iB != 0); break;
	case SEVT_CIVIC_ADOPTED:    gr_resolveCivic(e.iType, e.iC);      break;  // iC = adopting player
	case SEVT_PLAYER_INIT:      gr_resolvePlayerInit(e.iC);          break;  // iC = player (game start)
	// The per-turn provisions (increment 5). PLAYER-scoped only -- the GAME-scope boundary carries iC = -1 and is
	// the perf/observability fact, not a grant trigger. Legacy ran the per-turn spawn inside CvCity::doTurn within
	// CvPlayer::doTurn, so the player boundary is the ordering-faithful grain.
	case SEVT_TURN_STARTED:     if (e.iC >= 0) { gr_applyPerTurn(e.iC); } break;
	// Trigger (2) for free promotions: a building went ACTIVE in a city -- a fresh build OR a step out of
	// dormancy (processBuilding fires on both). Hand its promotions to everyone already standing there.
	// iType = building, iC = owner, iSrcLoc = city, iB = +1 in / -1 out.
	case SEVT_BUILDING_PROCESSED:
		if (!s_bSuppressed && e.iB > 0 && e.iC >= 0)
		{
			CvCity* pC = GET_PLAYER((PlayerTypes)e.iC).getCity(e.iSrcLoc);
			const int n = (pC != NULL) ? gr_promoteCityUnits(pC, GC.getBuildingInfo((BuildingTypes)e.iType)) : 0;
			if (n > 0)
			{
				eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_REPEAT, 1)
					.addI(GF_SUPPRESSED, 0).addI(GF_PLAYER, e.iC).addI(GF_CITY, e.iSrcLoc)
					.addI(GF_BUILDING, e.iType).addI(GF_FREEPROMOS, n));
			}
		}
		break;
	// Trigger (1) for free promotions: a unit entered a friendly city. iA = unit id, iC = owner, iSrcLoc = city.
	case SEVT_UNIT_ENTERED_CITY:
		if (!s_bSuppressed && e.iC >= 0)
		{
			CvPlayer& p = GET_PLAYER((PlayerTypes)e.iC);
			CvCity* pC = p.getCity(e.iSrcLoc);
			CvUnit* pU = p.getUnit(e.iA);
			const int n = gr_promoteOneUnit(pC, pU);
			if (n > 0)
			{
				eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_REPEAT, 1)
					.addI(GF_SUPPRESSED, 0).addI(GF_PLAYER, e.iC).addI(GF_CITY, e.iSrcLoc)
					.addI(GF_UNIT, e.iType).addI(GF_FREEPROMOS, n));
			}
		}
		break;
	}
}

static CvCascadeGrants s_cascadeGrants;

void cascadeRegisterGrants()
{
	gr_registerDomain();
	eventSpine().registerConsumer(&s_cascadeGrants);
}
