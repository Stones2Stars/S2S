//
//	CvTriggerEngine -- the #430 GRANTS machine consumer + the [GRANTS] spine domain. See the header + grants-machine.md.
//	Slice-1: on a building-built / unit-created DOMAIN event, resolve the source entity's GENUINE grants off its mapped
//	CvJson<X>Info (the composed CvGrants unit in the InfoRepo, minus the deferred mission-keys) and emit a [GRANTS]
//	diagnostic. Resolution only -- it does NOT apply (legacy applies); un-run parity (owner: no live parity until everything is in).
//

#include "CvGameCoreDLL.h"          // PCH umbrella
#include "Infos/CvClassificationIds.h"   // the generated classification id table
#include "Triggers/CvTriggerEngine.h"
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
enum TrEvt { TRE_BUILDING = 1, TRE_UNIT, TRE_TECH, TRE_RELIGION, TRE_CIVIC, TRE_GAMESTART, TRE_REPEAT, TRE_FOUND, TRE_ERA };
enum GrFld
{
	TF_PLAYER = 1, TF_BUILDING, TF_UNIT, TF_TECH, TF_RELIGION, TF_CIVIC,
	TF_PROMOTIONS, TF_GRANTBUILDINGS,                        // unit genuine grants (promotions + settle-time buildings)
	TF_TRIGGERENTRIES, TF_FREEPROMOS, TF_FREETECHS,          // building triggers + genuine grants
	TF_GOLDENAGE, TF_POPULATION,                             // building flag + scoped-pulse grants (increment 2)
	TF_FIRSTUNIT, TF_FIRSTPROPHET,                           // tech first-discover grants (increment 3a)
	TF_NUMFREEUNITS, TF_FREEUNIT, TF_REVOLUTION,             // religion + civic grants (increment 3b)
	TF_CIVICS, TF_TECHS, TF_BUILDINGS, TF_STARTINGGOLD,      // game-start civ + era/handicap grants (increment 3c)
	TF_SUPPRESSED,                                           // 1 = resolved but WITHHELD
	TF_FIRSTACQUIRE,                                         // buildings: 1 = genuine first build, 0 = conquest/restore
	TF_CITY, TF_SPAWNED, TF_HEALED,                          // the per-turn apply (increment 5): what actually LANDED
	TF_APPLIED,                                              // 1 = the machine ran the FIRST-BUILD apply (NOT a claim about every grant on the line)
	TF_MATMISMATCH,                                          // 1 = a mapFrom-materialized getter disagrees with its composed grants read
	TF_ERA, TF_SPECIALISTS                                   // the era-advance apply: which era, and how many specialists LANDED
};
static const char* tr_prefix(int evt)
{
	switch (evt)
	{
	case TRE_BUILDING: return "[TRIGGERS/building]";
	case TRE_UNIT:     return "[TRIGGERS/unit]";
	case TRE_TECH:     return "[TRIGGERS/tech]";
	case TRE_RELIGION: return "[TRIGGERS/religion]";
	case TRE_CIVIC:    return "[TRIGGERS/civic]";
	case TRE_GAMESTART: return "[TRIGGERS/gameStart]";
	case TRE_REPEAT:   return "[TRIGGERS/repeat]";
	case TRE_FOUND:    return "[TRIGGERS/cityFounded]";
	case TRE_ERA:      return "[TRIGGERS/era]";
	default:           return "[GRANTS]";
	}
}
static const char* tr_field(int tag, SpineFieldType* peType)
{
	*peType = SFT_INT;
	switch (tag)
	{
	case TF_PLAYER:         *peType = SFT_PLAYER;   return "player";
	case TF_BUILDING:       *peType = SFT_BUILDING; return "building";
	case TF_UNIT:           *peType = SFT_UNIT;     return "unit";
	case TF_TECH:           *peType = SFT_TECH;     return "tech";
	case TF_RELIGION:       *peType = SFT_RELIGION; return "religion";
	case TF_CIVIC:          *peType = SFT_CIVIC;    return "civic";
	case TF_FIRSTUNIT:      *peType = SFT_UNIT;     return "firstFreeUnit";
	case TF_FIRSTPROPHET:   *peType = SFT_UNIT;     return "firstFreeProphet";
	case TF_FREEUNIT:       *peType = SFT_UNIT;     return "freeUnit";
	case TF_NUMFREEUNITS:   return "numFreeUnits";
	case TF_REVOLUTION:     return "revolution";
	case TF_CIVICS:         return "civics";
	case TF_TECHS:          return "techs";
	case TF_BUILDINGS:      return "buildings";
	case TF_STARTINGGOLD:   return "startingGold";
	case TF_SUPPRESSED:     return "suppressed";
	case TF_FIRSTACQUIRE:   return "firstAcquire";
	case TF_PROMOTIONS:     return "promotions";
	case TF_GRANTBUILDINGS: return "grantBuildings";
	case TF_TRIGGERENTRIES: return "triggerEntries";
	case TF_FREEPROMOS:     return "freePromotions";
	case TF_FREETECHS:      return "freeTechs";
	case TF_GOLDENAGE:      return "goldenAge";
	case TF_POPULATION:     return "population";
	case TF_CITY:           *peType = SFT_INT;      return "city";
	case TF_SPAWNED:        *peType = SFT_UNIT;     return "spawned";
	case TF_HEALED:         return "healed";
	case TF_APPLIED:        return "appliedFirstBuild";
	case TF_MATMISMATCH:    return "matMismatch";
	case TF_ERA:            return "era";
	case TF_SPECIALISTS:    return "specialists";
	default:                return NULL;
	}
}
static void tr_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_TRIGGERS, tr_prefix, "Cascade.log", tr_field); s_reg = true; }
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
static const int tr_keyPromotions       = CvGrants::key("promotions");
static const int tr_keyBuildings        = CvGrants::key("buildings");
static const int tr_keyUnits            = CvGrants::key("units");
static const int tr_keyTechs            = CvGrants::key("techs");
static const int tr_keyCivics           = CvGrants::key("civics");
static const int tr_keyFreeTechs        = CvGrants::key("freeTechs");
static const int tr_keyGoldenAge        = CvGrants::key("goldenAge");
static const int tr_keyPopulation       = CvGrants::key("population");
static const int tr_keyScopeCity        = CvGrants::key("city");
static const int tr_keyScopeEmpire      = CvGrants::key("empire");
static const int tr_keySpecialists      = CvGrants::key("specialists");
static const int tr_keyFirstFreeUnit    = CvGrants::key("firstFreeUnit");
static const int tr_keyFirstFreeProphet = CvGrants::key("firstFreeProphet");
static const int tr_keyNumFreeUnits     = CvGrants::key("numFreeUnits");
static const int tr_keyFreeUnit         = CvGrants::key("freeUnit");
static const int tr_keyRevolution       = CvGrants::key("revolution");
static const int tr_keyStartingGold     = CvGrants::key("startingGold");

