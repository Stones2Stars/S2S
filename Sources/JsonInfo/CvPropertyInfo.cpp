//
//	CvPropertyInfo -- see the header. The composed units (grants/modifiers) carry the property's cascade data via
//	the base dispatch; mapFrom below reads the property's TYPED scalars (the AI value-normalization band, targetLevel +
//	its per-era overrides, the sourceDrain flag) that live outside the composed sections.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvPropertyInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdStr / jsonResolveId
#include "CvJsonModifiers.h"      // getModifiers() walk -> the property's own decay/per-pop families
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString (self property id)

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

CvPropertyInfo::CvPropertyInfo()
	: m_iAIWeight(0), m_eAIScaleType(AISCALE_NONE), m_iFontButtonIndex(-1),
	  m_iOperationalRangeMin(0), m_iOperationalRangeMax(0),
	  m_iTargetLevel(0), m_iTrainReluctance(0), m_bSourceDrain(false), m_iChar(0)
{}

void CvPropertyInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // base: text + the composed grants/modifiers section dispatch
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

	// -- the property-engine SOURCE bridge (property-audit.md increment A/B; DEC-data-first / DEC-no-xml-into-game).
	// The KEEP-legacy CvPropertySolver reads m_PropertyManipulators; feed the property's OWN manipulators from the
	// curated JSON: decay (<self>.{city|plot}.percent -> CvPropertySourceDecay toward targetLevel), the per-POPULATION
	// baseline (<self>.city.flat + per:POPULATION -> AttributeConstant), and the spatial diffuse propagators (the
	// `properties.diffuse[]` block). Conditioned entries (`enabled`) DEFER to the increment-4 BoolExpr translator.
	const PropertyTypes eSelf = (PropertyTypes)GC.getInfoTypeForString(getType(), true);
	if (eSelf != NO_PROPERTY)
	{
		const int iTarget = getTargetLevel();
		static const char* const SC[2] = { "city", "plot" };
		static const GameObjectTypes SG[2] = { GAMEOBJECT_CITY, GAMEOBJECT_PLOT };
		for (int si = 0; si < 2; ++si)
		{
			const CvJsonModFamily* f = getModifiers()->find(std::string(getType()) + "." + SC[si]);
			if (f == NULL) continue;
			for (int i = 0; i < f->size(); ++i)
			{
				const CvJsonModEntry* e = f->entries[i];
				if (e->enabled != NULL || e->disabled != NULL) continue;   // conditioned -> increment 4
				if (e->unit == CASC_UNIT_PERCENT)
					m_PropertyManipulators.addDecaySource(eSelf, e->value100 / 100, iTarget, SG[si]);
				else if (e->unit == CASC_UNIT_FLAT && e->hasPer && e->perType == "POPULATION")
					m_PropertyManipulators.addAttributeConstantSource(eSelf, ATTRIBUTE_POPULATION, e->value100 / 100, SG[si]);
				else if (e->unit == CASC_UNIT_FLAT && !e->hasPer)
					m_PropertyManipulators.addConstantSource(eSelf, e->value100 / 100, SG[si]);
			}
		}
		if (const picojson::object* po = jsonChildObj(o, "properties"))
		{
			picojson::object::const_iterator dit = po->find("diffuse");
			if (dit != po->end() && dit->second.is<picojson::array>())
			{
				const picojson::array& arr = dit->second.get<picojson::array>();
				for (size_t i = 0; i < arr.size(); ++i)
				{
					if (!arr[i].is<picojson::object>()) continue;
					const picojson::object& dd = arr[i].get<picojson::object>();
					if (dd.find("enabled") != dd.end()) continue;   // conditioned diffuse -> increment 4
					std::string from, to, rel;
					jsonIdStr(dd, "from", from); jsonIdStr(dd, "to", to); jsonIdStr(dd, "relation", rel);
					const GameObjectTypes eFrom = (from == "plot" || from == "plots") ? GAMEOBJECT_PLOT : GAMEOBJECT_CITY;
					const GameObjectTypes eTo   = (to == "plot"   || to == "plots")   ? GAMEOBJECT_PLOT : GAMEOBJECT_CITY;
					const RelationTypes eRel = (rel == "samePlot") ? RELATION_SAME_PLOT : RELATION_NEAR;
					m_PropertyManipulators.addDiffusePropagator(eSelf, jsonIdInt(dd, "percent"), eFrom, eTo, eRel, jsonIdInt(dd, "distance"));
				}
			}
		}
	}
}
