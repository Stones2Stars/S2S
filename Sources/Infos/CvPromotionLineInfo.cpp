//
//	CvPromotionLineInfo::mapFrom -- base (availability: tech enables.promotionLines; the game-option gates ride
//	the composed entity-level `enabled`/`disabled` gate, served whole via getGate), then the buildUp module flag
//	+ the identity FK lists. The member promotions are a runtime reverse index (not JSON). See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvPromotionLineInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdBool / jsonReadIdList / vectorHas

bool CvPromotionLineInfo::isNotOnDomain(int iDomain) const
{
	return vectorHas(m_aiNotOnDomains, iDomain);
}

void CvPromotionLineInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom
	m_bBuildUp = false;
	m_aiNotOnDomains.clear();
	m_aiUnitCombats.clear();
	m_aiNotOnUnitCombats.clear();
	CvInfo::mapFrom(entity);   // core + availability (tech enables.promotionLines; the entity-level gate via mutGate)
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	if (const picojson::object* pBuildUp = jsonChildObj(entityObj, "buildUp"))
	{
		m_bBuildUp = jsonIdBool(*pBuildUp, "active");
	}

	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		jsonReadIdList(*pIdentity, "notOnDomains", m_aiNotOnDomains);
		jsonReadIdList(*pIdentity, "unitCombats", m_aiUnitCombats);
		jsonReadIdList(*pIdentity, "notOnUnitCombats", m_aiNotOnUnitCombats);
	}
}