// ===================== resolution off the mapped CvInfo =====================
// A grantList bucket's id-count (0 if absent). GENUINE buckets only -- the deferred mission-keys (unit `buildings`/
// `greatPeople`/`greatPersonAction`/`goldenAge`) are simply not read here (they migrate in the missions pass).
static int tr_listCount(const CvInfo* j, int iBucketKey)
{
	const std::vector<int>* l = j->grantList(iBucketKey);
	return (l != NULL) ? (int)l->size() : 0;
}
static int tr_pulse(const CvInfo* j, int iChannelKey)   // pulses are stored ×100 -> /100 to the human count/amount
{
	return j->grantPulse(iChannelKey) / 100;
}
static int tr_flag(const CvInfo* j, int iFlagKey)   // a bool grant present? (goldenAge)
{
	return j->grantFlag(iFlagKey) ? 1 : 0;
}
static int tr_scopedPulseSum(const CvInfo* j, int iChannelKey)   // sum a scoped pulse over its scopes (×100 -> /100)
{
	const CvGrants* g = j->consideredGrants();
	return g ? g->scopedPulseSumAllScopes(iChannelKey) / 100 : 0;
}
static int tr_promoteEntryCount(const CvInfo* j)   // `triggers` promote entries (the end-turn free-promotion plane)
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
// It also closes a live defect: a unit that WALKED into a city never gained its promotions at all -- only units
// trained there did -- because the path that should have handled it was gated on a flag hardcoded to false.
// Apply ONE source's `onTurnEnd` promote entries to ONE unit -- the whole free-promotion plane, read off the
// TRIGGER entries the data actually authors (264 of them) -- the ONE place the payload lives, so every route
// (trained here, walked in, a building completing) lands through this single applier.
//
// Two gates survive the move, for different reasons. `canAcquirePromotion(Promote|ForFree)` is the PROMOTION
// SYSTEM's own validity rule and stays -- it is also why a granted promotion needs no take-away verb: when the
// promotion stops being valid that system drops it (owner). The legacy per-promotion BoolExpr becomes the entry's
// parsed `condition`, evaluated through the ONE evaluator ([DEC-single-implementation]) instead of a second tree.
static int tr_promoteFromEntries(CvCity* pCity, CvUnit* pUnit, const CvInfo* j)
{
	if (pCity == NULL || pUnit == NULL || j == NULL || j->getTriggers() == NULL) return 0;
	const std::vector<CvTriggerEntry*>& entries = j->getTriggers()->entries();
	if (entries.empty()) return 0;

	CvCascadeEvalCtx ec;
	pCity->getCityContext().fillEvalCtx(ec);
	GET_PLAYER(pCity->getOwner()).getEmpireContext().fillEvalCtx(ec);
	// The ENABLER's precomputed sets are the third leg of the eval state, fed in rather than re-derived
	// (patterns.md: the active/dormant verdict is the enabler's and the modifier READS it). Without this the
	// operating-set legs are EMPTY, so an entry condition asking an active-building or vicinity-provides
	// question evaluates against nothing and quietly answers false.
	EnablerKernel::wireOperatingBuildings(pCity, ec);
	ec.unit = pUnit;                                  // the entry's condition may ask about the unit being promoted
	CvCascadeEvalFlags kFlags;

	int n = 0;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const CvTriggerEntry* pEntry = entries[i];
		if (pEntry->happening != "onTurnEnd" || pEntry->promotePromotions.empty()) continue;
		if (pEntry->condition != NULL && !cascadeEvalCondition(pEntry->condition, ec, kFlags)) continue;
		for (size_t k = 0; k < pEntry->promotePromotions.size(); ++k)
		{
			const PromotionTypes ePromotion = (PromotionTypes)pEntry->promotePromotions[k];
			if (pUnit->isHasPromotion(ePromotion)) continue;
			if (!pUnit->canAcquirePromotion(ePromotion, PromotionRequirements::Promote | PromotionRequirements::ForFree)) continue;
			pUnit->setHasPromotion(ePromotion, true);
			++n;
		}
	}
	return n;
}

