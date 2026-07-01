//
//	CvCascadeGrants -- the #430 GRANTS machine consumer + the [GRANTS] spine domain. See the header + grants-machine.md.
//	Slice-1: on a building-built / unit-created DOMAIN event, resolve the source entity's GENUINE grants off its mapped
//	CvJsonInfo (grantLists/grantPulses in the InfoRepo, minus the deferred mission-keys) and emit a [GRANTS] diagnostic.
//	Resolution only -- it does NOT apply (legacy applies); un-run parity (owner: no live parity until everything is in).
//

#include "CvGameCoreDLL.h"          // PCH umbrella
#include "CvCascadeGrants.h"
#include "CvEventSpine.h"
#include "CvJsonInfo.h"             // CvJsonInfo::grantLists / grantPulses
#include "Repos/InfoRepo.h"        // InfoRepo<CvXInfo>::get().get(id) -> the mapped CvJsonInfo*
#include "CvBuildingInfo.h"        // InfoRepo<CvBuildingInfo>
#include "CvUnitInfo.h"            // InfoRepo<CvUnitInfo>
#include "AI/BetterBTSAI.h"        // gPlayerLogLevel -- the slice-1 observe gate
#include <map>
#include <string>
#include <vector>

// ===================== [GRANTS] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
enum GrEvt { GRE_BUILDING = 1, GRE_UNIT };
enum GrFld
{
	GF_PLAYER = 1, GF_BUILDING, GF_UNIT,
	GF_PROMOTIONS, GF_FOUNDBUILDINGS,          // unit genuine grants
	GF_REPEATABLE, GF_FREEPROMOS, GF_FREETECHS  // building genuine grants
};
static const char* gr_prefix(int evt)
{
	switch (evt)
	{
	case GRE_BUILDING: return "[GRANTS/building]";
	case GRE_UNIT:     return "[GRANTS/unit]";
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
	case GF_PROMOTIONS:     return "promotions";
	case GF_FOUNDBUILDINGS: return "foundBuildings";
	case GF_REPEATABLE:     return "repeatable";
	case GF_FREEPROMOS:     return "freePromotions";
	case GF_FREETECHS:      return "freeTechs";
	default:                return NULL;
	}
}
static void gr_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_GRANTS, gr_prefix, "Cascade.log", gr_field); s_reg = true; }
}

// ===================== resolution off the mapped CvJsonInfo =====================
// A grantList bucket's id-count (0 if absent). GENUINE buckets only -- the deferred mission-keys (unit `buildings`/
// `greatPeople`/`greatPersonAction`/`goldenAge`) are simply not read here (they migrate in the missions pass).
static int gr_listCount(const CvJsonInfo* j, const char* szBucket)
{
	std::map<std::string, std::vector<int> >::const_iterator it = j->grantLists.find(szBucket);
	return (it != j->grantLists.end()) ? (int)it->second.size() : 0;
}
static int gr_pulse(const CvJsonInfo* j, const char* szChannel)
{
	std::map<std::string, int>::const_iterator it = j->grantPulses.find(szChannel);
	return (it != j->grantPulses.end()) ? it->second : 0;
}

static void gr_resolveBuilding(int iBuilding, int iPlayer)
{
	const CvJsonInfo* j = InfoRepo<CvBuildingInfo>::get().get(iBuilding);
	if (j == NULL) return;
	const int nRepeat    = gr_listCount(j, "repeatable");       // per-turn spawn/heal (recurring)
	const int nFreePromo = gr_listCount(j, "freePromotions");   // end-turn promotions to units in the city (recurring)
	const int nFreeTech  = gr_pulse(j, "freeTechs");            // one-shot on first build
	if (nRepeat == 0 && nFreePromo == 0 && nFreeTech == 0) return;
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_BUILDING, 1)
		.addI(GF_PLAYER, iPlayer).addI(GF_BUILDING, iBuilding)
		.addI(GF_REPEATABLE, nRepeat).addI(GF_FREEPROMOS, nFreePromo).addI(GF_FREETECHS, nFreeTech));
}

static void gr_resolveUnit(int iUnit, int iPlayer)
{
	const CvJsonInfo* j = InfoRepo<CvUnitInfo>::get().get(iUnit);
	if (j == NULL) return;
	const int nPromos = gr_listCount(j, "promotions");        // free promotions on creation
	const int nFound  = gr_listCount(j, "foundBuildings");    // settle-time building seeds (settler)
	if (nPromos == 0 && nFound == 0) return;
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_GRANTS, GRE_UNIT, 1)
		.addI(GF_PLAYER, iPlayer).addI(GF_UNIT, iUnit)
		.addI(GF_PROMOTIONS, nPromos).addI(GF_FOUNDBUILDINGS, nFound));
}

void CvCascadeGrants::onEvent(const CvCascadeEvent& e)
{
	// Slice-1: observe-only (un-run shadow) -- free when logging is off. The DOMAIN interest-guard already dispatched us.
	if (gPlayerLogLevel < 1) return;
	if (e.eKind != EVENTKIND_DOMAIN || e.iB <= 0) return;   // iB = delta; only on ADD (built/created), not remove
	if (e.iEventId == CASCADE_EVT_BUILDING_COUNT)  gr_resolveBuilding(e.iType, e.iC);   // iType=building, iC=player
	else if (e.iEventId == CASCADE_EVT_UNIT_COUNT) gr_resolveUnit(e.iType, e.iC);       // iType=unit,     iC=player
}

static CvCascadeGrants s_cascadeGrants;

void cascadeRegisterGrants()
{
	gr_registerDomain();
	eventSpine().registerConsumer(&s_cascadeGrants);
}
