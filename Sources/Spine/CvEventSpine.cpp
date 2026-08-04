//
//	CvEventSpine -- out-of-line parts of the #430 cascade front door + its first (logging) consumer.
//	See CvEventSpine.h for the architecture (KIND firewall, raw payloads, consumer appetites).
//

#include "CvGameCoreDLL.h"
#include "Enabler/CvEnablerConsumer.h"     // the enabler registers its OWN consumer (one per system)
#include "CvModifierConsumer.h"            // the modifier cascade's OWN consumer (one per system)
#include "Engine/ContextConsumer.h"        // the contexts' OWN consumer (the plotAttrs load reseed)
#include "Engine/AmenityContext.h"
#include "Engine/PolicyContext.h"           // the enacted-policy dictionary's own consumer          // the amenity CONTEXT's own consumer ([DEC-dict-is-a-consumer])
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
#include "Triggers/CvTriggerEngine.h"   // the #430 GRANTS consumer -- registered at the composition root below
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
#include "CvCommerceInfo.h"   // SFT_COMMERCE render (the slider fact's channel) -- bounded by NUM_COMMERCE_TYPES
#include "CvTechInfo.h"   // SFT_TECH render (getTechInfo().getType()) -- imported directly (was a latent unity-transitive include)
#include "CvPropertyInfo.h"   // SFT_PROPERTY render (the property fact names PROPERTY_CRIME, not a raw int)
#include "CvUnitCombatInfo.h"   // SFT_UNITCOMBAT render (the unit plane's combat-class fact names its class)

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
		case SFT_UNITCOMBAT:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumUnitCombatInfos()) ? GC.getUnitCombatInfo((UnitCombatTypes)fld.v.i).getType() : "?");
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
		case SFT_PROPERTY:
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < GC.getNumPropertyInfos()) ? GC.getPropertyInfo((PropertyTypes)fld.v.i).getType() : "?");
			break;
		case SFT_COMMERCE:
			// CommerceTypes is a FIXED engine enum, so its bound is NUM_COMMERCE_TYPES, not a getNumXInfos() count.
			m = _snprintf(szBuf + n, iBufSize - n, " %s=%s", szName,
				(fld.v.i >= 0 && fld.v.i < NUM_COMMERCE_TYPES) ? GC.getCommerceInfo((CommerceTypes)fld.v.i).getType() : "?");
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
	// BROAD: logging sees every kind (the tally is the SELECTIVE counterpart). SAVELOAD included -- the load record
	// is a LOG of loading, and logging is exactly the consumer it is for.
	int wantedKinds() const
	{
		return (1 << EVENTKIND_DOMAIN) | (1 << EVENTKIND_SAVELOAD)
			| (1 << EVENTKIND_DIAGNOSTIC) | (1 << EVENTKIND_TRACE);
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
		return (1 << EVENTKIND_DOMAIN) | (1 << EVENTKIND_SAVELOAD)
			| (1 << EVENTKIND_DIAGNOSTIC) | (1 << EVENTKIND_TRACE);
	}

	void onEvent(const CvSpineEvent& kEvent)
	{
		if (!CvHttpServer::isEnabled()) return;
		// Only DOMAIN streams unconditionally. SAVELOAD rides gStreamLogLevel like the trace kinds -- deliberately,
		// because the load record is the highest-volume stream in the engine and must not spend the bounded SSE
		// slots during ordinary play.
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
	// The generic old->new pair, for a fact whose value is a bare engine enum with no info table to resolve
	// (PlotTypes, LandmarkTypes): the event PREFIX already names WHICH value it is, so the field stays generic.
	SPF_OLD_VALUE, SPF_NEW_VALUE,
	SPF_NAME_KIND, SPF_ENTITY_ID, SPF_NAME,
	// the [CASCADE] invalidate observability fields
	SPF_SCOPE, SPF_ID, SPF_PKG, SPF_SRC,
	SPF_HERITAGE, SPF_ERA, SPF_TAGS, SPF_COMMERCE,
	// the property fact: WHAT changed + WHICH KIND of object carries it (the kind is what makes SPF_ID readable)
	SPF_PROPERTY, SPF_OBJECT_KIND,
	// the game-option fact: WHICH option + which id SPACE it speaks (the space is what makes the id readable),
	// and the difficulty fact's handicap
	SPF_OPTION, SPF_OPTION_SPACE, SPF_HANDICAP,
	// the global-define fact: the string KEY (a define has no id space, so the name IS the identity) + the
	// float/string value forms the DOMAIN ints cannot carry
	SPF_DEFINE, SPF_VALUE_F, SPF_VALUE_STR,
	// the unit plane's two dirty triggers + the scoped ids the non-city/plot facts hang on
	SPF_PROMOTION, SPF_UNITCOMBAT, SPF_UNIT_ID, SPF_TEAM, SPF_AREA, SPF_TIMER,
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
	case SEVT_GAME_LOAD_STARTED:                return "[SPINE] gameLoadStarted";
	case SEVT_GAME_LOAD_FINISHED:               return "[SPINE] gameLoadFinished";
	case SEVT_TURN_STARTED:                     return "[SPINE] turnStarted";
	case SEVT_TURN_ENDED:                       return "[SPINE] turnEnded";
	case SEVT_GAME_OPTION_ADDED:                return "[SPINE] gameOptionAdded";
	case SEVT_GAME_OPTION_REMOVED:              return "[SPINE] gameOptionRemoved";
	case SEVT_GAME_HANDICAP_ADDED:              return "[SPINE] gameHandicapAdded";
	case SEVT_GAME_HANDICAP_REMOVED:            return "[SPINE] gameHandicapRemoved";
	case SEVT_GAME_GLOBAL_DEFINE_ADDED:         return "[SPINE] gameGlobalDefineAdded";
	case SEVT_GAME_GLOBAL_DEFINE_REMOVED:       return "[SPINE] gameGlobalDefineRemoved";
	case SEVT_WORLD_NUKES_BANNED_ADDED:         return "[SPINE] worldNukesBannedAdded";
	case SEVT_WORLD_NUKES_BANNED_REMOVED:       return "[SPINE] worldNukesBannedRemoved";
	case SEVT_WORLD_UNIT_CREATED_COUNT_ADDED:   return "[SPINE] worldUnitCreatedCountAdded";
	case SEVT_AREAS_RECALCULATED:               return "[SPINE] areasRecalculated";
	case SEVT_TEAM_MEMBER_ADDED:                return "[SPINE] teamMemberAdded";
	case SEVT_TEAM_MEMBER_REMOVED:              return "[SPINE] teamMemberRemoved";
	case SEVT_EMPIRE_TECH_ADDED:                return "[SPINE] empireTechAdded";
	case SEVT_EMPIRE_TECH_REMOVED:              return "[SPINE] empireTechRemoved";
	case SEVT_EMPIRE_TRAIT_ADDED:               return "[SPINE] empireTraitAdded";
	case SEVT_EMPIRE_TRAIT_REMOVED:             return "[SPINE] empireTraitRemoved";
	case SEVT_EMPIRE_PROJECT_ADDED:             return "[SPINE] empireProjectAdded";
	case SEVT_EMPIRE_PROJECT_REMOVED:           return "[SPINE] empireProjectRemoved";
	case SEVT_EMPIRE_HERITAGE_ADDED:            return "[SPINE] empireHeritageAdded";
	case SEVT_EMPIRE_HERITAGE_REMOVED:          return "[SPINE] empireHeritageRemoved";
	case SEVT_EMPIRE_STATE_RELIGION_ADDED:      return "[SPINE] empireStateReligionAdded";
	case SEVT_EMPIRE_STATE_RELIGION_REMOVED:    return "[SPINE] empireStateReligionRemoved";
	case SEVT_EMPIRE_GOLDEN_AGE_ADDED:          return "[SPINE] empireGoldenAgeAdded";
	case SEVT_EMPIRE_GOLDEN_AGE_REMOVED:        return "[SPINE] empireGoldenAgeRemoved";
	case SEVT_EMPIRE_ANARCHY_ADDED:             return "[SPINE] empireAnarchyAdded";
	case SEVT_EMPIRE_ANARCHY_REMOVED:           return "[SPINE] empireAnarchyRemoved";
	case SEVT_EMPIRE_ERA_ADDED:                 return "[SPINE] empireEraAdded";
	case SEVT_EMPIRE_ERA_REMOVED:               return "[SPINE] empireEraRemoved";
	case SEVT_EMPIRE_HANDICAP_ADDED:            return "[SPINE] empireHandicapAdded";
	case SEVT_EMPIRE_HANDICAP_REMOVED:          return "[SPINE] empireHandicapRemoved";
	case SEVT_EMPIRE_NUKES_ENABLED_ADDED:       return "[SPINE] empireNukesEnabledAdded";
	case SEVT_EMPIRE_NUKES_ENABLED_REMOVED:     return "[SPINE] empireNukesEnabledRemoved";
	case SEVT_EMPIRE_COMMERCE_PERCENT_ADDED:    return "[SPINE] empireCommercePercentAdded";
	case SEVT_EMPIRE_COMMERCE_PERCENT_REMOVED:  return "[SPINE] empireCommercePercentRemoved";
	case SEVT_EMPIRE_CAPITAL_ADDED:             return "[SPINE] empireCapitalAdded";
	case SEVT_EMPIRE_CAPITAL_REMOVED:           return "[SPINE] empireCapitalRemoved";
	case SEVT_EMPIRE_BUILDING_COUNT_ADDED:      return "[SPINE] empireBuildingCountAdded";
	case SEVT_EMPIRE_BUILDING_COUNT_REMOVED:    return "[SPINE] empireBuildingCountRemoved";
	case SEVT_EMPIRE_UNIT_COUNT_ADDED:          return "[SPINE] empireUnitCountAdded";
	case SEVT_EMPIRE_UNIT_COUNT_REMOVED:        return "[SPINE] empireUnitCountRemoved";
	case SEVT_CITY_BUILDING_ADDED:              return "[SPINE] cityBuildingAdded";
	case SEVT_CITY_BUILDING_REMOVED:            return "[SPINE] cityBuildingRemoved";
	case SEVT_CITY_BUILDING_ACTIVATED:          return "[SPINE] cityBuildingActivated";
	case SEVT_CITY_BUILDING_DORMANTED:          return "[SPINE] cityBuildingDormanted";
	case SEVT_CITY_BUILDING_OBSOLETED_ADDED:    return "[SPINE] cityBuildingObsoletedAdded";
	case SEVT_CITY_BUILDING_OBSOLETED_REMOVED:  return "[SPINE] cityBuildingObsoletedRemoved";
	case SEVT_CITY_RELIGION_ADDED:              return "[SPINE] cityReligionAdded";
	case SEVT_CITY_RELIGION_REMOVED:            return "[SPINE] cityReligionRemoved";
	case SEVT_CITY_CORPORATION_ADDED:           return "[SPINE] cityCorporationAdded";
	case SEVT_CITY_CORPORATION_REMOVED:         return "[SPINE] cityCorporationRemoved";
	case SEVT_CITY_BONUS_ADDED:                 return "[SPINE] cityBonusAdded";
	case SEVT_CITY_BONUS_REMOVED:               return "[SPINE] cityBonusRemoved";
	case SEVT_CITY_VICINITY_BONUS_ADDED:        return "[SPINE] cityVicinityBonusAdded";
	case SEVT_CITY_VICINITY_BONUS_REMOVED:      return "[SPINE] cityVicinityBonusRemoved";
	case SEVT_CITY_POPULATION_ADDED:            return "[SPINE] cityPopulationAdded";
	case SEVT_CITY_POPULATION_REMOVED:          return "[SPINE] cityPopulationRemoved";
	case SEVT_CITY_SPECIALIST_ADDED:            return "[SPINE] citySpecialistAdded";
	case SEVT_CITY_SPECIALIST_REMOVED:          return "[SPINE] citySpecialistRemoved";
	case SEVT_CITY_POWER_ADDED:                 return "[SPINE] cityPowerAdded";
	case SEVT_CITY_POWER_REMOVED:               return "[SPINE] cityPowerRemoved";
	case SEVT_CITY_POWER_DISABLED_ADDED:        return "[SPINE] cityPowerDisabledAdded";
	case SEVT_CITY_POWER_DISABLED_REMOVED:      return "[SPINE] cityPowerDisabledRemoved";
	case SEVT_CITY_FRESH_WATER_ADDED:           return "[SPINE] cityFreshWaterAdded";
	case SEVT_CITY_FRESH_WATER_REMOVED:         return "[SPINE] cityFreshWaterRemoved";
	case SEVT_CITY_GOVERNMENT_CENTER_ADDED:     return "[SPINE] cityGovernmentCenterAdded";
	case SEVT_CITY_GOVERNMENT_CENTER_REMOVED:   return "[SPINE] cityGovernmentCenterRemoved";
	case SEVT_CITY_HOLY_CITY_ADDED:             return "[SPINE] cityHolyCityAdded";
	case SEVT_CITY_HOLY_CITY_REMOVED:           return "[SPINE] cityHolyCityRemoved";
	case SEVT_CITY_HEADQUARTERS_ADDED:          return "[SPINE] cityHeadquartersAdded";
	case SEVT_CITY_HEADQUARTERS_REMOVED:        return "[SPINE] cityHeadquartersRemoved";
	case SEVT_CITY_CULTURE_LEVEL_ADDED:         return "[SPINE] cityCultureLevelAdded";
	case SEVT_CITY_CULTURE_LEVEL_REMOVED:       return "[SPINE] cityCultureLevelRemoved";
	case SEVT_CITY_OWNER_ADDED:                 return "[SPINE] cityOwnerAdded";
	case SEVT_CITY_OWNER_REMOVED:               return "[SPINE] cityOwnerRemoved";
	case SEVT_CITY_NETWORK_ADDED:               return "[SPINE] cityNetworkAdded";
	case SEVT_CITY_NETWORK_REMOVED:             return "[SPINE] cityNetworkRemoved";
	case SEVT_CITY_ORDER_ADDED:                 return "[SPINE] cityOrderAdded";
	case SEVT_CITY_ORDER_REMOVED:               return "[SPINE] cityOrderRemoved";
	case SEVT_CITY_FOUNDED:                     return "[SPINE] cityFounded";
	case SEVT_PLOT_TERRAIN_ADDED:               return "[SPINE] plotTerrainAdded";
	case SEVT_PLOT_TERRAIN_REMOVED:             return "[SPINE] plotTerrainRemoved";
	case SEVT_PLOT_FEATURE_ADDED:               return "[SPINE] plotFeatureAdded";
	case SEVT_PLOT_FEATURE_REMOVED:             return "[SPINE] plotFeatureRemoved";
	case SEVT_PLOT_IMPROVEMENT_ADDED:           return "[SPINE] plotImprovementAdded";
	case SEVT_PLOT_IMPROVEMENT_REMOVED:         return "[SPINE] plotImprovementRemoved";
	case SEVT_PLOT_ROUTE_ADDED:                 return "[SPINE] plotRouteAdded";
	case SEVT_PLOT_ROUTE_REMOVED:               return "[SPINE] plotRouteRemoved";
	case SEVT_PLOT_BONUS_ADDED:                 return "[SPINE] plotBonusAdded";
	case SEVT_PLOT_BONUS_REMOVED:               return "[SPINE] plotBonusRemoved";
	case SEVT_PLOT_TYPE_ADDED:                  return "[SPINE] plotTypeAdded";
	case SEVT_PLOT_TYPE_REMOVED:                return "[SPINE] plotTypeRemoved";
	case SEVT_PLOT_LANDMARK_ADDED:              return "[SPINE] plotLandmarkAdded";
	case SEVT_PLOT_LANDMARK_REMOVED:            return "[SPINE] plotLandmarkRemoved";
	case SEVT_PLOT_RIVER_ADDED:                 return "[SPINE] plotRiverAdded";
	case SEVT_PLOT_RIVER_REMOVED:               return "[SPINE] plotRiverRemoved";
	case SEVT_PLOT_IRRIGATION_ADDED:            return "[SPINE] plotIrrigationAdded";
	case SEVT_PLOT_IRRIGATION_REMOVED:          return "[SPINE] plotIrrigationRemoved";
	case SEVT_PLOT_OWNER_ADDED:                 return "[SPINE] plotOwnerAdded";
	case SEVT_PLOT_OWNER_REMOVED:               return "[SPINE] plotOwnerRemoved";
	case SEVT_PLOT_WORKING_CITY_ADDED:          return "[SPINE] plotWorkingCityAdded";
	case SEVT_PLOT_WORKING_CITY_REMOVED:        return "[SPINE] plotWorkingCityRemoved";
	case SEVT_PLOT_WORKED_ADDED:                return "[SPINE] plotWorkedAdded";
	case SEVT_PLOT_WORKED_REMOVED:              return "[SPINE] plotWorkedRemoved";
	case SEVT_PLOT_CITY_ADDED:                  return "[SPINE] plotCityAdded";
	case SEVT_PLOT_CITY_REMOVED:                return "[SPINE] plotCityRemoved";
	case SEVT_PLOTGROUP_BONUS_ADDED:            return "[SPINE] plotgroupBonusAdded";
	case SEVT_PLOTGROUP_BONUS_REMOVED:          return "[SPINE] plotgroupBonusRemoved";
	case SEVT_AREA_TILE_ADDED:                  return "[SPINE] areaTileAdded";
	case SEVT_AREA_TILE_REMOVED:                return "[SPINE] areaTileRemoved";
	case SEVT_AREA_CLEAN_POWER_ADDED:           return "[SPINE] areaCleanPowerAdded";
	case SEVT_AREA_CLEAN_POWER_REMOVED:         return "[SPINE] areaCleanPowerRemoved";
	case SEVT_UNIT_CREATED:                     return "[SPINE] unitCreated";
	case SEVT_UNIT_KILLED:                      return "[SPINE] unitKilled";
	case SEVT_UNIT_DEATH_SCHEDULE_ADDED:        return "[SPINE] unitDeathScheduleAdded";
	case SEVT_UNIT_DEATH_SCHEDULE_REMOVED:      return "[SPINE] unitDeathScheduleRemoved";
	case SEVT_UNIT_ENTERED_CITY:                return "[SPINE] unitEnteredCity";
	case SEVT_UNIT_LEFT_CITY:                   return "[SPINE] unitLeftCity";
	case SEVT_UNIT_PROMOTION_ADDED:             return "[SPINE] unitPromotionAdded";
	case SEVT_UNIT_PROMOTION_REMOVED:           return "[SPINE] unitPromotionRemoved";
	case SEVT_UNIT_COMBAT_ADDED:                return "[SPINE] unitCombatAdded";
	case SEVT_UNIT_COMBAT_REMOVED:              return "[SPINE] unitCombatRemoved";
	case SEVT_PROPERTY_ADDED:                   return "[SPINE] propertyAdded";
	case SEVT_PROPERTY_REMOVED:                 return "[SPINE] propertyRemoved";
	case SEVT_TECH_ACQUIRED:                    return "[SPINE] techAcquired";
	case SEVT_RELIGION_FOUNDED:                 return "[SPINE] religionFounded";
	case SEVT_CIVIC_ADOPTED:                    return "[SPINE] civicAdopted";
	case SEVT_PLAYER_INIT:                      return "[SPINE] playerInit";
	case SEVT_NAME_CHANGE:                      return "[SPINE] nameChange";
	case SEVT_CITY_BUILDING_PROCESSED:          return "[SPINE] cityBuildingProcessed";
	case SEVT_LOAD_PIPELINE:                    return "[SPINE] loadPipeline";
	default:                                 return "[SPINE] ?";
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
	case SPF_DEFINE:        *peType = SFT_STR;       return "define";
	case SPF_VALUE_F:       *peType = SFT_FLOAT;     return "valueF";
	case SPF_VALUE_STR:     *peType = SFT_STR;       return "valueStr";
	case SPF_OPTION:        *peType = SFT_INT;       return "option";
	case SPF_OPTION_SPACE:  *peType = SFT_INT;       return "optionSpace";
	case SPF_HANDICAP:      *peType = SFT_INT;       return "handicap";
	case SPF_OLD_VALUE:   *peType = SFT_INT;         return "oldValue";
	case SPF_NEW_VALUE:   *peType = SFT_INT;         return "newValue";
	case SPF_NAME_KIND:   *peType = SFT_STR;         return "kind";
	case SPF_ENTITY_ID:   *peType = SFT_INT;         return "id";
	case SPF_NAME:        *peType = SFT_WSTR;        return "name";
	case SPF_SCOPE:       *peType = SFT_STR;         return "scope";
	case SPF_ID:          *peType = SFT_INT;         return "id";
	case SPF_PKG:         *peType = SFT_STR;         return "pkg";
	case SPF_SRC:         *peType = SFT_STR;         return "src";
	case SPF_HERITAGE:    *peType = SFT_INT;         return "heritage";
	case SPF_ERA:         *peType = SFT_INT;         return "era";
	case SPF_COMMERCE:    *peType = SFT_COMMERCE;    return "commerce";
	case SPF_PROPERTY:    *peType = SFT_PROPERTY;    return "property";
	case SPF_OBJECT_KIND: *peType = SFT_STR;         return "objectKind";
	case SPF_PROMOTION:   *peType = SFT_PROMOTION;   return "promotion";
	case SPF_UNITCOMBAT:  *peType = SFT_UNITCOMBAT;  return "unitCombat";
	case SPF_UNIT_ID:     *peType = SFT_INT;         return "unitId";
	case SPF_TEAM:        *peType = SFT_INT;         return "team";
	case SPF_AREA:        *peType = SFT_INT;         return "area";
	case SPF_TIMER:       *peType = SFT_INT;         return "timer";
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

// The short name of a spine event id (strips the "[SPINE] " render prefix) -- the invalidate observability's `src`.
const char* spineEventName(int iEventId)
{
	const char* szPrefix = spineDomainPrefix(iEventId);
	if (szPrefix != NULL && strncmp(szPrefix, "[SPINE] ", 8) == 0) return szPrefix + 8;
	return (szPrefix != NULL) ? szPrefix : "?";
}

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
	// ONE consumer PER SYSTEM ([DEC-enabler-not-cascade]): the ENABLER's own consumer (load-active -- the
	// reseed's in-read emits BUILD its domains) and the MODIFIER cascade's own consumer (load-active for
	// cache building -- the reseed's emits derive the dirty marks; the first reads after load recompute from
	// current state). Both derive their reactions from their own compiled surfaces; no shared consumer, no
	// hand-wired mutation-site marks.
	// ⛔ REGISTRATION ORDER IS A CONTRACT HERE, not tidiness, and it binds BOTH state-building machines.
	// Consumers are dispatched in registration order, and GAME_LOAD_FINISHED is where the dependency bites:
	//   - the CONTEXTS' consumer BUILDS the CityContext / EmpireContext stores on that event (it buffers the
	//     load bracket's working-city facts and drains them through CvCity::onCityPlotChanged -- the plotAttrs
	//     reseed), so it must go FIRST;
	//   - the ENABLER's load-end pass gates every city, and each gate evaluates its conditions THROUGH those
	//     stores (BuildingEnabler -> getCityContext().fillEvalCtx). Gating ahead of the contexts would evaluate
	//     against EMPTY plotAttrs and empty vicinity sets -- every verdict silently wrong, with nothing to
	//     re-derive it later because a read is a bare fetch and no self-heal exists;
	//   - the MODIFIER's drain rebuilds every package the reseed marked, and a package rebuild evaluates its
	//     conditions against the same stores, so it goes LAST.
	// Contexts -> enabler -> modifier. Anything reading a context store registers AFTER the contexts.
	contextRegisterConsumer();
	// ⚖ Inside the CONTEXTS band, and that is a contract rather than a placement: a context DICTIONARY is its own
	// consumer with its own declared interest set ([DEC-dict-is-a-consumer]), so this file gains one line per
	// dictionary as each converts -- but every one of them lands HERE, ahead of the enabler, because the enabler's
	// load-end gate pass evaluates through these stores. Order is a property of the band, never of which
	// translation unit happened to initialize first.
	amenityContextRegisterConsumer();
	policyContextRegisterConsumer();
	enablerRegisterConsumer();
	modifierRegisterConsumer();
	// The TRIGGER machine (json.md §5: a grant is a trigger with a null condition) registers LAST, and that is the
	// same contract one line up rather than a separate rule: it READS both of the state-building machines' output --
	// the per-scope CONTEXTS (every entry condition evaluates through getCityContext().fillEvalCtx /
	// getEmpireContext().fillEvalCtx) and the ENABLER's operating-building set (a DORMANT building grants nothing).
	// Registered ahead of them it would evaluate a trigger against stores that have not yet seen the very fact that
	// fired it -- and since the trigger APPLIES (places buildings, spawns units, promotes), a stale read is a wrong
	// grant handed out, not merely a wrong number, with nothing to re-derive it afterwards.
	triggerRegisterConsumer();
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

// ===== the DOMAIN emit ENDPOINTS. ONE per happening; the CALLER picks the endpoint that names what it just
// did, so no endpoint takes a direction argument and no payload carries a sign ([DEC-facts-name-happenings]).
// A SLOT REPLACEMENT calls REMOVED then ADDED -- emit() is synchronous, so that ordering is what makes the
// withdrawal resolve against the state it was computed against ([state-repositories.md] THE INVARIANT). =====

// iA = bFirst -- whether the first-build payload is OWED, never how the building arrived.
void emitCityBuildingAdded(int iCity, int iOwner, int iBuilding, bool bFirst)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_BUILDING_ADDED, iBuilding, bFirst ? 1 : 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_HAS, bFirst ? 1 : 0);
	eventSpine().emit(e);
}