static int tr_promoteOneUnit(CvCity* pCity, CvUnit* pUnit)
{
	if (pCity == NULL || pUnit == NULL) return 0;
	if (pUnit->getTeam() != GET_PLAYER(pCity->getOwner()).getTeam()) return 0;
	const OperatingBuildings& ob = EnablerKernel::operatingBuildings(pCity);
	int n = 0;
	for (std::set<int>::const_iterator it = ob.active.begin(); it != ob.active.end(); ++it)
	{
		n += tr_promoteFromEntries(pCity, pUnit, InfoRepo<CvBuildingInfo>::get().get(*it));
	}
	return n;
}

static int tr_promoteCityUnits(CvCity* pCity, const CvInfo* j)
{
	if (pCity == NULL || j == NULL) return 0;
	const TeamTypes eTeam = GET_PLAYER(pCity->getOwner()).getTeam();
	int n = 0;
	foreach_(CvUnit* pLoopUnit, pCity->plot()->units())
	{
		if (pLoopUnit->getTeam() != eTeam) continue;
		n += tr_promoteFromEntries(pCity, pLoopUnit, j);
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
static void tr_applyBuildingFirstBuild(const CvInfo* j, int iBuilding, int iPlayer, int iCity)
{
	CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
	CvCity* pCity = player.getCity(iCity);
	if (pCity == NULL || j->consideredGrants() == NULL) return;


	// LOCAL population -- legacy applied this OUTSIDE the isFinalInitialized/WorldBuilder guard, so it does too.
	const int iPopCity = j->consideredGrants()->scopedPulse(tr_keyPopulation, tr_keyScopeCity) / 100;
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

	if (j->consideredGrants()->flag(tr_keyGoldenAge))
	{
		player.changeGoldenAgeTurns(1 + player.getGoldenAgeLength());
	}

	const int iPopEmpire = j->consideredGrants()->scopedPulse(tr_keyPopulation, tr_keyScopeEmpire) / 100;
	if (iPopEmpire > 0)
	{
		const CvBuildingInfo& kB = GC.getBuildingInfo((BuildingTypes)iBuilding);
		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			// isTeamShare spreads it across the TEAM; otherwise just the owner's own cities.
			if (!kB.hasAttribute(CLS_ATTRIBUTE_TEAM_SHARE) ? (iI != iPlayer) : !GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(pCity->getTeam())) continue;
			foreach_(CvCity* pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
			{
				for (int i = 0; i < iPopEmpire; ++i) pLoopCity->changeFood(pLoopCity->growthThreshold());
				pLoopCity->AI_updateAssignWork();
			}
		}
	}

	const int iFreeTechs = j->consideredGrants()->pulse(tr_keyFreeTechs) / 100;
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

static void tr_resolveBuilding(int iBuilding, int iPlayer, int iCity)
{
	const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(iBuilding);
	if (j == NULL) return;
	const int nRepeat    = (j->getTriggers() != NULL) ? (int)j->getTriggers()->entries().size() : 0;   // the `triggers` entries (per-turn spawn/heal/promote)
	const int nFreePromo = tr_promoteEntryCount(j);   // end-turn promotions to units in the city (triggers promote entries)
	const int nFreeTech  = tr_pulse(j, tr_keyFreeTechs);            // one-shot on first build
	const int nGoldenAge = tr_flag(j, tr_keyGoldenAge);             // one-shot golden age (bool grant, increment 2)
	const int nPop       = tr_scopedPulseSum(j, tr_keyPopulation);  // one-shot population boost (scoped pulse, increment 2)
	if (nRepeat == 0 && nFreePromo == 0 && nFreeTech == 0 && nGoldenAge == 0 && nPop == 0) return;
	// The two population scopes SEPARATELY (nPop is their sum) -- the apply and the tripwire need them apart.
	const int nPopCity   = (j->consideredGrants() != NULL) ? j->consideredGrants()->scopedPulse(tr_keyPopulation, tr_keyScopeCity)   / 100 : 0;
	const int nPopEmpire = (j->consideredGrants() != NULL) ? j->consideredGrants()->scopedPulse(tr_keyPopulation, tr_keyScopeEmpire) / 100 : 0;
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
	if (bApplied) tr_applyBuildingFirstBuild(j, iBuilding, iPlayer, iCity);

	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_BUILDING, 1)
		.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(TF_FIRSTACQUIRE, s_bFirstAcquire ? 1 : 0)
		.addI(TF_APPLIED, bApplied ? 1 : 0).addI(TF_MATMISMATCH, bMatMismatch ? 1 : 0)
		.addI(TF_PLAYER, iPlayer).addI(TF_BUILDING, iBuilding)
		.addI(TF_TRIGGERENTRIES, nRepeat).addI(TF_FREEPROMOS, nFreePromo).addI(TF_FREETECHS, nFreeTech)
		.addI(TF_GOLDENAGE, nGoldenAge).addI(TF_POPULATION, nPop));
}

// The unit's OWN `grants.promotions`, handed to the created INSTANCE. This is the ONLY leg of the legacy
// CvUnit::setFreePromotion that is a grant: the player free-promotion registry is written solely by
// CvPlayer::applyEvent (random events -- out of scope) and the trait-derived promotions are refcounted with the
// trait (a MODIFIER, alive-with-source). See grant-apply-sites.md §4.
static void tr_resolveUnit(int iUnit, int iPlayer, int iUnitId)
{
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(iUnit);
	if (j == NULL) return;
	const int nPromos = tr_listCount(j, tr_keyPromotions);   // free promotions on creation
	const int nFound  = tr_listCount(j, tr_keyBuildings);    // settle-time building seeds (grants.buildings on the settler)
	if (nPromos == 0 && nFound == 0) return;

	int nApplied = 0;
	if (!s_bSuppressed && iUnitId >= 0 && iPlayer >= 0 && nPromos > 0)
	{
		CvUnit* pUnit = GET_PLAYER((PlayerTypes)iPlayer).getUnit(iUnitId);
		const std::vector<int>* promos = (j->consideredGrants() != NULL) ? j->consideredGrants()->list(tr_keyPromotions) : NULL;
		if (pUnit != NULL && promos != NULL)
		{
			for (size_t i = 0; i < promos->size(); ++i)
			{
				const PromotionTypes eP = (PromotionTypes)(*promos)[i];
				if (!pUnit->isHasPromotion(eP)) { pUnit->setHasPromotion(eP, true, true); ++nApplied; }
			}
		}
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_UNIT, 1)
		.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(TF_APPLIED, nApplied)
		.addI(TF_PLAYER, iPlayer).addI(TF_UNIT, iUnit)
		.addI(TF_PROMOTIONS, nPromos).addI(TF_GRANTBUILDINGS, nFound));
}

