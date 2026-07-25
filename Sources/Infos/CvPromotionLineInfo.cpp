//
//	CvPromotionLineInfo::mapFrom -- base (availability: tech enables.promotionLines; the game-option gates ride
//	the composed entity-level `enabled`/`disabled` gate), then the build-up flag + the parked not-on-domain list.
//	The member promotions are a runtime reverse index (not JSON). See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvPromotionLineInfo.h"
#include "CvJsonParse.h"          // jsonResolveId (DOMAIN_ FKs) + the shared walkers (jsonChildObj/jsonIdBool)

bool CvPromotionLineInfo::isNotOnDomainType(int iDomain) const
{
	for (size_t i = 0; i < m_aeNotOnDomains.size(); ++i)
		if (m_aeNotOnDomains[i] == iDomain) return true;
	return false;
}

// a string array -> FK id vector (identity unitcombat lists) -- the CvPromotionInfo pl_readIdList idiom.
static void pl_readIdList(const picojson::object& parent, const char* key, std::vector<int>& out)
{
	picojson::object::const_iterator it = parent.find(key);
	if (it == parent.end() || !it->second.is<picojson::array>()) return;
	const picojson::array& a = it->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) out.push_back(id); }
}

// flatten the entity-level gate condition into the flat GAMEOPTION id list the legacy NotOnGameOption getter exposes.
// The curator authors a single option as a bare GAMEOPTION_ string (-> PRESENCE atom) and several as an {all}/{anyOf}
// tree (-> GROUP); recurse the group vectors and collect the resolved GAMEOPTION_ presence ids. (CvPromotionInfo idiom.)
static void pl_collectGameOptions(const CvCondition* c, std::vector<int>& out)
{
	if (!c) return;
	if (c->kind == CASC_COND_PRESENCE)
	{
		if (c->id >= 0 && c->type.compare(0, 11, "GAMEOPTION_") == 0) out.push_back(c->id);
		return;
	}
	if (c->kind == CASC_COND_GROUP)
	{
		for (size_t i = 0; i < c->all.size(); ++i)    pl_collectGameOptions(c->all[i], out);
		for (size_t i = 0; i < c->anyOf.size(); ++i)  pl_collectGameOptions(c->anyOf[i], out);
		for (size_t i = 0; i < c->noneOf.size(); ++i) pl_collectGameOptions(c->noneOf[i], out);
		pl_collectGameOptions(c->enabled, out);
		pl_collectGameOptions(c->disabled, out);
	}
}

void CvPromotionLineInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom
	m_aiNotOnGameOptions.clear(); m_aeNotOnDomains.clear(); m_aiUnitCombats.clear(); m_aiNotOnUnitCombats.clear();
	CvInfo::mapFrom(entity);   // core + availability (tech enables.promotionLines; the entity-level gate via mutGate)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// entity-level gate (populated by the base dispatch into m_gate) -> the flat NotOnGameOptions list
	pl_collectGameOptions(m_gate.disabled, m_aiNotOnGameOptions);

	if (const picojson::object* bu = jsonChildObj(o, "buildUp"))
		m_bBuildUp = jsonIdBool(*bu, "active");

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		picojson::object::const_iterator nd = io->find("notOnDomains");
		if (nd != io->end() && nd->second.is<picojson::array>())
		{
			const picojson::array& a = nd->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeNotOnDomains.push_back(id); }
		}
		pl_readIdList(*io, "unitCombats", m_aiUnitCombats);          // UnitCombatPrereqTypes (unit-combats the line applies to)
		pl_readIdList(*io, "notOnUnitCombats", m_aiNotOnUnitCombats); // NotOnUnitCombatTypes (excluded unit-combats)
	}
}
