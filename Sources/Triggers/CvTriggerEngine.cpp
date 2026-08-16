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
#include "AI/CvTeamAI.h"          // GET_TEAM -- the obsolete-building guard on a granted placement
#include "Engine/CvGame.h"        // GC.getGame().getStartEra() -- the era the game-start grants key on
#include "Engine/CvGameSpeedScale.h" // the ONE consuming-system speed calc -- never re-read the raw scalar
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
static const int tr_keyCulture          = CvGrants::key("culture");
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
//   (2) a promo building GOES ACTIVE -> grant to every own-team unit present       (SEVT_CITY_BUILDING_ACTIVATED)
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
// Apply ONE source's promote entries to ONE unit -- the whole free-promotion plane, read off the
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
		if (pEntry->happening != TRIGGER_UNIT_ENTERED_CITY || pEntry->promotePromotions.empty()) continue;
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
// The per-RECEIVER placement of a granted building. The entry's condition is evaluated against the city
// RECEIVING it, never the granting one (json §8's per-receiver rule): a gate like IS_CAPITAL means the
// RECEIVER's capital-ness, so evaluating it once at the source would strand the grant wherever it was authored.
// The eval ctx is built only when there IS a condition -- the overwhelmingly common entry is unconditional.
static int tr_placeGrantedBuilding(CvCity* pCity, CvPlayer& player, int iBuilding,
	const CvCondition* pEnabled, const CvCascadeEvalFlags& kFlags)
{
	if (pCity == NULL || iBuilding < 0 || pCity->hasBuilding((BuildingTypes)iBuilding))
	{
		return 0;
	}
	// An obsolete building must not be resurrected in every city. The team already owns the tech list, so this is
	// the derived predicate rather than a stored flag.
	if (GET_TEAM(pCity->getTeam()).isObsoleteBuilding((BuildingTypes)iBuilding))
	{
		return 0;
	}
	// ⚠ WHERE it may stand is the building's OWN `requires.build`, evaluated by the ONE evaluator
	// ([DEC-single-implementation]) -- the coastal/river/terrain/map-category clauses the legacy placement gate
	// re-derived by hand are exactly that condition, authored. A grant changes the LIFETIME of the provision and
	// skips the production cost; it does not change whether the receiver can hold the thing at all.
	// ⛔ It is NOT the enabler's queue verdict: that answers "may this city QUEUE it", which a grant bypasses by
	// construction (triggers.md -- the only divergence from normal creation is the cost step).
	const CvInfo* pBuildingInfo = &GC.getBuildingInfo((BuildingTypes)iBuilding);
	const CvCondition* pRequiresBuild = pBuildingInfo->requiresBuild();
	if (pRequiresBuild != NULL || pEnabled != NULL)
	{
		// the contexts ARE the eval state (contexts.md): the fill seams, never a hand-assembled raw ctx
		CvCascadeEvalCtx ec;
		pCity->getCityContext().fillEvalCtx(ec);
		player.getEmpireContext().fillEvalCtx(ec);
		EnablerKernel::wireOperatingBuildings(pCity, ec);   // the enabler's sets are the third leg
		if (pRequiresBuild != NULL && !cascadeEvalCondition(pRequiresBuild, ec, kFlags))
		{
			return 0;
		}
		if (pEnabled != NULL && !cascadeEvalCondition(pEnabled, ec, kFlags))
		{
			return 0;
		}
	}
	pCity->changeHasBuilding((BuildingTypes)iBuilding, true);
	return 1;
}

