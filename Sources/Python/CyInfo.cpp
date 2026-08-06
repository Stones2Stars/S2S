//
//	CyInfo -- the Python info surface (see the header for the role and the boost rule). Every body is a bare
//	relay through the ONE infotype-prefix -> InfoRepo dispatch, so this file holds no registry knowledge of its
//	own and cannot drift from the load pipeline's table.
//

#include "CvGameCoreDLL.h"
// boost::python::dict is NOT in the PCH's boost set (list/tuple/class/object are). Included HERE rather than
// widened into CvGameCoreDLL.h: the umbrella is a foundational header, and adding to it rebuilds every TU.
// ⚠ This resolves to Boost 1.32 -- the `python::` alias -- never boost155 (engine.md: two Boosts coexist).
#include <boost/python/dict.hpp>
#include "CyInfo.h"
#include "Data/CvReadJson.h"     // the ONE infotype-prefix -> InfoRepo dispatch, READ-ONLY half (rjInfoForTypeConst / rjCountForType)
#include "Infos/CvInfo.h"
#include "Infos/CvEdges.h"           // the load-derived edge families ([DEC-one-reverse-view])
#include "Infos/CvClassificationRegistry.h"   // the cold-path authored-key -> generated-id resolve
#include "Infos/CvCivicInfo.h"       // the civic column the bulk index reads
// The straggler dispatch reaches these concrete registries -- specific headers, never the CvInfos.h umbrella.
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvUnitInfo.h"             // getGrantedBuildings -- the unit's MISSION_CONSTRUCT repertoire
#include "Infos/CvAllowed.h"
#include "Infos/CvSpecialBuildingInfo.h"   // the GROUP that may hold the cap for its members
#include "Infos/CvBonusInfo.h"
#include "Infos/CvSpecialistInfo.h"
#include "Infos/CvYieldInfo.h"
#include "Infos/CvControlInfo.h"
//
//	The XML-ONLY registries the identity dispatch serves. They are listed one per line, SPECIFICALLY, because a
//	derived->base conversion needs the COMPLETE type -- an incomplete one fails to compile rather than silently
//	doing the wrong thing, which is the compiler doing its job. ⛔ Never the CvInfos.h umbrella (AGENTS.md): it
//	is flagged for retirement, and trading a visible list for one hidden include is what makes a dependency
//	unreadable.
//
#include "Infos/CvCommerceInfo.h"
#include "Infos/CvMPOptionInfo.h"
#include "Infos/CvForceControlInfo.h"
#include "Infos/CvPlayerColorInfo.h"
#include "Infos/CvClimateInfo.h"
#include "Infos/CvSeaLevelInfo.h"
#include "Infos/CvGameOptionInfo.h"
#include "Infos/CvColorInfo.h"
#include "Infos/CvMissionInfo.h"
#include "Infos/CvProcessInfo.h"   // getProductionToCommerce -- the process conversion group
#include "Infos/CvActionInfo.h"
#include "Infos/CvAdvisorInfo.h"
#include "Infos/CvEmphasizeInfo.h"
#include "Infos/CvEspionageMissionInfo.h"
#include "Infos/CvGoodyInfo.h"
#include "Infos/CvUpkeepInfo.h"
#include "Infos/CvGameSpeedInfo.h" // getTotalTurns -- the game-length straggler
#include "Infos/CvWorldInfo.h"     // getDefaultPlayers -- the map-setup straggler (PYINT_DEFAULT_PLAYERS)
#include "Infos/CvCorporationInfo.h"
#include "Infos/CvProjectInfo.h"   // isSpaceship -- the build-progress readout (PYINT_IS_SPACESHIP)
#include "Infos/CvVictoryInfo.h"   // isPermanent -- the scenario victory-list filter
#include "Infos/CvTechInfo.h"      // isRepeat -- the scenario repeat-tech loop (PYINT_IS_REPEAT)
#include "Infos/CvVoteSourceInfo.h"
#include "Infos/CvInvisibleInfo.h"
#include "Infos/CvEventInfo.h"
#include "Infos/CvEventTriggerInfo.h"
#include "Defines/CvGlobals.h"        // GC.getNumCivicInfos / getCivicInfo

namespace
{
	// Resolve without asserting: a script may legitimately probe an id past the end of a registry, and the
	// honest answer there is "nothing", not a crash -- the same discipline CyEnabler and CyState apply.
	const CvInfo* cyi_info(const std::string& szTypePrefix, int iId)
	{
		if (iId < 0) return NULL;
		// ⛔ the CONST twin: a read must answer NULL past the end, never grow the repo (CvReadJson.cpp)
		return rjInfoForTypeConst(szTypePrefix, iId);
	}

