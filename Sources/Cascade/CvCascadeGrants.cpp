//
//	CvCascadeGrants -- the #430 GRANTS machine consumer + the [GRANTS] spine domain. See the header + grants-machine.md.
//	Slice-1: on a building-built / unit-created DOMAIN event, resolve the source entity's GENUINE grants off its mapped
//	CvJson<X>Info (the composed CvJsonGrants unit in the InfoRepo, minus the deferred mission-keys) and emit a [GRANTS]
//	diagnostic. Resolution only -- it does NOT apply (legacy applies); un-run parity (owner: no live parity until everything is in).
//

#include "CvGameCoreDLL.h"          // PCH umbrella
#include "CvCascadeGrants.h"
#include "CvEventSpine.h"
#include "CvInfo.h"             // CvInfo::grantList / grantPulse100 / grantFlag (the CvJsonGrants unit's read-throughs)
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
#include "AI/BetterBTSAI.h"        // gPlayerLogLevel -- the slice-1 observe gate
#include <map>
#include <string>
#include <vector>

// ===================== [GRANTS] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
enum GrEvt { GRE_BUILDING = 1, GRE_UNIT, GRE_TECH, GRE_RELIGION, GRE_CIVIC, GRE_GAMESTART };
enum GrFld
{
	GF_PLAYER = 1, GF_BUILDING, GF_UNIT, GF_TECH, GF_RELIGION, GF_CIVIC,
	GF_PROMOTIONS, GF_FOUNDBUILDINGS,                        // unit genuine grants
	GF_REPEATABLE, GF_FREEPROMOS, GF_FREETECHS,              // building genuine grants
	GF_GOLDENAGE, GF_POPULATION,                             // building flag + scoped-pulse grants (increment 2)
	GF_FIRSTUNIT, GF_FIRSTPROPHET,                           // tech first-discover grants (increment 3a)
	GF_NUMFREEUNITS, GF_FREEUNIT, GF_REVOLUTION,             // religion + civic grants (increment 3b)
	GF_CIVICS, GF_TECHS, GF_BUILDINGS, GF_STARTINGGOLD       // game-start civ + era/handicap grants (increment 3c)
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
	case GF_PROMOTIONS:     return "promotions";
	case GF_FOUNDBUILDINGS: return "foundBuildings";
	case GF_REPEATABLE:     return "repeatable";
	case GF_FREEPROMOS:     return "freePromotions";
	case GF_FREETECHS:      return "freeTechs";
	case GF_GOLDENAGE:      return "goldenAge";
	case GF_POPULATION:     return "population";
	default:                return NULL;
	}
}
static void gr_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_GRANTS, gr_prefix, "Cascade.log", gr_field); s_reg = true; }
}

// ===================== resolution off the mapped CvInfo =====================
// A grantList bucket's id-count (0 if absent). GENUINE buckets only -- the deferred mission-keys (unit `buildings`/
// `greatPeople`/`greatPersonAction`/`goldenAge`) are simply not read here (they migrate in the missions pass).
static int gr_listCount(const CvInfo* j, const char* szBucket)
{
	const std::vector<int>* l = j->grantList(szBucket);
	return (l != NULL) ? (int)l->size() : 0;
}
static int gr_pulse(const CvInfo* j, const char* szChannel)   // pulses are stored ×100 -> /100 to the human count/amount
{
	return j->grantPulse100(szChannel) / 100;
}
static int gr_flag(const CvInfo* j, const char* szFlag)   // a bool grant present? (goldenAge)
{
	return j->grantFlag(szFlag) ? 1 : 0;
}
static int gr_scopedPulseSum(const CvInfo* j, const char* szChannel)   // sum a scoped pulse over its scopes (×100 -> /100)
{
	const CvJsonGrants* g = j->getGrants();
	return g ? g->scopedPulseSumAllScopes100(szChannel) / 100 : 0;
}

static void gr_resolveBuilding(int iBuilding, int iPlayer)
{
	const CvInfo* j = InfoRepo<CvBuildingInfo>::get().get(iBuilding);
	if (j == NULL) return;
	const int nRepeat    = (j->getGrants() != NULL) ? (int)j->getGrants()->repeatables().size() : 0;   // per-turn spawn/heal (recurring) -- the structured set (2b)
	const int nFreePromo = gr_listCount(j, "freePromotions");   // end-turn promotions to units in the city (recurring)
	const int nFreeTech  = gr_pulse(j, "freeTechs");            // one-shot on first build
	const int nGoldenAge = gr_flag(j, "goldenAge");            // one-shot golden age (bool grant, increment 2)
	const int nPop       = gr_scopedPulseSum(j, "population");  // one-shot population boost (scoped pulse, increment 2)
	if (nRepeat == 0 && nFreePromo == 0 && nFreeTech == 0 && nGoldenAge == 0 && nPop == 0) return;
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_BUILDING, 1)
		.addI(GF_PLAYER, iPlayer).addI(GF_BUILDING, iBuilding)
		.addI(GF_REPEATABLE, nRepeat).addI(GF_FREEPROMOS, nFreePromo).addI(GF_FREETECHS, nFreeTech)
		.addI(GF_GOLDENAGE, nGoldenAge).addI(GF_POPULATION, nPop));
}

static void gr_resolveUnit(int iUnit, int iPlayer)
{
	const CvInfo* j = InfoRepo<CvUnitInfo>::get().get(iUnit);
	if (j == NULL) return;
	const int nPromos = gr_listCount(j, "promotions");        // free promotions on creation
	const int nFound  = gr_listCount(j, "foundBuildings");    // settle-time building seeds (settler)
	if (nPromos == 0 && nFound == 0) return;
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_UNIT, 1)
		.addI(GF_PLAYER, iPlayer).addI(GF_UNIT, iUnit)
		.addI(GF_PROMOTIONS, nPromos).addI(GF_FOUNDBUILDINGS, nFound));
}

