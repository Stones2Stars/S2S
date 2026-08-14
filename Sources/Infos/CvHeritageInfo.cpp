//
//	CvHeritageInfo -- the heritage poco's own typed reading on top of the base section dispatch (see the
//	header). mapFrom materializes the identity set + the property bridge ONCE ([DEC-materialize-at-mapfrom]);
//	the ERA-banded empire commerce lives on the compiled conditioned entries (base surface), never as a
//	mirrored band table. The acquisition prereqs materialize in the general reverse pass's post-map derivation
//	step from THIS info's own load-populated reverse view (EDGEF_RELATED), the exact enables-predicate confirmed
//	against each related source's forward edge.
//

#include "CvGameCoreDLL.h"
#include "CvHeritageInfo.h"
#include "CvTechInfo.h"                // the forward-edge confirm (GC.getTechInfo(...).getEdges())
#include "CvJsonParse.h"               // jsonChildObj / jsonIdBool
#include "Property/CvPropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk

CvHeritageInfo::CvHeritageInfo()
	: m_bNeedsLanguage(false)
	, m_iMissionType(-1)
	, m_iPrereqTech(NO_TECH)
{
}

// The prereq read-back (see the header): iterate THIS info's own EDGEF_RELATED lists (the load-populated
// reverse view -- a candidate SUPERSET of everything referencing this heritage) and keep exactly the sources
// whose FORWARD enables.heritages edge lists this heritage -- the consumer-kept exact predicate over the
// RELATED superset. Bounded by the related lists; no repo-wide scan. Run ONCE from inside the general reverse
// pass (rp_deriveHeritagePrereqs), after it has landed the EDGEF_RELATED families, so both getters are bare
// member reads and the info carries no memo ([DEC-materialize-at-mapfrom]).
void CvHeritageInfo::deriveAtRegistryComplete()
{
	// Idempotent like the sibling sub-passes: fully redefine the output every run.
	m_iPrereqTech = NO_TECH;
	m_prereqOrHeritage.clear();

	const int iThis = GC.getInfoTypeForString(getType(), true);   // this heritage's own registered id
	if (iThis < 0)
	{
		return;
	}

	// PrereqTech: the (single, legacy) related tech whose enables.heritages includes this heritage.
	const std::vector<int>* pRelatedTechs = edge(EDGEF_RELATED, EDGEB_TECHS);
	if (pRelatedTechs != NULL)
	{
		for (size_t iRelated = 0; iRelated < pRelatedTechs->size() && m_iPrereqTech == NO_TECH; ++iRelated)
		{
			const int iTech = (*pRelatedTechs)[iRelated];
			const CvEdges* pTechEdges = GC.getTechInfo((TechTypes)iTech).getEdges();
			if (pTechEdges == NULL)
			{
				continue;
			}
			const std::vector<int>* pEnabled = pTechEdges->find(EDGEF_ENABLES, EDGEB_HERITAGES);
			if (pEnabled == NULL)
			{
				continue;
			}
			for (size_t iEnabled = 0; iEnabled < pEnabled->size(); ++iEnabled)
			{
				if ((*pEnabled)[iEnabled] == iThis)
				{
					m_iPrereqTech = iTech;
					break;
				}
			}
		}
	}

	// PrereqOrHeritage: every related heritage whose enables.heritages includes this heritage (the
	// folklore->taxon predecessor chain).
	const std::vector<int>* pRelatedHeritages = edge(EDGEF_RELATED, EDGEB_HERITAGES);
	if (pRelatedHeritages != NULL)
	{
		for (size_t iRelated = 0; iRelated < pRelatedHeritages->size(); ++iRelated)
		{
			const int iHeritage = (*pRelatedHeritages)[iRelated];
			if (iHeritage == iThis)
			{
				continue;
			}
			const CvEdges* pHeritageEdges = GC.getHeritageInfo((HeritageTypes)iHeritage).getEdges();
			if (pHeritageEdges == NULL)
			{
				continue;
			}
			const std::vector<int>* pEnabled = pHeritageEdges->find(EDGEF_ENABLES, EDGEB_HERITAGES);
			if (pEnabled == NULL)
			{
				continue;
			}
			for (size_t iEnabled = 0; iEnabled < pEnabled->size(); ++iEnabled)
			{
				if ((*pEnabled)[iEnabled] == iThis)
				{
					m_prereqOrHeritage.push_back((HeritageTypes)iHeritage);
					break;
				}
			}
		}
	}
}

void CvHeritageInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers, fills edges)

	// PROPERTY_* per-turn SOURCES: player-gathered, fanned to every owner city -- the ONE shared walk.
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_ASSOCIATED, 0, NULL, getType());

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every member mapFrom owns. The
	// acquisition prereqs are NOT mapFrom's -- they come from the cross-entity view and are redefined by
	// materializeCrossEntity(), which the reader runs after this pass.
	m_bNeedsLanguage = false;

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_bNeedsLanguage = jsonIdBool(*pIdentity, "needsLanguage");
	}
}