void emitCityBuildingRemoved(int iCity, int iOwner, int iBuilding)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_BUILDING_REMOVED, iBuilding, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

// DIAGNOSTIC -- says WHAT CODE DID, never what the state IS. No consumer may build state from it.
void emitCityBuildingProcessed(int iCity, int iOwner, int iBuilding, int iCount)
{
	CvSpineEvent e(EVENTKIND_DIAGNOSTIC, SEVT_CITY_BUILDING_PROCESSED, iBuilding, iCount, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitCityBuildingActivated(int iCity, int iOwner, int iBuilding)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_BUILDING_ACTIVATED, iBuilding, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityBuildingDormanted(int iCity, int iOwner, int iBuilding)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_BUILDING_DORMANTED, iBuilding, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityBuildingObsoletedAdded(int iCity, int iOwner, int iBuilding)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_BUILDING_OBSOLETED_ADDED, iBuilding, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityBuildingObsoletedRemoved(int iCity, int iOwner, int iBuilding)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_BUILDING_OBSOLETED_REMOVED, iBuilding, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityReligionAdded(int iCity, int iOwner, int iReligion)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_RELIGION_ADDED, iReligion, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityReligionRemoved(int iCity, int iOwner, int iReligion)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_RELIGION_REMOVED, iReligion, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityCorporationAdded(int iCity, int iOwner, int iCorporation)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_CORPORATION_ADDED, iCorporation, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CORPORATION, iCorporation).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityCorporationRemoved(int iCity, int iOwner, int iCorporation)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_CORPORATION_REMOVED, iCorporation, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CORPORATION, iCorporation).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