static int tr_firstId(const CvInfo* j, int iBucketKey)   // a single-id grant bucket's id (-1 if absent)
{
	return (j->consideredGrants() != NULL) ? j->consideredGrants()->firstListId(iBucketKey) : -1;
}

// The TECH first-discoverer provisions. Mirrors the CvTeam::setHasTech first-discover block, whose apply legs are
// DELETED -- what stays there is the non-grant residue: the `bClearResearchQueueAI` rider (a free tech invalidates
// the AI's queued research) and the "first to tech" announcements, both keyed off the same data.
// The prophet leg is the tech's own `firstFreeProphet` gated on GAMEOPTION_RELIGION_DIVINE_PROPHETS -- exactly what
// CvPlayer::getTechFreeProphet does (a pure info read + the option), so it moves without changing the resolution.
static void tr_applyTechFirstDiscover(int iTech, int iPlayer, int iFirstUnit, int iFirstProphet, int nFreeTechs)
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

static void tr_resolveTech(int iTech, int iPlayer)
{
	const CvInfo* j = InfoRepo<CvTechInfo>::get().get(iTech);
	if (j == NULL) return;
	const int iFirstUnit    = tr_firstId(j, tr_keyFirstFreeUnit);     // first-discover free unit id (-1 none)
	const int iFirstProphet = tr_firstId(j, tr_keyFirstFreeProphet);  // first-discover free prophet id (option-gated)
	const int nFreeTechs    = tr_pulse(j, tr_keyFreeTechs);           // first-discover free tech picks (count)
	if (iFirstUnit < 0 && iFirstProphet < 0 && nFreeTechs == 0) return;
	const bool bApplied = !s_bSuppressed && iPlayer >= 0;
	if (bApplied) tr_applyTechFirstDiscover(iTech, iPlayer, iFirstUnit, iFirstProphet, nFreeTechs);
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_TECH, 1)
		.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(TF_APPLIED, bApplied ? 1 : 0)
		.addI(TF_PLAYER, iPlayer).addI(TF_TECH, iTech)
		.addI(TF_FIRSTUNIT, iFirstUnit).addI(TF_FIRSTPROPHET, iFirstProphet).addI(TF_FREETECHS, nFreeTechs));
}

