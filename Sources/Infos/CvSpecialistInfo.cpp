//
//	CvSpecialistInfo -- the specialist poco's own typed reading on top of the base section dispatch (see the
//	header). mapFrom materializes the census identity set + the property bridge ONCE
//	([DEC-materialize-at-mapfrom]); every output magnitude is a compiled point/conditioned read (header), never
//	a mirrored array. Idempotent by contract (unconditional assigns, clear-first containers).
//

#include "CvGameCoreDLL.h"
#include "AI/CvGameAI.h"   // folder-consolidation: keeps the unity batch self-sufficient (unchanged from the poco era)
#include "CvSpecialistInfo.h"
#include "CvModEntry.h"
#include "CvJsonParse.h"               // jsonChildObj / jsonIdBool / jsonIdStr / jsonReadFlavours / jsonResolveId
#include "Property/CvPropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk

CvSpecialistInfo::CvSpecialistInfo()
	: m_iGreatPeopleUnitType(-1)
	, m_bSlave(false)
	, m_bVisible(false)
	, m_iMissionType(NO_MISSION)
{
}

int CvSpecialistInfo::getFlavorValue(int iFlavor) const
{
	return mapValueOrDefault(m_flavours, iFlavor);
}

void CvSpecialistInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers)

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member. The base map
	// runs FIRST and this type's own members are reset after it -- the documented order every sibling follows.
	m_iGreatPeopleUnitType = -1;
	m_bSlave = false;
	m_bVisible = false;
	m_aiCategories.clear();
	m_flavours.clear();

	// the KEYED experience plane: experience.city.unitCombats.{UNITCOMBAT_*}.flat -- scanned ONCE from the
	// compiled entries (the sanctioned load-time scan source). Read per target; never folded scope-wide.
	m_unitCombatExperience.clear();
	{
		const int iUnitCombatsSeg = modSegmentLookup("unitCombats");
		const std::vector<CvModEntry*>& entries = m_modifiers.entries();
		for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
		{
			const CvModEntry* pEntry = entries[iEntry];
			if (pEntry->family == MODFAM_EXPERIENCE && pEntry->targetSeg == iUnitCombatsSeg
			&&  pEntry->targetFk >= 0 && iUnitCombatsSeg >= 0)
			{
				m_unitCombatExperience[pEntry->targetFk] += pEntry->value;
			}
		}
	}
	m_szTexture.clear();

	// PROPERTY_* per-turn SOURCES: a specialist's <PROPERTY_X>.city.flat (the doctor's disease cut, the
	// law-keeper crime cuts) deposits in ITS city, once per assigned specialist (the city gather count-scales)
	// -- RELATION_SAME_PLOT mirrors the legacy CITY+SAME_PLOT shape. The ONE shared walk over the compiled entries.
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_SAME_PLOT);

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_bSlave = jsonIdBool(*pIdentity, "slave");
		m_bVisible = jsonIdBool(*pIdentity, "visible");
		picojson::object::const_iterator unitIt = pIdentity->find("greatPeopleUnit");
		if (unitIt != pIdentity->end() && unitIt->second.is<std::string>())
		{
			m_iGreatPeopleUnitType = jsonResolveId(unitIt->second.get<std::string>());
		}
		// identity.categories -- CATEGORY_* FK list (assignability/grouping classification; authored on every
		// shipped specialist)
		picojson::object::const_iterator categoriesIt = pIdentity->find("categories");
		if (categoriesIt != pIdentity->end() && categoriesIt->second.is<picojson::array>())
		{
			const picojson::array& categories = categoriesIt->second.get<picojson::array>();
			for (size_t iCategory = 0; iCategory < categories.size(); ++iCategory)
			{
				if (categories[iCategory].is<std::string>())
				{
					const int iCategoryId = jsonResolveId(categories[iCategory].get<std::string>());
					if (iCategoryId >= 0)
					{
						m_aiCategories.push_back(iCategoryId);
					}
				}
			}
		}
	}

	// ui.art.texture -- the specialist's city-screen glyph
	if (const picojson::object* pUi = jsonChildObj(entityObj, "ui"))
	{
		if (const picojson::object* pArt = jsonChildObj(*pUi, "art"))
		{
			jsonIdStr(*pArt, "texture", m_szTexture);
		}
	}

	// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects (NOT a map)
	if (const picojson::object* pAi = jsonChildObj(entityObj, "ai"))
	{
		jsonReadFlavours(*pAi, m_flavours);
	}
}
