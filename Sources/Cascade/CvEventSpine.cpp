//
//	CvEventSpine -- out-of-line parts of the #430 cascade front door + its first (logging) consumer.
//	See CvEventSpine.h for the architecture (KIND firewall, raw payloads, consumer appetites).
//

#include "CvGameCoreDLL.h"
#include "CvEventSpine.h"
#include "BetterBTSAI.h"   // gPlayerLogLevel (reused as the slice-1 gate; dedicated gate/BUG option + the live
                           // CvHttpServer feed come next)
#include "CvGlobals.h"        // GC -- resolve raw Type indices to readable names in the (gated) consumer
#include "CvBuildingInfo.h"
#include "CvUnitInfo.h"
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
		char szBuf[256];
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
