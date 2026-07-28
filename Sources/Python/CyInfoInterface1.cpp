#include "CvGameCoreDLL.h"
#include "CvBuildingInfo.h"
#include "CvUnitCombatInfo.h"
#include "CvInfos.h"

//
// Python interface for info classes (formerly structs)
// These are simple enough to be exposed directly - no wrappers
//

void CyInfoPythonInterface1()
{
	OutputDebugString("Python Extension Module - CyInfoPythonInterface1\n");

	python::class_<CvInfoBase>("CvInfoBase", python::no_init)

		.def("isGraphicalOnly", &CvInfoBase::isGraphicalOnly, "bool ()")

		.def("getType", &CvInfoBase::getType, "string ()")
		.def("getButton", &CvInfoBase::getButton, "string ()")

		.def("getTextKey", &CvInfoBase::pyGetTextKey, "wstring ()")
		.def("getText", &CvInfoBase::pyGetText, "wstring ()")
		.def("getDescription", &CvInfoBase::pyGetDescription, "wstring ()")
		.def("getDescriptionForm", &CvInfoBase::pyGetDescriptionForm, "wstring ()")
		.def("getCivilopedia", &CvInfoBase::pyGetCivilopedia, "wstring ()")
		.def("getStrategy", &CvInfoBase::pyGetStrategy, "wstring ()")
		.def("getHelp", &CvInfoBase::pyGetHelp, "wstring ()")
		;

	python::class_<CvHotkeyInfo, python::bases<CvInfoBase> >("CvHotkeyInfo", python::no_init)
		.def("getHotKeyVal", &CvHotkeyInfo::getHotKeyVal)
		.def("getHotKeyString", &CvHotkeyInfo::pyGetHotKeyString)
		.def("isAltDown", &CvHotkeyInfo::isAltDown)
		.def("isShiftDown", &CvHotkeyInfo::isShiftDown)
		.def("isCtrlDown", &CvMapInfo::isCtrlDown)
	;

	python::class_<CvScalableInfo>("CvScalableInfo", python::no_init)
		.def("getScale", &CvScalableInfo::getScale, "float ()")
		;

	python::class_<CvSpecialistInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvSpecialistInfo", python::no_init)

		.def("getGreatPeopleUnitType", &CvSpecialistInfo::getGreatPeopleUnitType, "int ()")
		.def("getMissionType", &CvSpecialistInfo::getMissionType, "int ()")

		.def("isVisible", &CvSpecialistInfo::isVisible, "bool ()")
		.def("isSlave", &CvSpecialistInfo::isSlave, "bool ()")

		.def("getExperience", &CvSpecialistInfo::getExperience, "int ()")
		.def("getFlavorValue", &CvSpecialistInfo::getFlavorValue, "int (int i)")

		.def("getTexture", &CvSpecialistInfo::getTexture, "string ()")
	;

	python::class_<CvTechInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvTechInfo", python::no_init)
		.def("getResearchCost", &CvTechInfo::getResearchCost, "int ()")
		.def("getEra", &CvTechInfo::getEra, "int ()")

		.def("getGridX", &CvTechInfo::getGridX, "int ()")
		.def("getGridY", &CvTechInfo::getGridY, "int ()")

		.def("isRepeat", &CvTechInfo::isRepeat, "bool ()")
		.def("isDisable", &CvTechInfo::isDisable, "bool ()")
		.def("isGoodyTech", &CvTechInfo::isGoodyTech, "bool ()")

		.def("getQuote", &CvTechInfo::getQuote, "wstring ()")
		.def("getSound", &CvTechInfo::getSound, "string ()")

		.def("getFlavorValue", &CvTechInfo::getFlavorValue, "int (int i)")
		.def("getPrereqOrTechs", &CvTechInfo::getPrereqOrTechs, python::return_value_policy<python::reference_existing_object>())
		.def("getPrereqAndTechs", &CvTechInfo::getPrereqAndTechs, python::return_value_policy<python::reference_existing_object>())


		;

	python::class_<CvPromotionInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvPromotionInfo", python::no_init)

		.def("getActionInfoIndex", &CvPromotionInfo::getActionInfoIndex, "int ()")

		.def("getTechPrereq", &CvPromotionInfo::getTechPrereq, "int ()")
		.def("getStateReligionPrereq", &CvPromotionInfo::getStateReligionPrereq, "int ()")
		.def("getCommandType", &CvPromotionInfo::getCommandType, "int ()")

		.def("isLeader", &CvPromotionInfo::isLeader, "bool ()")

		.def("getObsoleteTech", &CvPromotionInfo::getObsoleteTech, "int ()")

		.def("getSound", &CvPromotionInfo::getSound, "string ()")

		// Arrays


		.def("getPromotionLine", &CvPromotionInfo::getPromotionLine, "int ()")


		.def("isStatus", &CvPromotionInfo::isStatus, "bool ()")
		//.def("isAffliction", &CvPromotionInfo::isAffliction, "bool ()")

		;

	python::class_<CvMissionInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvMissionInfo", python::no_init)
		.def("getTime", &CvMissionInfo::getTime, "int ()")

		.def("isSound", &CvMissionInfo::isSound, "bool ()")
		.def("isTarget", &CvMissionInfo::isTarget, "bool ()")
		.def("isBuild", &CvMissionInfo::isBuild, "bool ()")
		.def("getVisible", &CvMissionInfo::getVisible, "bool ()")
		;

	python::class_<CvActionInfo, boost::noncopyable>("CvActionInfo", python::no_init)
		.def("getButton", &CvActionInfo::getButton, "string ()")
		;

	python::class_<CvUnitInfo, python::bases<CvInfoBase, CvScalableInfo>, boost::noncopyable>("CvUnitInfo", python::no_init)

		.def("getProductionCost", &CvUnitInfo::getProductionCost, "int ()")
		.def("getMinAreaSize", &CvUnitInfo::getMinAreaSize, "int ()")
		.def("getWorkRate", &CvUnitInfo::getWorkRate, "int ()")
		.def("getEspionagePoints", &CvUnitInfo::getEspionagePoints, "int ()")
		.def("getAirCombat", &CvUnitInfo::getAirCombat, "int ()")

		.def("getSpecialUnitType", &CvUnitInfo::getSpecialUnitType, "int ()")


		.def("getCommandType", &CvUnitInfo::getCommandType, "int ()")


		// Arrays



		.def("hasBuild", &CvUnitInfo::hasBuild, "bool (BuildTypes eBuild)")
		.def("getHeritage", &CvUnitInfo::getHeritage, "int ()")
		//.def("getTerrainImpassable", &CvUnitInfo::getTerrainImpassable, "bool (int i)")
		//.def("getFeatureImpassable", &CvUnitInfo::getFeatureImpassable, "bool (int i)")
		.def("getArtInfo", &CvUnitInfo::getArtInfo,  python::return_value_policy<python::reference_existing_object>(), "CvArtInfoUnit* (int i, bool bLate)")
		//TB SubCombat Mod begin  TB Combat Mods Begin
		//boolean vectors
		.def("getTotalModifiedCombatStrength100", &CvUnitInfo::getTotalModifiedCombatStrength, "int (bool bSizeMatters)")
		//TB Combat Mods End  TB SubCombat Mod end

		.def("getMapCategories", &CvUnitInfo::getMapCategories, python::return_value_policy<python::reference_existing_object>())

		;

	python::class_<CvSpecialUnitInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvSpecialUnitInfo", python::no_init)
		.def("isValid", &CvSpecialUnitInfo::isValid, "bool ()")
		;

	python::class_<CvCivicOptionInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvCivicOptionInfo", python::no_init)
		//.def("getTraitNoUpkeep", &CvCivicOptionInfo::getTraitNoUpkeep, "bool (int i)")
		;

	python::class_<CvCivicInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvCivicInfo", python::no_init)





		// Arrays

		.def("getYieldModifier", &CvCivicInfo::getYieldModifier, "int (int i)")
		.def("getCommerceModifier", &CvCivicInfo::getCommerceModifier, "int (int i)")

		;

	python::class_<CvBuildingInfo, python::bases<CvInfoBase, CvScalableInfo>, boost::noncopyable>("CvBuildingInfo", python::no_init)


		.def("getGreatPeopleUnitType", &CvBuildingInfo::getGreatPeopleUnitType, "int ()")
		.def("getMaintenanceModifier", &CvBuildingInfo::getMaintenanceModifier, "int ()")
		.def("getAirUnitCapacity", &CvBuildingInfo::getAirUnitCapacity, "int ()")
		.def("getAirlift", &CvBuildingInfo::getAirlift, "int ()")

		.def("isAutoBuild", &CvBuildingInfo::isAutoBuild, "bool ()")
		.def("isAllowsNukes", &CvBuildingInfo::isAllowsNukes, "bool ()")


		.def("getHotKey", &CvBuildingInfo::getHotKey, "string ()")

		// Arrays


		.def("getYieldModifier", &CvBuildingInfo::getYieldModifier, "int (int i)")
		.def("getCommerceModifier", &CvBuildingInfo::getCommerceModifier, "int (int i)")












		.def("getMapCategories", &CvBuildingInfo::getMapCategories, python::return_value_policy<python::reference_existing_object>())

		;

	python::class_<CvSpecialBuildingInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvSpecialBuildingInfo", python::no_init)
		.def("getObsoleteTech", &CvSpecialBuildingInfo::getObsoleteTech, "int ()")
		.def("getTechPrereq", &CvSpecialBuildingInfo::getTechPrereq, "int ()")
		.def("isValid", &CvSpecialBuildingInfo::isValid, "bool ()")
		;

	python::class_<CvPromotionLineInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvPromotionLineInfo", python::no_init)
		.def("getPrereqTech", &CvPromotionLineInfo::getPrereqTech, "int ()")
		.def("getObsoleteTech", &CvPromotionLineInfo::getObsoleteTech, "int ()")
		.def("isBuildUp", &CvPromotionLineInfo::isBuildUp, "bool ()")
		;

	python::class_<CvUnitCombatInfo, python::bases<CvInfoBase>, boost::noncopyable>("CvUnitCombatInfo", python::no_init)
		;
}