	//
	//	The XML-ONLY registries -- the SECOND HALF of the prefix dispatch.
	//
	//	⛔ Without this, "the info surface" is only half a surface, and the half it is missing is invisible: a
	//	JSON-backed type answers and an XML-only one raises, from the SAME call with the SAME shape. The engine
	//	still holds these registries perfectly well; they simply have no InfoRepo entry, so rjInfoForType cannot
	//	see them.
	//
	//	⚑ They resolve to CvInfoBase, which is where getType / getDescription / getButton actually live -- so the
	//	identity plane is servable for EVERY registry, JSON or not, and a consumer never has to know which kind it
	//	is holding. That is the whole point: the JSON/XML split is OUR migration state, not something script
	//	should have to model.
	//
	//	⚠ These are NOT the banned read surface. What crosses is the entity's own IDENTITY -- its stable type key,
	//	its localized name, its button tag -- never a legacy getter contract ([DEC-cy-not-fixed] bans the
	//	info/state GETTER surface; a type key is what a scenario serializer and a config string need, and
	//	python-read-map names CvWBDesc as requiring exactly it).
	//
	const CvInfoBase* cyi_xmlOnlyInfo(const std::string& szTypePrefix, int iId)
	{
		// Fixed-enum registries: the bound is the enum terminator, there being no count accessor.
		if (szTypePrefix == "YIELD_")       return iId < NUM_YIELD_TYPES     ? &GC.getYieldInfo((YieldTypes)iId) : NULL;
		if (szTypePrefix == "COMMERCE_")    return iId < NUM_COMMERCE_TYPES  ? &GC.getCommerceInfo((CommerceTypes)iId) : NULL;
		if (szTypePrefix == "CONTROL_")     return iId < NUM_CONTROL_TYPES   ? &GC.getControlInfo((ControlTypes)iId) : NULL;
		if (szTypePrefix == "DOMAIN_")      return iId < NUM_DOMAIN_TYPES    ? &GC.getDomainInfo((DomainTypes)iId) : NULL;
		if (szTypePrefix == "UNITAI_")      return iId < NUM_UNITAI_TYPES    ? &GC.getUnitAIInfo((UnitAITypes)iId) : NULL;
		if (szTypePrefix == "ATTITUDE_")    return iId < NUM_ATTITUDE_TYPES  ? &GC.getAttitudeInfo((AttitudeTypes)iId) : NULL;
		if (szTypePrefix == "MEMORY_")      return iId < NUM_MEMORY_TYPES    ? &GC.getMemoryInfo((MemoryTypes)iId) : NULL;

		// Counted registries.
		if (szTypePrefix == "DENIAL_")           return iId < GC.getNumDenialInfos()           ? &GC.getDenialInfo((DenialTypes)iId) : NULL;
		if (szTypePrefix == "CALENDAR_")         return iId < GC.getNumCalendarInfos()         ? &GC.getCalendarInfo((CalendarTypes)iId) : NULL;
		if (szTypePrefix == "SEASON_")           return iId < GC.getNumSeasonInfos()           ? &GC.getSeasonInfo((SeasonTypes)iId) : NULL;
		if (szTypePrefix == "MONTH_")            return iId < GC.getNumMonthInfos()            ? &GC.getMonthInfo((MonthTypes)iId) : NULL;
		if (szTypePrefix == "MPOPTION_")         return iId < GC.getNumMPOptionInfos()         ? &GC.getMPOptionInfo((MultiplayerOptionTypes)iId) : NULL;
		if (szTypePrefix == "FORCECONTROL_")     return iId < GC.getNumForceControlInfos()     ? &GC.getForceControlInfo((ForceControlTypes)iId) : NULL;
		if (szTypePrefix == "PLAYERCOLOR_")      return iId < GC.getNumPlayerColorInfos()      ? &GC.getPlayerColorInfo((PlayerColorTypes)iId) : NULL;
		if (szTypePrefix == "CLIMATE_")          return iId < GC.getNumClimateInfos()          ? &GC.getClimateInfo((ClimateTypes)iId) : NULL;
		if (szTypePrefix == "SEALEVEL_")         return iId < GC.getNumSeaLevelInfos()         ? &GC.getSeaLevelInfo((SeaLevelTypes)iId) : NULL;
		if (szTypePrefix == "GAMEOPTION_")       return iId < GC.getNumGameOptionInfos()       ? &GC.getGameOptionInfo((GameOptionTypes)iId) : NULL;
		if (szTypePrefix == "COLOR_")            return iId < GC.getNumColorInfos()            ? &GC.getColorInfo((ColorTypes)iId) : NULL;
		if (szTypePrefix == "MISSION_")          return iId < GC.getNumMissionInfos()          ? &GC.getMissionInfo((MissionTypes)iId) : NULL;
		if (szTypePrefix == "ADVISOR_")          return iId < GC.getNumAdvisorInfos()          ? &GC.getAdvisorInfo((AdvisorTypes)iId) : NULL;
		if (szTypePrefix == "EMPHASIZE_")        return iId < GC.getNumEmphasizeInfos()        ? &GC.getEmphasizeInfo((EmphasizeTypes)iId) : NULL;
		if (szTypePrefix == "ESPIONAGEMISSION_") return iId < GC.getNumEspionageMissionInfos() ? &GC.getEspionageMissionInfo((EspionageMissionTypes)iId) : NULL;
		if (szTypePrefix == "GOODY_")            return iId < GC.getNumGoodyInfos()            ? &GC.getGoodyInfo((GoodyTypes)iId) : NULL;
		if (szTypePrefix == "UPKEEP_")           return iId < GC.getNumUpkeepInfos()           ? &GC.getUpkeepInfo((UpkeepTypes)iId) : NULL;
		if (szTypePrefix == "VOTESOURCE_")       return iId < GC.getNumVoteSourceInfos()       ? &GC.getVoteSourceInfo((VoteSourceTypes)iId) : NULL;
		if (szTypePrefix == "INVISIBLE_")        return iId < GC.getNumInvisibleInfos()        ? &GC.getInvisibleInfo((InvisibleTypes)iId) : NULL;
		if (szTypePrefix == "CONCEPT_")          return iId < GC.getNumConceptInfos()          ? &GC.getConceptInfo((ConceptTypes)iId) : NULL;
		if (szTypePrefix == "EVENT_")            return iId < GC.getNumEventInfos()            ? &GC.getEventInfo((EventTypes)iId) : NULL;
		if (szTypePrefix == "EVENTTRIGGER_")     return iId < GC.getNumEventTriggerInfos()     ? &GC.getEventTriggerInfo((EventTriggerTypes)iId) : NULL;
		// ⛔ ACTION_ is the one registry whose entry is NOT an info: a CvActionInfo is a SLOT in the hotkey
		// table, holding an index and a subtype, and it MIRRORS the CvInfoBase reads by delegating each one to
		// the entity the slot stands for. So the identity plane resolves through that entity, which is a real
		// CvHotkeyInfo -- and the derived->base conversion is the compiler-checked one every registry above
		// gets. A pointer cast here compiles and then dispatches a virtual call through the wrong vtable.
		if (szTypePrefix == "ACTION_")           return iId < GC.getNumActionInfos()           ? GC.getActionInfo(iId).getHotkeyInfo() : NULL;
		return NULL;
	}