// The NETWORK supply PRESENCE CROSSING (0 <-> non-zero), not a count move.
void emitCityBonusAdded(int iCity, int iOwner, int iBonus)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_BONUS_ADDED, iBonus, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityBonusRemoved(int iCity, int iOwner, int iBonus)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_BONUS_REMOVED, iBonus, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

// iCount = HOW MANY, unsigned -- a city can hold several of a bonus locally.
void emitCityVicinityBonusAdded(int iCity, int iOwner, int iBonus, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_VICINITY_BONUS_ADDED, iBonus, iCount, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitCityVicinityBonusRemoved(int iCity, int iOwner, int iBonus, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_VICINITY_BONUS_REMOVED, iBonus, iCount, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

// iCount = HOW MANY population MOVED, never the new total. The save read calls this with the stored amount.
void emitCityPopulationAdded(int iCity, int iOwner, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_POPULATION_ADDED, -1, iCount, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitCityPopulationRemoved(int iCity, int iOwner, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_POPULATION_REMOVED, -1, iCount, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitCitySpecialistAdded(int iCity, int iOwner, int iSpecialist, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_SPECIALIST_ADDED, iSpecialist, iCount, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_SPECIALIST, iSpecialist).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitCitySpecialistRemoved(int iCity, int iOwner, int iSpecialist, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_SPECIALIST_REMOVED, iSpecialist, iCount, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_SPECIALIST, iSpecialist).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitCityPowerAdded(int iCity, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_POWER_ADDED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityPowerRemoved(int iCity, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_POWER_REMOVED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

// The derived 0-CROSSING only -- the timer ticks down every turn and a per-decrement emit would announce
// a fact that did not change. The general rule for every timer-backed fact.
void emitCityPowerDisabledAdded(int iCity, int iOwner, int iTimer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_POWER_DISABLED_ADDED, -1, 0, iTimer, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_TIMER, iTimer);
	eventSpine().emit(e);
}

void emitCityPowerDisabledRemoved(int iCity, int iOwner, int iTimer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_POWER_DISABLED_REMOVED, -1, 0, iTimer, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_TIMER, iTimer);
	eventSpine().emit(e);
}

void emitCityFreshWaterAdded(int iCity, int iOwner, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_FRESH_WATER_ADDED, -1, 0, iCount, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitCityFreshWaterRemoved(int iCity, int iOwner, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_FRESH_WATER_REMOVED, -1, 0, iCount, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitCityGovernmentCenterAdded(int iCity, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_GOVERNMENT_CENTER_ADDED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityGovernmentCenterRemoved(int iCity, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_GOVERNMENT_CENTER_REMOVED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityHolyCityAdded(int iCity, int iOwner, int iReligion)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_HOLY_CITY_ADDED, iReligion, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityHolyCityRemoved(int iCity, int iOwner, int iReligion)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_HOLY_CITY_REMOVED, iReligion, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityHeadquartersAdded(int iCity, int iOwner, int iCorporation)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_HEADQUARTERS_ADDED, iCorporation, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CORPORATION, iCorporation).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityHeadquartersRemoved(int iCity, int iOwner, int iCorporation)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_HEADQUARTERS_REMOVED, iCorporation, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CORPORATION, iCorporation).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityCultureLevelAdded(int iCity, int iOwner, int iLevel)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_CULTURE_LEVEL_ADDED, -1, iLevel, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_VALUE, iLevel);
	eventSpine().emit(e);
}

void emitCityCultureLevelRemoved(int iCity, int iOwner, int iLevel)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_CULTURE_LEVEL_REMOVED, -1, iLevel, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_VALUE, iLevel);
	eventSpine().emit(e);
}

void emitCityOwnerAdded(int iCity, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_OWNER_ADDED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityOwnerRemoved(int iCity, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_OWNER_REMOVED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitCityNetworkAdded(int iOwner, int iCity, int iPlotGroup)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_NETWORK_ADDED, -1, iPlotGroup, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_ID, iPlotGroup);
	eventSpine().emit(e);
}

void emitCityNetworkRemoved(int iOwner, int iCity, int iPlotGroup)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_NETWORK_REMOVED, -1, iPlotGroup, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_ID, iPlotGroup);
	eventSpine().emit(e);
}

