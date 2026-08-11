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
#include "Infos/CvClassificationIds.h"        // the GENERATED CLS_* ids the NAMED endpoints resolve internally
#include "Engine/CvCity.h"       // the what-if's CityContext + plot group
#include "Engine/CvPlayer.h"     // the what-if's EmpireContext
#include "AI/CvPlayerAI.h"       // GET_PLAYER -- the player resolve behind the (player, city) address
#include "Infos/CvUnitInfo.h"                 // the spread block behind the named canSpreadReligion endpoint
#include "Infos/CvCivicInfo.h"       // the civic column the bulk index reads
// The straggler dispatch reaches these concrete registries -- specific headers, never the CvInfos.h umbrella.
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvPromotionInfo.h"      // the promotion classification endpoints
#include "Infos/CvPromotionLineInfo.h"  // isBuildUp lives on the LINE, not the rung
#include "Infos/CvHurryInfo.h"          // HURRY_ -- the hurry list's own numbers
#include "Defines/CvDiplomacyClasses.h" // DIPLOMACY_ -- the response set the diplo screen filters
#include "Infos/CvUnitInfo.h"             // getGrantedBuildings -- the unit's MISSION_CONSTRUCT repertoire
#include "Infos/CvAllowed.h"
#include "Infos/CvHandicapInfo.h"         // HANDICAP_ -- the civic-upkeep difficulty scaler
#include "Infos/CvSpecialBuildingInfo.h"   // the GROUP that may hold the cap for its members
#include "Infos/CvBonusInfo.h"
#include "Infos/CvImprovementInfo.h"      // IMPROVEMENT_ -- the pillage-gold intrinsic
#include "Infos/CvSpecialistInfo.h"
#include "Infos/CvYieldInfo.h"
#include "Infos/CvControlInfo.h"
#include "Infos/CvCommandInfo.h"          // COMMAND_ -- the sibling fixed-enum registry beside CONTROL_
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
#include "Infos/CvPropertyInfo.h"   // PROPERTY_ -- the city property list's names
#include "Infos/CvProjectInfo.h"   // isSpaceship -- the build-progress readout (PYINT_IS_SPACESHIP)
#include "Infos/CvVictoryInfo.h"   // isPermanent -- the scenario victory-list filter
#include "Infos/CvTechInfo.h"      // isRepeat -- the scenario repeat-tech loop (PYINT_IS_REPEAT)
#include "Infos/CvVoteSourceInfo.h"
#include "Infos/CvInvisibleInfo.h"
#include "Infos/CvEventInfo.h"
#include "Infos/CvEventTriggerInfo.h"
// Both reach the identity plane by the derived->base conversion, which the compiler can only check against a
// COMPLETE type -- a forward declaration compiles everywhere else and fails exactly at that return.
#include "Infos/CvEffectInfo.h"
#include "Infos/CvMapInfo.h"
#include "Defines/CvDiplomacyClasses.h"
// The AUTHORED IDENTITY TEXTS that are not on CvInfoBase -- the civilization's name forms, and the two
// key-backed siblings ([json.md] §7).
#include "Infos/CvCivilizationInfo.h"
#include "Infos/CvFeatureInfo.h"
#include "Infos/CvReligionInfo.h"
#include "Infos/CvTraitInfo.h"
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
		if (szTypePrefix == "COMMAND_")     return iId < NUM_COMMAND_TYPES   ? &GC.getCommandInfo((CommandTypes)iId) : NULL;
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
		if (szTypePrefix == "PROPERTY_")         return iId < GC.getNumPropertyInfos()         ? &GC.getPropertyInfo((PropertyTypes)iId) : NULL;
		if (szTypePrefix == "GOODY_")            return iId < GC.getNumGoodyInfos()            ? &GC.getGoodyInfo((GoodyTypes)iId) : NULL;
		if (szTypePrefix == "UPKEEP_")           return iId < GC.getNumUpkeepInfos()           ? &GC.getUpkeepInfo((UpkeepTypes)iId) : NULL;
		if (szTypePrefix == "VOTESOURCE_")       return iId < GC.getNumVoteSourceInfos()       ? &GC.getVoteSourceInfo((VoteSourceTypes)iId) : NULL;
		if (szTypePrefix == "INVISIBLE_")        return iId < GC.getNumInvisibleInfos()        ? &GC.getInvisibleInfo((InvisibleTypes)iId) : NULL;
		if (szTypePrefix == "CONCEPT_")          return iId < GC.getNumConceptInfos()          ? &GC.getConceptInfo((ConceptTypes)iId) : NULL;
		if (szTypePrefix == "EVENT_")            return iId < GC.getNumEventInfos()            ? &GC.getEventInfo((EventTypes)iId) : NULL;
		if (szTypePrefix == "EVENTTRIGGER_")     return iId < GC.getNumEventTriggerInfos()     ? &GC.getEventTriggerInfo((EventTriggerTypes)iId) : NULL;
		if (szTypePrefix == "EFFECT_")           return iId < GC.getNumEffectInfos()           ? &GC.getEffectInfo(iId) : NULL;
		//	The MULTIMAP registry -- the maps a game may hold, not a map SCRIPT (those are their own boundary and
		//	are not served here, [python-read-map.md] par.7).
		if (szTypePrefix == "MAP_")              return iId < GC.getNumMapInfos()              ? &GC.getMapInfo((MapTypes)iId) : NULL;
		// ⛔ THE PREFIX ROUTES A REGISTRY, AND USUALLY -- BUT NOT ALWAYS -- EQUALS THE AUTHORED INFOTYPE PREFIX
		// ([naming.md]). Two registries below break that coincidence, so the token names the REGISTRY:
		//   · DIPLOMACY_  holds TWO authored prefixes, AI_DIPLOCOMMENT_* and USER_DIPLOCOMMENT_*, so no single
		//     authored prefix addresses it -- and CvDiplomacy.py string-compares getType() against both.
		//   · NEWCONCEPT_ authors CONCEPT_*, the SAME prefix the separate Concept registry authors, so routing on
		//     the authored token would silently answer from the wrong registry. That collision is precisely why
		//     this one was unreachable rather than merely unwired.
		if (szTypePrefix == "DIPLOMACY_")        return iId < GC.getNumDiplomacyInfos()        ? &GC.getDiplomacyInfo(iId) : NULL;
		if (szTypePrefix == "NEWCONCEPT_")       return iId < GC.getNumNewConceptInfos()       ? &GC.getNewConceptInfo((NewConceptTypes)iId) : NULL;
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