// The RELIGION FOUNDER provisions. The two religions are DIFFERENT on purpose (CvPlayer::foundReligion): the SLOT
// being claimed sets the COUNT, the religion the player CHOSE sets the unit TYPE. Legacy's apply is deleted.
// (Under GAMEOPTION_RELIGION_DIVINE_PROPHETS foundReligion never runs -- founding is an OUTCOME there, a separate
// system -- so this path simply does not fire, by design.)
static void tr_resolveReligion(int iReligion, int iSlotReligion, int iPlayer, int iCity, bool bAward)
{
	const CvInfo* jChosen = InfoRepo<CvReligionInfo>::get().get(iReligion);
	const CvInfo* jSlot   = InfoRepo<CvReligionInfo>::get().get(iSlotReligion);
	if (jChosen == NULL || jSlot == NULL) return;
	const int nNumFree  = tr_pulse(jSlot, tr_keyNumFreeUnits);   // count of founder units -- from the SLOT
	const int iFreeUnit = tr_firstId(jChosen, tr_keyFreeUnit);   // the founder unit type -- from the CHOSEN religion
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
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_RELIGION, 1)
		.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(TF_APPLIED, bApplied ? 1 : 0)
		.addI(TF_PLAYER, iPlayer).addI(TF_RELIGION, iReligion).addI(TF_CITY, iCity)
		.addI(TF_NUMFREEUNITS, nNumFree).addI(TF_FREEUNIT, iFreeUnit));
}

static void tr_resolveCivic(int iCivic, int iPlayer)
{
	const CvInfo* j = InfoRepo<CvCivicInfo>::get().get(iCivic);
	if (j == NULL) return;
	const int nRev = tr_pulse(j, tr_keyRevolution);   // rev-index pulse on adopt (signed; Python-applied in legacy)
	if (nRev == 0) return;
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_CIVIC, 1)
		.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0)
		.addI(TF_PLAYER, iPlayer).addI(TF_CIVIC, iCivic).addI(TF_REVOLUTION, nRev));
}

// The player's ERA ADVANCED. Era advance is TECH-DRIVEN (owner): researching a tech whose era exceeds the
// player's raises it (CvTeam::setHasTech), so the era is a happening no single source owns -- which is exactly
// why a trait's era-advance specialist fires HERE and not on the trait's own considered action (json.md §5).
// ⚑ The grant is a persisted PULSE, the one carve-out on `grants.specialists`: it lands in the city's
// UNATTRIBUTED typed-free ledger, so it OUTLIVES the trait that paid for it. An alive-with-source specialist is
// the freeSpecialists MODIFIER family instead and never reaches this plane.
// ⚠ The trait walk is a PRESENCE read (the HAVE axis), not the banned own-data inversion: it asks which traits
// the player HOLDS, never asks every trait what it deposits. Era advance fires a handful of times per game.
static void tr_resolveEraChanged(int iPlayer)
{
	// A load RESTORES an era rather than advancing into one, so the reseed's emit must not hand out the pulse
	// again ([DEC-spine-reseed]: a grant is the RESULT of a genuine in-play acquisition).
	if (s_bSuppressed) return;
	CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
	int iGranted = 0;
	for (int iTrait = 0; iTrait < GC.getNumTraitInfos(); iTrait++)
	{
		if (!player.hasTrait((TraitTypes)iTrait)) continue;
		const CvInfo* j = InfoRepo<CvTraitInfo>::get().get(iTrait);
		if (j == NULL || j->getTriggers() == NULL) continue;
		const std::vector<CvTriggerEntry*>& entries = j->getTriggers()->entries();
		for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
		{
			const CvTriggerEntry* pEntry = entries[iEntry];
			if (pEntry->happening != "onEraChanged") continue;
			if (pEntry->grant == NULL) continue;
			const std::vector<int>* pSpecialists = pEntry->grant->list(tr_keySpecialists);
			if (pSpecialists == NULL || pSpecialists->empty()) continue;
			// Every city of the empire, matching the legacy apply's own fan.
			foreach_(CvCity* pCity, player.cities())
			{
				for (size_t iSpec = 0; iSpec < pSpecialists->size(); ++iSpec)
				{
					pCity->changeFreeSpecialistCount((SpecialistTypes)(*pSpecialists)[iSpec], 1, true);
					++iGranted;
				}
			}
		}
	}
	if (iGranted > 0)
	{
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_ERA, 1)
			.addI(TF_SUPPRESSED, 0).addI(TF_PLAYER, iPlayer)
			.addI(TF_ERA, (int)player.getCurrentEra()).addI(TF_SPECIALISTS, iGranted));
	}
}