void emitCityOrderAdded(int iCity, int iOwner, int iOrderType, int iItem)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_ORDER_ADDED, iItem, iOrderType, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_ID, iItem).addI(SPF_VALUE, iOrderType);
	eventSpine().emit(e);
}

void emitCityOrderRemoved(int iCity, int iOwner, int iOrderType, int iItem)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CITY_ORDER_REMOVED, iItem, iOrderType, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity).addI(SPF_ID, iItem).addI(SPF_VALUE, iOrderType);
	eventSpine().emit(e);
}

void emitPlotTerrainAdded(int iPlot, int iOwner, int iTerrain)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_TERRAIN_ADDED, iTerrain, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TERRAIN, iTerrain).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotTerrainRemoved(int iPlot, int iOwner, int iTerrain)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_TERRAIN_REMOVED, iTerrain, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TERRAIN, iTerrain).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotFeatureAdded(int iPlot, int iOwner, int iFeature)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_FEATURE_ADDED, iFeature, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_FEATURE, iFeature).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotFeatureRemoved(int iPlot, int iOwner, int iFeature)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_FEATURE_REMOVED, iFeature, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_FEATURE, iFeature).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotImprovementAdded(int iPlot, int iOwner, int iImprovement)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_IMPROVEMENT_ADDED, iImprovement, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_IMPROVEMENT, iImprovement).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotImprovementRemoved(int iPlot, int iOwner, int iImprovement)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_IMPROVEMENT_REMOVED, iImprovement, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_IMPROVEMENT, iImprovement).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotRouteAdded(int iPlot, int iOwner, int iRoute)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_ROUTE_ADDED, iRoute, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_ROUTE, iRoute).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotRouteRemoved(int iPlot, int iOwner, int iRoute)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_ROUTE_REMOVED, iRoute, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_ROUTE, iRoute).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotBonusAdded(int iPlot, int iOwner, int iBonus)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_BONUS_ADDED, iBonus, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotBonusRemoved(int iPlot, int iOwner, int iBonus)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_BONUS_REMOVED, iBonus, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotTypeAdded(int iPlot, int iOwner, int iPlotType)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_TYPE_ADDED, iPlotType, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_VALUE, iPlotType);
	eventSpine().emit(e);
}

