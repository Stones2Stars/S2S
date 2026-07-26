//
//	CvEventSpine -- out-of-line parts of the #430 cascade front door + its first (logging) consumer.
//	See CvEventSpine.h for the architecture (KIND firewall, raw payloads, consumer appetites).
//

#include "CvGameCoreDLL.h"
#include "Enabler/CvEnablerConsumer.h"     // the enabler registers its OWN consumer (one per system)
#include "CvModifierConsumer.h"            // the modifier cascade's OWN consumer (one per system)
#include "Engine/ContextConsumer.h"        // the contexts' OWN consumer (the plotAttrs load reseed)
#include "CvCascadeChannelRegistry.h"      // the [CASCADE] mask decode (channel names per scope)
#include "Spine/CvEventSpine.h"
#include "Tools/CvHttpServer.h"   // the /events STREAM consumer (isEnabled + publishEvent)
#include "Infrastructure/CvLogWriter.h"   // the off-thread log file writer (the FILE consumer's sink)
#include "AI/BetterBTSAI.h"   // gPlayerLogLevel (reused as the slice-1 gate; dedicated gate/BUG option + the live
                           // CvHttpServer feed come next)
#include "Defines/CvGlobals.h"        // GC -- resolve raw Type indices to readable names in the (gated) consumer
#include "AI/CvPlayerAI.h"            // GET_PLAYER (the CvPlayerAI::getPlayer macro) + CvPlayer::getName() in the
                                      // SFT_PLAYER consumer render (line ~202) -- imported DIRECTLY (was a latent
                                      // missing include masked by a unity batch-mate until readJson stopped pulling
                                      // CvPlayer.h; unity builds hide missing includes -- structural-cleanup.md §2)
#include "CvBuildingInfo.h"
#include "CvUnitInfo.h"
#include "Grants/CvGrantsEngine.h"   // the #430 GRANTS consumer -- registered at the composition root below
// typeIndex name-resolution in the consumer: the Info headers for each SFT_ kind (so GC.getXInfo(i).getType() compiles).
// Imported DIRECTLY (no CvInfos.h umbrella -- owner 2026-06-18: that umbrella should be retired, import directly).
#include "CvBonusInfo.h"
#include "CvImprovementInfo.h"
#include "CvPromotionInfo.h"
#include "CvReligionInfo.h"
#include "CvCorporationInfo.h"
#include "CvFeatureInfo.h"
#include "CvTerrainInfo.h"
#include "CvCivicInfo.h"
#include "CvProjectInfo.h"
#include "CvSpecialistInfo.h"
#include "CvTraitInfo.h"
#include "CvRouteInfo.h"
#include "CvTechInfo.h"   // SFT_TECH render (getTechInfo().getType()) -- imported directly (was a latent unity-transitive include)

// ===================== the spine =====================

void CvEventSpine::registerConsumer(IEventConsumer* pConsumer)
{
	if (pConsumer == NULL)
	{
		return;
	}
	m_consumers.push_back(pConsumer);
	m_iInterestMask |= pConsumer->wantedKinds();
}

void CvEventSpine::emit(const CvSpineEvent& kEvent)
{
	const int iBit = 1 << kEvent.eKind;
	if ((m_iInterestMask & iBit) == 0)
	{
		return; // interest-guard: no registered consumer wants this kind -> skip entirely (the cheap path)
	}
	for (std::vector<IEventConsumer*>::const_iterator it = m_consumers.begin(); it != m_consumers.end(); ++it)
	{
		if (((*it)->wantedKinds() & iBit) != 0)
		{
			(*it)->onEvent(kEvent);
		}
	}
}

CvEventSpine& eventSpine()
{
	static CvEventSpine s_spine;
	return s_spine;
}

// ===================== slice-1 first consumer: the BROAD logging consumer =====================
// Outputs every event it sees to Cascade.log, gated by gPlayerLogLevel. It renders the RAW payload HERE -- only
// when the gate is on -- which is the whole point of the spine: call sites emit raw fields with no if(loglevel),
// and the costly formatting lives in the gated consumer. (The tally is the SELECTIVE consumer, registered + seeded below.)

// ===================== RAW field rendering (the logging consumer's formatter) =====================
// Field name/type knowledge lives PER DOMAIN (each domain registers a SpineFieldInfoFn) -- the spine holds no global
// field registry. (Constant labels stay in the event prefix, never here.)

// Per-domain registry -- a domain self-registers its prefix provider, destination file, AND field-info resolver
// (the spine never names a domain).
struct SpineDomainReg { SpineLinePrefixFn prefixFn; const char* szLogFile; SpineFieldInfoFn fieldFn; };
static SpineDomainReg g_domains[NUM_SPINE_DOMAINS] = { { 0, 0, 0 } };

void spineRegisterDomain(int iDomainTag, SpineLinePrefixFn prefixFn, const char* szLogFile, SpineFieldInfoFn fieldFn)
{
	if (iDomainTag >= 0 && iDomainTag < NUM_SPINE_DOMAINS)
	{
		g_domains[iDomainTag].prefixFn = prefixFn;
		g_domains[iDomainTag].szLogFile = szLogFile;
		g_domains[iDomainTag].fieldFn = fieldFn;
	}
}

// The constant line PREFIX for (domain, eventId): ask the domain's registered provider; until a domain registers, fall
// back to "[domain=<n>] evt=<id>" so a stray field event is still readable, never crashes.
static const char* spineLinePrefix(int iDomainTag, int iEventId, char* szTmp, int iTmpSize)
{
	if (iDomainTag >= 0 && iDomainTag < NUM_SPINE_DOMAINS && g_domains[iDomainTag].prefixFn != NULL)
	{
		const char* sz = g_domains[iDomainTag].prefixFn(iEventId);
		if (sz != NULL) return sz;
	}
	_snprintf(szTmp, iTmpSize, "[domain=%d] evt=%d", iDomainTag, iEventId);
	szTmp[iTmpSize - 1] = '\0';
	return szTmp;
}

