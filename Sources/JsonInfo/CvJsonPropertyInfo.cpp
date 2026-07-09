//
//	CvJsonPropertyInfo -- see the header. The composed units (grants/modifiers) carry the property's cascade data via
//	the base dispatch; mapFrom below reads the property's TYPED scalars (the AI value-normalization band, targetLevel +
//	its per-era overrides, the sourceDrain flag) that live outside the composed sections.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonPropertyInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdStr / jsonResolveId

// ai.scale -- the curator emits the AIScaleTypes enum name prefix-stripped + lowercased (AISCALE_CITY -> "city";
// "none" is never emitted). AIScaleTypes is a FIXED C-enum (CvEnums.h), NOT a registered info type, so map the tokens
// explicitly rather than via GC.getInfoTypeForString. Unknown/absent -> AISCALE_NONE.
static AIScaleTypes aiScaleFromString(const std::string& s)
{
	if (s == "city")   return AISCALE_CITY;
	if (s == "area")   return AISCALE_AREA;
	if (s == "player") return AISCALE_PLAYER;
	if (s == "team")   return AISCALE_TEAM;
	return AISCALE_NONE;
}

CvJsonPropertyInfo::CvJsonPropertyInfo()
	: m_iAIWeight(0), m_eAIScaleType(AISCALE_NONE), m_iFontButtonIndex(-1),
	  m_iOperationalRangeMin(0), m_iOperationalRangeMax(0),
	  m_iTargetLevel(0), m_iTrainReluctance(0), m_bSourceDrain(false), m_iChar(0)
{}

void CvJsonPropertyInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // base: text + the composed grants/modifiers section dispatch
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// ai bucket -- AI value-normalization scalars, read AS-IS (curator did NOT x100 these; they are AI-native ints)
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
	{
		m_iAIWeight        = jsonIdInt(*ai, "weight");            // ai.weight
		m_iTrainReluctance = jsonIdInt(*ai, "trainReluctance");  // ai.trainReluctance
		std::string szScale;
		if (jsonIdStr(*ai, "scale", szScale)) m_eAIScaleType = aiScaleFromString(szScale);  // ai.scale
		if (const picojson::object* orange = jsonChildObj(*ai, "operationalRange"))
		{
			m_iOperationalRangeMin = jsonIdInt(*orange, "min");  // ai.operationalRange.min
			m_iOperationalRangeMax = jsonIdInt(*orange, "max");  // ai.operationalRange.max
		}
	}

	// targetLevel -- the isolated equilibrium field: a bare number (flat base), OR { base, byEra:{ ERA_x: n } }
	picojson::object::const_iterator tl = o.find("targetLevel");
	if (tl != o.end())
	{
		if (tl->second.is<double>())
		{
			m_iTargetLevel = (int)tl->second.get<double>();
		}
		else if (tl->second.is<picojson::object>())
		{
			const picojson::object& tlo = tl->second.get<picojson::object>();
			m_iTargetLevel = jsonIdInt(tlo, "base");
			if (const picojson::object* byEra = jsonChildObj(tlo, "byEra"))
			{
				for (picojson::object::const_iterator it = byEra->begin(); it != byEra->end(); ++it)
				{
					if (!it->second.is<double>()) continue;
					const int iEra = jsonResolveId(it->first);   // ERA_* -> era id (unresolved surfaces via jsonResolveId)
					if (iEra >= 0) m_aTargetLevelbyEraTypes[iEra] = (int)it->second.get<double>();
				}
			}
		}
	}

	// identity: the property-system behaviour flag + the UI font-button index
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_bSourceDrain = jsonIdBool(*io, "sourceDrain");                          // identity.sourceDrain
		if (io->find("fontButtonIndex") != io->end())
			m_iFontButtonIndex = jsonIdInt(*io, "fontButtonIndex");               // identity.fontButtonIndex (keep -1 when absent)
	}

	// text.prereqMin / text.prereqMax -- the prereq display TXT_KEYs. The curator nests these under `text` (NOT
	// identity): fold_text_to_identity folds only the top-level TEXT_KEYS set (description/adjective/...), and these
	// live in the nested text block, so they stay there (verified vs shipped property JSON + curate_common.py).
	if (const picojson::object* txt = jsonChildObj(o, "text"))
	{
		std::string szMin;
		if (jsonIdStr(*txt, "prereqMin", szMin) && !szMin.empty()) m_szPrereqMinDisplayText = CvWString(szMin.c_str());
		std::string szMax;
		if (jsonIdStr(*txt, "prereqMax", szMax) && !szMax.empty()) m_szPrereqMaxDisplayText = CvWString(szMax.c_str());
	}
}