// Game start: resolve the player's game-start grants off its civilization (civics/techs/buildings), era + handicap
// (startingGold). The apply is spread across legacy init points; the cascade resolves the whole set at ONE trigger.
static void tr_resolvePlayerInit(int iPlayer)
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
	const int nCivics = (jc != NULL) ? tr_listCount(jc, tr_keyCivics)    : 0;
	const int nTechs  = (jc != NULL) ? tr_listCount(jc, tr_keyTechs)     : 0;
	const int nBuild  = (jc != NULL) ? tr_listCount(jc, tr_keyBuildings) : 0;
	const int nGold   = ((je != NULL) ? tr_pulse(je, tr_keyStartingGold) : 0) + ((jh != NULL) ? tr_pulse(jh, tr_keyStartingGold) : 0);
	if (nCivics == 0 && nTechs == 0 && nBuild == 0 && nGold == 0) return;

	// STARTING GOLD is the machine's: (handicap + era startingGold) x gamespeed, replacing CvPlayer::initFreeState's
	// apply. The gamespeed scaling is engine pacing, not data ([mission-outcome-system.md]: Adapt* is pure engine),
	// so it stays here rather than being baked into the grant.
	const bool bApplied = !s_bSuppressed && nGold > 0;
	if (bApplied)
	{
		CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
		player.setGold(0);
		// ⚠ ÷10000, not ÷100: the speed scalar is ×100 like every other value on the surface
		// ([DEC-fixedpoint-x100]), so multiplying a human gold amount by it lands in ×100 space and takes the
		// second reduction. A ÷100 here would inflate starting gold 100-fold -- the latent 100x class the
		// scale sweep exists to catch (info-rebuild ledger item 27).
		const int iSpeedPercent =
			GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getScalar(SCALAR_SPEED, CASC_SCOPE_WORLD, CASC_UNIT_PERCENT);
		player.changeGold(nGold * iSpeedPercent / 10000);
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_GAMESTART, 1)
		.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(TF_APPLIED, bApplied ? 1 : 0)
		.addI(TF_PLAYER, iPlayer).addI(TF_CIVICS, nCivics).addI(TF_TECHS, nTechs)
		.addI(TF_BUILDINGS, nBuild).addI(TF_STARTINGGOLD, nGold));
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
// observability.md). It is the ONLY per-turn spawn/heal path in the engine, and being on the spine is the point:
// every evaluation ANNOUNCES itself (the TRE_REPEAT line below), so a spawn that fires -- or fails to -- is
// visible. An off-loop path that rolls dice silently is invisible on both axes at once, unexercised AND
// uninstrumented, which is what makes that class the worst legacy to leave standing.
//
// It reads the COMPOSED getTriggers() entries (json.md §5 trigger -> chance -> action), never any legacy collapse
// member. Gated on the enabler's operating-building set: a DORMANT building grants nothing.
//
// A granted unit is an ORDINARY unit (owner ruling): placed through initUnit exactly as a trained one, so it fires
// the ordinary DOMAIN events and nothing downstream can tell the difference -- only the production debit is skipped.

// The full-heal provision -- heal up to iCount damaged own-team units on the city plot, chosen at random.
// Mirrors the legacy CvCity::doHeal body verbatim (DEC-mirror-then-redesign), including its RNG draw.
static int tr_applyFullHeal(CvCity* pCity, int iCount)
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

// The property-scaled unit spawn -- the city's current
// property value, halved, is the per-10000 chance; PROPERTY index 0 (crime) additionally backs off by the plot's
// criminal count and bails once criminals reach half the population. A NEGATIVE-weight property spawns for the
// BARBARIAN player (the crime/disease case), a positive one for the city owner.
// THE TRIGGER'S ODDS, as a per-10000 chance. json.md §5: the odds live ON THE TRIGGER, never inside a payload,
// so this is the entry's business and the action below merely places what the roll allows. Two authored shapes:
//   - `chance: { per: PROPERTY_X }` -- a chance carrying ONLY a per means the SCALED COUNT *is* the odds (§5).
//     The city's property value, halved, per-10000; PROPERTY index 0 (crime) additionally backs off by the plot's
//     criminal count and yields nothing once criminals reach half the population.
//   - `chance: N` -- a flat percent. Parsed ×100, and a percent ×100 IS its own per-10000 figure (5% -> 500).
// ⛔ The flat form previously could not fire at all: the spawn was reached through a property lookup that
// returned early on the absent per, so every entry authored with plain odds was silently inert.
static int tr_triggerChance10000(const CvCity* pCity, const CvTriggerEntry* pEntry)
{
	const PropertyTypes eProperty = (PropertyTypes)pEntry->chancePerTypeId;
	if (eProperty < 0 || eProperty >= GC.getNumPropertyInfos())
	{
		return std::max(0, pEntry->chanceValue);
	}
	int iValue = std::max(0, pCity->getPropertiesConst()->getValueByProperty(eProperty));
	if (eProperty == 0)
	{
		const int iNumCriminals = pCity->plot()->getNumCriminals();
		if (iNumCriminals >= pCity->getPopulation() / 2) return 0;
		iValue -= iNumCriminals * iValue / 10;
	}
	return std::max(0, iValue) / 2;
}