// The destination .log file for a domain's field lines (R-2: per-domain files). Unregistered / NULL => Cascade.log.
static const char* spineDomainFile(int iDomainTag)
{
	if (iDomainTag >= 0 && iDomainTag < NUM_SPINE_DOMAINS && g_domains[iDomainTag].szLogFile != NULL)
	{
		return g_domains[iDomainTag].szLogFile;
	}
	return "Cascade.log";
}

void spineRenderEventLine(char* szBuf, int iBufSize, const CvSpineEvent& kEvent)
{
	char szPre[48];
	// Every spine line carries the game turn by default (owner 2026-07-16) -- first field after the tag, so
	// prefix-anchored greps keep working and any line is placeable in time without cross-referencing bursts.
	int n = _snprintf(szBuf, iBufSize, "%s t=%d", spineLinePrefix(kEvent.iDomainTag, kEvent.iEventId, szPre, sizeof(szPre)), GC.getGame().getGameTurn());
	if (n < 0 || n >= iBufSize) { szBuf[iBufSize - 1] = '\0'; return; }
	// Resolve each field's name + type via the domain's registered field-info resolver (per-domain isolation).
	SpineFieldInfoFn fieldFn = (kEvent.iDomainTag >= 0 && kEvent.iDomainTag < NUM_SPINE_DOMAINS)
		? g_domains[kEvent.iDomainTag].fieldFn : NULL;
	for (int k = 0; k < kEvent.iFieldCount && n < iBufSize - 1; ++k)
	{
		const CvSpineEventField& fld = kEvent.aFields[k];
		SpineFieldType eType = SFT_INT;
		const char* szName = (fieldFn != NULL) ? fieldFn(fld.eTag, &eType) : NULL;
		char szIdx[12];
		if (szName == NULL) { _snprintf(szIdx, sizeof(szIdx), "f%d", fld.eTag); szIdx[sizeof(szIdx)-1] = '\0'; szName = szIdx; eType = SFT_INT; } // fallback: fN=value
		int m = 0;
		switch (eType)
		{
		case SFT_FLOAT:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%g", szName, fld.v.f);
			break;
		case SFT_BUILDING:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumBuildingInfos()) ? GC.getBuildingInfo((BuildingTypes)fld.v.i).getType() : "?");
			break;
		case SFT_UNIT:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumUnitInfos()) ? GC.getUnitInfo((UnitTypes)fld.v.i).getType() : "?");
			break;
		case SFT_TECH:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumTechInfos()) ? GC.getTechInfo((TechTypes)fld.v.i).getType() : "?");
			break;
		case SFT_BONUS:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumBonusInfos()) ? GC.getBonusInfo((BonusTypes)fld.v.i).getType() : "?");
			break;
		case SFT_IMPROVEMENT:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumImprovementInfos()) ? GC.getImprovementInfo((ImprovementTypes)fld.v.i).getType() : "?");
			break;
		case SFT_PROMOTION:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumPromotionInfos()) ? GC.getPromotionInfo((PromotionTypes)fld.v.i).getType() : "?");
			break;
		case SFT_RELIGION:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumReligionInfos()) ? GC.getReligionInfo((ReligionTypes)fld.v.i).getType() : "?");
			break;
		case SFT_CORPORATION:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumCorporationInfos()) ? GC.getCorporationInfo((CorporationTypes)fld.v.i).getType() : "?");
			break;
		case SFT_FEATURE:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumFeatureInfos()) ? GC.getFeatureInfo((FeatureTypes)fld.v.i).getType() : "?");
			break;
		case SFT_TERRAIN:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumTerrainInfos()) ? GC.getTerrainInfo((TerrainTypes)fld.v.i).getType() : "?");
			break;
		case SFT_CIVIC:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumCivicInfos()) ? GC.getCivicInfo((CivicTypes)fld.v.i).getType() : "?");
			break;
		case SFT_PROJECT:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumProjectInfos()) ? GC.getProjectInfo((ProjectTypes)fld.v.i).getType() : "?");
			break;
		case SFT_SPECIALIST:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumSpecialistInfos()) ? GC.getSpecialistInfo((SpecialistTypes)fld.v.i).getType() : "?");
			break;
		case SFT_TRAIT:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumTraitInfos()) ? GC.getTraitInfo((TraitTypes)fld.v.i).getType() : "?");
			break;
		case SFT_ROUTE:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumRouteInfos()) ? GC.getRouteInfo((RouteTypes)fld.v.i).getType() : "?");
			break;
		case SFT_PLAYER:
			// Instance name + raw id, BOTH (owner 2026-06-19): name=human readability, (id)=stable machine join-key for the
			// primary consumers (AI agents during shadow-verify + StoneBase). Resolved LIVE here, which is exact + safe:
			// onEvent renders synchronously on the GAME thread, so this touches no live object off-thread and captures the
			// name as-of-emit. m_iID is serialized (stable across load), so keying on the id holds across a reload too.
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%S(%d)", szName,
				(fld.v.i >= 0 && fld.v.i < MAX_PLAYERS) ? GET_PLAYER((PlayerTypes)fld.v.i).getName() : L"?", fld.v.i);
			break;
		case SFT_STR:  // borrowed narrow string pointer (no copy); valid because render is synchronous at emit
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName, (fld.v.s != NULL) ? fld.v.s : "?");
			break;
		case SFT_WSTR: // borrowed wide string pointer (no copy)
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%S", szName, (fld.v.w != NULL) ? fld.v.w : L"?");
			break;
		default: // SFT_INT / SFT_BOOL -> the raw int
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%d", szName, fld.v.i);
			break;
		}
		if (m <= 0) break;
		n += m;
	}
	szBuf[iBufSize - 1] = '\0';
}

class CvSpineLogConsumer : public IEventConsumer
{
public:
	// BROAD: logging sees every kind (the tally is the SELECTIVE counterpart).
	int wantedKinds() const
	{
		return (1 << EVENTKIND_DOMAIN) | (1 << EVENTKIND_DIAGNOSTIC) | (1 << EVENTKIND_TRACE);
	}

