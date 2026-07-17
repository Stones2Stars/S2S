//
//	CvRouteInfo::mapFrom -- base core reading, then the route's LIVE members from the current JSON shapes: the plot
//	yields (food/production/commerce.plot.flat), the intrinsic identity stats (value / advancedStart.cost / movementCost /
//	flatMovementCost / seaTunnel), and the tech-gated move deltas reconstructed from the `movement.plot.flat` family
//	(each entry {value, enabled:{type:TECH_X}} -> m_techMovementChange[TECH_X] = value). See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvRouteInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)
#include "AI/CvGameAI.h"          // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, mirrors the archive)

CvRouteInfo::CvRouteInfo()
	: m_iValue(0), m_iAdvancedStartCost(100), m_iMovementCost(0), m_iFlatMovementCost(0), m_iZobristValue(0),
	  m_ePrereqBonus(NO_BONUS), m_bSeaTunnel(false)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
	// Non-XML runtime map-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// CvRouteInfo ctor did (SourceArchive/Infos/CvRouteInfo.cpp:41). CvPlot XORs it into m_movementCharacteristicsHash.
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

int CvRouteInfo::getTechMovementChange(int iTech) const
{
	std::map<int, int>::const_iterator it = m_techMovementChange.find(iTech);
	return it != m_techMovementChange.end() ? it->second : 0;
}

void CvRouteInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading: type + identity text + button
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// plot yields
	m_aiYieldChange[YIELD_FOOD]       = jsonFamVal(o, "food", "plot", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = jsonFamVal(o, "production", "plot", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = jsonFamVal(o, "commerce", "plot", "flat");

	// tech-gated move deltas: the `movement.plot.flat` list ({value, enabled:{type:TECH_X}}) -> per-tech map
	if (const picojson::object* mv = jsonChildObj(o, "movement"))
		if (const picojson::object* pl = jsonChildObj(*mv, "plot"))
		{
			picojson::object::const_iterator fl = pl->find("flat");
			if (fl != pl->end() && fl->second.is<picojson::array>())
			{
				const picojson::array& a = fl->second.get<picojson::array>();
				for (size_t i = 0; i < a.size(); ++i)
				{
					if (!a[i].is<picojson::object>()) continue;
					const picojson::object& e = a[i].get<picojson::object>();
					picojson::object::const_iterator vv = e.find("value");
					const int value = (vv != e.end() && vv->second.is<double>()) ? (int)vv->second.get<double>() : 0;
					const picojson::object* en = jsonChildObj(e, "enabled");
					if (!en) continue;
					picojson::object::const_iterator ty = en->find("type");
					if (ty == en->end() || !ty->second.is<std::string>()) continue;
					const int techId = jsonResolveId(ty->second.get<std::string>());
					if (techId >= 0) m_techMovementChange[techId] = value;
				}
			}
		}

	// identity: the intrinsic route stats
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iValue            = jsonIdInt(*io, "value");
		m_iMovementCost     = jsonIdInt(*io, "movementCost");
		m_iFlatMovementCost = jsonIdInt(*io, "flatMovementCost");
		m_bSeaTunnel        = jsonIdBool(*io, "seaTunnel");
		if (const picojson::object* as = jsonChildObj(*io, "advancedStart"))
			m_iAdvancedStartCost = jsonIdInt(*as, "cost", 100);  // legacy load default 100 (archive .add)
	}

	// (the route's bonus prerequisite is NOT read here -- it is modelled as the BONUS's enables.routes availability
	//  relationship, read off the bonus info by the cascade GENERATE pass; see the header.)
	// (m_iZobristValue is drawn in the ctor -- non-XML runtime value, see there.)
}