// PLACE one spawned unit. The odds were already decided by the caller (above); this only performs the action.
// The spawn OWNER is the property's sign: a negative-weight property (crime, disease) spawns hostile for the
// BARBARIAN player, a positive one -- and a flat-chance entry, which has no property to ask -- for the city owner.
static int tr_applySpawn(CvCity* pCity, int iChancePerProperty, int iSpawnUnit)
{
	const PropertyTypes eProperty = (PropertyTypes)iChancePerProperty;
	const bool bPositiveProperty =
		(eProperty >= 0 && eProperty < GC.getNumPropertyInfos()) ? (GC.getPropertyInfo(eProperty).getAIWeight() >= 0) : true;

	const UnitTypes eUnit = (UnitTypes)iSpawnUnit;
	// A TRIGGER spawn places the unit -- the queue's offer (and its `allowed` cap) is not the question here.
	if (!EnablerKernel::requiresMetForPlayer(GET_PLAYER(pCity->getOwner()), EDGEB_UNITS, (int)eUnit)) return -1;

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

static void tr_applyCityPerTurn(CvCity* pCity)
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
				const std::vector<int>* pSpawnUnits = pEntry->grant->list(tr_keyUnits);
				if (pSpawnUnits != NULL && !pSpawnUnits->empty())
				{
					// ONE roll per ENTRY -- the odds belong to the trigger (json.md §5), not to each unit --
					// and then the action places EVERY unit the entry grants. ⛔ Taking only [0] silently
					// dropped the rest of an authored list, which no compiler or runtime could have caught.
					const int iChance = tr_triggerChance10000(pCity, pEntry);
					if (iChance > 0 && GC.getGame().getSorenRandNum(10000, "Trigger Spawn Check") < iChance)
					{
						for (size_t u = 0; u < pSpawnUnits->size(); ++u)
						{
							const int iUnit = tr_applySpawn(pCity, pEntry->chancePerTypeId, (*pSpawnUnits)[u]);
							if (iUnit >= 0) { iSpawned = iUnit; ++iSpawnCount; }
						}
					}
				}
			}

		}

	}

	const int iHealed = (iFullHeal > 0) ? tr_applyFullHeal(pCity, iFullHeal) : 0;
	if (iSpawnCount > 0 || iHealed > 0)
	{
		// The SPAWNED field is added ONLY when a unit actually landed: a sentinel -1 through the SFT_UNIT index
		// formatter renders "spawned=?", which cannot be told apart from "a spawn was attempted and failed" --
		// an ambiguous line defeats the point of a surface you reconstruct state from.
		CvSpineEvent ev(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_REPEAT, 1);
		ev.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0)
		  .addI(TF_PLAYER, pCity->getOwner()).addI(TF_CITY, pCity->getID())
		  .addI(TF_HEALED, iHealed);
		if (iSpawnCount > 0) { ev.addI(TF_SPAWNED, iSpawned); }
		eventSpine().emit(ev);
	}
}

// The player's per-turn provisions. Suppressed inside the load bracket like every other apply: a grant is the
// RESULT of a genuine in-play acquisition, and a load is not one ([DEC-spine-reseed]).
static void tr_applyPerTurn(int iPlayer)
{
	if (s_bSuppressed) return;
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS) return;
	CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
	if (!player.isAlive()) return;
	foreach_(CvCity* pLoopCity, player.cities())
	{
		tr_applyCityPerTurn(pLoopCity);
	}
}

// A CITY WAS FOUNDED -- the settle-time provisions. Today this hands over the FOUNDER's `grants.buildings`
// (json §5: the settler's considered action IS founding): a settler seeding buildings into the city it founds.
// That is a NEW mechanic coined for this rework, so there is no legacy apply to mirror -- the data has been
// authored and inert, waiting for a trigger.
// ⛔ The other settle-time provisions (start-era freePopulation, civilization buildings, FreeStartEra, the trait
// settle keys, barbarianInitialDefenders) still apply in CvPlayer::found: several are not authored in a `grants`
// block at all, so the machine cannot resolve them off consideredGrants() until the curator emits them
// (grant-apply-sites.md §5.4). The TRIGGER now exists; the DATA is the remaining blocker.
static void tr_resolveCityFounded(int iOwner, int iCity, int iFounderType)
{
	if (iOwner < 0 || iFounderType < 0) return;
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(iFounderType);
	if (j == NULL || j->consideredGrants() == NULL) return;
	const std::vector<int>* pSeeds = j->consideredGrants()->list(tr_keyBuildings);
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
		EnablerKernel::wireOperatingBuildings(pCity, ec);   // the enabler's sets are the third leg (see above)
		const CvCascadeEvalFlags kFlags;
		for (size_t i = 0; i < pSeeds->size(); ++i)
		{
			const int iBuilding = (*pSeeds)[i];
			if (iBuilding < 0) continue;
			// the entry's own `enabled` condition (the §3.9 conditioned object form), index-parallel to the ids
			const CvCondition* pEnabled = j->consideredGrants()->listCond(tr_keyBuildings, i);
			if (pEnabled != NULL && !cascadeEvalCondition(pEnabled, ec, kFlags)) continue;
			if (pCity->hasBuilding((BuildingTypes)iBuilding)) continue;
			pCity->changeHasBuilding((BuildingTypes)iBuilding, true);
			++nPlaced;
		}
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_FOUND, 1)
		.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(TF_APPLIED, nPlaced)
		.addI(TF_PLAYER, iOwner).addI(TF_CITY, iCity).addI(TF_UNIT, iFounderType)
		.addI(TF_GRANTBUILDINGS, (int)pSeeds->size()));
}

// THE CAPITAL RELOCATED -- re-seed the palace into the new capital. The palace is what MAKES a city the capital
// (setupBuilding's isCapital branch calls setCapitalCity), so without this a captured capital never relocates:
// the settler's `grants.buildings` covers FOUNDING only, and the civilization building list that used to carry the palace on
// relocation no longer does. Same gate as the founding case -- the building on its OWN absence -- so an empire
// that still holds a palace somewhere gets nothing.
static void tr_resolveCapitalChanged(int iOwner, int iCity)
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
			if (GC.getBuildingInfo((BuildingTypes)i).providesAmenity(CLS_AMENITY_CAPITAL)) { s_iCapitalBuilding = i; break; }
		}
	}
	if (s_iCapitalBuilding < 0) return;

	const int nHave = player.getBuildingCount((BuildingTypes)s_iCapitalBuilding);
	const bool bApplied = !s_bSuppressed && nHave == 0 && !pCity->hasBuilding((BuildingTypes)s_iCapitalBuilding);
	if (bApplied) pCity->changeHasBuilding((BuildingTypes)s_iCapitalBuilding, true);

	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_FOUND, 1)
		.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(TF_APPLIED, bApplied ? 1 : 0)
		.addI(TF_PLAYER, iOwner).addI(TF_CITY, iCity).addI(TF_BUILDING, s_iCapitalBuilding));
}