	void onEvent(const CvSpineEvent& kEvent)
	{
		// ONE render path, no per-event string here (event-spine.md the C++ shape). EVERY event -- the [SPINE] DOMAIN
		// state-changes + count/name + the load bracket, and the [HAI]/[MODIFIER]/... DIAGNOSTIC domains -- renders
		// through its domain's REGISTERED prefix + field-info resolvers via spineRenderEventLine. The costly index->text
		// formatting lives here, only when the gate is on. Gated by the event's OWN surveillance level (1 Telescreen ..
		// 4 Thought Police; the [SPINE] load bracket uses 0 so it always logs, since GAME_LOAD_STARTED fires before the
		// BUG log level is pushed). Every emit endpoint tags its domain, so an untagged (SD_NONE) event never occurs in
		// practice; spineRenderEventLine's generic prefix/field fallback keeps even that case readable, not crashing.
		if (gPlayerLogLevel < kEvent.iLevel) return;
		char szBuf[512];
		spineRenderEventLine(szBuf, sizeof(szBuf), kEvent);
		// Off-thread file write (owner 2026-07-16): render game-thread (live-object name resolution), enqueue,
		// the CvLogWriter thread does the disk I/O -- and its per-batch flush keeps the file READABLE while the
		// game runs. Per-domain file ([HAI] -> HunterAI.log; [SPINE] -> Cascade.log).
		CvLogWriter::write(spineDomainFile(kEvent.iDomainTag), szBuf);
	}
};

static CvSpineLogConsumer s_cascadeLogConsumer;

// ===================== the /events STREAM consumer =====================
// /events is its OWN spine consumer (owner ruling: never a tee inside the logging consumer -- that chained
// stream visibility to the FILE gate, so a quiet gPlayerLogLevel silently starved the stream). DOMAIN events --
// the FACTS the machine consumers see, and the out-of-process replay feed (event-spine.md) -- stream
// UNCONDITIONALLY whenever the HTTP server is up. DIAGNOSTIC/TRACE lines stream at the stream's OWN verbosity
// knob (gStreamLogLevel / Autolog__LogLevelStream), fully decoupled from file logging: streaming everything
// never requires opening the level-4 file firehose. The not-yet-spine BBAI helpers keep their own
// streamLogTee until each domain retires onto the spine (observability.md target consolidation).
class CvSpineStreamConsumer : public IEventConsumer
{
public:
	int wantedKinds() const
	{
		return (1 << EVENTKIND_DOMAIN) | (1 << EVENTKIND_DIAGNOSTIC) | (1 << EVENTKIND_TRACE);
	}

	void onEvent(const CvSpineEvent& kEvent)
	{
		if (!CvHttpServer::isEnabled()) return;
		if (kEvent.eKind != EVENTKIND_DOMAIN && gStreamLogLevel < kEvent.iLevel) return;
		char szBuf[512];
		spineRenderEventLine(szBuf, sizeof(szBuf), kEvent);
		CvHttpServer::publishEvent("log", szBuf);
	}
};

static CvSpineStreamConsumer s_cascadeStreamConsumer;

// ===================== the [SPINE] DOMAIN's own render registration =====================
// The spine's DOMAIN events (the per-source state-changes, the empire counts, name-change, the grant triggers, and
// the load bracket) all render through the SAME registered-prefix path every other domain uses (event-spine.md the
// C++ shape) -- a line PREFIX per eventId + a field-info resolver for its LOCAL field tags -- never an inline
// per-event string in the consumer. The DOMAIN ints (iType/iA/iB/iC) still ride for grants/cache; these fields are
// the readable render twin (the struct carries both payloads).

// The [SPINE] domain's LOCAL field tags (the eTag passed to addI). Domain-local per the per-domain isolation rule --
// the spine holds no global field registry; spineDomainFieldInfo below maps each to (name, render-type).
enum SpineDomainField
{
	SPF_BUILDING = 0, SPF_UNIT, SPF_RELIGION, SPF_CORPORATION, SPF_BONUS, SPF_SPECIALIST,
	SPF_TECH, SPF_TRAIT, SPF_CIVIC, SPF_PROJECT, SPF_IMPROVEMENT, SPF_TERRAIN, SPF_FEATURE, SPF_ROUTE,
	SPF_OWNER, SPF_OLD_OWNER, SPF_NEW_OWNER,
	SPF_CITY, SPF_PLOT, SPF_OLD_CITY, SPF_NEW_CITY,
	SPF_DELTA, SPF_HAS, SPF_VALUE, SPF_COUNT, SPF_ON,
	SPF_NAME_KIND, SPF_ENTITY_ID, SPF_NAME,
	// the [CASCADE] invalidate observability fields
	SPF_SCOPE, SPF_ID, SPF_PKG, SPF_SRC,
	SPF_HERITAGE, SPF_ERA, SPF_TAGS,
	// the load-pipeline diagnostic fields (SEVT_LOAD_PIPELINE)
	SPF_MS_REBUILD, SPF_MS_FIXPOINT, SPF_PASSES, SPF_MS_PLOTWARM, SPF_MS_PKGWARM,
	SPF_FLIPS, SPF_CONVERGED, SPF_VERIFY_CATCH, SPF_MS_FIX_ENSURE, SPF_MS_FIX_PROCESS
};