// THE FREE BUILDING -- the ONE placement both legs share ([DEC-single-implementation]). A grant hands the
// building OVER and the receiving city genuinely HAS it, which is load-bearing rather than cosmetic: the
// authored data gates on holding these targets in over a thousand `requires` atoms, so a shape delivering only
// the EFFECTS would satisfy none of them.
//
// An entry's SCOPE says WHERE it lands (json §3.9's universal entry field): `empire` reaches every city the
// player holds; absent means the considered action's own city, which is why the settler's founder-buildings and
// the civ capital list are unaffected.
//
// ⚠ Deliberately NOT refcounted against the source's presence. Legacy removed the copies when the source went;
// a grant PERSISTS (owner: "in all scenarios they behave like grants") -- a stated behaviour change, not an
// omission ([legacy-grant-apply-sites.md] §4).
static int tr_grantBuildingsFrom(const CvInfo* j, CvCity* pSourceCity, CvPlayer& player)
{
	if (j == NULL || j->consideredGrants() == NULL)
	{
		return 0;
	}
	const std::vector<int>* pList = j->consideredGrants()->list(tr_keyBuildings);
	if (pList == NULL || pList->empty())
	{
		return 0;
	}
	int nPlaced = 0;
	const CvCascadeEvalFlags kFlags;
	for (size_t i = 0; i < pList->size(); ++i)
	{
		const int iBuilding = (*pList)[i];
		const int iScope = j->consideredGrants()->listScope(tr_keyBuildings, i);
		// the entry's own `enabled` condition (the §3.9 conditioned object form), index-parallel to the ids
		const CvCondition* pEnabled = j->consideredGrants()->listCond(tr_keyBuildings, i);

		if (iScope == tr_keyScopeEmpire)
		{
			foreach_(CvCity* pLoopCity, player.cities())
			{
				nPlaced += tr_placeGrantedBuilding(pLoopCity, player, iBuilding, pEnabled, kFlags);
			}
		}
		else
		{
			nPlaced += tr_placeGrantedBuilding(pSourceCity, player, iBuilding, pEnabled, kFlags);
		}
	}
	return nPlaced;
}

// THE CONTESTED-AUTOBUILD AWARD -- first-to-earn (enabler.md §3). A world/team-capped autoBuild (Valley of the
// Kings is the whole shipped population) is excluded from system placement: its cap is a cross-player race
// dormancy cannot express, and its one-shot pulses (freeTechs, goldenAge) need a genuine first acquisition. So
// it is AWARDED once, in the city whose state first satisfies its operate gate while the cap has room -- placed
// with bFirst=true, so the ordinary ADDED path fires the pulses exactly once; thereafter it stands and dormancy
// toggles its standing effects like anything else.
// ⚑ Called from the facts that can move such a gate (a building added, a tech acquired, a population step); the
// contested census is one entity, so the check is an early-out for everyone else.
static void tr_awardContestedAutoBuilds(CvCity* pCity)
{
	PROFILE_EXTRA_FUNC();
	if (pCity == NULL) return;
	const std::vector<int>& aContested = EnablerKernel::contestedAutoBuildings();
	if (aContested.empty()) return;
	for (size_t i = 0; i < aContested.size(); ++i)
	{
		const int iBuilding = aContested[i];
		// the ONE arrival gate for a queue-excluded building -- cap + obsolescence + the operate condition
		// ([DEC-single-implementation]; the mission-construct path asks the same question of the same body)
		if (!EnablerKernel::queueExcludedArrivalOk(pCity, iBuilding)) continue;
		pCity->changeHasBuilding((BuildingTypes)iBuilding, true);   // a genuine first acquisition: bFirst fires the pulses
	}
}