void CvTriggerEngine::onEvent(const CvSpineEvent& e)
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
			tr_resolveBuilding(e.iType, e.iC, e.iSrcLoc);   // iSrcLoc = the city the building landed in
		}
		break;
	// The per-TYPE tally carries no instance, so it cannot apply -- the instance-aware SEVT_UNIT_CREATED does.
	case SEVT_UNIT_CREATED:
		tr_resolveUnit(e.iType, e.iC, e.iA);   // iA = the created unit's id
		// A unit TRAINED in a city takes that city's free promotions -- the same payload, through the same ONE
		// applier the entered-city and building-processed routes use. It rides the spine rather than a hook in
		// CvCity: the promote payload lives in the `triggers` entries, so a second walk beside this one would be
		// a duplicate implementation of it ([DEC-single-implementation]).
		if (e.iC >= 0 && e.iA >= 0)
		{
			CvUnit* pNewUnit = GET_PLAYER((PlayerTypes)e.iC).getUnit(e.iA);
			if (pNewUnit != NULL && pNewUnit->plot() != NULL)
			{
				tr_promoteOneUnit(pNewUnit->plot()->getPlotCity(), pNewUnit);
			}
		}
		break;
	// iType = founding unit's type, iC = owner, iSrcLoc = the new city
	case SEVT_CITY_FOUNDED:   tr_resolveCityFounded(e.iC, e.iSrcLoc, e.iType); break;
	// iC = owner, iSrcLoc = the new capital (-1 = none left)
	case SEVT_CAPITAL_CHANGED: tr_resolveCapitalChanged(e.iC, e.iSrcLoc); break;
	case SEVT_TECH_ACQUIRED:    tr_resolveTech(e.iType, e.iC);       break;  // first-discover only (iC = discoverer)
	// iType = chosen religion, iA = slot religion, iB = bAward, iC = founding player, iSrcLoc = holy city
	case SEVT_RELIGION_FOUNDED: tr_resolveReligion(e.iType, e.iA, e.iC, e.iSrcLoc, e.iB != 0); break;
	case SEVT_CIVIC_ADOPTED:    tr_resolveCivic(e.iType, e.iC);      break;  // iC = adopting player
	case SEVT_PLAYER_INIT:      tr_resolvePlayerInit(e.iC);          break;  // iC = player (game start)
	// iA = the new era, iC = the player whose era advanced (tech-driven -- CvTeam::setHasTech)
	case SEVT_ERA_CHANGED:      if (e.iC >= 0) { tr_resolveEraChanged(e.iC); } break;
	// The per-turn provisions (increment 5). PLAYER-scoped only -- the GAME-scope boundary carries iC = -1 and is
	// the perf/observability fact, not a grant trigger. Legacy ran the per-turn spawn inside CvCity::doTurn within
	// CvPlayer::doTurn, so the player boundary is the ordering-faithful grain.
	case SEVT_TURN_STARTED:     if (e.iC >= 0) { tr_applyPerTurn(e.iC); } break;
	// Trigger (2) for free promotions: a building went ACTIVE in a city -- a fresh build OR a step out of
	// dormancy (processBuilding fires on both). Hand its promotions to everyone already standing there.
	// iType = building, iC = owner, iSrcLoc = city, iB = +1 in / -1 out.
	case SEVT_BUILDING_PROCESSED:
		if (!s_bSuppressed && e.iB > 0 && e.iC >= 0)
		{
			CvCity* pC = GET_PLAYER((PlayerTypes)e.iC).getCity(e.iSrcLoc);
			const int n = (pC != NULL) ? tr_promoteCityUnits(pC, InfoRepo<CvBuildingInfo>::get().get(e.iType)) : 0;
			if (n > 0)
			{
				eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_REPEAT, 1)
					.addI(TF_SUPPRESSED, 0).addI(TF_PLAYER, e.iC).addI(TF_CITY, e.iSrcLoc)
					.addI(TF_BUILDING, e.iType).addI(TF_FREEPROMOS, n));
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
			const int n = tr_promoteOneUnit(pC, pU);
			if (n > 0)
			{
				eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_REPEAT, 1)
					.addI(TF_SUPPRESSED, 0).addI(TF_PLAYER, e.iC).addI(TF_CITY, e.iSrcLoc)
					.addI(TF_UNIT, e.iType).addI(TF_FREEPROMOS, n));
			}
		}
		break;
	}
}

static CvTriggerEngine s_cascadeGrants;

void triggerRegisterConsumer()
{
	tr_registerDomain();
	eventSpine().registerConsumer(&s_cascadeGrants);
}
