//
//	CvCultureLevelInfo -- the culture-level poco's own typed reading on top of the base section dispatch (see
//	the header). mapFrom materializes the census identity set + the §4.4 wonder caps ONCE
//	(docs/architecture/patterns.md §Materialize at mapFrom); the tier defense is a compiled point read (header), never a mirrored
//	scalar. Idempotent by contract (unconditional assigns).
//

#include "CvGameCoreDLL.h"
#include "CvCultureLevelInfo.h"
#include "CvJsonParse.h"           // jsonChildObj / jsonIdInt
#include "Infos/CvGameSpeedInfo.h" // getSpeedPercent -- the per-speed culture-threshold multiplier

CvCultureLevelInfo::CvCultureLevelInfo()
	: m_iCityRadius(1)
	, m_iCultureThreshold(0)
	, m_iMaxWorldWonders(0)
	, m_iMaxTeamWonders(0)
	, m_iMaxNationalWonders(0)
	, m_iPrereqGameOption(NO_GAMEOPTION)
	, m_iLevel(0)
{
}

// The per-GameSpeed culture threshold = base × the gamespeed's speed percent / 100 (the legacy SpeedThresholds
// table was this exact precompute; curator COLLAPSE kept only the base -- the multiplier lives ON the
// gamespeed). The speed percent is the gamespeed's 1-kind straggler read (speed.world.percent via the base
// getScalar) -- a PERCENT, which is NOT scaled (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)), so it is the human percent and the
// division is the ordinary percent-as-ratio /100. ⛔ A /10000 here reads the percent as ×100 and puts every
// culture threshold two orders low, which clears whole culture levels instantly.
int CvCultureLevelInfo::getSpeedThreshold(int iSpeed) const
{
	const int iSpeedPercent = GC.getGameSpeedInfo((GameSpeedTypes)iSpeed).getScalar(SCALAR_SPEED, CASC_SCOPE_WORLD, CASC_UNIT_PERCENT);
	return m_iCultureThreshold * iSpeedPercent / 100;
}

void CvCultureLevelInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core + the section dispatch (the entity-level gate, replacedBy/enables edges, the allowed caps, m_modifiers)

	// materialized §4.4 wonder-category caps -- legacy 0-for-absent convention (base cap() returns -1 = absent)
	const int iWorldCap = m_allowed.cap(ALLOWEDCAP_WORLD_WONDERS);
	const int iTeamCap = m_allowed.cap(ALLOWEDCAP_TEAM_WONDERS);
	const int iNationalCap = m_allowed.cap(ALLOWEDCAP_NATIONAL_WONDERS);
	m_iMaxWorldWonders = iWorldCap >= 0 ? iWorldCap : 0;
	m_iMaxTeamWonders = iTeamCap >= 0 ? iTeamCap : 0;
	m_iMaxNationalWonders = iNationalCap >= 0 ? iNationalCap : 0;

	// The legacy scalar PrereqGameOption is the entity-level `enabled` gate (docs/specs/json.md §2 (Applicability row) + docs/specs/enabler.md §WHAT THE ENABLER IS NOT): the curator emits
	// a bare `"enabled": "GAMEOPTION_..."`, which parses to a PRESENCE atom on m_gate.enabled (dispatched by
	// CvInfo::mapFrom above). The live consumer (CvGlobals::cacheGameSpecificValues) asks for the single option
	// id -- materialized here ONCE (docs/architecture/patterns.md §Materialize at mapFrom; GameOptionInfos register in LoadPreMenuGlobals
	// before either loadJson pass, so the id resolves at every mapFrom).
	m_iPrereqGameOption = NO_GAMEOPTION;
	const CvCondition* pEnabled = m_gate.enabled;
	if (pEnabled != NULL && pEnabled->kind == CASC_COND_PRESENCE && pEnabled->type.compare(0, 11, "GAMEOPTION_") == 0)
	{
		m_iPrereqGameOption = GC.getInfoTypeForString(pEnabled->type.c_str(), true);
	}

	// idempotency (CvInfo.h): unconditional redefinition of the identity members
	m_iCityRadius = 1;   // legacy load default 1
	m_iCultureThreshold = 0;

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iCityRadius = jsonIdInt(*pIdentity, "cityRadius", 1);
		// cultureThreshold: a bare scalar, OR (if a game speed breaks the geometric ratio) a {base, overrides}
		// object -- read the base; the per-speed value derives by × gamespeed% (getSpeedThreshold).
		picojson::object::const_iterator thresholdIt = pIdentity->find("cultureThreshold");
		if (thresholdIt != pIdentity->end())
		{
			if (thresholdIt->second.is<double>())
			{
				m_iCultureThreshold = (int)thresholdIt->second.get<double>();
			}
			else if (thresholdIt->second.is<picojson::object>())
			{
				const picojson::object& thresholdObj = thresholdIt->second.get<picojson::object>();
				picojson::object::const_iterator baseIt = thresholdObj.find("base");
				if (baseIt != thresholdObj.end() && baseIt->second.is<double>())
				{
					m_iCultureThreshold = (int)baseIt->second.get<double>();
				}
			}
		}
	}
}