std::wstring CyInfo::getHelp(const std::string& szTypePrefix, int iId) const
{
	const CvInfoBase* pInfo = cyi_infoBase(szTypePrefix, iId);
	return pInfo ? std::wstring(pInfo->getHelp()) : std::wstring();
}

//	⚑ RESOLVED TEXT, like every bare identity read here ([patterns.md]: a `*Key` returns a key, the bare form
//	returns text). Where a type stores the authored value as a TXT_KEY rather than a resolved form, the key is
//	resolved HERE -- the library returns localized display strings for what it serves, which is the sanctioned
//	shape; handing a raw key back under a bare name is what puts one in front of a player.
//	⚠ uiForm selects the grammatical variant and only CIVILIZATION_ carries the forms; the key-backed types hold
//	a single authored string, so the form has nothing to select there.
std::wstring CyInfo::getAdjective(const std::string& szTypePrefix, int iId, int iForm) const
{
	if (iId < 0) return std::wstring();

	if (szTypePrefix == "CIVILIZATION_" && iId < GC.getNumCivilizationInfos())
	{
		return std::wstring(GC.getCivilizationInfo((CivilizationTypes)iId).getAdjective((uint)std::max(0, iForm)));
	}
	if (szTypePrefix == "RELIGION_" && iId < GC.getNumReligionInfos())
	{
		return std::wstring(gDLL->getText(CvString(GC.getReligionInfo((ReligionTypes)iId).getAdjectiveKey()).c_str()));
	}
	return std::wstring();
}

std::wstring CyInfo::getShortDescription(const std::string& szTypePrefix, int iId, int iForm) const
{
	if (iId < 0) return std::wstring();

	if (szTypePrefix == "CIVILIZATION_" && iId < GC.getNumCivilizationInfos())
	{
		return std::wstring(GC.getCivilizationInfo((CivilizationTypes)iId).getShortDescription((uint)std::max(0, iForm)));
	}
	if (szTypePrefix == "TRAIT_" && iId < GC.getNumTraitInfos())
	{
		return std::wstring(gDLL->getText(CvString(GC.getTraitInfo((TraitTypes)iId).getShortDescriptionKey()).c_str()));
	}
	return std::wstring();
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

std::string CyInfo::getPediaCategory(const std::string& szTypePrefix, int iId) const
{
	//	The JSON base, not CvInfoBase: the field is on CvInfo, so an XML-only registry answers empty -- which is
	//	the ordinary bucket, i.e. the same answer an uncategorised entity gives.
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	if (pInfo == NULL) return std::string();
	const char* szCategory = pInfo->getPediaCategory();
	return szCategory ? std::string(szCategory) : std::string();
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
		values.append(pInfo ? pInfo->modifier(MODFAM_REVOLUTION, iKind, (CvCascScope)iScope,
			infoKindUnit(MODFAM_REVOLUTION, iKind, (CvCascScope)iScope)) : 0);
	}
	return values;
}