void emitPlotTypeRemoved(int iPlot, int iOwner, int iPlotType)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_TYPE_REMOVED, iPlotType, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_VALUE, iPlotType);
	eventSpine().emit(e);
}

void emitPlotLandmarkAdded(int iPlot, int iOwner, int iLandmark)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_LANDMARK_ADDED, iLandmark, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_VALUE, iLandmark);
	eventSpine().emit(e);
}

void emitPlotLandmarkRemoved(int iPlot, int iOwner, int iLandmark)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_LANDMARK_REMOVED, iLandmark, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_VALUE, iLandmark);
	eventSpine().emit(e);
}

void emitPlotRiverAdded(int iPlot, int iOwner, int iCrossingCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_RIVER_ADDED, -1, 0, iCrossingCount, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_COUNT, iCrossingCount);
	eventSpine().emit(e);
}

void emitPlotRiverRemoved(int iPlot, int iOwner, int iCrossingCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_RIVER_REMOVED, -1, 0, iCrossingCount, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_COUNT, iCrossingCount);
	eventSpine().emit(e);
}

void emitPlotIrrigationAdded(int iPlot, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_IRRIGATION_ADDED, -1, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotIrrigationRemoved(int iPlot, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_IRRIGATION_REMOVED, -1, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotOwnerAdded(int iPlot, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_OWNER_ADDED, -1, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotOwnerRemoved(int iPlot, int iOwner)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_OWNER_REMOVED, -1, 0, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitPlotWorkingCityAdded(int iPlot, int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_WORKING_CITY_ADDED, -1, iCity, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitPlotWorkingCityRemoved(int iPlot, int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_WORKING_CITY_REMOVED, -1, iCity, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitPlotWorkedAdded(int iPlot, int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_WORKED_ADDED, -1, 0, iCity, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitPlotWorkedRemoved(int iPlot, int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_WORKED_REMOVED, -1, 0, iCity, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitPlotCityAdded(int iPlot, int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_CITY_ADDED, -1, iCity, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitPlotCityRemoved(int iPlot, int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOT_CITY_REMOVED, -1, iCity, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitEmpireTechAdded(int iPlayer, int iTech)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_TECH_ADDED, iTech, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TECH, iTech).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireTechRemoved(int iPlayer, int iTech)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_TECH_REMOVED, iTech, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TECH, iTech).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireTraitAdded(int iPlayer, int iTrait)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_TRAIT_ADDED, iTrait, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TRAIT, iTrait).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireTraitRemoved(int iPlayer, int iTrait)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_TRAIT_REMOVED, iTrait, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TRAIT, iTrait).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireProjectAdded(int iPlayer, int iProject, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_PROJECT_ADDED, iProject, iCount, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_PROJECT, iProject).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitEmpireProjectRemoved(int iPlayer, int iProject, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_PROJECT_REMOVED, iProject, iCount, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_PROJECT, iProject).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitEmpireHeritageAdded(int iPlayer, int iHeritage)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_HERITAGE_ADDED, iHeritage, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_HERITAGE, iHeritage).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireHeritageRemoved(int iPlayer, int iHeritage)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_HERITAGE_REMOVED, iHeritage, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_HERITAGE, iHeritage).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireStateReligionAdded(int iPlayer, int iReligion)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_STATE_RELIGION_ADDED, iReligion, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireStateReligionRemoved(int iPlayer, int iReligion)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_STATE_RELIGION_REMOVED, iReligion, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_RELIGION, iReligion).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireGoldenAgeAdded(int iPlayer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_GOLDEN_AGE_ADDED, -1, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireGoldenAgeRemoved(int iPlayer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_GOLDEN_AGE_REMOVED, -1, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireAnarchyAdded(int iPlayer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_ANARCHY_ADDED, -1, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireAnarchyRemoved(int iPlayer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_ANARCHY_REMOVED, -1, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireEraAdded(int iPlayer, int iEra)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_ERA_ADDED, iEra, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_ERA, iEra).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireEraRemoved(int iPlayer, int iEra)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_ERA_REMOVED, iEra, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_ERA, iEra).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireHandicapAdded(int iPlayer, int iHandicap)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_HANDICAP_ADDED, iHandicap, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_HANDICAP, iHandicap).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireHandicapRemoved(int iPlayer, int iHandicap)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_HANDICAP_REMOVED, iHandicap, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_HANDICAP, iHandicap).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireNukesEnabledAdded(int iPlayer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_NUKES_ENABLED_ADDED, -1, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitEmpireNukesEnabledRemoved(int iPlayer)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_NUKES_ENABLED_REMOVED, -1, 0, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

// iPoints = HOW MANY percent points moved, unsigned. ONE slider move emits SEVERAL of these -- the setter
// rebalances the other channels in place, and each channel it moves is its own state change.
void emitEmpireCommercePercentAdded(int iPlayer, int iCommerce, int iPoints)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_COMMERCE_PERCENT_ADDED, iCommerce, iPoints, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_COMMERCE, iCommerce).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iPoints);
	eventSpine().emit(e);
}