	// The WHOLE identity plane: JSON-backed first (the shared dispatch), then the XML-only registries. A caller
	// asks by prefix and does not learn which half answered.
	const CvInfoBase* cyi_infoBase(const std::string& szTypePrefix, int iId)
	{
		if (iId < 0) return NULL;
		// ⛔ the CONST twin, for the same reason cyi_info above uses it: this is a READ, and the get-or-create
		// dispatch would GROW the registry for any id handed to it -- which is how a walk over the identity
		// plane ran away instead of finding its end.
		const CvInfo* pJson = rjInfoForTypeConst(szTypePrefix, iId);
		if (pJson != NULL) return pJson;   // CvInfo -> CvHotkeyInfo -> CvInfoBase
		return cyi_xmlOnlyInfo(szTypePrefix, iId);
	}

	// WHICH ownable scope a SELF-cap sits at, or -1 for none. The cap's scope IS the wonder category
	// ([json.md] 4.4), so the category and the is-it-capped test read the SAME function -- two tests would be
	// two meanings of "capped" that can drift.
	// ⚠ The CATEGORY count-caps are a different axis and are not a self-cap.
	int cyi_selfCapScope(const CvAllowed* pAllowed)
	{
		if (pAllowed == NULL) return -1;
		if (pAllowed->cap(ALLOWEDCAP_WORLD)  >= 0) return ALLOWEDCAP_WORLD;
		if (pAllowed->cap(ALLOWEDCAP_TEAM)   >= 0) return ALLOWEDCAP_TEAM;
		if (pAllowed->cap(ALLOWEDCAP_EMPIRE) >= 0) return ALLOWEDCAP_EMPIRE;
		return -1;
	}