// The constant line PREFIX for each spine DOMAIN eventId ("[SPINE] <eventName>"). The variable fields follow as
// name=value (spineRenderEventLine).
static const char* spineDomainPrefix(int iEventId)
{
	switch (iEventId)
	{
	case SEVT_BUILDING_COUNT:         return "[SPINE] buildingCount";
	case SEVT_UNIT_COUNT:             return "[SPINE] unitCount";
	case SEVT_NAME_CHANGE:            return "[SPINE] nameChange";
	case SEVT_TECH_ACQUIRED:          return "[SPINE] techAcquired";
	case SEVT_RELIGION_FOUNDED:       return "[SPINE] religionFounded";
	case SEVT_CIVIC_ADOPTED:          return "[SPINE] civicAdopted";
	case SEVT_PLAYER_INIT:            return "[SPINE] playerInit";
	case SEVT_BUILDING_CHANGED:       return "[SPINE] buildingChanged";
	case SEVT_BUILDING_PROCESSED:     return "[SPINE] buildingProcessed";
	case SEVT_LOAD_PIPELINE:          return "[SPINE] loadPipeline";
	case SEVT_TURN_STARTED:           return "[SPINE] turnStarted";
	case SEVT_TURN_ENDED:             return "[SPINE] turnEnded";
	case SEVT_UNIT_ENTERED_CITY:      return "[SPINE] unitEnteredCity";
	case SEVT_UNIT_CREATED:           return "[SPINE] unitCreated";
	case SEVT_CITY_FOUNDED:           return "[SPINE] cityFounded";
	case SEVT_CAPITAL_CHANGED:        return "[SPINE] capitalChanged";
	case SEVT_RELIGION_CHANGED:       return "[SPINE] religionChanged";
	case SEVT_CORPORATION_CHANGED:    return "[SPINE] corporationChanged";
	case SEVT_BONUS_CHANGED:          return "[SPINE] bonusChanged";
	case SEVT_POPULATION_CHANGED:     return "[SPINE] populationChanged";
	case SEVT_SPECIALIST_CHANGED:     return "[SPINE] specialistChanged";
	case SEVT_POWER_CHANGED:          return "[SPINE] powerChanged";
	case SEVT_IMPROVEMENT_CHANGED:    return "[SPINE] improvementChanged";
	case SEVT_PLOT_BONUS_CHANGED:     return "[SPINE] plotBonusChanged";
	case SEVT_TERRAIN_CHANGED:        return "[SPINE] terrainChanged";
	case SEVT_FEATURE_CHANGED:        return "[SPINE] featureChanged";
	case SEVT_ROUTE_CHANGED:          return "[SPINE] routeChanged";
	case SEVT_TECH_CHANGED:           return "[SPINE] techChanged";
	case SEVT_TRAIT_CHANGED:          return "[SPINE] traitChanged";
	case SEVT_PROJECT_CHANGED:        return "[SPINE] projectChanged";
	case SEVT_GOLDEN_AGE_CHANGED:     return "[SPINE] goldenAgeChanged";
	case SEVT_STATE_RELIGION_CHANGED: return "[SPINE] stateReligionChanged";
	case SEVT_HERITAGE_CHANGED:       return "[SPINE] heritageChanged";
	case SEVT_PLOTGROUP_BONUS_CHANGED: return "[SPINE] plotGroupBonusChanged";
	case SEVT_CITY_NETWORK_CHANGED:    return "[SPINE] cityNetworkChanged";
	case SEVT_VICINITY_BONUS_CHANGED:  return "[SPINE] vicinityBonusChanged";
	case SEVT_ERA_CHANGED:             return "[SPINE] eraChanged";
	case SEVT_NUKES_CHANGED:           return "[SPINE] nukesChanged";
	case SEVT_CITY_CULTURE_LEVEL_CHANGED: return "[SPINE] cultureLevelChanged";
	case SEVT_HOLY_CITY_CHANGED:       return "[SPINE] holyCityChanged";
	case SEVT_CITY_OWNER_CHANGED:     return "[SPINE] cityOwnerChanged";
	case SEVT_PLOT_OWNER_CHANGED:     return "[SPINE] plotOwnerChanged";
	case SEVT_WORKING_CITY_CHANGED:   return "[SPINE] workingCityChanged";
	case SEVT_GAME_LOAD_STARTED:      return "[SPINE] gameLoadStarted";
	case SEVT_GAME_LOAD_FINISHED:     return "[SPINE] gameLoadFinished";
	case SEVT_CACHE_INVALIDATE:       return "[CASCADE] invalidate";
	case SEVT_CACHE_REBUILT:          return "[CASCADE] rebuilt";
	default:                          return "[SPINE] ?";
	}
}

// Resolve a [SPINE]-LOCAL field tag to its name + render-type (event-spine.md: field knowledge lives in the domain).
static const char* spineDomainFieldInfo(int iFieldTag, SpineFieldType* peType)
{
	switch (iFieldTag)
	{
	case SPF_BUILDING:    *peType = SFT_BUILDING;    return "building";
	case SPF_UNIT:        *peType = SFT_UNIT;        return "unit";
	case SPF_RELIGION:    *peType = SFT_RELIGION;    return "religion";
	case SPF_CORPORATION: *peType = SFT_CORPORATION; return "corporation";
	case SPF_BONUS:       *peType = SFT_BONUS;       return "bonus";
	case SPF_SPECIALIST:  *peType = SFT_SPECIALIST;  return "specialist";
	case SPF_TECH:        *peType = SFT_TECH;        return "tech";
	case SPF_TRAIT:       *peType = SFT_TRAIT;       return "trait";
	case SPF_CIVIC:       *peType = SFT_CIVIC;       return "civic";
	case SPF_PROJECT:     *peType = SFT_PROJECT;     return "project";
	case SPF_IMPROVEMENT: *peType = SFT_IMPROVEMENT; return "improvement";
	case SPF_TERRAIN:     *peType = SFT_TERRAIN;     return "terrain";
	case SPF_FEATURE:     *peType = SFT_FEATURE;     return "feature";
	case SPF_ROUTE:       *peType = SFT_ROUTE;       return "route";
	case SPF_OWNER:       *peType = SFT_PLAYER;      return "owner";
	case SPF_OLD_OWNER:   *peType = SFT_PLAYER;      return "oldOwner";
	case SPF_NEW_OWNER:   *peType = SFT_PLAYER;      return "newOwner";
	case SPF_CITY:        *peType = SFT_INT;         return "city";
	case SPF_PLOT:        *peType = SFT_INT;         return "plot";
	case SPF_OLD_CITY:    *peType = SFT_INT;         return "oldCity";
	case SPF_NEW_CITY:    *peType = SFT_INT;         return "newCity";
	case SPF_DELTA:       *peType = SFT_INT;         return "delta";
	case SPF_HAS:         *peType = SFT_INT;         return "has";
	case SPF_VALUE:       *peType = SFT_INT;         return "value";
	case SPF_COUNT:       *peType = SFT_INT;         return "count";
	case SPF_ON:          *peType = SFT_INT;         return "on";
	case SPF_NAME_KIND:   *peType = SFT_STR;         return "kind";
	case SPF_ENTITY_ID:   *peType = SFT_INT;         return "id";
	case SPF_NAME:        *peType = SFT_WSTR;        return "name";
	case SPF_SCOPE:       *peType = SFT_STR;         return "scope";
	case SPF_ID:          *peType = SFT_INT;         return "id";
	case SPF_PKG:         *peType = SFT_STR;         return "pkg";
	case SPF_SRC:         *peType = SFT_STR;         return "src";
	case SPF_HERITAGE:    *peType = SFT_INT;         return "heritage";
	case SPF_ERA:         *peType = SFT_INT;         return "era";
	case SPF_TAGS:        *peType = SFT_STR;         return "tags";
	case SPF_MS_REBUILD:  *peType = SFT_INT;         return "networkRebuildMs";
	case SPF_MS_FIXPOINT: *peType = SFT_INT;         return "dormancyFixpointMs";
	case SPF_PASSES:      *peType = SFT_INT;         return "passes";
	case SPF_MS_PLOTWARM: *peType = SFT_INT;         return "plotYieldWarmMs";
	case SPF_MS_PKGWARM:  *peType = SFT_INT;         return "packageWarmMs";
	case SPF_FLIPS:       *peType = SFT_INT;         return "flips";
	case SPF_CONVERGED:   *peType = SFT_INT;         return "converged";
	case SPF_VERIFY_CATCH:*peType = SFT_INT;         return "verifyCatches";
	case SPF_MS_FIX_ENSURE:  *peType = SFT_INT;      return "fixEnsureMs";
	case SPF_MS_FIX_PROCESS: *peType = SFT_INT;      return "fixProcessMs";
	default:              *peType = SFT_INT;         return NULL;
	}
}