// Ordering note: the legacy applied these MID-setup; the machine applies at SEVT_CITY_BUILDING_ADDED, which fires at
// the END of setHasBuilding (after setupBuilding) -- so the building is fully set up before its provisions land,
// which is strictly the safer order.
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

	// THE FREE BUILDING -- `grants.buildings` on the source (owner: "in all scenarios they behave like grants").
	// An entry's SCOPE says WHERE it lands: `empire` reaches every city the player holds, absent means this city.
	// ⚑ This is leg ONE of two. It fans over the cities that ALREADY STAND; a city founded or acquired LATER is
	// covered by the city-founded leg, which folds what its owner already holds. A fan alone would pass on every
	// city standing today and silently miss every future one ("every city AFTERWARDS gets a free copy", owner).
	tr_grantBuildingsFrom(j, pCity, player);

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
	const int nGrantBld  = tr_listCount(j, tr_keyBuildings);        // the free building (grants.buildings on the source)
	if (nRepeat == 0 && nFreePromo == 0 && nFreeTech == 0 && nGoldenAge == 0 && nPop == 0 && nGrantBld == 0) return;
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
// The prophet leg is the tech's own `firstFreeProphet` bucket gated on GAMEOPTION_RELIGION_DIVINE_PROPHETS: a pure
// grant read plus the live option, resolved here beside the other two first-discoverer legs.
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
				player.createUnit((UnitTypes)iFreeUnit, pCity->getX(), pCity->getY());
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
		// nGold is already HUMAN (tr_pulse reduces the ×100 pulse), and the speed percent is a PERCENT, which is
		// NOT scaled ([DEC-fixedpoint-x100]) -- so this is the ordinary percent-as-ratio /100. ⛔ A /10000 here
		// reads the percent as ×100 and truncates the whole starting grant to nothing.
		// ⚑ Asked through CvGameSpeedScale, the ONE consuming-system speed calc: re-reading the raw scalar per
		// call site is what let three separate consumers each invent their own scale ([DEC-single-implementation]).
		player.changeGold(nGold * CvGameSpeedScale::speedPercent() / 100);
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
// A granted unit is an ORDINARY unit (owner ruling): created through CvPlayer::createUnit, the ONE creation step a
// trained unit also ends at, so it is owed identically and nothing downstream can tell the difference -- only the
// production debit is skipped.

// The full-heal provision -- heal up to iCount damaged own-team units on the city plot, chosen at random.
// ⛔ The selection shuffles on CvGame::SorenRand, the SYNCHRONIZED stream, so how many values are consumed here
// is shared save state ([DEC-synced-rng-is-shared-state]). Changing the shuffle is a deliberate change to every
// client's sequence, never a refactor -- and that, not "the legacy body did it", is why the draw stays as it is.
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
	CvUnit* pUnit = GET_PLAYER(eSpawnOwner).createUnit(eUnit, pCity->getX(), pCity->getY(), UNITAI_BARB_CRIMINAL);
	if (pUnit == NULL) return -1;

	// The two PLACEMENT rules a spawn carries of its own: an excile cannot remain in the city it surfaced in,
	// and a spawned unit has already had its turn. Both are this payload's business, not creation's.
	if (pUnit->isExcile()) pUnit->jumpToNearestValidPlot(false);
	pUnit->finishMoves();

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

