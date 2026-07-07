//
//	CvJsonRouteInfo::mapFrom -- base core reading, then the route's LIVE members from the current JSON shapes: the plot
//	yields (food/production/commerce.plot.flat), the intrinsic identity stats (value / advancedStart.cost / movementCost /
//	flatMovementCost / seaTunnel), and the tech-gated move deltas reconstructed from the `movement.plot.flat` family
//	(each entry {value, enabled:{type:TECH_X}} -> m_techMovementChange[TECH_X] = value). See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvJsonRouteInfo.h"
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString -- tech FK for the movement deltas

CvJsonRouteInfo::CvJsonRouteInfo()
	: m_iValue(0), m_iAdvancedStartCost(0), m_iMovementCost(0), m_iFlatMovementCost(0), m_iZobristValue(0),
	  m_bSeaTunnel(false)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
}

int CvJsonRouteInfo::getTechMovementChange(int iTech) const
{
	std::map<int, int>::const_iterator it = m_techMovementChange.find(iTech);
	return it != m_techMovementChange.end() ? it->second : 0;
}

// --- local JSON helpers (no cascade dependency) ---
static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
static int fam_val(const picojson::object& o, const char* family, const char* scope, const char* unit)
{
	const picojson::object* fo = child_obj(o, family);  if (!fo) return 0;
	const picojson::object* so = child_obj(*fo, scope); if (!so) return 0;
	picojson::object::const_iterator u = so->find(unit);
	return (u != so->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}
static int  id_int (const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static bool id_bool(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }

void CvJsonRouteInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading: type + identity text + button
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// plot yields
	m_aiYieldChange[YIELD_FOOD]       = fam_val(o, "food", "plot", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = fam_val(o, "production", "plot", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = fam_val(o, "commerce", "plot", "flat");

	// tech-gated move deltas: the `movement.plot.flat` list ({value, enabled:{type:TECH_X}}) -> per-tech map
	if (const picojson::object* mv = child_obj(o, "movement"))
		if (const picojson::object* pl = child_obj(*mv, "plot"))
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
					const picojson::object* en = child_obj(e, "enabled");
					if (!en) continue;
					picojson::object::const_iterator ty = en->find("type");
					if (ty == en->end() || !ty->second.is<std::string>()) continue;
					const int techId = GC.getInfoTypeForString(ty->second.get<std::string>().c_str(), true);
					if (techId >= 0) m_techMovementChange[techId] = value;
				}
			}
		}

	// identity: the intrinsic route stats
	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_iValue            = id_int(*io, "value");
		m_iMovementCost     = id_int(*io, "movementCost");
		m_iFlatMovementCost = id_int(*io, "flatMovementCost");
		m_bSeaTunnel        = id_bool(*io, "seaTunnel");
		if (const picojson::object* as = child_obj(*io, "advancedStart"))
			m_iAdvancedStartCost = id_int(*as, "cost");
	}

	// (the route's bonus prerequisite is NOT read here -- it is modelled as the BONUS's enables.routes availability
	//  relationship, read off the bonus info by the cascade GENERATE pass; see the header.)
	// ⏳ m_iZobristValue: needs the exact legacy zobrist map-hash (OOS) -- left 0, flagged not faked.
}