// ===================== the [CASCADE] invalidate OBSERVABILITY =====================
// Decode a package dirty-mask to a "|"-joined HUMAN-READABLE channel-name string, per scope -- the registry
// owns the per-scope bit contract (channel bits + the trailing receiver-sum bits, rendered "sum:<channel>"),
// so the log reads "production|sum:culture", never a raw bit number.
static void invDecodePackageNames(int iScope, int64_t iMask, char* szOut, int iOutSize)
{
	CascadeChannelRegistry::decodeMask((CvCascScope)iScope, iMask, szOut, iOutSize);
}

// The package scope's log spelling (the CvCascScope containment spine).
static const char* invScopeName(int iScope)
{
	switch (iScope)
	{
	case CASC_SCOPE_WORLD:  return "world";
	case CASC_SCOPE_TEAM:   return "team";
	case CASC_SCOPE_EMPIRE: return "empire";
	case CASC_SCOPE_AREA:   return "area";
	case CASC_SCOPE_CITY:   return "city";
	case CASC_SCOPE_PLOT:   return "plot";
	default:                return "?";
	}
}

// The short name of a spine event id (strips the "[SPINE] " render prefix) -- the invalidate observability's `src`.
const char* spineEventName(int iEventId)
{
	const char* szPrefix = spineDomainPrefix(iEventId);
	if (szPrefix != NULL && strncmp(szPrefix, "[SPINE] ", 8) == 0) return szPrefix + 8;
	return (szPrefix != NULL) ? szPrefix : "?";
}

// Announce a package dirty-mark (DIAGNOSTIC -- logging only, gated at level 1). Renders via the registered SD_SPINE
// path as "[CASCADE] invalidate scope=<> id=<> pkg=<NAMES> src=<why>". Called by the modifier consumer's derived
// marks (szSource = the DOMAIN event that derived them) -- the whole invalidation flow is visible in Cascade.log.
// Shared render for the [CASCADE] invalidate/rebuilt observability (DIAGNOSTIC -- logging only, gated at level 1).
// scope + owner (the empire; the (owner,id) tuple is the unambiguous handle since city ids repeat across empires) +
// id (the scoped object: cityId / plotId / areaId / teamId) + the channel names + an optional src (invalidate only).
static void emitCacheEvent(int iEventId, int iScope, int iOwner, int iId, int64_t iMask, const char* szSource)
{
	char szPackages[512];
	invDecodePackageNames(iScope, iMask, szPackages, sizeof(szPackages));
	CvSpineEvent kEvent(EVENTKIND_DIAGNOSTIC, SD_SPINE, iEventId, 1);
	kEvent.addStr(SPF_SCOPE, invScopeName(iScope));
	if (iOwner >= 0) kEvent.addI(SPF_OWNER, iOwner);
	if (iScope != CASC_SCOPE_EMPIRE) kEvent.addI(SPF_ID, iId);
	kEvent.addStr(SPF_PKG, szPackages);
	if (szSource != NULL) kEvent.addStr(SPF_SRC, szSource);
	eventSpine().emit(kEvent);   // synchronous render -> szPackages / szSource still in scope
}
void emitCacheInvalidate(int iScope, int iOwner, int iId, int64_t iMask, const char* szSource)
{ emitCacheEvent(SEVT_CACHE_INVALIDATE, iScope, iOwner, iId, iMask, (szSource != NULL) ? szSource : "?"); }
void emitCacheRebuilt(int iScope, int iOwner, int iId, int64_t iMask)
{ emitCacheEvent(SEVT_CACHE_REBUILT, iScope, iOwner, iId, iMask, NULL); }

// The load-end pipeline diagnostic (once per load): stage timings + fixpoint depth, through the ONE registered
// render path -- never an inline log call at the site.
void emitLoadPipeline(int iRebuildMs, int iFixpointMs, int iFixEnsureMs, int iFixProcessMs, int iPasses, int iFlips, int iConverged, int iVerifyCatches, int iPlotWarmMs, int iPackageWarmMs)
{
	CvSpineEvent kEvent(EVENTKIND_DIAGNOSTIC, SD_SPINE, SEVT_LOAD_PIPELINE, 1);
	kEvent.addI(SPF_MS_REBUILD, iRebuildMs).addI(SPF_MS_FIXPOINT, iFixpointMs)
	      .addI(SPF_MS_FIX_ENSURE, iFixEnsureMs).addI(SPF_MS_FIX_PROCESS, iFixProcessMs).addI(SPF_PASSES, iPasses)
	      .addI(SPF_FLIPS, iFlips).addI(SPF_CONVERGED, iConverged).addI(SPF_VERIFY_CATCH, iVerifyCatches)
	      .addI(SPF_MS_PLOTWARM, iPlotWarmMs).addI(SPF_MS_PKGWARM, iPackageWarmMs);
	eventSpine().emit(kEvent);
}