	// The building's cap scope, following the SAME two routes the enabler does: its own `allowed`, else the
	// SPECIALBUILDING GROUP that holds the cap for all its members.
	int cyi_buildingCapScope(int iId)
	{
		const CvBuildingInfo& kBuilding = GC.getBuildingInfo((BuildingTypes)iId);
		const int iOwn = cyi_selfCapScope(kBuilding.getAllowed());
		if (iOwn >= 0) return iOwn;

		const int iSpecialBuilding = kBuilding.getSpecialBuildingType();
		if (iSpecialBuilding != NO_SPECIALBUILDING)
		{
			return cyi_selfCapScope(GC.getSpecialBuildingInfo((SpecialBuildingTypes)iSpecialBuilding).getAllowed());
		}
		return -1;
	}
}

std::wstring CyInfo::getDescription(const std::string& szTypePrefix, int iId) const
{
	const CvInfoBase* pInfo = cyi_infoBase(szTypePrefix, iId);
	return pInfo ? std::wstring(pInfo->getDescription()) : std::wstring();
}

std::wstring CyInfo::getCivilopedia(const std::string& szTypePrefix, int iId) const
{
	const CvInfoBase* pInfo = cyi_infoBase(szTypePrefix, iId);
	return pInfo ? std::wstring(pInfo->getCivilopedia()) : std::wstring();
}

std::wstring CyInfo::getStrategy(const std::string& szTypePrefix, int iId) const
{
	const CvInfoBase* pInfo = cyi_infoBase(szTypePrefix, iId);
	return pInfo ? std::wstring(pInfo->getStrategy()) : std::wstring();
}

std::wstring CyInfo::getTextKey(const std::string& szTypePrefix, int iId) const
{
	const CvInfoBase* pInfo = cyi_infoBase(szTypePrefix, iId);
	if (pInfo == NULL || pInfo->getTextKeyWide() == NULL)
	{
		return std::wstring();
	}
	return std::wstring(pInfo->getTextKeyWide());
}

std::string CyInfo::getType(const std::string& szTypePrefix, int iId) const
{
	const CvInfoBase* pInfo = cyi_infoBase(szTypePrefix, iId);
	return pInfo ? std::string(pInfo->getType()) : std::string();
}

std::string CyInfo::getButton(const std::string& szTypePrefix, int iId) const
{
	const CvInfoBase* pInfo = cyi_infoBase(szTypePrefix, iId);
	if (pInfo == NULL) return std::string();
	const char* szButton = pInfo->getButton();
	return szButton ? std::string(szButton) : std::string();
}

bool CyInfo::exists(const std::string& szTypePrefix, int iId) const
{
	return cyi_infoBase(szTypePrefix, iId) != NULL;
}

python::list CyInfo::getRevolution(const std::string& szTypePrefix, int iId, int iScope) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	for (int iKind = 0; iKind < NUM_REVOLUTION_KINDS; ++iKind)
	{
		// An entity authoring none answers 0 across the group -- a total read, never an error.
		values.append(pInfo ? pInfo->modifier(MODFAM_REVOLUTION, iKind, (CvCascScope)iScope, CASC_UNIT_FLAT) : 0);
	}
	return values;
}

python::list CyInfo::getProductionToCommerce(const std::string& szTypePrefix, int iId, int iScope) const
{
	python::list values;
	const bool bProcess = (szTypePrefix == "PROCESS_" && iId >= 0 && iId < GC.getNumProcessInfos());
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		values.append(bProcess
			? GC.getProcessInfo((ProcessTypes)iId).getProductionToCommerce((CommerceTypes)iCommerce, (CvCascScope)iScope)
			: 0);
	}
	return values;
}

bool CyInfo::hasSkill(const std::string& szTypePrefix, int iId, int iSkillId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	return pInfo ? pInfo->hasSkill(iSkillId) : false;
}
bool CyInfo::hasTag(const std::string& szTypePrefix, int iId, int iTagId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	return pInfo ? pInfo->hasTag(iTagId) : false;
}
bool CyInfo::hasAttribute(const std::string& szTypePrefix, int iId, int iAttributeId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	return pInfo ? pInfo->hasAttribute(iAttributeId) : false;
}
bool CyInfo::hasCharacteristic(const std::string& szTypePrefix, int iId, int iCharacteristicId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	return pInfo ? pInfo->hasCharacteristic(iCharacteristicId) : false;
}
bool CyInfo::providesAmenity(const std::string& szTypePrefix, int iId, int iAmenityId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	return pInfo ? pInfo->providesAmenity(iAmenityId) : false;
}
bool CyInfo::revokesSkill(const std::string& szTypePrefix, int iId, int iSkillId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	return pInfo ? pInfo->revokesSkill(iSkillId) : false;
}
bool CyInfo::providesCapability(const std::string& szTypePrefix, int iId, int iCapabilityId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	// A block-less entity answers FALSE, so the read is TOTAL ([patterns.md] category 2).
	return pInfo ? pInfo->providesCapability(iCapabilityId) : false;
}

