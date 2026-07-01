//
//	CvJsonBuildingInfo::mapFrom -- common sections (base) + the building `identity` block: the StoneBase BuildingInfo
//	flags (notConstructible / governmentCenter / forceNoPrereqScaling / specialBuilding) + the shrine/corpHQ/religion FKs
//	+ the stateReligionCommerce / commerceDoubleTime maps. SELF-CONTAINED (the engine getReligionType / getGlobal*Commerce
//	reads are RETIRED). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonBuildingInfo.h"
#include "CvCascadeJsonParse.h"     // cascadeJsonResolveId / cascadeJsonCommerceMap

void CvJsonBuildingInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it;
	// TOP-LEVEL bespoke FK sections (migrated OUT of identity, owner 2026-07-01): `shrine` -> religion FK,
	// `headquarters` -> corporation FK (was identity.shrine / identity.corporationHQ). Feed the shrine/corp-HQ commerce calc.
	if ((it = o.find("shrine")) != o.end() && it->second.is<std::string>())       shrineReligion = cascadeJsonResolveId(it->second.get<std::string>());
	if ((it = o.find("headquarters")) != o.end() && it->second.is<std::string>()) corpHQ = cascadeJsonResolveId(it->second.get<std::string>());
	// The `attributes` classification block (migrated OUT of identity): governmentCenter feeds the IS_GOVERNMENT_CENTER predicate.
	picojson::object::const_iterator at = o.find("attributes");
	if (at != o.end() && at->second.is<picojson::object>())
	{
		const picojson::object& ao = at->second.get<picojson::object>();
		if ((it = ao.find("governmentCenter")) != ao.end() && it->second.is<bool>()) governmentCenter = it->second.get<bool>();
	}
	// The `identity` block: the fields that STAYED intrinsic -- state-religion FK, the cascade commerce markers, buildability flags.
	picojson::object::const_iterator id = o.find("identity");
	if (id == o.end() || !id->second.is<picojson::object>()) return;
	const picojson::object& io = id->second.get<picojson::object>();
	if ((it = io.find("religion")) != io.end() && it->second.is<std::string>())      religion = cascadeJsonResolveId(it->second.get<std::string>());
	if ((it = io.find("stateReligionCommerce")) != io.end()) cascadeJsonCommerceMap(it->second, stateReligionCommerce);
	if ((it = io.find("commerceDoubleTime")) != io.end())    cascadeJsonCommerceMap(it->second, commerceDoubleTime);
	if ((it = io.find("notConstructible")) != io.end() && it->second.is<bool>())     notConstructible = it->second.get<bool>();
	if ((it = io.find("forceNoPrereqScaling")) != io.end() && it->second.is<bool>()) forceNoPrereqScaling = it->second.get<bool>();
	if ((it = io.find("specialBuilding")) != io.end() && it->second.is<std::string>()) specialBuildingType = it->second.get<std::string>();
}