void spineRegisterConsumers()
{
	static bool s_bRegistered = false;
	if (s_bRegistered)
	{
		return;
	}
	s_bRegistered = true;
	// The poor-man's-DI composition root: the BROAD logging consumer. The tally is NOT a consumer -- it READS the
	// object-owned counts on demand (CvCascadeTally.h), so it neither registers here nor needs a load-time seed. The
	// DOMAIN count events still flow to logging (observability) + the future invalidation/offline consumers.
	eventSpine().registerConsumer(&s_cascadeLogConsumer);
	// The /events STREAM consumer -- DOMAIN facts unconditionally, DIAGNOSTIC/TRACE at gStreamLogLevel (its own
	// knob, decoupled from the file gate).
	eventSpine().registerConsumer(&s_cascadeStreamConsumer);
	// The [SPINE] DOMAIN: register its prefix + field-info resolvers so EVERY spine event (state-changes, counts,
	// name-change, grant triggers, the load bracket) renders through the consumer's structured path
	// (spineRenderEventLine), never an inline string. NULL log file => Cascade.log.
	spineRegisterDomain(SD_SPINE, spineDomainPrefix, NULL, spineDomainFieldInfo);
	// The #430 GRANTS machine: a SELECTIVE DOMAIN consumer -- on a building-built / unit-created event it resolves the
	// source entity's genuine grants off the mapped CvInfo and emits a [GRANTS] shadow diagnostic (resolution only,
	// un-run parity). The tally stays a non-consumer (reads object-owned counts).
	cascadeRegisterGrants();
	// ONE consumer PER SYSTEM ([DEC-enabler-not-cascade]): the ENABLER's own consumer (load-active -- the
	// reseed's in-read emits BUILD its domains) and the MODIFIER cascade's own consumer (load-active for
	// cache building -- the reseed's emits derive the dirty marks; the first reads after load recompute from
	// current state). Both derive their reactions from their own compiled surfaces; no shared consumer, no
	// hand-wired mutation-site marks.
	enablerRegisterConsumer();
	modifierRegisterConsumer();
	// The CONTEXTS' own consumer (contexts.md): buffers the load bracket's working-city facts and drains them
	// once at GAME_LOAD_FINISHED through CvCity::onCityPlotChanged -- the plotAttrs reseed.
	contextRegisterConsumer();
}

void emitNameChange(int iKind, int iOwner, int iEntityId)
{
	// DOMAIN kind: a rename is synced state an observer must track (Orwell bar). Resolve the NEW name + kind LIVE and
	// pass them as render fields -- the emit() render is synchronous on the game thread, so the CvWString + the literal
	// kind both outlive it (SFT_WSTR/SFT_STR carry a borrowed pointer, event-spine.md §3). iType/iA/iB keep the raw
	// payload (NameChangeKind, owner, entity id) an out-of-process consumer can key on.
	const char* szKind = "?";
	CvWString szName = L"?";
	if (iOwner >= 0 && iOwner < MAX_PLAYERS)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)iOwner);
		switch (iKind)
		{
		case NAMECHANGE_PLAYER: szKind = "player"; szName = kP.getName(); break;
		case NAMECHANGE_CIV:    szKind = "civ";    szName = kP.getCivilizationShortDescription(); break;
		case NAMECHANGE_CITY:   szKind = "city";   { const CvCity* pCity = kP.getCity(iEntityId); if (pCity != NULL) szName = pCity->getName(); } break;
		case NAMECHANGE_UNIT:   szKind = "unit";   { const CvUnit* pUnit = kP.getUnit(iEntityId); if (pUnit != NULL) szName = pUnit->getName(); } break;
		}
	}
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_NAME_CHANGE, iKind, iOwner, iEntityId, 0);
	e.iDomainTag = SD_SPINE;
	e.addStr(SPF_NAME_KIND, szKind).addI(SPF_OWNER, iOwner).addI(SPF_ENTITY_ID, iEntityId).addWStr(SPF_NAME, szName.GetCString());
	eventSpine().emit(e);   // synchronous render -> szName / szKind still in scope
}