void emitEmpireCommercePercentRemoved(int iPlayer, int iCommerce, int iPoints)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_COMMERCE_PERCENT_REMOVED, iCommerce, iPoints, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_COMMERCE, iCommerce).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iPoints);
	eventSpine().emit(e);
}

void emitEmpireCapitalAdded(int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_CAPITAL_ADDED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitEmpireCapitalRemoved(int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_CAPITAL_REMOVED, -1, 0, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
	eventSpine().emit(e);
}

void emitEmpireBuildingCountAdded(int iPlayer, int iBuilding, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_BUILDING_COUNT_ADDED, iBuilding, iCount, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitEmpireBuildingCountRemoved(int iPlayer, int iBuilding, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_BUILDING_COUNT_REMOVED, iBuilding, iCount, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BUILDING, iBuilding).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitEmpireUnitCountAdded(int iPlayer, int iUnit, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_UNIT_COUNT_ADDED, iUnit, iCount, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnit).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitEmpireUnitCountRemoved(int iPlayer, int iUnit, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_EMPIRE_UNIT_COUNT_REMOVED, iUnit, iCount, 0, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnit).addI(SPF_OWNER, iPlayer).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitPlotGroupBonusAdded(int iOwner, int iPlotGroupId, int iBonus, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOTGROUP_BONUS_ADDED, iBonus, iCount, 0, iOwner, iPlotGroupId);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_ID, iPlotGroupId).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitPlotGroupBonusRemoved(int iOwner, int iPlotGroupId, int iBonus, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PLOTGROUP_BONUS_REMOVED, iBonus, iCount, 0, iOwner, iPlotGroupId);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_BONUS, iBonus).addI(SPF_OWNER, iOwner).addI(SPF_ID, iPlotGroupId).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitAreaTileAdded(int iArea, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_AREA_TILE_ADDED, -1, iCount, 0, -1, iArea);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_AREA, iArea).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitAreaTileRemoved(int iArea, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_AREA_TILE_REMOVED, -1, iCount, 0, -1, iArea);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_AREA, iArea).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitAreaCleanPowerAdded(int iArea, int iTeam)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_AREA_CLEAN_POWER_ADDED, -1, 0, iTeam, -1, iArea);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_AREA, iArea).addI(SPF_TEAM, iTeam);
	eventSpine().emit(e);
}