python::list CyInfo::getMovementKinds(const std::string& szTypePrefix, int iId, int iScope) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	for (int iKind = 0; iKind < NUM_MOVEMENT_KINDS; ++iKind)
	{
		// An entity that authors none answers 0 across the group -- a total read, never an error.
		values.append(pInfo ? pInfo->modifier(MODFAM_MOVEMENT, iKind, (CvCascScope)iScope,
			infoKindUnit(MODFAM_MOVEMENT, iKind, (CvCascScope)iScope)) : 0);
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
//
//	The NAMED classification tests -- the Python CONSUMER surface (the header's owner ruling). Each is a one-line
//	delegate to the parameterized read above, so the bitset walk has exactly ONE implementation
//	([DEC-single-implementation]) and a named endpoint costs a line rather than a second code path.
//	⚑ Note the two answer off DIFFERENT PLANES -- hidden nationality is a unit SKILL, spy is a unit TAG -- which
//	is precisely what the naming buys: the caller asks its question and never learns which plane holds it.
//
bool CyInfo::isHiddenNationality(int iUnitId) const
{
	return hasSkill("UNIT_", iUnitId, CLS_SKILL_HIDDEN_NATIONALITY);
}
bool CyInfo::isSpy(int iUnitId) const
{
	return hasTag("UNIT_", iUnitId, CLS_TAG_SPY);
}
bool CyInfo::isAnimal(int iUnitId) const
{
	return hasTag("UNIT_", iUnitId, CLS_TAG_ANIMAL);
}
int CyInfo::getUnitAirCombat(int iUnitId) const
{
	const CvUnitInfo* pUnit = static_cast<const CvUnitInfo*>(cyi_info("UNIT_", iUnitId));
	return pUnit ? pUnit->getAirCombat() : 0;
}
bool CyInfo::canSpreadReligion(int iUnitId) const
{
	const CvUnitInfo* pUnit = static_cast<const CvUnitInfo*>(cyi_info("UNIT_", iUnitId));
	return pUnit ? !pUnit->getReligionSpread().empty() : false;
}
//	The SPECIFIC twin, and the pair is deliberate: "spreads anything" is the MISSIONARY test, while a
//	recommender picking a missionary for the empire's own faith needs "spreads THIS one". Answering the
//	specific question with the general read offers a unit that spreads somebody else's religion.
//	⚑ It reads the unit's own authored `spread.religion` map by key -- never a sweep of the religion registry
//	asking each id whether this unit spreads it, which is the own-data inversion [DEC-one-reverse-view] bans
//	and which the info's own accessor comment warns about.
bool CyInfo::spreadsReligion(int iUnitId, int iReligion) const
{
	if (iReligion < 0) return false;
	const CvUnitInfo* pUnit = static_cast<const CvUnitInfo*>(cyi_info("UNIT_", iUnitId));
	if (pUnit == NULL) return false;
	const std::map<int, int>& kSpread = pUnit->getReligionSpread();
	const std::map<int, int>::const_iterator it = kSpread.find(iReligion);
	return it != kSpread.end() && it->second > 0;
}
//	Is this unit WORLD-UNIQUE? The unit twin of the building's cap-scope read: a `world` self-cap is what makes a
//	unit world-unique, exactly as a cap's SCOPE is what makes a building a world / team / national wonder
//	([json.md] §4.4). Units carry world/empire caps only -- there is no team cap for a unit.
bool CyInfo::isWorldUnit(int iUnitId) const
{
	const CvUnitInfo* pUnit = static_cast<const CvUnitInfo*>(cyi_info("UNIT_", iUnitId));
	return pUnit ? pUnit->getAllowed()->cap(ALLOWEDCAP_WORLD) >= 0 : false;
}
//	Does this building play a completion MOVIE -- the whole of what the wonder-movie popup gates on. The screen
//	that follows resolves the ART_DEF_MOVIE_* tag itself, so the boundary hands over a verdict and never a tag.
bool CyInfo::hasMovie(int iBuildingId) const
{
	const CvBuildingInfo* pBuilding = static_cast<const CvBuildingInfo*>(cyi_info("BUILDING_", iBuildingId));
	return pBuilding ? pBuilding->hasMovie() : false;
}
bool CyInfo::isGlobalTech(int iTechId) const
{
	const CvInfo* pTech = cyi_info("TECH_", iTechId);
	if (pTech == NULL || pTech->getAllowed() == NULL) return false;
	return pTech->getAllowed()->cap(ALLOWEDCAP_WORLD) >= 0;
}
bool CyInfo::hasUnitInstanceCap(int iUnitId) const
{
	const CvInfo* pUnit = cyi_info("UNIT_", iUnitId);
	if (pUnit == NULL || pUnit->getAllowed() == NULL) return false;
	const CvAllowed* pAllowed = pUnit->getAllowed();
	//	Units carry no TEAM cap ([json.md] §4.4), so the two scopes below are the whole of it.
	return pAllowed->cap(ALLOWEDCAP_WORLD) >= 0 || pAllowed->cap(ALLOWEDCAP_EMPIRE) >= 0;
}
bool CyInfo::providesBonus(const std::string& szTypePrefix, int iId, int iBonusId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	if (pInfo == NULL || pInfo->getProvides() == NULL) return false;
	return pInfo->getProvides()->has(iBonusId);
}
bool CyInfo::isStatusPromotion(int iPromotionId) const
{
	const CvPromotionInfo* pPromotion = static_cast<const CvPromotionInfo*>(cyi_info("PROMOTION_", iPromotionId));
	return pPromotion ? pPromotion->isStatus() : false;
}
bool CyInfo::isBuildUpPromotion(int iPromotionId) const
{
	const CvPromotionInfo* pPromotion = static_cast<const CvPromotionInfo*>(cyi_info("PROMOTION_", iPromotionId));
	if (pPromotion == NULL) return false;
	//	The flag is the LINE's, so the hop lives here rather than in every caller -- and a promotion on no line
	//	answers false rather than resolving a -1 id.
	const PromotionLineTypes eLine = pPromotion->getPromotionLine();
	if (eLine == NO_PROMOTIONLINE) return false;
	const CvPromotionLineInfo* pLine =
		static_cast<const CvPromotionLineInfo*>(cyi_info("PROMOTIONLINE_", (int)eLine));
	return pLine ? pLine->isBuildUp() : false;
}
bool CyInfo::isShrine(int iBuildingId) const
{
	const CvBuildingInfo* pBuilding = static_cast<const CvBuildingInfo*>(cyi_info("BUILDING_", iBuildingId));
	return pBuilding ? pBuilding->getShrineReligion() >= 0 : false;
}
bool CyInfo::isReligiousBuilding(int iBuildingId) const
{
	const CvBuildingInfo* pBuilding = static_cast<const CvBuildingInfo*>(cyi_info("BUILDING_", iBuildingId));
	return pBuilding ? pBuilding->getReligion() >= 0 : false;
}
bool CyInfo::isAutoBuild(int iBuildingId) const
{
	const CvBuildingInfo* pBuilding = static_cast<const CvBuildingInfo*>(cyi_info("BUILDING_", iBuildingId));
	return pBuilding ? pBuilding->isAutoBuild() : false;
}
bool CyInfo::providesNukeImmunity(int iBuildingId) const
{
	return providesAmenity("BUILDING_", iBuildingId, CLS_AMENITY_NUKE_IMMUNE);
}
bool CyInfo::providesCapitalStatus(int iBuildingId) const
{
	return providesAmenity("BUILDING_", iBuildingId, CLS_AMENITY_CAPITAL);
}
//	The handicap's civic-upkeep percentage. The HUMAN audience is read deliberately: the caller weighs one
//	player's difficulty against another's, and the AI sibling is a separate stage the engine applies itself
//	(CvHandicapInfo -- every point getter carries its own audience).
//	⚠ 0 when the handicap is unknown, which the caller must not divide by.
int CyInfo::getHandicapCivicUpkeepPercent(int iHandicapId) const
{
	const CvHandicapInfo* pHandicap = static_cast<const CvHandicapInfo*>(cyi_info("HANDICAP_", iHandicapId));
	return pHandicap ? pHandicap->getUpkeepModifier(UPKEEP_CIVIC, CASC_SCOPE_EMPIRE, false) : 0;
}

int CyInfo::getCivicUpkeep(int iCivicId) const
{
	const CvCivicInfo* pCivic = static_cast<const CvCivicInfo*>(cyi_info("CIVIC_", iCivicId));
	return pCivic ? pCivic->getUpkeepLevel() : -1;
}

int CyInfo::getAllowedCap(const std::string& szTypePrefix, int iId, int iCap) const
{
	if (iCap < 0 || iCap >= NUM_ALLOWEDCAP) return -1;

	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	if (pInfo == NULL) return -1;

	//	An entity that authors no `allowed` block is UNCAPPED, which is the same answer as an unauthored cap --
	//	so both return -1 rather than one of them being an error the caller has to tell apart.
	const CvAllowed* pAllowed = pInfo->getAllowed();
	if (pAllowed == NULL) return -1;

	return pAllowed->cap((EnAllowedCap)iCap);
}

int CyInfo::getFeatureGrowthProbability(int iFeatureId) const
{
	const CvFeatureInfo* pFeature = static_cast<const CvFeatureInfo*>(cyi_info("FEATURE_", iFeatureId));
	return pFeature ? pFeature->getGrowthProbability() : 0;
}

int CyInfo::getFeatureDisappearanceProbability(int iFeatureId) const
{
	const CvFeatureInfo* pFeature = static_cast<const CvFeatureInfo*>(cyi_info("FEATURE_", iFeatureId));
	return pFeature ? pFeature->getDisappearanceProbability() : 0;
}

python::list CyInfo::getCivilizationLeaders(int iCivilizationId) const
{
	python::list lIds;
	const CvCivilizationInfo* pCiv =
		static_cast<const CvCivilizationInfo*>(cyi_info("CIVILIZATION_", iCivilizationId));
	if (pCiv == NULL) return lIds;

	const std::vector<LeaderHeadTypes>& leaders = pCiv->getLeaders();
	for (size_t i = 0; i < leaders.size(); ++i)
	{
		lIds.append((int)leaders[i]);
	}
	return lIds;
}

python::list CyInfo::getCivilizationCityNames(int iCivilizationId) const
{
	python::list lNames;
	const CvCivilizationInfo* pCiv =
		static_cast<const CvCivilizationInfo*>(cyi_info("CIVILIZATION_", iCivilizationId));
	if (pCiv == NULL) return lNames;

	for (int iName = 0; iName < pCiv->getNumCityNames(); ++iName)
	{
		lNames.append(pCiv->getCityName(iName));
	}
	return lNames;
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

//	The DIPLOMACY_ response plane. The registry is XML-side and its data is intact; only the Python read was
//	missing, which raised at the first contact and took the whole screen's option list with it.
//	⚠ Bounds are answered, not asserted: the caller walks these indices to find out which responses apply, so an
//	out-of-range index is an ordinary "no" rather than a fault.
static const CvDiplomacyInfo* cyi_diplomacyResponse(int iComment, int iResponse)
{
	if (iComment < 0 || iComment >= GC.getNumDiplomacyInfos())
	{
		return NULL;
	}
	const CvDiplomacyInfo& kInfo = GC.getDiplomacyInfo(iComment);
	if (iResponse < 0 || iResponse >= kInfo.getNumResponses())
	{
		return NULL;
	}
	return &kInfo;
}

int CyInfo::getDiplomacyNumResponses(int iComment) const
{
	if (iComment < 0 || iComment >= GC.getNumDiplomacyInfos())
	{
		return 0;
	}
	return GC.getDiplomacyInfo(iComment).getNumResponses();
}

bool CyInfo::getDiplomacyResponseAttitude(int iComment, int iResponse, int iAttitude) const
{
	const CvDiplomacyInfo* pInfo = cyi_diplomacyResponse(iComment, iResponse);
	return pInfo != NULL && iAttitude >= 0 && iAttitude < NUM_ATTITUDE_TYPES
	    && pInfo->getAttitudeTypes(iResponse, iAttitude);
}

bool CyInfo::getDiplomacyResponseCivilization(int iComment, int iResponse, int iCivilization) const
{
	const CvDiplomacyInfo* pInfo = cyi_diplomacyResponse(iComment, iResponse);
	return pInfo != NULL && iCivilization >= 0 && iCivilization < GC.getNumCivilizationInfos()
	    && pInfo->getCivilizationTypes(iResponse, iCivilization);
}

bool CyInfo::getDiplomacyResponseLeaderHead(int iComment, int iResponse, int iLeaderHead) const
{
	const CvDiplomacyInfo* pInfo = cyi_diplomacyResponse(iComment, iResponse);
	return pInfo != NULL && iLeaderHead >= 0 && iLeaderHead < GC.getNumLeaderHeadInfos()
	    && pInfo->getLeaderHeadTypes(iResponse, iLeaderHead);
}

bool CyInfo::getDiplomacyResponsePower(int iComment, int iResponse, int iPower) const
{
	const CvDiplomacyInfo* pInfo = cyi_diplomacyResponse(iComment, iResponse);
	return pInfo != NULL && iPower >= 0 && iPower < NUM_DIPLOMACYPOWER_TYPES
	    && pInfo->getDiplomacyPowerTypes(iResponse, iPower);
}

int CyInfo::getDiplomacyNumText(int iComment, int iResponse) const
{
	const CvDiplomacyInfo* pInfo = cyi_diplomacyResponse(iComment, iResponse);
	return pInfo != NULL ? pInfo->getNumDiplomacyText(iResponse) : 0;
}

std::string CyInfo::getDiplomacyText(int iComment, int iResponse, int iText) const
{
	const CvDiplomacyInfo* pInfo = cyi_diplomacyResponse(iComment, iResponse);
	if (pInfo == NULL || iText < 0 || iText >= pInfo->getNumDiplomacyText(iResponse))
	{
		return std::string();
	}
	const char* szText = pInfo->getDiplomacyText(iResponse, iText);
	return szText != NULL ? std::string(szText) : std::string();
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
	case PYLIST_PREREQ_EVENTS:
		{
			if (szTypePrefix != "EVENTTRIGGER_" || iId >= GC.getNumEventTriggerInfos()) break;
			const CvEventTriggerInfo& kTrigger = GC.getEventTriggerInfo((EventTriggerTypes)iId);
			for (int i = 0; i < kTrigger.getNumPrereqEvents(); ++i) ids.append(kTrigger.getPrereqEvent(i));
		}
		break;
	default:
		break;
	}
	return ids;
}

python::list CyInfo::getFlatYields(const std::string& szTypePrefix, int iId, int iScope) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		// An entity that authors none answers 0 across the group -- a total read, never an error.
		values.append(pInfo ? pInfo->modifier(infoYieldFamily((YieldTypes)iYield), CHANNEL_AMOUNT,
			(CvCascScope)iScope, CASC_UNIT_FLAT) : 0);
	}
	return values;
}