static int gr_firstId(const CvInfo* j, const char* szBucket)   // a single-id grant bucket's id (-1 if absent)
{
	return (j->getGrants() != NULL) ? j->getGrants()->firstListId(szBucket) : -1;
}

static void gr_resolveTech(int iTech, int iPlayer)
{
	const CvInfo* j = InfoRepo<CvTechInfo>::get().get(iTech);
	if (j == NULL) return;
	const int iFirstUnit    = gr_firstId(j, "firstFreeUnit");     // first-discover free unit id (-1 none)
	const int iFirstProphet = gr_firstId(j, "firstFreeProphet");  // first-discover free prophet id (option-gated)
	const int nFreeTechs    = gr_pulse(j, "freeTechs");          // first-discover free tech picks (count)
	if (iFirstUnit < 0 && iFirstProphet < 0 && nFreeTechs == 0) return;
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_TECH, 1)
		.addI(GF_PLAYER, iPlayer).addI(GF_TECH, iTech)
		.addI(GF_FIRSTUNIT, iFirstUnit).addI(GF_FIRSTPROPHET, iFirstProphet).addI(GF_FREETECHS, nFreeTechs));
}

static void gr_resolveReligion(int iReligion, int iPlayer)
{
	const CvInfo* j = InfoRepo<CvReligionInfo>::get().get(iReligion);
	if (j == NULL) return;
	const int nNumFree  = gr_pulse(j, "numFreeUnits");   // count of founder units
	const int iFreeUnit = gr_firstId(j, "freeUnit");      // the founder unit type
	if (nNumFree == 0 && iFreeUnit < 0) return;
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_RELIGION, 1)
		.addI(GF_PLAYER, iPlayer).addI(GF_RELIGION, iReligion)
		.addI(GF_NUMFREEUNITS, nNumFree).addI(GF_FREEUNIT, iFreeUnit));
}

static void gr_resolveCivic(int iCivic, int iPlayer)
{
	const CvInfo* j = InfoRepo<CvCivicInfo>::get().get(iCivic);
	if (j == NULL) return;
	const int nRev = gr_pulse(j, "revolution");   // rev-index pulse on adopt (signed; Python-applied in legacy)
	if (nRev == 0) return;
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_CIVIC, 1)
		.addI(GF_PLAYER, iPlayer).addI(GF_CIVIC, iCivic).addI(GF_REVOLUTION, nRev));
}

// Game start: resolve the player's game-start grants off its civilization (civics/techs/buildings), era + handicap
// (startingGold). The apply is spread across legacy init points; the cascade resolves the whole set at ONE trigger.
static void gr_resolvePlayerInit(int iPlayer)
{
	const CvPlayer& p = GET_PLAYER((PlayerTypes)iPlayer);
	const CvInfo* jc = InfoRepo<CvCivilizationInfo>::get().get(p.getCivilizationType());
	const CvInfo* je = InfoRepo<CvEraInfo>::get().get(p.getCurrentEra());
	const CvInfo* jh = InfoRepo<CvHandicapInfo>::get().get(p.getHandicapType());
	const int nCivics = (jc != NULL) ? gr_listCount(jc, "civics")    : 0;
	const int nTechs  = (jc != NULL) ? gr_listCount(jc, "techs")     : 0;
	const int nBuild  = (jc != NULL) ? gr_listCount(jc, "buildings") : 0;
	const int nGold   = ((je != NULL) ? gr_pulse(je, "startingGold") : 0) + ((jh != NULL) ? gr_pulse(jh, "startingGold") : 0);
	if (nCivics == 0 && nTechs == 0 && nBuild == 0 && nGold == 0) return;
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_GAMESTART, 1)
		.addI(GF_PLAYER, iPlayer).addI(GF_CIVICS, nCivics).addI(GF_TECHS, nTechs)
		.addI(GF_BUILDINGS, nBuild).addI(GF_STARTINGGOLD, nGold));
}

void CvCascadeGrants::onEvent(const CvCascadeEvent& e)
{
	// Observe-only (un-run shadow) -- free when logging is off. The DOMAIN interest-guard already dispatched us.
	if (gPlayerLogLevel < 1 || e.eKind != EVENTKIND_DOMAIN) return;
	switch (e.iEventId)
	{
	case CASCADE_EVT_BUILDING_COUNT: if (e.iB > 0) gr_resolveBuilding(e.iType, e.iC); break;  // iB = delta; only on ADD (built)
	case CASCADE_EVT_UNIT_COUNT:     if (e.iB > 0) gr_resolveUnit(e.iType, e.iC);     break;  // only on ADD (created)
	case CASCADE_EVT_TECH_ACQUIRED:  gr_resolveTech(e.iType, e.iC);                   break;  // first-discover only (iC = discoverer)
	case CASCADE_EVT_RELIGION_FOUNDED: gr_resolveReligion(e.iType, e.iC);            break;  // iC = founding player
	case CASCADE_EVT_CIVIC_ADOPTED:  gr_resolveCivic(e.iType, e.iC);                 break;  // iC = adopting player
	case CASCADE_EVT_PLAYER_INIT:    gr_resolvePlayerInit(e.iC);                     break;  // iC = player (game start)
	}
}

static CvCascadeGrants s_cascadeGrants;

void cascadeRegisterGrants()
{
	gr_registerDomain();
	eventSpine().registerConsumer(&s_cascadeGrants);
}