// ===== the DOMAIN emit ENDPOINTS (event-spine.md) -- source-carrying: iType = WHAT, iC = WHO (owner/triggering
// player), iSrcLoc = WHERE (cityId | plotId | -1). Each builds the event, tags the [SPINE] domain, and adds its
// render fields, then emits; NO consumer/routing here (that is a separate build). The DOMAIN ints (iType/iA/iB/iC)
// are kept for grants/cache; the addI fields are the readable render twin. The interest-guard makes an emit ~free
// when no consumer wants DOMAIN. Ctor order is (kind, eventId, iType, iA, iB, iC, iSrcLoc). Call AFTER the state
// field is updated.
// iA carries bFIRST -- whether this is a GENUINE first acquisition (1) or a transfer/restore (0). The engine's
// own grant gate is exactly this bit: CvCity::setupBuilding runs its first-build block only when bFirst, and
// CvPlayer::acquireCity re-adds every captured building with bFirst=false precisely so conquest does NOT re-fire
// the grants. A consumer that acts on building acquisition MUST see it, or capturing a city re-grants the whole
// city's first-build bonuses. The reseed passes 0 for the same reason: a save load is a restore, not a build.
void emitBuildingChanged(int iCity, int iOwner, int iBuilding, int iDelta, bool bFirst)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_BUILDING_CHANGED, iBuilding, bFirst ? 1 : 0, iDelta, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}
void emitBuildingProcessed(int iCity, int iOwner, int iBuilding, int iDelta)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_BUILDING_PROCESSED, iBuilding, 0, iDelta, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}
void emitReligionChanged(int iCity, int iOwner, int iReligion, bool bHas)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_RELIGION_CHANGED, iReligion, bHas ? 1 : 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_HAS, bHas ? 1 : 0);
	eventSpine().emit(e);
}
void emitCorporationChanged(int iCity, int iOwner, int iCorporation, bool bHas)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CORPORATION_CHANGED, iCorporation, bHas ? 1 : 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CORPORATION, iCorporation).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_HAS, bHas ? 1 : 0);
	eventSpine().emit(e);
}
void emitBonusChanged(int iCity, int iOwner, int iBonus, int iChange)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_BONUS_CHANGED, iBonus, 0, iChange, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_DELTA, iChange);
	eventSpine().emit(e);
}
void emitPopulationChanged(int iCity, int iOwner, int iNewPop)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_POPULATION_CHANGED, -1, iNewPop, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_VALUE, iNewPop);
	eventSpine().emit(e);
}
void emitSpecialistChanged(int iCity, int iOwner, int iSpecialist, int iDelta)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_SPECIALIST_CHANGED, iSpecialist, 0, iDelta, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_SPECIALIST, iSpecialist).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}
void emitPowerChanged(int iCity, int iOwner, int iDelta)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_POWER_CHANGED, -1, 0, iDelta, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}
void emitImprovementChanged(int iPlot, int iOwner, int iImprovement)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_IMPROVEMENT_CHANGED, iImprovement, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_IMPROVEMENT, iImprovement).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}
void emitPlotBonusChanged(int iPlot, int iOwner, int iBonus, int iChange)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_BONUS_CHANGED, iBonus, 0, iChange, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_DELTA, iChange);
	eventSpine().emit(e);
}
void emitTerrainChanged(int iPlot, int iOwner, int iTerrain)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_TERRAIN_CHANGED, iTerrain, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TERRAIN, iTerrain).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}
void emitFeatureChanged(int iPlot, int iOwner, int iFeature)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_FEATURE_CHANGED, iFeature, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_FEATURE, iFeature).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}
void emitRouteChanged(int iPlot, int iOwner, int iRoute)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_ROUTE_CHANGED, iRoute, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_ROUTE, iRoute).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}
void emitTechChanged(int iPlayer, int iTech, bool bHas)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_TECH_CHANGED, iTech, bHas ? 1 : 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TECH, iTech).addI(SPF_OWNER, iPlayer).addI(SPF_HAS, bHas ? 1 : 0);
	eventSpine().emit(e);
}
void emitTraitChanged(int iPlayer, int iTrait, bool bAdd)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_TRAIT_CHANGED, iTrait, bAdd ? 1 : 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TRAIT, iTrait).addI(SPF_OWNER, iPlayer).addI(SPF_HAS, bAdd ? 1 : 0);
	eventSpine().emit(e);
}
void emitCivicAdopted(int iPlayer, int iCivic, int iOldCivic)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CIVIC_ADOPTED, iCivic, 0, iOldCivic, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CIVIC, iCivic).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}
void emitProjectChanged(int iPlayer, int iProject, int iDelta)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PROJECT_CHANGED, iProject, 0, iDelta, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_PROJECT, iProject).addI(SPF_OWNER, iPlayer).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}
void emitGoldenAgeChanged(int iPlayer, bool bOn)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_GOLDEN_AGE_CHANGED, -1, bOn ? 1 : 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer).addI(SPF_ON, bOn ? 1 : 0);
	eventSpine().emit(e);
}
void emitStateReligionChanged(int iPlayer, int iReligion)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_STATE_RELIGION_CHANGED, iReligion, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}
void emitHeritageChanged(int iPlayer, int iHeritage, bool bAdd)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_HERITAGE_CHANGED, iHeritage, bAdd ? 1 : 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_HERITAGE, iHeritage).addI(SPF_OWNER, iPlayer).addI(SPF_HAS, bAdd ? 1 : 0);
	eventSpine().emit(e);
}
void emitPlotGroupBonusChanged(int iOwner, int iPlotGroupId, int iBonus, int iDelta)
{
	// iSrcLoc = the plot-group id (the network identity; unique within the owner, like a city id)
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOTGROUP_BONUS_CHANGED, iBonus, 0, iDelta, iOwner, iPlotGroupId);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_ID, iPlotGroupId).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}

