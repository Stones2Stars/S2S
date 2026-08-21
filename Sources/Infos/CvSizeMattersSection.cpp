//
//	CvSizeMattersSection -- the shared load-time parse of the json.md par.9 `sizeMatters` block (see the header).
//	Base ranks keep the -10 unset sentinel when absent; everything else defaults 0. Idempotent (clear-first).
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvSizeMattersSection.h"
#include "CvJsonParse.h"     // jsonChildObj / jsonIdInt

CvSizeMattersSection::CvSizeMattersSection()
	: qualityBase(-10)
	, groupBase(-10)
	, sizeBase(-10)
	, quality(0)
	, group(0)
	, sizeModifier(0)
	, maxHP(0)
	, combatModifierPerSizeMore(0)
	, combatModifierPerSizeLess(0)
	, combatModifierPerVolumeMore(0)
	, combatModifierPerVolumeLess(0)
	, cargoSmSpace(0)
	, cargoVolume(0)
	, cargoVolumeModifier(0)
	, groupSize(0)
	, baseCargoVolume(0)
{
}

void CvSizeMattersSection::clearParsed()
{
	qualityBase = -10;
	groupBase = -10;
	sizeBase = -10;
	quality = 0;
	group = 0;
	sizeModifier = 0;
	maxHP = 0;
	combatModifierPerSizeMore = 0;
	combatModifierPerSizeLess = 0;
	combatModifierPerVolumeMore = 0;
	combatModifierPerVolumeLess = 0;
	cargoSmSpace = 0;
	cargoVolume = 0;
	cargoVolumeModifier = 0;
	groupSize = 0;
	baseCargoVolume = 0;
}

void CvSizeMattersSection::parse(const picojson::value& entity)
{
	clearParsed();
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	const picojson::object* pBlock = jsonChildObj(entityObj, "sizeMatters");
	if (pBlock == NULL)
	{
		return;
	}
	// base ranks: keep the -10 sentinel when absent (0 is a real rank -- json.md par.9)
	qualityBase = jsonIdInt(*pBlock, "qualityBase", -10);
	groupBase = jsonIdInt(*pBlock, "groupBase", -10);
	sizeBase = jsonIdInt(*pBlock, "sizeBase", -10);
	quality = jsonIdInt(*pBlock, "quality");
	group = jsonIdInt(*pBlock, "group");
	sizeModifier = jsonIdInt(*pBlock, "sizeModifier");
	maxHP = jsonIdInt(*pBlock, "maxHP");
	groupSize = jsonIdInt(*pBlock, "groupSize");
	baseCargoVolume = jsonIdInt(*pBlock, "baseCargoVolume");
	const picojson::object* pCombatModifier = jsonChildObj(*pBlock, "combatModifier");
	if (pCombatModifier != NULL)
	{
		combatModifierPerSizeMore = jsonIdInt(*pCombatModifier, "perSizeMore");
		combatModifierPerSizeLess = jsonIdInt(*pCombatModifier, "perSizeLess");
		combatModifierPerVolumeMore = jsonIdInt(*pCombatModifier, "perVolumeMore");
		combatModifierPerVolumeLess = jsonIdInt(*pCombatModifier, "perVolumeLess");
	}
	const picojson::object* pCargo = jsonChildObj(*pBlock, "cargo");
	if (pCargo != NULL)
	{
		cargoSmSpace = jsonIdInt(*pCargo, "smSpace");
		cargoVolume = jsonIdInt(*pCargo, "volume");
		cargoVolumeModifier = jsonIdInt(*pCargo, "volumeModifier");
	}
}