bool CyInfo::providesPolicy(const std::string& szTypePrefix, int iId, int iPolicyId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	// The empire-STATE sibling of providesCapability (json.md §9): same O(1) bitset test, same total read.
	return pInfo ? pInfo->providesPolicy(iPolicyId) : false;
}

bool CyInfo::canTradeItem(const std::string& szTypePrefix, int iId, const std::string& szItem) const
{
	// TECH_ is the only grantor the block is authored on today; another kind answers FALSE rather than erroring.
	if (szTypePrefix != "TECH_" || iId < 0 || iId >= GC.getNumTechInfos())
	{
		return false;
	}
	// A COLD, name-keyed caller (the Python surface resolves by authored key, not by generated id), so the
	// registry does the name->id resolve here rather than the block carrying a string plane for it.
	return GC.getTechInfo((TechTypes)iId).providesCanTrade(
		ClassificationRegistry::keyId(CLSD_CANTRADE, szItem));
}

python::list CyInfo::getWellbeing(const std::string& szTypePrefix, int iId, int iScope) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	for (int iChannel = 0; iChannel < NUM_WELLBEING_CHANNELS; ++iChannel)
	{
		// An entity that authors none answers 0 across the group -- a total read, never an error.
		values.append(pInfo ? pInfo->getFlatWellbeing((WellbeingChannel)iChannel, (CvCascScope)iScope) : 0);
	}
	return values;
}

int CyInfo::getScalar(const std::string& szTypePrefix, int iId, int iScalar, int iScope, int iUnit) const
{
	if (iScalar < 0 || iScalar >= NUM_INFO_SCALARS)
	{
		return 0;
	}
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	if (pInfo == NULL)
	{
		return 0;
	}
	return pInfo->getScalar((InfoScalar)iScalar, (CvCascScope)iScope, (CvCascUnit)iUnit);
}

python::list CyInfo::getIdList(const std::string& szTypePrefix, int iId, int iSlot) const
{
	// Each SLOT names its own registry -- the pair (prefix, slot) has to agree, so the bound is checked per
	// slot rather than by one hard-coded gate on the function.
	python::list ids = python::list();
	if (iId < 0) return ids;

	switch (iSlot)
	{
	case PYLIST_HEADQUARTERS_BUILDINGS:
		{
			if (szTypePrefix != "CORPORATION_" || iId >= GC.getNumCorporationInfos()) break;
			const std::vector<BuildingTypes>& buildings = GC.getCorporationInfo((CorporationTypes)iId).getHeadquartersBuildings();
			for (size_t i = 0; i < buildings.size(); ++i) ids.append((int)buildings[i]);
		}
		break;
	case PYLIST_CONSUMED_BONUSES:
		{
			if (szTypePrefix != "CORPORATION_" || iId >= GC.getNumCorporationInfos()) break;
			const std::vector<int>& bonuses = GC.getCorporationInfo((CorporationTypes)iId).getConsumedBonuses();
			for (size_t i = 0; i < bonuses.size(); ++i) ids.append(bonuses[i]);
		}
		break;
	case PYLIST_GRANTED_BUILDINGS:
		{
			if (szTypePrefix != "UNIT_" || iId >= GC.getNumUnitInfos()) break;
			const std::vector<int>& buildings = GC.getUnitInfo((UnitTypes)iId).getGrantedBuildings();
			for (size_t i = 0; i < buildings.size(); ++i) ids.append(buildings[i]);
		}
		break;
	case PYLIST_PREREQ_AND_TECHS:
		{
			if (szTypePrefix != "TECH_" || iId >= GC.getNumTechInfos()) break;
			const std::vector<TechTypes>& techs = GC.getTechInfo((TechTypes)iId).getPrereqAndTechs();
			for (size_t i = 0; i < techs.size(); ++i) ids.append((int)techs[i]);
		}
		break;
	case PYLIST_PREREQ_OR_TECHS:
		{
			if (szTypePrefix != "TECH_" || iId >= GC.getNumTechInfos()) break;
			const std::vector<TechTypes>& techs = GC.getTechInfo((TechTypes)iId).getPrereqOrTechs();
			for (size_t i = 0; i < techs.size(); ++i) ids.append((int)techs[i]);
		}
		break;
	default:
		break;
	}
	return ids;
}