void emitAreaCleanPowerRemoved(int iArea, int iTeam)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_AREA_CLEAN_POWER_REMOVED, -1, 0, iTeam, -1, iArea);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_AREA, iArea).addI(SPF_TEAM, iTeam);
	eventSpine().emit(e);
}

void emitTeamMemberAdded(int iTeam, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_TEAM_MEMBER_ADDED, -1, iCount, 0, -1, iTeam);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TEAM, iTeam).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitTeamMemberRemoved(int iTeam, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_TEAM_MEMBER_REMOVED, -1, iCount, 0, -1, iTeam);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_TEAM, iTeam).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitUnitDeathScheduleAdded(int iUnitType, int iUnitId, int iOwner, int iPlot)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_DEATH_SCHEDULE_ADDED, iUnitType, iUnitId, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnitType).addI(SPF_UNIT_ID, iUnitId).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitUnitDeathScheduleRemoved(int iUnitType, int iUnitId, int iOwner, int iPlot)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_DEATH_SCHEDULE_REMOVED, iUnitType, iUnitId, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnitType).addI(SPF_UNIT_ID, iUnitId).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitUnitPromotionAdded(int iUnitId, int iOwner, int iPromotion)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_PROMOTION_ADDED, iPromotion, iUnitId, 0, iOwner, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_PROMOTION, iPromotion).addI(SPF_UNIT_ID, iUnitId).addI(SPF_OWNER, iOwner);
	eventSpine().emit(e);
}

void emitUnitPromotionRemoved(int iUnitId, int iOwner, int iPromotion)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_PROMOTION_REMOVED, iPromotion, iUnitId, 0, iOwner, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_PROMOTION, iPromotion).addI(SPF_UNIT_ID, iUnitId).addI(SPF_OWNER, iOwner);
	eventSpine().emit(e);
}

void emitUnitCombatAdded(int iUnitId, int iOwner, int iUnitCombat)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_COMBAT_ADDED, iUnitCombat, iUnitId, 0, iOwner, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNITCOMBAT, iUnitCombat).addI(SPF_UNIT_ID, iUnitId).addI(SPF_OWNER, iOwner);
	eventSpine().emit(e);
}

void emitUnitCombatRemoved(int iUnitId, int iOwner, int iUnitCombat)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_COMBAT_REMOVED, iUnitCombat, iUnitId, 0, iOwner, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNITCOMBAT, iUnitCombat).addI(SPF_UNIT_ID, iUnitId).addI(SPF_OWNER, iOwner);
	eventSpine().emit(e);
}

