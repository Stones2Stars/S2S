//
//	CvTraitInfo -- the trait poco's own typed reading on top of the base section dispatch (see the header).
//	mapFrom materializes the census identity set ONCE into typed members ([DEC-materialize-at-mapfrom]);
//	idempotent by contract (unconditional assigns).
//

#include "CvGameCoreDLL.h"
#include "CvTraitInfo.h"
#include "CvJsonParse.h"   // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdStr

CvTraitInfo::CvTraitInfo()
	: m_bNegativeTrait(false)
	, m_bBarbarianSelectionOnly(false)
	, m_bImpurePropertyManipulators(false)
	, m_bImpurePromotions(false)
	, m_iMinAnarchy(0)
	, m_iMaxAnarchy(0)
{
}

void CvTraitInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers)

	m_bNegativeTrait = false;
	m_bBarbarianSelectionOnly = false;
	m_bImpurePropertyManipulators = false;
	m_bImpurePromotions = false;
	m_iMinAnarchy = 0;
	m_iMaxAnarchy = 0;
	m_szShortDescriptionKey.clear();

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_bNegativeTrait = jsonIdBool(*pIdentity, "negativeTrait");
		m_bBarbarianSelectionOnly = jsonIdBool(*pIdentity, "barbarianSelectionOnly");
		m_bImpurePropertyManipulators = jsonIdBool(*pIdentity, "impurePropertyManipulators");
		m_bImpurePromotions = jsonIdBool(*pIdentity, "impurePromotions");
		m_iMinAnarchy = jsonIdInt(*pIdentity, "minAnarchy");
		m_iMaxAnarchy = jsonIdInt(*pIdentity, "maxAnarchy");
		std::string szTextKey;
		if (jsonIdStr(*pIdentity, "shortDescription", szTextKey))
		{
			m_szShortDescriptionKey = CvWString(szTextKey.c_str());
		}
	}
}