// ⚖ THE FEATURE DIES AS ITS CITY GROWS -- the `destroy: self` action, whose one live carrier is a FEATURE.
// The containment chain is ordinary and needs no target vocabulary: a city knows its plot, the plot carries the
// feature, so the feature reads the city's POPULATION fact and goes.
// ⛔ THE SUBJECT IS THE CITY'S OWN PLOT, NEVER ITS WORKED RADIUS. This is urbanisation consuming the tile the
// city stands on; FOREST / JUNGLE / SWAMP / FLOOD_PLAINS all author it, so reading "self" as the radius would
// strip every one of them from every city that reached the threshold.
// ⚑ ONE hook covers founding AND growth: CvCity::init sets the population, so the ADDED fact fires there too --
// which is why the legacy pair (a founding branch plus a setPopulation branch) collapses to this single route.
static void tr_resolveFeatureDestroy(int iOwner, int iCity)
{
	if (iOwner < 0 || iOwner >= MAX_PLAYERS)
	{
		return;
	}
	CvCity* pCity = GET_PLAYER((PlayerTypes)iOwner).getCity(iCity);
	if (pCity == NULL)
	{
		return;
	}
	CvPlot* pPlot = pCity->plot();
	if (pPlot == NULL || pPlot->getFeatureType() == NO_FEATURE)
	{
		return;
	}
	const CvTriggers* pTriggers = GC.getFeatureInfo(pPlot->getFeatureType()).getTriggers();
	if (pTriggers == NULL)
	{
		return;
	}
	const CvPlayer& player = GET_PLAYER((PlayerTypes)iOwner);
	CvCascadeEvalCtx ec;
	pCity->getCityContext().fillEvalCtx(ec);
	player.getEmpireContext().fillEvalCtx(ec);
	EnablerKernel::wireOperatingBuildings(pCity, ec);
	const CvCascadeEvalFlags kFlags;

	const std::vector<CvTriggerEntry*>& entries = pTriggers->entries();
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const CvTriggerEntry* pEntry = entries[i];
		if (!pEntry->destroySelf)
		{
			continue;
		}
		// The threshold is an ordinary §3 state condition, through the ONE evaluator ([DEC-single-implementation]).
		if (pEntry->condition != NULL && !cascadeEvalCondition(pEntry->condition, ec, kFlags))
		{
			continue;
		}
		pPlot->setFeatureType(NO_FEATURE);
		return;   // the subject is gone -- a second entry has nothing left to act on
	}
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
			// This is the onTurn plane only -- the promote entries ride the targeted-propagation
			// free-promotion path (SEVT_CITY_BUILDING_ACTIVATED / SEVT_UNIT_ENTERED_CITY), never a per-turn rescan.
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
// It also hands over the founder's conditioned NUMERIC pulses -- the trait start CULTURE and bonus POPULATION,
// which are conditional grants living on the founder ([json.md] §5), each gated by the trait as the entry's own
// `enabled`. They are read off the conditioned-pulse tail rather than the pulse MAP: the map holds one summed
// number per channel with nowhere to put a condition, so reading them there would hand every civilization the
// sum of every trait's bonus ([CvGrants.h] -- the split mirrors the modifier plane's compiled-sum + tail).
// ⛔ The other settle-time provisions (start-era freePopulation, civilization buildings, FreeStartEra,
// barbarianInitialDefenders) still apply in CvPlayer::found: several are not authored in a `grants` block at
// all, so the machine cannot resolve them off consideredGrants() until the curator emits them
// (grant-apply-sites.md §5.4).
static void tr_resolveCityFounded(int iOwner, int iCity, int iFounderType)
{
	if (iOwner < 0 || iFounderType < 0) return;
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(iFounderType);
	if (j == NULL || j->consideredGrants() == NULL) return;
	const std::vector<int>* pSeeds = j->consideredGrants()->list(tr_keyBuildings);
	const std::vector<int>* pUnits = j->consideredGrants()->list(tr_keyUnits);
	const std::vector<int>* pCulture = j->consideredGrants()->pulseEntries(tr_keyCulture);
	const std::vector<int>* pPopulation = j->consideredGrants()->pulseEntries(tr_keyPopulation);
	// ⚠ The guard covers EVERY payload this resolver hands over, not just the buildings. A founder carrying only
	// pulses would otherwise return before applying any of them.
	if ((pSeeds == NULL || pSeeds->empty())
	&& (pUnits == NULL || pUnits->empty())
	&& (pCulture == NULL || pCulture->empty())
	&& (pPopulation == NULL || pPopulation->empty())) return;

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
		for (size_t i = 0; pSeeds != NULL && i < pSeeds->size(); ++i)
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
		// The founder's `grants.units` -- a settler that CARRIES an escort hands it over on its considered action,
		// exactly as it hands over its founder buildings. A granted unit is an ORDINARY unit ([triggers.md]):
		// created through the ONE creation step, so it is owed exactly what a trained unit is owed and nothing
		// downstream can tell it apart from one.
		// ⛔ It inherits NOTHING from the founder. Handing the escort the settler's accumulated experience was the
		// legacy Python shape and is DEAD (owner: paying XP for walking a settler is bad game design) -- the unit
		// is created fresh, like any other.
		for (size_t i = 0; pUnits != NULL && i < pUnits->size(); ++i)
		{
			const int iUnit = (*pUnits)[i];
			if (iUnit < 0) continue;
			const CvCondition* pEnabled = j->consideredGrants()->listCond(tr_keyUnits, i);
			if (pEnabled != NULL && !cascadeEvalCondition(pEnabled, ec, kFlags)) continue;
			player.createUnit((UnitTypes)iUnit, pCity->getX(), pCity->getY());
			++nPlaced;
		}
		// The conditioned numeric pulses. Each entry is gated by its own `enabled` through the ONE evaluator, so
		// only the founder's owner's live traits contribute -- which is the whole reason they cannot be summed
		// off the pulse map. ⚠ Pulse values are ×100 at parse, so each reduces here at its point of use.
		int iCulture100 = 0;
		for (size_t i = 0; pCulture != NULL && i < pCulture->size(); ++i)
		{
			const CvCondition* pEnabled = j->consideredGrants()->pulseEntryCond(tr_keyCulture, i);
			if (pEnabled != NULL && !cascadeEvalCondition(pEnabled, ec, kFlags)) continue;
			iCulture100 += (*pCulture)[i];
		}
		if (iCulture100 / 100 > 0)
		{
			pCity->changeCulture(player.getID(), iCulture100 / 100, true, true);
		}
		int iPop100 = 0;
		for (size_t i = 0; pPopulation != NULL && i < pPopulation->size(); ++i)
		{
			const CvCondition* pEnabled = j->consideredGrants()->pulseEntryCond(tr_keyPopulation, i);
			if (pEnabled != NULL && !cascadeEvalCondition(pEnabled, ec, kFlags)) continue;
			iPop100 += (*pPopulation)[i];
		}
		if (iPop100 / 100 > 0)
		{
			pCity->changePopulation(iPop100 / 100);
		}
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_FOUND, 1)
		.addI(TF_SUPPRESSED, s_bSuppressed ? 1 : 0).addI(TF_APPLIED, nPlaced)
		.addI(TF_PLAYER, iOwner).addI(TF_CITY, iCity).addI(TF_UNIT, iFounderType)
		// ⚠ NULL-guarded: the payload guard above admits a founder carrying only pulses or only units, so
		// `pSeeds` is legitimately absent here.
		.addI(TF_GRANTBUILDINGS, (pSeeds != NULL) ? (int)pSeeds->size() : 0));
}