python::list CyInfo::getFlatCommerces(const std::string& szTypePrefix, int iId, int iScope) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		values.append(pInfo ? pInfo->modifier(infoCommerceFamily(iCommerce), CHANNEL_AMOUNT,
			(CvCascScope)iScope, CASC_UNIT_FLAT) : 0);
	}
	return values;
}

namespace
{
	// The what-if's three inputs, resolved from the (player, city) address every read on this surface takes.
	// Answers false when either half does not resolve, so a caller gets an empty list rather than a zero it
	// would read as a real verdict.
	bool cyi_valuationCtx(int iPlayer, int iCity,
		const CvCity** ppCity, const CvPlayer** ppOwner)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS || iCity < 0) return false;
		const CvPlayer& kOwner = GET_PLAYER((PlayerTypes)iPlayer);
		const CvCity* pCity = kOwner.getCity(iCity);
		if (pCity == NULL) return false;
		*ppCity = pCity;
		*ppOwner = &kOwner;
		return true;
	}
}

python::list CyInfo::expectedWellbeing(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	const CvCity* pCity = NULL;
	const CvPlayer* pOwner = NULL;
	if (pInfo == NULL || !cyi_valuationCtx(iPlayer, iCity, &pCity, &pOwner)) return values;
	int aChannels[NUM_WELLBEING_CHANNELS];
	pInfo->expectedWellbeing(pCity->getCityContext(), pOwner->getEmpireContext(),
		pCity->plotGroup((PlayerTypes)iPlayer), aChannels);
	for (int i = 0; i < NUM_WELLBEING_CHANNELS; ++i) values.append(aChannels[i]);
	return values;
}