int CyInfo::getIntrinsic(const std::string& szTypePrefix, int iId, int iSlot) const
{
	if (iId < 0) return -1;

	// ⚑ Two populations, deliberately in ONE place. The JSON-backed types resolve through the shared
	// infotype-prefix dispatch; the XML-only registries (yields, controls) have no InfoRepo entry and are
	// reached through their own accessor. Keeping both here means a consumer never has to know which is which,
	// and the split cannot leak into script.
	switch (iSlot)
	{
	case PYINT_COST:
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
			return GC.getBuildingInfo((BuildingTypes)iId).getCost();
		break;

	case PYINT_BONUS_CLASS:
		if (szTypePrefix == "BONUS_" && iId < GC.getNumBonusInfos())
			return GC.getBonusInfo((BonusTypes)iId).getBonusClassType();
		break;

	case PYINT_IS_MAP_BONUS:
		if (szTypePrefix == "BONUS_" && iId < GC.getNumBonusInfos())
			return GC.getBonusInfo((BonusTypes)iId).isMapBonus() ? 1 : 0;
		break;

	case PYINT_IS_VISIBLE:
		if (szTypePrefix == "SPECIALIST_" && iId < GC.getNumSpecialistInfos())
			return GC.getSpecialistInfo((SpecialistTypes)iId).isVisible() ? 1 : 0;
		break;

	case PYINT_COLOR_TYPE:
		if (szTypePrefix == "YIELD_" && iId < NUM_YIELD_TYPES)
			return GC.getYieldInfo((YieldTypes)iId).getColorType();
		break;

	case PYINT_DEFAULT_PLAYERS:
		if (szTypePrefix == "WORLD_" && iId < GC.getNumWorldInfos())
			return GC.getWorldInfo((WorldSizeTypes)iId).getDefaultPlayers();
		break;

	case PYINT_HEADQUARTERS_CORPORATION:
		// ⚑ The FK lives on the BUILDING (json §9 `headquarters`: the relationship IS the data), so asking a
		// building which corporation it heads is a straight member read. ⛔ It is NOT the inverse question --
		// "which building founds corporation X" is a reverse lookup and belongs to the edge families
		// ([DEC-one-reverse-view]), never to a scan of every building testing this slot.
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
			return GC.getBuildingInfo((BuildingTypes)iId).getHeadquartersCorporation();
		break;

	case PYINT_IS_SEE_DEMOGRAPHICS:
		if (szTypePrefix == "ESPIONAGEMISSION_" && iId < GC.getNumEspionageMissionInfos())
			return GC.getEspionageMissionInfo((EspionageMissionTypes)iId).isSeeDemographics() ? 1 : 0;
		break;

	case PYINT_IS_SEE_RESEARCH:
		if (szTypePrefix == "ESPIONAGEMISSION_" && iId < GC.getNumEspionageMissionInfos())
			return GC.getEspionageMissionInfo((EspionageMissionTypes)iId).isSeeResearch() ? 1 : 0;
		break;

	case PYINT_IS_SPACESHIP:
		if (szTypePrefix == "PROJECT_" && iId < GC.getNumProjectInfos())
			return GC.getProjectInfo((ProjectTypes)iId).isSpaceship() ? 1 : 0;
		break;

	case PYINT_ERA:
		if (szTypePrefix == "TECH_" && iId < GC.getNumTechInfos())
			return GC.getTechInfo((TechTypes)iId).getEra();
		break;

	case PYINT_TOTAL_TURNS:
		if (szTypePrefix == "GAMESPEED_" && iId < GC.getNumGameSpeedInfos())
			return GC.getGameSpeedInfo((GameSpeedTypes)iId).getTotalTurns();
		break;

	case PYINT_ADVISOR:
		if (szTypePrefix == "TECH_" && iId < GC.getNumTechInfos())
			return GC.getTechInfo((TechTypes)iId).getAdvisor();
		break;

	case PYINT_GRID_X:
		if (szTypePrefix == "TECH_" && iId < GC.getNumTechInfos())
			return GC.getTechInfo((TechTypes)iId).getGridX();
		break;

	case PYINT_GRID_Y:
		if (szTypePrefix == "TECH_" && iId < GC.getNumTechInfos())
			return GC.getTechInfo((TechTypes)iId).getGridY();
		break;

	case PYINT_TRADE_ROUTE_AMOUNT:
		//	Kind 0 IS the scope-wide flat route COUNT (CvInfoKinds.h TRADE_ROUTE_AMOUNT) -- the memberless
		//	deposit, read at the scope the caller names rather than through a per-scope getter name.
		if (szTypePrefix == "TECH_" && iId < GC.getNumTechInfos())
			return GC.getTechInfo((TechTypes)iId).getTradeRoute(TRADE_ROUTE_AMOUNT, CASC_SCOPE_CITY);
		break;
	case PYINT_WONDER_SCOPE:
		//	WHICH scope the self-cap sits at -- an ALLOWEDCAP_* value, or -1 when the building is uncapped.
		//	That scope IS the wonder category: WORLD -> world wonder, TEAM -> team wonder, EMPIRE -> national.
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
			return cyi_buildingCapScope(iId);
		break;

	case PYINT_IS_NO_INSTANCE_LIMIT:
		//	RELOCATABLE: the building waives the EMPIRE (national-wonder) cap, so it can be rebuilt elsewhere --
		//	the palace and the culture buildings. The cap itself stays; only its empire enforcement is waived
		//	(CvBuildingEnabler), which is why this is its own fact and not the absence of a cap.
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
			return GC.getBuildingInfo((BuildingTypes)iId).isNoInstanceLimit() ? 1 : 0;
		break;

	case PYINT_ACTION_INFO_INDEX:
		// CvControlInfo derives from CvHotkeyInfo, which owns the index -- the control registry is XML-only,
		// so there is no InfoRepo route to it.
		// Controls are a FIXED enum rather than a counted registry, so the bound is the enum terminator.
		if (szTypePrefix == "CONTROL_" && iId < NUM_CONTROL_TYPES)
			return GC.getControlInfo((ControlTypes)iId).getActionInfoIndex();
		break;

	case PYINT_IS_LIMITED_WONDER:
		// ⚑ A building's wonder CATEGORY is derived from WHICH self-cap it authors ([json.md §4.4]: the cap's
		// scope is what makes it a world / team / national wonder), never from an isWorldWonder mirror.
		//
		// ⛔ TWO WAYS TO BE CAPPED, and reading only the first is wrong. A building may carry its own self-cap,
		// OR belong to a SPECIALBUILDING GROUP that holds the cap for all its members (json.md §4.4: the member
		// authors identity.specialBuildingType, the GROUP entity holds `allowed`). A grouped wonder authors no
		// cap of its own, so a self-cap-only test calls it unlimited -- silently, and only for the grouped ones.
		// This mirrors the enabler's own gate (CvBuildingEnabler bd_groupCapOk / the group-capped re-gate), so
		// there is ONE meaning of "capped" rather than two that can drift.
		//
		// ⚠ The CATEGORY count-caps (worldWonders/teamWonders/nationalWonders) are a DIFFERENT axis -- a per-city
		// bound set by CultureLevel -- and deliberately do not make a building "limited".
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
		{
			return cyi_buildingCapScope(iId) >= 0 ? 1 : 0;
		}
		break;

	case PYINT_IS_REPEAT:
		// A REPEATABLE tech can be researched more than once; the scenario writer emits one Tech line per
		// completion, so it needs the flag to know whether to loop the team's tech count.
		if (szTypePrefix == "TECH_" && iId < GC.getNumTechInfos())
		{
			return GC.getTechInfo((TechTypes)iId).isRepeat() ? 1 : 0;
		}
		break;

	case PYINT_IS_PERMANENT:
		// A PERMANENT victory is one a scenario never lists: the WorldBuilder description writes only the
		// non-permanent valid victories, because a permanent one is always in force and cannot be toggled.
		if (szTypePrefix == "VICTORY_" && iId < GC.getNumVictoryInfos())
		{
			return GC.getVictoryInfo((VictoryTypes)iId).isPermanent() ? 1 : 0;
		}
		break;

	default:
		break;
	}
	return -1;
}