void emitVicinityBonusChanged(int iCity, int iOwner, int iBonus, int iDelta)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_VICINITY_BONUS_CHANGED, iBonus, 0, iDelta, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_ID, iCity).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}
void emitCityNetworkChanged(int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_NETWORK_CHANGED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CITY, iCity).addI(SPF_OWNER, iOwner);
	eventSpine().emit(e);
}
void emitEraChanged(int iPlayer, int iEra)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_ERA_CHANGED, -1, iEra, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_ERA, iEra).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}
void emitNukesChanged(int iPlayer, int iState)
{
	// iA = state: 0 DISABLED / 1 ENABLED / 2 BANNED
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_NUKES_CHANGED, -1, iState, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer).addI(SPF_VALUE, iState);
	eventSpine().emit(e);
}
void emitCultureLevelChanged(int iCity, int iOwner, int iNewLevel, int iOldLevel)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_CULTURE_LEVEL_CHANGED, -1, iNewLevel, iOldLevel, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CITY, iCity).addI(SPF_OWNER, iOwner).addI(SPF_VALUE, iNewLevel);
	eventSpine().emit(e);
}
void emitHolyCityChanged(int iCity, int iOwner, int iReligion, bool bIsHoly)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_HOLY_CITY_CHANGED, iReligion, bIsHoly ? 1 : 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_CITY, iCity).addI(SPF_OWNER, iOwner).addI(SPF_HAS, bIsHoly ? 1 : 0);
	eventSpine().emit(e);
}
void emitCityOrderChanged(int iCity, int iOwner, int iOrderType, int iItem, int iDelta)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_ORDER_CHANGED, iItem, iOrderType, iDelta, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CITY, iCity).addI(SPF_OWNER, iOwner).addI(SPF_VALUE, iOrderType).addI(SPF_ID, iItem).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}
void emitCityOwnerChanged(int iCity, int iOldOwner, int iNewOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_OWNER_CHANGED, -1, iOldOwner, 0, iNewOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CITY, iCity).addI(SPF_OLD_OWNER, iOldOwner).addI(SPF_NEW_OWNER, iNewOwner);
	eventSpine().emit(e);
}
//	The turn boundaries. The rendered line already carries the game turn as its first field, so the VALUE field
//	is the turn the boundary belongs to (identical on the game pair, and the turn a player's phase sat in).
void emitTurnStarted(int iTurn, int iPlayer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_TURN_STARTED, iTurn, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer).addI(SPF_VALUE, iTurn);
	eventSpine().emit(e);
}
void emitTurnEnded(int iTurn, int iPlayer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_TURN_ENDED, iTurn, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer).addI(SPF_VALUE, iTurn);
	eventSpine().emit(e);
}
void emitUnitEnteredCity(int iUnitType, int iUnitId, int iOwner, int iCity)
{
	// The unit's TAGS ride the event (json §8 -- immutable, type-derived membership). A consumer can then act on
	// WHAT ENTERED without a second lookup: a Riding School only cares about `mounted`, so it filters on the tag
	// instead of probing every arrival. Resolved LIVE and passed as a render field -- the emit renders
	// synchronously on the game thread, so the local string outlives it (the emitNameChange precedent).
	std::string szTags;
	if (iUnitType >= 0 && iUnitType < GC.getNumUnitInfos())
	{
		const CvClassificationBlock* pTags = GC.getUnitInfo((UnitTypes)iUnitType).getTags();
		if (pTags != NULL)
		{
			const std::set<std::string>& names = pTags->all();
			for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it)
			{
				if (!szTags.empty()) szTags += ",";
				szTags += *it;
			}
		}
	}
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_ENTERED_CITY, iUnitType, iUnitId, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnitType).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity)
	 .addStr(SPF_TAGS, szTags.c_str());
	eventSpine().emit(e);   // synchronous render -> szTags still in scope
}
void emitUnitCreated(int iUnitType, int iUnitId, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_CREATED, iUnitType, iUnitId, 0, iOwner, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnitType).addI(SPF_OWNER, iOwner);
	eventSpine().emit(e);
}
void emitCityFounded(int iOwner, int iCity, int iFounderType, int iFounderId)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_FOUNDED, iFounderType, iFounderId, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CITY, iCity).addI(SPF_OWNER, iOwner).addI(SPF_UNIT, iFounderType);
	eventSpine().emit(e);
}
void emitCapitalChanged(int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CAPITAL_CHANGED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CITY, iCity).addI(SPF_OWNER, iOwner);
	eventSpine().emit(e);
}
void emitPlotOwnerChanged(int iPlot, int iOldOwner, int iNewOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_OWNER_CHANGED, -1, iOldOwner, 0, iNewOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_PLOT, iPlot).addI(SPF_OLD_OWNER, iOldOwner).addI(SPF_NEW_OWNER, iNewOwner);
	eventSpine().emit(e);
}
void emitWorkingCityChanged(int iPlot, int iOwner, int iOldCity, int iNewCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_WORKING_CITY_CHANGED, -1, iOldCity, iNewCity, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_PLOT, iPlot).addI(SPF_OWNER, iOwner).addI(SPF_OLD_CITY, iOldCity).addI(SPF_NEW_CITY, iNewCity);
	eventSpine().emit(e);
}

// The empire-count observability events + the grant-trigger events. iSrcLoc = -1 (empire/world footprint). grants
// reads iType/iA/iB/iC off these (CvCascadeGrants); the addI fields are the render twin. One endpoint each so the
// CvPlayer / CvTeam emit sites never build a CvSpineEvent inline.
void emitBuildingCount(int iPlayer, int iBuilding, int iNewCount, int iDelta)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_BUILDING_COUNT, iBuilding, iNewCount, iDelta, iPlayer);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iNewCount).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}
void emitUnitCount(int iPlayer, int iUnit, int iNewCount, int iDelta)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_COUNT, iUnit, iNewCount, iDelta, iPlayer);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnit).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iNewCount).addI(SPF_DELTA, iDelta);
	eventSpine().emit(e);
}
void emitTechAcquired(int iPlayer, int iTech)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_TECH_ACQUIRED, iTech, 1, 0, iPlayer);   // iA = 1 (first-discoverer)
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TECH, iTech).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}
void emitReligionFounded(int iPlayer, int iReligion, int iSlotReligion, int iCity, bool bAward)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_RELIGION_FOUNDED, iReligion, iSlotReligion, bAward ? 1 : 0, iPlayer, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_OWNER, iPlayer).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}
void emitPlayerInit(int iPlayer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLAYER_INIT, iPlayer, 0, 0, iPlayer);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

// The load-lifecycle bracket (event-spine.md the load-RESEED). Paired via a static flag so a FINISHED fires ONLY
// when a load actually STARTED: onFinalInitialized calls emitGameLoadFinished() for new game too, where it no-ops
// (no matching STARTED). Result-producers (grants) will gate on these -- that is the grant engine's own job, later.
static bool s_bGameLoadInProgress = false;
// DOMAIN-kind (so the grant engine will see the bracket) but tagged with the [SPINE] domain so the logging consumer
// renders it through the registered prefix; iLevel 0 = a lifecycle signal that ALWAYS logs (STARTED fires before the
// BUG log level is pushed).
void emitGameLoadStarted()
{
	s_bGameLoadInProgress = true;
	CvSpineEvent kEvt(EVENTKIND_DOMAIN, SEVT_GAME_LOAD_STARTED);
	kEvt.iDomainTag = SD_SPINE;
	kEvt.iLevel = 0;
	eventSpine().emit(kEvt);
}
void emitGameLoadFinished()
{
	if (!s_bGameLoadInProgress) { return; }   // new game: no STARTED fired -> no FINISHED
	CvSpineEvent kEvt(EVENTKIND_DOMAIN, SEVT_GAME_LOAD_FINISHED);
	kEvt.iDomainTag = SD_SPINE;
	kEvt.iLevel = 0;
	eventSpine().emit(kEvt);
	s_bGameLoadInProgress = false;
}

// True in the load-active window (between GAME_LOAD_STARTED and GAME_LOAD_FINISHED). The R3 consumer's MODIFIER-MARK
// half reads this to SKIP the play-time targeted ripples during the reseed -- the frontier/operating-building
// reverse indices are not built until onFinalInitialized (buildFrontierIndices), so a mid-reseed ripple is invalid.
// (Its ENABLER half stays load-active -- the reseed events BUILD the enabler domains, DEC-spine-reseed.)
bool spineGameLoadInProgress() { return s_bGameLoadInProgress; }