// LEG TWO of the free building: a city that STARTS EXISTING folds what its owner ALREADY holds. Leg one fans the
// grantor over the cities standing at the time, so without this a city founded or acquired AFTERWARDS never
// receives its copies -- *"I build it in the first city, and then every city afterwards gets a free copy"*
// (owner). ⚑ It is the amenity fold's two-leg shape, for the same reason: a grantor fact cannot reach a city
// that does not exist yet, so the CITY side folds on arrival ([contexts.md](../architecture/contexts.md)).
//
// ⛔ EMPIRE-scoped entries ONLY. An unscoped entry is the considered action's own target and was already placed
// where it was earned; re-placing it in every later city would silently promote a local grant to an empire one.
//
// ⚑ The self-granting population needs no special case and that is the point: once ANY city holds the source its
// empire count is non-zero, so every city arriving afterwards folds a copy -- which is exactly what "build it in
// the first city, every city afterwards gets one" means.
static int tr_foldOwnerGrantedBuildings(int iOwner, int iCity)
{
	if (iOwner < 0 || iCity < 0 || s_bSuppressed)
	{
		return 0;
	}
	CvPlayer& player = GET_PLAYER((PlayerTypes)iOwner);
	CvCity* pCity = player.getCity(iCity);
	if (pCity == NULL)
	{
		return 0;
	}
	int nPlaced = 0;
	const CvCascadeEvalFlags kFlags;
	const int iNumBuildings = GC.getNumBuildingInfos();
	for (int iSource = 0; iSource < iNumBuildings; ++iSource)
	{
		// the player's own O(1) empire aggregate -- the tally's read-not-store rule (tally.md §2)
		if (player.getBuildingCount((BuildingTypes)iSource) <= 0)
		{
			continue;
		}
		const CvInfo* jSource = InfoRepo<CvBuildingInfo>::get().get(iSource);
		if (jSource == NULL || jSource->consideredGrants() == NULL)
		{
			continue;
		}
		const std::vector<int>* pList = jSource->consideredGrants()->list(tr_keyBuildings);
		if (pList == NULL)
		{
			continue;
		}
		for (size_t i = 0; i < pList->size(); ++i)
		{
			if (jSource->consideredGrants()->listScope(tr_keyBuildings, i) != tr_keyScopeEmpire)
			{
				continue;
			}
			nPlaced += tr_placeGrantedBuilding(pCity, player, (*pList)[i],
				jSource->consideredGrants()->listCond(tr_keyBuildings, i), kFlags);
		}
	}
	return nPlaced;
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
	// BUILDING grants ride the presence ADD, not the player COUNT event. Three reasons, all load-bearing: the count
	// event is emitted only from CvPlayer::changeBuildingCount and so NEVER fires during the save read -- the machine
	// was blind to every building grant on load (0 lines, indistinguishable from "no grants"); the presence fact
	// fires on the genuine flip in play AND per building in the reseed read loop, which is exactly one resolve in
	// both worlds; and it is NOT the dormancy signal (that is the operate crossing, which also fires on every
	// disable/enable flip -- a CONSTRUCTED building granting there would re-grant each time it woke up).
	// ⚖ The SYSTEM-PLACED population is the deliberate exception: its considered action IS the activation (it is
	// placed dormant with bFirst=false), so the ACTIVATED case below fires its building-grant leg, which is
	// idempotent under re-wake by construction (enabler.md §3).
	// ⚑ The machine subscribes to the ARRIVAL and never sees a removal, so there is no direction to test: a payload
	// guard here was the fact's missing name, wearing an `if` ([DEC-facts-name-happenings]).
	// iC = owner, iSrcLoc = city.
	case SEVT_CITY_BUILDING_ADDED:
		// iA = bFirst. A conquest transfer / load restore resolves (so it is visible) but is WITHHELD --
		// the engine grants nothing in that case either (CvCity::setupBuilding's bFirst gate).
		s_bFirstAcquire = (e.iA != 0);
		if (!s_bFirstAcquire) s_bSuppressed = true;
		tr_resolveBuilding(e.iType, e.iC, e.iSrcLoc);   // iSrcLoc = the city the building landed in
		// ...and a building arriving here may complete a contested autoBuild's gate (the Sphinx joining the
		// Pyramid completes Valley of the Kings). Not during the load bracket: a loaded save already holds
		// whatever it earned, and the legacy one-turn award lag is its own semantics.
		if (!spineGameLoadInProgress() && e.iC >= 0)
		{
			tr_awardContestedAutoBuilds(GET_PLAYER((PlayerTypes)e.iC).getCity(e.iSrcLoc));
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
	case SEVT_CITY_FOUNDED:
		tr_resolveCityFounded(e.iC, e.iSrcLoc, e.iType);
		// ...and the new city folds the empire-scoped buildings its owner already holds (the free building's
		// second leg -- "every city AFTERWARDS gets a free copy").
		tr_foldOwnerGrantedBuildings(e.iC, e.iSrcLoc);
		break;
	// A city ACQUIRED (conquest/trade) arrives under a new owner who may already hold granting sources, so it
	// folds them exactly as a founded city does. iC = the NEW owner (-1 on dispose -- the fold guards it).
	case SEVT_CITY_OWNER_ADDED: tr_foldOwnerGrantedBuildings(e.iC, e.iSrcLoc); break;
	// iC = owner, iSrcLoc = the capital that ARRIVED. A move is REMOVED beside ADDED, so "none left" is simply
// the absence of an ADDED fact rather than a -1 this case has to recognise.
	case SEVT_EMPIRE_CAPITAL_ADDED: tr_resolveCapitalChanged(e.iC, e.iSrcLoc); break;
	case SEVT_TECH_ACQUIRED:
		tr_resolveTech(e.iType, e.iC);   // first-discover only (iC = discoverer)
		// A tech can complete a contested autoBuild's gate (Sculpture for Valley of the Kings) -- check the
		// discoverer's cities. The contested census is one entity, so this is cities × 1 gate on a rare fact.
		if (!s_bSuppressed && e.iC >= 0 && !EnablerKernel::contestedAutoBuildings().empty())
		{
			foreach_(CvCity* pLoopCity, GET_PLAYER((PlayerTypes)e.iC).cities())
			{
				tr_awardContestedAutoBuilds(pLoopCity);
			}
		}
		break;
	// iType = chosen religion, iA = slot religion, iB = bAward, iC = founding player, iSrcLoc = holy city
	case SEVT_RELIGION_FOUNDED: tr_resolveReligion(e.iType, e.iA, e.iC, e.iSrcLoc, e.iB != 0); break;
	case SEVT_CIVIC_ADOPTED:    tr_resolveCivic(e.iType, e.iC);      break;  // iC = adopting player
	case SEVT_PLAYER_INIT:      tr_resolvePlayerInit(e.iC);          break;  // iC = player (game start)
	// iA = the new era, iC = the player whose era advanced (tech-driven -- CvTeam::setHasTech)
	case SEVT_EMPIRE_ERA_ADDED: if (e.iC >= 0) { tr_resolveEraChanged(e.iC); } break;
	// The per-turn provisions (increment 5). PLAYER-scoped only -- the GAME-scope boundary carries iC = -1 and is
	// the perf/observability fact, not a grant trigger. Legacy ran the per-turn spawn inside CvCity::doTurn within
	// CvPlayer::doTurn, so the player boundary is the ordering-faithful grain.
	case SEVT_TURN_STARTED:     if (e.iC >= 0) { tr_applyPerTurn(e.iC); } break;
	// The city GREW -- its own plot's feature may have a population threshold it has now crossed.
	// iC = owner, iSrcLoc = city. Withheld during the save read like every other apply: the saved plot already
	// carries whatever feature it should, and mutating the map mid-stream is not this machine's business.
	case SEVT_CITY_POPULATION_ADDED:
		if (!s_bSuppressed)
		{
			tr_resolveFeatureDestroy(e.iC, e.iSrcLoc);
			// ...and a contested autoBuild's gate may carry a population threshold this step crossed.
			if (e.iC >= 0) { tr_awardContestedAutoBuilds(GET_PLAYER((PlayerTypes)e.iC).getCity(e.iSrcLoc)); }
		}
		break;
	// Trigger (2) for free promotions: a building went ACTIVE in a city -- a fresh build OR a step out of
	// dormancy (the operate crossing covers both). Hand its promotions to everyone already standing there.
	// ⚑ The machine subscribes to the ACTIVATION alone, so there is no direction left to test
	// ([DEC-facts-name-happenings]). iType = building, iC = owner, iSrcLoc = city.
	case SEVT_CITY_BUILDING_ACTIVATED:
		if (!s_bSuppressed && e.iC >= 0)
		{
			CvCity* pC = GET_PLAYER((PlayerTypes)e.iC).getCity(e.iSrcLoc);
			const CvInfo* jB = InfoRepo<CvBuildingInfo>::get().get(e.iType);
			const int n = (pC != NULL) ? tr_promoteCityUnits(pC, jB) : 0;
			// A SYSTEM-PLACED building (band / autoBuild): its considered action is the ACTIVATION, never the
			// placement (placed dormant everywhere with bFirst=false, enabler.md §3), so the considered
			// BUILDING-GRANT leg fires here -- the C_AD adoption marker granting its C_AC access marker is the
			// live case. A re-activation re-fires it, and that is safe by construction: the place path skips a
			// held target and the empire-level choke point folds to held-once, so the grant is idempotent.
			// ⛔ The one-shot PULSE legs (population / goldenAge / freeTechs) deliberately do NOT fire here --
			// a building that can wake repeatedly gives them no defined moment, which is exactly why the
			// world-capped autoBuild that authors them (Valley of the Kings) is excluded from system placement.
			int nGranted = 0;
			if (pC != NULL && jB != NULL && EnablerKernel::isSystemPlacedBuilding(e.iType))
			{
				nGranted = tr_grantBuildingsFrom(jB, pC, GET_PLAYER((PlayerTypes)e.iC));
			}
			if (n > 0 || nGranted > 0)
			{
				eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_TRIGGERS, TRE_REPEAT, 1)
					.addI(TF_SUPPRESSED, 0).addI(TF_PLAYER, e.iC).addI(TF_CITY, e.iSrcLoc)
					.addI(TF_BUILDING, e.iType).addI(TF_FREEPROMOS, n).addI(TF_APPLIED, nGranted));
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