python::list CyInfo::getIndex(const std::string& szTypePrefix) const
{
	python::list lEntries;

	//	⛔ THE BOUND IS ASKED FOR, NEVER PROBED. Walking until the first NULL is not a bound: a JSON repo answers
	//	through InfoRepo, which may hold a NULL hole, and the id space does not end where the first hole is.
	//	(Probing the get-or-create dispatch was worse still -- it GREW the registry, verified in-game as a
	//	MemoryError.) So each half supplies its own real end: rjCountForType for a JSON registry, and for the
	//	XML-only half the dispatch's own explicit bound-check, which genuinely answers NULL past the end.
	const int iJsonCount = rjCountForType(szTypePrefix);

	for (int iId = 0; iJsonCount < 0 || iId < iJsonCount; iId++)
	{
		const CvInfoBase* pInfo = cyi_infoBase(szTypePrefix, iId);
		if (pInfo == NULL)
		{
			if (iJsonCount < 0) break;   // XML-only: NULL IS the end
			continue;                    // JSON: a hole, not the end -- keep walking to the real count
		}

		python::dict kEntry;
		kEntry["id"]          = iId;
		kEntry["type"]        = std::string(pInfo->getType() != NULL ? pInfo->getType() : "");
		kEntry["description"] = std::wstring(pInfo->getDescription());
		kEntry["textKey"]     = std::wstring(pInfo->getTextKeyWide() != NULL ? pInfo->getTextKeyWide() : L"");
		kEntry["button"]      = std::string(pInfo->getButton() != NULL ? pInfo->getButton() : "");
		lEntries.append(kEntry);
	}
	return lEntries;
}

