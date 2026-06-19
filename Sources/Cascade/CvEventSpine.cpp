//
//	CvEventSpine -- out-of-line parts of the #430 cascade front door + its first (logging) consumer.
//	See CvEventSpine.h for the architecture (KIND firewall, raw payloads, consumer appetites).
//

#include "CvGameCoreDLL.h"
#include "CvEventSpine.h"
#include "AI/BetterBTSAI.h"   // gPlayerLogLevel (reused as the slice-1 gate; dedicated gate/BUG option + the live
                           // CvHttpServer feed come next)
#include "Defines/CvGlobals.h"        // GC -- resolve raw Type indices to readable names in the (gated) consumer
#include "CvBuildingInfo.h"
#include "CvUnitInfo.h"
// typeIndex name-resolution in the consumer: the Info headers for each SFT_ kind (so GC.getXInfo(i).getType() compiles).
// Imported DIRECTLY (no CvInfos.h umbrella -- owner 2026-06-18: that umbrella should be retired, import directly).
#include "CvBonusInfo.h"
#include "CvImprovementInfo.h"
#include "Infos/CvPromotionInfo.h"
#include "Infos/CvReligionInfo.h"
#include "Infos/CvCorporationInfo.h"
#include "Infos/CvFeatureInfo.h"
#include "Infos/CvTerrainInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvProjectInfo.h"
#include "Infos/CvSpecialistInfo.h"
#include "CvCascadeTally.h"   // register + seed the tally (the first selective DOMAIN consumer)

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

void CvEventSpine::emit(const CvCascadeEvent& kEvent)
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

static const char* cascadeKindName(EventKind eKind)
{
	switch (eKind)
	{
	case EVENTKIND_DOMAIN:     return "DOMAIN";
	case EVENTKIND_DIAGNOSTIC: return "DIAGNOSTIC";
	case EVENTKIND_TRACE:      return "TRACE";
	default:                   return "?";
	}
}

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