python::list CyInfo::expectedFlatYields(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	const CvCity* pCity = NULL;
	const CvPlayer* pOwner = NULL;
	if (pInfo == NULL || !cyi_valuationCtx(iPlayer, iCity, &pCity, &pOwner)) return values;
	int aYields[NUM_YIELD_TYPES];
	pInfo->expectedFlatYields(pCity->getCityContext(), pOwner->getEmpireContext(),
		pCity->plotGroup((PlayerTypes)iPlayer), aYields);
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) values.append(aYields[i]);
	return values;
}

python::list CyInfo::expectedPlotYields(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	const CvCity* pCity = NULL;
	const CvPlayer* pOwner = NULL;
	if (pInfo == NULL || !cyi_valuationCtx(iPlayer, iCity, &pCity, &pOwner)) return values;
	int aYields[NUM_YIELD_TYPES];
	pInfo->expectedPlotYields(pCity->getCityContext(), pOwner->getEmpireContext(),
		pCity->plotGroup((PlayerTypes)iPlayer), aYields);
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) values.append(aYields[i]);
	return values;
}

python::list CyInfo::expectedFlatCommerces(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	const CvCity* pCity = NULL;
	const CvPlayer* pOwner = NULL;
	if (pInfo == NULL || !cyi_valuationCtx(iPlayer, iCity, &pCity, &pOwner)) return values;
	int aCommerce[NUM_COMMERCE_TYPES];
	pInfo->expectedFlatCommerce(pCity->getCityContext(), pOwner->getEmpireContext(),
		pCity->plotGroup((PlayerTypes)iPlayer), aCommerce);
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) values.append(aCommerce[i]);
	return values;
}