void emitWorldNukesBannedAdded()
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_WORLD_NUKES_BANNED_ADDED, -1, 0, 0, -1, -1);
	e.iDomainTag = SD_SPINE;
	eventSpine().emit(e);
}

void emitWorldNukesBannedRemoved()
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_WORLD_NUKES_BANNED_REMOVED, -1, 0, 0, -1, -1);
	e.iDomainTag = SD_SPINE;
	eventSpine().emit(e);
}

// MONOTONIC -- the counter only ever grows, so there is no REMOVED half.
void emitWorldUnitCreatedCountAdded(int iUnitType, int iCount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_WORLD_UNIT_CREATED_COUNT_ADDED, iUnitType, iCount, 0, -1, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnitType).addI(SPF_COUNT, iCount);
	eventSpine().emit(e);
}

void emitGameOptionAdded(int iOption, int iValue, int eSpace)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_GAME_OPTION_ADDED, iOption, iValue, eSpace, -1, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OPTION, iOption).addI(SPF_VALUE, iValue).addI(SPF_OPTION_SPACE, eSpace);
	eventSpine().emit(e);
}

void emitGameOptionRemoved(int iOption, int iValue, int eSpace)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_GAME_OPTION_REMOVED, iOption, iValue, eSpace, -1, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_OPTION, iOption).addI(SPF_VALUE, iValue).addI(SPF_OPTION_SPACE, eSpace);
	eventSpine().emit(e);
}

void emitGameHandicapAdded(int iHandicap)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_GAME_HANDICAP_ADDED, iHandicap, 0, 0, -1, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_HANDICAP, iHandicap);
	eventSpine().emit(e);
}

void emitGameHandicapRemoved(int iHandicap)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_GAME_HANDICAP_REMOVED, iHandicap, 0, 0, -1, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_HANDICAP, iHandicap);
	eventSpine().emit(e);
}

// iAmount = HOW MUCH the value moved, unsigned. Emitted at the CvProperties sites and NEVER in
// CvGameObject::eventPropertyChanged (CvGameObjectUnit overrides it without chaining to the base).
void emitPropertyAdded(int iObjectKind, int iObjectId, int iOwner, int iProperty, int iAmount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PROPERTY_ADDED, iProperty, iAmount, iObjectKind, iOwner, iObjectId);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_PROPERTY, iProperty).addI(SPF_COUNT, iAmount).addI(SPF_OBJECT_KIND, iObjectKind).addI(SPF_OWNER, iOwner).addI(SPF_ID, iObjectId);
	eventSpine().emit(e);
}

void emitPropertyRemoved(int iObjectKind, int iObjectId, int iOwner, int iProperty, int iAmount)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_PROPERTY_REMOVED, iProperty, iAmount, iObjectKind, iOwner, iObjectId);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_PROPERTY, iProperty).addI(SPF_COUNT, iAmount).addI(SPF_OBJECT_KIND, iObjectKind).addI(SPF_OWNER, iOwner).addI(SPF_ID, iObjectId);
	eventSpine().emit(e);
}

void emitGameGlobalDefineAdded(const char* szName, int eKind, int iValue, float fValue, const char* szValue)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_GAME_GLOBAL_DEFINE_ADDED, -1, iValue, eKind, -1, -1);
	e.iDomainTag = SD_SPINE;
	// The NAME is the define's whole identity (it has no id space), so it always rides; the value goes in
	// whichever field its KIND can actually carry.
	e.addStr(SPF_DEFINE, szName).addI(SPF_OPTION_SPACE, eKind);
	if (eKind == GLOBALDEFINE_FLOAT)
	{
		e.addF(SPF_VALUE_F, fValue);
	}
	else if (eKind == GLOBALDEFINE_STRING)
	{
		e.addStr(SPF_VALUE_STR, szValue != NULL ? szValue : "");
	}
	else
	{
		e.addI(SPF_VALUE, iValue);
	}
	eventSpine().emit(e);
}

void emitGameGlobalDefineRemoved(const char* szName, int eKind, int iValue, float fValue, const char* szValue)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_GAME_GLOBAL_DEFINE_REMOVED, -1, iValue, eKind, -1, -1);
	e.iDomainTag = SD_SPINE;
	// The NAME is the define's whole identity (it has no id space), so it always rides; the value goes in
	// whichever field its KIND can actually carry.
	e.addStr(SPF_DEFINE, szName).addI(SPF_OPTION_SPACE, eKind);
	if (eKind == GLOBALDEFINE_FLOAT)
	{
		e.addF(SPF_VALUE_F, fValue);
	}
	else if (eKind == GLOBALDEFINE_STRING)
	{
		e.addStr(SPF_VALUE_STR, szValue != NULL ? szValue : "");
	}
	else
	{
		e.addI(SPF_VALUE, iValue);
	}
	eventSpine().emit(e);
}

void emitCivicAdopted(int iPlayer, int iCivic, int iOldCivic)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_CIVIC_ADOPTED, iCivic, 0, iOldCivic, iPlayer, -1);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_CIVIC, iCivic).addI(SPF_OWNER, iPlayer);
	eventSpine().emit(e);
}

void emitUnitKilled(int iUnitType, int iUnitId, int iOwner, int iPlot)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_KILLED, iUnitType, iUnitId, 0, iOwner, iPlot);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnitType).addI(SPF_UNIT_ID, iUnitId).addI(SPF_OWNER, iOwner).addI(SPF_PLOT, iPlot);
	eventSpine().emit(e);
}

void emitUnitLeftCity(int iUnitType, int iUnitId, int iOwner, int iCity)
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_UNIT_LEFT_CITY, iUnitType, iUnitId, 0, iOwner, iCity);
	e.iDomainTag = SD_SPINE;
	e.addI(SPF_UNIT, iUnitType).addI(SPF_UNIT_ID, iUnitId).addI(SPF_OWNER, iOwner).addI(SPF_CITY, iCity);
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

// The wholesale area-identity reassignment. No payload and no owner: every area id in the game is replaced at once,
// so the fact cannot be attributed to a source and every holder re-reads.
void emitAreasRecalculated()
{
	CvSpineEvent e(EVENTKIND_DOMAIN, SEVT_AREAS_RECALCULATED, -1, 0, 0, -1, -1);
	e.iDomainTag = SD_SPINE;
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
