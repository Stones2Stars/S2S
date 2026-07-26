//
//	CvUnitCombatInfo -- the base dispatch fills the composed units (the par.6 modifier families compile into
//	CvModifiers; the par.8 `skills` bool block; the entity-level gate); this subclass parses ONLY what the type
//	genuinely owns: the identity set, the par.9 vision + sizeMatters sections, the par.8 keyed-skill FK lists,
//	and the par.8 outcomes section. NO family-address read survives here ([DEC-materialize-at-mapfrom]: the
//	modifier values are served by the compiled point reads / entry lists, never re-parsed subclass-side).
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson + GC (getGame().getSorenRand())
#include "CvUnitCombatInfo.h"
#include "CvJsonParse.h"            // jsonChildObj / jsonIdFk / jsonIdBool / jsonReadIdList / jsonReadKeyedBoolIdList
#include "AI/CvGameAI.h"            // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist seed)

CvUnitCombatInfo::CvUnitCombatInfo()
	: m_iReligion(-1)
	, m_iEra(-1)
	, m_bForMilitary(false)
	, m_bForNavalMilitary(false)
	, m_bChangesMoveThroughPlots(false)
	, m_iZobristValue(0)
{
	// Runtime, non-XML: a per-type random hash contribution -- drawn ONCE at construction exactly as the
	// archived ctor did (never in mapFrom: the full-registry pass re-runs mapFrom, and a re-run must not
	// redraw the synced RNG).
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

void CvUnitCombatInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom -- fully define every appending vector.
	m_aiGGPointsForUnits.clear();
	m_aiDefaultStatuses.clear();
	m_aiTerrainDoubleMove.clear();
	m_aiFeatureDoubleMove.clear();
	m_bChangesMoveThroughPlots = false;

	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers; fills skills/gate)

	// par.8/par.9 typed sections (clear-first inside their own parse)
	m_vision.parse(entity);
	m_sizeMatters.parse(entity);
	m_outcomes.parse(entity);

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// --- identity: refs + AI tags + the parked FK lists ---
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iReligion = jsonIdFk(*pIdentity, "religion");
		m_iEra = jsonIdFk(*pIdentity, "era");
		m_bForMilitary = jsonIdBool(*pIdentity, "forMilitary");
		m_bForNavalMilitary = jsonIdBool(*pIdentity, "forNavalMilitary");
		jsonReadIdList(*pIdentity, "ggPointsForUnits", m_aiGGPointsForUnits);
		jsonReadIdList(*pIdentity, "defaultStatuses", m_aiDefaultStatuses);
	}

	// --- par.8 keyed-skill FK lists (skills.<name>.{TYPE}:true) ---
	if (const picojson::object* pSkills = jsonChildObj(entityObj, "skills"))
	{
		jsonReadKeyedBoolIdList(*pSkills, "terrainDoubleMove", m_aiTerrainDoubleMove);
		jsonReadKeyedBoolIdList(*pSkills, "featureDoubleMove", m_aiFeatureDoubleMove);
	}

	// --- the derived move-through-plots verdict, materialized over the type's own data (the ONE shared
	// derivation, deriveChangesMoveThroughPlots; the getter is a bare member read) ---
	m_bChangesMoveThroughPlots = deriveChangesMoveThroughPlots(m_skills, m_aiTerrainDoubleMove, m_aiFeatureDoubleMove);
}