python::list CyInfo::getEdgeIds(const std::string& szTypePrefix, int iId, int iFamily, int iBucket) const
{
	python::list lIds;

	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	if (pInfo == NULL) return lIds;

	const CvEdges* pEdges = pInfo->getEdges();
	if (pEdges == NULL) return lIds;   // an entity that authors no edge answers EMPTY, never an error

	if (iFamily < 0 || iFamily >= NUM_EDGEF) return lIds;
	if (iBucket < 0 || iBucket >= NUM_EDGEB) return lIds;

	const std::vector<int>* pList = pEdges->find((EnEdgeFamily)iFamily, (EnEdgeBucket)iBucket);
	if (pList == NULL) return lIds;

	for (std::vector<int>::const_iterator it = pList->begin(); it != pList->end(); ++it)
	{
		lIds.append(*it);
	}
	return lIds;
}

// The civic -> CIVICOPTION_ column, compiled ONCE on first use and handed out by reference thereafter. Static
// rather than rebuilt per call because IDValueMap is noncopyable and Python holds a reference: the object has to
// outlive the call. Info data is immutable after load, so one build is correct for the process.
const IDValueMap<CivicTypes, int>& CyInfo::civicOptions() const
{
	static IDValueMap<CivicTypes, int> s_civicOptions;
	static bool s_built = false;
	if (!s_built)
	{
		s_built = true;
		for (int iCivic = 0; iCivic < GC.getNumCivicInfos(); ++iCivic)
		{
			s_civicOptions.insert((CivicTypes)iCivic, GC.getCivicInfo((CivicTypes)iCivic).getCivicOption());
		}
	}
	return s_civicOptions;
}

void CyInfo::pythonPublish()
{
	publishIDValueMapPythonInterface<IDValueMap<CivicTypes, int> >();

	python::class_<CyInfo>("CyInfo")
		.def("getDescription", &CyInfo::getDescription)
		.def("getTextKey",     &CyInfo::getTextKey)
		.def("getCivilopedia", &CyInfo::getCivilopedia)
		.def("getStrategy",    &CyInfo::getStrategy)
		.def("getType",        &CyInfo::getType)
		.def("getButton",      &CyInfo::getButton)
		.def("exists",         &CyInfo::exists)
		.def("getIndex",       &CyInfo::getIndex)
		.def("getEdgeIds",     &CyInfo::getEdgeIds)
		.def("getWellbeing",   &CyInfo::getWellbeing)
		.def("getRevolution",  &CyInfo::getRevolution)
		.def("getProductionToCommerce", &CyInfo::getProductionToCommerce)
		.def("hasSkill",           &CyInfo::hasSkill)
		.def("hasTag",             &CyInfo::hasTag)
		.def("hasAttribute",       &CyInfo::hasAttribute)
		.def("hasCharacteristic",  &CyInfo::hasCharacteristic)
		.def("providesAmenity",    &CyInfo::providesAmenity)
		.def("providesCapability", &CyInfo::providesCapability)
		.def("providesPolicy", &CyInfo::providesPolicy)
		.def("revokesSkill",       &CyInfo::revokesSkill)
		.def("canTradeItem",   &CyInfo::canTradeItem)
		.def("getScalar",      &CyInfo::getScalar)
		.def("getIntrinsic",   &CyInfo::getIntrinsic)
		.def("getIdList", &CyInfo::getIdList)
		.def("civicOptions",   &CyInfo::civicOptions, python::return_value_policy<python::reference_existing_object>())
		;
}
