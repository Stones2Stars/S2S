//
//	CvVisionSection -- the shared load-time parse of the json.md par.9 `vision` block (see the header).
//	One walker per authored shape: FK scalar, flat-range object, FK list (the shared jsonReadIdList),
//	{INVISIBLE:intensity} pair-map, and the {invisible, terrain|feature|improvement, intensity} struct-row
//	lists. All reads go through the shared CvJsonParse primitives; unresolved FKs surface via jsonResolveId's
//	diagnostic (the Orwell bar).
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvVisionSection.h"
#include "CvJsonParse.h"     // jsonChildObj / jsonIdInt / jsonIdFk / jsonReadIdList / jsonResolveId

namespace
{
	// vision[key] = [ { invisible, terrain, intensity }, ... ] -> typed rows.
	void vs_readTerrainRows(const picojson::object& vision, const char* szKey, std::vector<InvisibleTerrainChanges>& out)
	{
		picojson::object::const_iterator iter = vision.find(szKey);
		if (iter == vision.end() || !iter->second.is<picojson::array>())
		{
			return;
		}
		const picojson::array& entries = iter->second.get<picojson::array>();
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (!entries[i].is<picojson::object>())
			{
				continue;
			}
			const picojson::object& row = entries[i].get<picojson::object>();
			InvisibleTerrainChanges typedRow;
			typedRow.eInvisible = static_cast<InvisibleTypes>(jsonIdFk(row, "invisible"));
			typedRow.eTerrain = static_cast<TerrainTypes>(jsonIdFk(row, "terrain"));
			typedRow.iIntensity = jsonIdInt(row, "intensity");
			out.push_back(typedRow);
		}
	}

	// vision[key] = [ { invisible, feature, intensity }, ... ] -> typed rows.
	void vs_readFeatureRows(const picojson::object& vision, const char* szKey, std::vector<InvisibleFeatureChanges>& out)
	{
		picojson::object::const_iterator iter = vision.find(szKey);
		if (iter == vision.end() || !iter->second.is<picojson::array>())
		{
			return;
		}
		const picojson::array& entries = iter->second.get<picojson::array>();
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (!entries[i].is<picojson::object>())
			{
				continue;
			}
			const picojson::object& row = entries[i].get<picojson::object>();
			InvisibleFeatureChanges typedRow;
			typedRow.eInvisible = static_cast<InvisibleTypes>(jsonIdFk(row, "invisible"));
			typedRow.eFeature = static_cast<FeatureTypes>(jsonIdFk(row, "feature"));
			typedRow.iIntensity = jsonIdInt(row, "intensity");
			out.push_back(typedRow);
		}
	}

	// vision[key] = [ { invisible, improvement, intensity }, ... ] -> typed rows.
	void vs_readImprovementRows(const picojson::object& vision, const char* szKey, std::vector<InvisibleImprovementChanges>& out)
	{
		picojson::object::const_iterator iter = vision.find(szKey);
		if (iter == vision.end() || !iter->second.is<picojson::array>())
		{
			return;
		}
		const picojson::array& entries = iter->second.get<picojson::array>();
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (!entries[i].is<picojson::object>())
			{
				continue;
			}
			const picojson::object& row = entries[i].get<picojson::object>();
			InvisibleImprovementChanges typedRow;
			typedRow.eInvisible = static_cast<InvisibleTypes>(jsonIdFk(row, "invisible"));
			typedRow.eImprovement = static_cast<ImprovementTypes>(jsonIdFk(row, "improvement"));
			typedRow.iIntensity = jsonIdInt(row, "intensity");
			out.push_back(typedRow);
		}
	}
}

CvVisionSection::CvVisionSection()
	: invisible(-1)
	, range(0)
{
}

void CvVisionSection::clearParsed()
{
	invisible = -1;
	range = 0;
	negates.clear();
	visibilityIntensity.clear();
	invisibilityIntensity.clear();
	visibilityIntensityRange.clear();
	visibilityIntensitySameTile.clear();
	invisibleTerrain.clear();
	invisibleFeature.clear();
	invisibleImprovement.clear();
	visibleTerrain.clear();
	visibleFeature.clear();
	visibleImprovement.clear();
	visibleTerrainRange.clear();
	visibleFeatureRange.clear();
	visibleImprovementRange.clear();
}

void CvVisionSection::parse(const picojson::value& entity)
{
	clearParsed();
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	const picojson::object* pVision = jsonChildObj(entityObj, "vision");
	if (pVision == NULL)
	{
		return;
	}
	invisible = jsonIdFk(*pVision, "invisible");
	// vision.range = { "flat": N } -- the extra-visibility-range object leaf
	const picojson::object* pRange = jsonChildObj(*pVision, "range");
	if (pRange != NULL)
	{
		range = jsonIdInt(*pRange, "flat");
	}
	jsonReadIdList(*pVision, "negates", negates);
	jsonReadFkMap(*pVision,"visibilityIntensity", visibilityIntensity);
	jsonReadFkMap(*pVision,"invisibilityIntensity", invisibilityIntensity);
	jsonReadFkMap(*pVision,"visibilityIntensityRange", visibilityIntensityRange);
	jsonReadFkMap(*pVision,"visibilityIntensitySameTile", visibilityIntensitySameTile);
	vs_readTerrainRows(*pVision, "invisibleTerrain", invisibleTerrain);
	vs_readFeatureRows(*pVision, "invisibleFeature", invisibleFeature);
	vs_readImprovementRows(*pVision, "invisibleImprovement", invisibleImprovement);
	vs_readTerrainRows(*pVision, "visibleTerrain", visibleTerrain);
	vs_readFeatureRows(*pVision, "visibleFeature", visibleFeature);
	vs_readImprovementRows(*pVision, "visibleImprovement", visibleImprovement);
	vs_readTerrainRows(*pVision, "visibleTerrainRange", visibleTerrainRange);
	vs_readFeatureRows(*pVision, "visibleFeatureRange", visibleFeatureRange);
	vs_readImprovementRows(*pVision, "visibleImprovementRange", visibleImprovementRange);
}