void cascadeRenderEventLine(char* szBuf, int iBufSize, const CvCascadeEvent& kEvent)
{
	char szPre[48];
	int n = _snprintf(szBuf, iBufSize, "%s", spineLinePrefix(kEvent.iDomainTag, kEvent.iEventId, szPre, sizeof(szPre)));
	if (n < 0 || n >= iBufSize) { szBuf[iBufSize - 1] = '\0'; return; }
	// Resolve each field's name + type via the domain's registered field-info resolver (per-domain isolation).
	SpineFieldInfoFn fieldFn = (kEvent.iDomainTag >= 0 && kEvent.iDomainTag < NUM_SPINE_DOMAINS)
		? g_domains[kEvent.iDomainTag].fieldFn : NULL;
	for (int k = 0; k < kEvent.iFieldCount && n < iBufSize - 1; ++k)
	{
		const CvCascadeEventField& fld = kEvent.aFields[k];
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
		case SFT_PLAYER:
			// Instance name + raw id, BOTH (owner 2026-06-19): name=human readability, (id)=stable machine join-key for the
			// primary consumers (AI agents during shadow-verify + GameTracker). Resolved LIVE here, which is exact + safe:
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

class CvCascadeLogConsumer : public IEventConsumer
{
public:
	// BROAD: logging sees every kind (the tally is the SELECTIVE counterpart).
	int wantedKinds() const
	{
		return (1 << EVENTKIND_DOMAIN) | (1 << EVENTKIND_DIAGNOSTIC) | (1 << EVENTKIND_TRACE);
	}

	void onEvent(const CvCascadeEvent& kEvent)
	{
		if (gPlayerLogLevel < 1)
		{
			return; // gate: format + write only when logging is enabled
		}
		// Raw payload -> text HERE (gated), which is the whole point of the raw-payload design: resolve known
		// data-Type indices to readable names so a reader sees BUILDING_FORGE, not "type=1613". Unknown/other
		// events (e.g. the temporary self-test, whose iType is a playerId) fall back to the raw fields.
		char szBuf[512];
		// RAW-FIELD logging event (DIAGNOSTIC/TRACE with fields): render via the generic field formatter -> the
		// migrated [TAG] key=value line. This is the consolidation target path (event-spine-spec section 3). Gated by
		// the event's own surveillance LEVEL (1 Telescreen .. 4 Thought Police), so per-line levels are respected.
		if (kEvent.iFieldCount > 0)
		{
			if (gPlayerLogLevel < kEvent.iLevel) return;
			cascadeRenderEventLine(szBuf, sizeof(szBuf), kEvent);
			gDLL->logMsg(spineDomainFile(kEvent.iDomainTag), szBuf); // R-2: per-domain file ([HAI] -> HunterAI.log)
			streamLogTee(1, szBuf);
			return;
		}
		if (kEvent.eKind == EVENTKIND_DOMAIN && kEvent.iEventId == CASCADE_EVT_BUILDING_COUNT
			&& kEvent.iType >= 0 && kEvent.iType < GC.getNumBuildingInfos())
		{
			sprintf(szBuf, "[SPINE/DOMAIN] buildingCount %s player=%d count=%d delta=%+d",
				GC.getBuildingInfo((BuildingTypes)kEvent.iType).getType(), kEvent.iC, kEvent.iA, kEvent.iB);
		}
		else if (kEvent.eKind == EVENTKIND_DOMAIN && kEvent.iEventId == CASCADE_EVT_UNIT_COUNT
			&& kEvent.iType >= 0 && kEvent.iType < GC.getNumUnitInfos())
		{
			sprintf(szBuf, "[SPINE/DOMAIN] unitCount %s player=%d count=%d delta=%+d",
				GC.getUnitInfo((UnitTypes)kEvent.iType).getType(), kEvent.iC, kEvent.iA, kEvent.iB);
		}
		else if (kEvent.eKind == EVENTKIND_DOMAIN && kEvent.iEventId == CASCADE_EVT_NAME_CHANGE)
		{
			// iType = NameChangeKind, iA = owner player, iB = entity id. Resolve the NEW name LIVE (synchronous on the game
			// thread -> exact + off-thread-safe). CvWString holds it so getName()-by-value can't dangle through the sprintf.
			const PlayerTypes eOwner = (PlayerTypes)kEvent.iA;
			const char* szKind = "?";
			CvWString szName = L"?";
			if (eOwner >= 0 && eOwner < MAX_PLAYERS)
			{
				const CvPlayer& kP = GET_PLAYER(eOwner);
				switch (kEvent.iType)
				{
				case NAMECHANGE_PLAYER: szKind = "player"; szName = kP.getName(); break;
				case NAMECHANGE_CIV:    szKind = "civ";    szName = kP.getCivilizationShortDescription(); break;
				case NAMECHANGE_CITY:   szKind = "city";   { const CvCity* pCity = kP.getCity(kEvent.iB); if (pCity != NULL) szName = pCity->getName(); } break;
				case NAMECHANGE_UNIT:   szKind = "unit";   { const CvUnit* pUnit = kP.getUnit(kEvent.iB); if (pUnit != NULL) szName = pUnit->getName(); } break;
				}
			}
			sprintf(szBuf, "[SPINE/DOMAIN] nameChange kind=%s player=%d id=%d name=%S", szKind, kEvent.iA, kEvent.iB, szName.GetCString());
		}
		else
		{
			sprintf(szBuf, "[SPINE/%s] eventId=%d type=%d a=%d b=%d c=%d",
				cascadeKindName(kEvent.eKind), kEvent.iEventId, kEvent.iType, kEvent.iA, kEvent.iB, kEvent.iC);
		}
		gDLL->logMsg("Cascade.log", szBuf);
		// Tee onto the live /events SSE stream (#419) so spine events are observable out-of-process (curl /events)
		// without holding the .log file open. Shared tee with the BBAI log helpers; gated by gStreamLogLevel.
		streamLogTee(1, szBuf);
	}
};

static CvCascadeLogConsumer s_cascadeLogConsumer;

void cascadeRegisterConsumers()
{
	static bool s_bRegistered = false;
	if (s_bRegistered)
	{
		return;
	}
	s_bRegistered = true;
	eventSpine().registerConsumer(&s_cascadeLogConsumer);
	// The TALLY: the first SELECTIVE (DOMAIN-only) consumer. Registration is once; the SEED (rebuild from the
	// loaded objects, §9) happens at CvGame::onFinalInitialized on EVERY load/new-game -- NOT here -- so a stale
	// tally can't gate before the first end-of-turn, and a 2nd in-session load reseeds. DOMAIN events maintain it.
	eventSpine().registerConsumer(&cascadeTally());
}

void cascadeEmitNameChange(int iKind, int iOwner, int iEntityId)
{
	// DOMAIN kind: a rename is synced state an observer must track (Orwell bar). The tally IGNORES it (its switch default
	// returns; iC = 0 keeps the player-slot guard happy); the logging consumer renders it, resolving the NEW name live.
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DOMAIN, CASCADE_EVT_NAME_CHANGE, iKind, iOwner, iEntityId, 0));
}
