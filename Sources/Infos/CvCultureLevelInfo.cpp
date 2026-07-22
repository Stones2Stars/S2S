//
//	CvCultureLevelInfo::mapFrom -- base (availability: the entity-level `enabled`/`disabled` gate + the
//	replacedBy edge + the `allowed` wonder caps, all composed units the base dispatch fills), then the tier's city
//	defense + radius + culture threshold. HUMAN-native. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvCultureLevelInfo.h"
#include "CvJsonParse.h"          // the shared walkers (jsonChildObj/jsonFamMemberVal/jsonIdInt)
#include "Infos/CvGameSpeedInfo.h" // getSpeedPercent -- the per-speed culture-threshold multiplier

// The legacy scalar PrereqGameOption is now the entity-level `enabled` gate (DEC-entity-gate): the curator emits a
// bare `"enabled": "GAMEOPTION_CULTURE_REALISTIC_SPREAD"`, which parses to a PRESENCE atom on m_gate.enabled. The live
// consumer (CvGlobals::cacheGameSpecificValues) still asks for the single option id, so extract it here. A stub
// NO_GAMEOPTION made the 12 gated tiers apply unconditionally regardless of the option.
int CvCultureLevelInfo::getPrereqGameOption() const
{
	const CvJsonCondition* e = m_gate.enabled;
	if (e != NULL && e->kind == CASC_COND_PRESENCE && e->type.compare(0, 11, "GAMEOPTION_") == 0)
		return GC.getInfoTypeForString(e->type.c_str(), true);
	return NO_GAMEOPTION;
}

// The per-GameSpeed culture threshold = base(Normal) × GameSpeed.speedPercent / 100 (the legacy SpeedThresholds
// table was this exact precompute; curator COLLAPSE kept only the base). The multiplier lives ON the gamespeed
// (owner 2026-07-11) -- re-apply it here, mirroring CvGame::getGoldenAgeLength / getVictoryDelay (× speedPercent).
int CvCultureLevelInfo::getSpeedThreshold(int iSpeed) const
{
	return m_iCultureThreshold * GC.getGameSpeedInfo((GameSpeedTypes)iSpeed).getSpeedPercent() / 100;
}

void CvCultureLevelInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core + availability (the entity-level gate, replacedBy edge, the allowed caps)
	// materialized wonder-category caps (json §4.4; the getters are bare member reads)
	m_iMaxWorldWonders    = wonderCap("worldWonders");
	m_iMaxTeamWonders     = wonderCap("teamWonders");
	m_iMaxNationalWonders = wonderCap("nationalWonders");
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	m_iCityDefenseModifier = jsonFamMemberVal(o, "defense", "city", "amount", "percent");

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iCityRadius = jsonIdInt(*io, "cityRadius", 1);  // legacy load default 1 (archive .add)
		// cultureThreshold: a bare scalar, OR (if a game speed breaks the geometric ratio) a {base, overrides} object --
		// read the base; the per-speed overrides are STUB deferred (the consumer derives per-speed by ×gamespeed%).
		picojson::object::const_iterator ct = io->find("cultureThreshold");
		if (ct != io->end())
		{
			if (ct->second.is<double>()) m_iCultureThreshold = (int)ct->second.get<double>();
			else if (ct->second.is<picojson::object>())
			{
				const picojson::object& cto = ct->second.get<picojson::object>();
				picojson::object::const_iterator b = cto.find("base");
				if (b != cto.end() && b->second.is<double>()) m_iCultureThreshold = (int)b->second.get<double>();
			}
		}
	}
}