python::list CyInfo::expectedCommerceModifiers(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	const CvCity* pCity = NULL;
	const CvPlayer* pOwner = NULL;
	if (pInfo == NULL || !cyi_valuationCtx(iPlayer, iCity, &pCity, &pOwner)) return values;
	int aCommerce[NUM_COMMERCE_TYPES];
	pInfo->expectedCommerceModifiers(pCity->getCityContext(), pOwner->getEmpireContext(),
		pCity->plotGroup((PlayerTypes)iPlayer), aCommerce);
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) values.append(aCommerce[i]);
	return values;
}

python::list CyInfo::expectedDefenseKinds(const std::string& szTypePrefix, int iId, int iPlayer, int iCity) const
{
	python::list values;
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	const CvCity* pCity = NULL;
	const CvPlayer* pOwner = NULL;
	if (pInfo == NULL || !cyi_valuationCtx(iPlayer, iCity, &pCity, &pOwner)) return values;
	int aKinds[NUM_DEFENSE_KINDS];
	pInfo->expectedDefenseKinds(pCity->getCityContext(), pOwner->getEmpireContext(),
		pCity->plotGroup((PlayerTypes)iPlayer), aKinds);
	for (int i = 0; i < NUM_DEFENSE_KINDS; ++i) values.append(aKinds[i]);
	return values;
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
		//	⚠ EVERY buildable registry routes here, and the reason is that an unrouted prefix does not fail --
		//	it falls through to the shared -1, which reads as a real cost. A consumer testing `cost <= 0` then
		//	classifies the whole registry as free, and a consumer multiplying by it hands out a negative price.
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
			return GC.getBuildingInfo((BuildingTypes)iId).getCost();
		if (szTypePrefix == "UNIT_" && iId < GC.getNumUnitInfos())
			return GC.getUnitInfo((UnitTypes)iId).getProductionCost();
		if (szTypePrefix == "PROJECT_" && iId < GC.getNumProjectInfos())
			return GC.getProjectInfo((ProjectTypes)iId).getProductionCost();
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

	case PYINT_PILLAGE_GOLD:
		if (szTypePrefix == "IMPROVEMENT_" && iId < GC.getNumImprovementInfos())
			return GC.getImprovementInfo((ImprovementTypes)iId).getPillageGold();
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

	//	⚠ The INFO-side getter is still named for the legacy XML tag (`getDiploVoteType`); the slot is named for
	//	what the value IS, which is the direction the rename goes ([todo.md]: diploVoteType -> the `voteSource`
	//	section). A consumer therefore never learns the legacy spelling.
	case PYINT_VOTE_SOURCE:
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
			return GC.getBuildingInfo((BuildingTypes)iId).getDiploVoteType();
		break;

	case PYINT_IS_SPACESHIP:
		if (szTypePrefix == "PROJECT_" && iId < GC.getNumProjectInfos())
			return GC.getProjectInfo((ProjectTypes)iId).isSpaceship() ? 1 : 0;
		break;

	case PYINT_ERA:
		if (szTypePrefix == "TECH_" && iId < GC.getNumTechInfos())
			return GC.getTechInfo((TechTypes)iId).getEra();
		break;

	case PYINT_IS_DISABLED:
		if (szTypePrefix == "TECH_" && iId < GC.getNumTechInfos())
			return GC.getTechInfo((TechTypes)iId).isDisable() ? 1 : 0;
		break;

	case PYINT_UNIT_COMBAT:
		if (szTypePrefix == "UNIT_" && iId < GC.getNumUnitInfos())
			return GC.getUnitInfo((UnitTypes)iId).getCombatClass();
		break;

	case PYINT_HURRY_GOLD_PER_PRODUCTION:
		if (szTypePrefix == "HURRY_" && iId < GC.getNumHurryInfos())
			return GC.getHurryInfo((HurryTypes)iId).getGoldPerProduction();
		break;

	case PYINT_HURRY_PRODUCTION_PER_POPULATION:
		if (szTypePrefix == "HURRY_" && iId < GC.getNumHurryInfos())
			return GC.getHurryInfo((HurryTypes)iId).getProductionPerPopulation();
		break;

	case PYINT_HURRY_IS_ANGER:
		if (szTypePrefix == "HURRY_" && iId < GC.getNumHurryInfos())
			return GC.getHurryInfo((HurryTypes)iId).causesAnger() ? 1 : 0;
		break;

	case PYINT_ESPIONAGE_COST:
		if (szTypePrefix == "ESPIONAGEMISSION_" && iId < GC.getNumEspionageMissionInfos())
			return GC.getEspionageMissionInfo((EspionageMissionTypes)iId).getCost();
		break;

	case PYINT_ESPIONAGE_TARGETS_CITY:
		if (szTypePrefix == "ESPIONAGEMISSION_" && iId < GC.getNumEspionageMissionInfos())
			return GC.getEspionageMissionInfo((EspionageMissionTypes)iId).isTargetsCity() ? 1 : 0;
		break;

	case PYINT_ESPIONAGE_IS_PASSIVE:
		if (szTypePrefix == "ESPIONAGEMISSION_" && iId < GC.getNumEspionageMissionInfos())
			return GC.getEspionageMissionInfo((EspionageMissionTypes)iId).isPassive() ? 1 : 0;
		break;

	case PYINT_ESPIONAGE_TECH_PREREQ:
		if (szTypePrefix == "ESPIONAGEMISSION_" && iId < GC.getNumEspionageMissionInfos())
			return (int)GC.getEspionageMissionInfo((EspionageMissionTypes)iId).getTechPrereq();
		break;

	case PYINT_IS_BUILD:
		if (szTypePrefix == "MISSION_" && iId < GC.getNumMissionInfos())
			return GC.getMissionInfo((MissionTypes)iId).isBuild() ? 1 : 0;
		break;

	case PYINT_SPECIAL_BUILDING:
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
			return GC.getBuildingInfo((BuildingTypes)iId).getSpecialBuildingType();
		break;

	case PYINT_DOMAIN:
		if (szTypePrefix == "UNIT_" && iId < GC.getNumUnitInfos())
			return GC.getUnitInfo((UnitTypes)iId).getDomain();
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
		//	The ART-ONLY marker. It is on CvInfoBase, so it is total across both halves of the dispatch, and it
		//	belongs on the INDEX because its only consumer is a LISTING deciding what to show -- an entity that
		//	exists solely to carry art is not something a player can be shown a page for.
		kEntry["graphicalOnly"] = pInfo->isGraphicalOnly();
		//	The pedia BUCKET, on the index because the pedia builds LISTS: filtering a category is then one
		//	crossing over the registry rather than a per-entity ask. Empty for an XML-only registry (the field is
		//	on the JSON base) and for an uncategorised entity -- both mean the ordinary bucket.
		const CvInfo* pJson = cyi_info(szTypePrefix, iId);
		kEntry["pediaCategory"] = std::string(pJson != NULL && pJson->getPediaCategory() != NULL
		                                      ? pJson->getPediaCategory() : "");
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
		.def("getHelp",        &CyInfo::getHelp)
		.def("getAdjective",        &CyInfo::getAdjective)
		.def("getShortDescription", &CyInfo::getShortDescription)
		.def("getType",        &CyInfo::getType)
		.def("getButton",      &CyInfo::getButton)
		.def("getPediaCategory", &CyInfo::getPediaCategory)
		.def("exists",         &CyInfo::exists)
		.def("getIndex",       &CyInfo::getIndex)
		.def("getEdgeIds",     &CyInfo::getEdgeIds)
		.def("getWellbeing",   &CyInfo::getWellbeing)
		.def("getRevolution",  &CyInfo::getRevolution)
		.def("getProductionToCommerce", &CyInfo::getProductionToCommerce)
		.def("getMovementKinds", &CyInfo::getMovementKinds)
		.def("hasSkill",           &CyInfo::hasSkill)
		.def("hasTag",             &CyInfo::hasTag)
		.def("hasAttribute",       &CyInfo::hasAttribute)
		.def("hasCharacteristic",  &CyInfo::hasCharacteristic)
		.def("providesAmenity",    &CyInfo::providesAmenity)
		.def("providesCapability", &CyInfo::providesCapability)
		.def("providesPolicy", &CyInfo::providesPolicy)
		.def("revokesSkill",       &CyInfo::revokesSkill)
		// The NAMED classification tests -- the Python CONSUMER surface. Endpoint COUNT is deliberately not the
		// target here; being readable at the call site is (owner). Grows one line per call site that asks.
		.def("isHiddenNationality", &CyInfo::isHiddenNationality)
		.def("isSpy",               &CyInfo::isSpy)
		.def("isAnimal",            &CyInfo::isAnimal)
		.def("canSpreadReligion",   &CyInfo::canSpreadReligion)
		.def("spreadsReligion",     &CyInfo::spreadsReligion)
		.def("isWorldUnit",         &CyInfo::isWorldUnit)
		.def("hasUnitInstanceCap",      &CyInfo::hasUnitInstanceCap)
		.def("getUnitAirCombat",        &CyInfo::getUnitAirCombat)
		.def("providesBonus",           &CyInfo::providesBonus)
		.def("isGlobalTech",            &CyInfo::isGlobalTech)
		.def("isStatusPromotion",       &CyInfo::isStatusPromotion)
		.def("isBuildUpPromotion",      &CyInfo::isBuildUpPromotion)
		.def("hasMovie",            &CyInfo::hasMovie)
		.def("isShrine",            &CyInfo::isShrine)
		.def("isReligiousBuilding", &CyInfo::isReligiousBuilding)
		.def("isAutoBuild",         &CyInfo::isAutoBuild)
		.def("providesNukeImmunity",  &CyInfo::providesNukeImmunity)
		.def("providesCapitalStatus", &CyInfo::providesCapitalStatus)
		.def("getHandicapCivicUpkeepPercent", &CyInfo::getHandicapCivicUpkeepPercent)
		.def("canTradeItem",   &CyInfo::canTradeItem)
		.def("getDiplomacyNumResponses",        &CyInfo::getDiplomacyNumResponses)
		.def("getDiplomacyResponseAttitude",    &CyInfo::getDiplomacyResponseAttitude)
		.def("getDiplomacyResponseCivilization",&CyInfo::getDiplomacyResponseCivilization)
		.def("getDiplomacyResponseLeaderHead",  &CyInfo::getDiplomacyResponseLeaderHead)
		.def("getDiplomacyResponsePower",       &CyInfo::getDiplomacyResponsePower)
		.def("getDiplomacyNumText",             &CyInfo::getDiplomacyNumText)
		.def("getDiplomacyText",                &CyInfo::getDiplomacyText)
		.def("getScalar",      &CyInfo::getScalar)
		.def("getIntrinsic",   &CyInfo::getIntrinsic)
		.def("getFlatYields", &CyInfo::getFlatYields)
		.def("getFlatCommerces", &CyInfo::getFlatCommerces)
		.def("expectedWellbeing", &CyInfo::expectedWellbeing)
		.def("expectedFlatYields", &CyInfo::expectedFlatYields)
		.def("expectedPlotYields", &CyInfo::expectedPlotYields)
		.def("expectedFlatCommerces", &CyInfo::expectedFlatCommerces)
		.def("expectedCommerceModifiers", &CyInfo::expectedCommerceModifiers)
		.def("expectedDefenseKinds", &CyInfo::expectedDefenseKinds)
		.def("getIdList", &CyInfo::getIdList)
		.def("getCivicUpkeep", &CyInfo::getCivicUpkeep)
		.def("getAllowedCap", &CyInfo::getAllowedCap)
		.def("getFeatureGrowthProbability", &CyInfo::getFeatureGrowthProbability)
		.def("getFeatureDisappearanceProbability", &CyInfo::getFeatureDisappearanceProbability)
		.def("getCivilizationLeaders", &CyInfo::getCivilizationLeaders)
		.def("getCivilizationCityNames", &CyInfo::getCivilizationCityNames)
		.def("civicOptions",   &CyInfo::civicOptions, python::return_value_policy<python::reference_existing_object>())
		;
}
