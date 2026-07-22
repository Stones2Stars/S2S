//
//	CvHeritageInfo::mapFrom -- base core reading + availability (tech enables.heritages + this heritage's
//	enables.heritages succession ride the base), then the era-threshold-gated empire commerce + the language gate.
//	Commerce is HUMAN (÷100-descaled by the curator). PropertyManipulators are deferred to the property subsystem.
//	See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvHeritageInfo.h"
#include "CvTechInfo.h"       // getPrereqTech reverse scan reads GC.getTechInfo(i).getEdges() -- needs the full type (C2027)
#include "CvJsonParse.h"          // the shared walkers (jsonChildObj/jsonIdBool)
#include "CvCascadePropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk

// Reverse-scan the FORWARD enables.heritages edges the curator store-inverts the two acquisition prereqs onto
// (curate_heritage.py DROP + store inversion): the tech that lists THIS heritage in enables.heritages is its
// PrereqTech (legacy single); every OTHER heritage that lists THIS heritage in enables.heritages is one of its
// PrereqOrHeritage predecessors (the folklore->taxon succession). Lazy + memoized -- the scan needs every tech/
// heritage poco loaded, which holds at every runtime/UI caller (canAddHeritage, CvGameTextMgr) but not during load.
void CvHeritageInfo::resolvePrereqs() const
{
	if (m_bPrereqsResolved) return;
	m_bPrereqsResolved = true;

	const int iThis = GC.getInfoTypeForString(getType(), true);   // this heritage's own registered id
	if (iThis < 0) return;

	// PrereqTech: the (single, legacy) tech whose enables.heritages includes this heritage.
	for (int t = 0; t < GC.getNumTechInfos() && m_iPrereqTech == NO_TECH; ++t)
	{
		const CvJsonEdges* pEdges = GC.getTechInfo((TechTypes)t).getEdges();
		if (pEdges == NULL) continue;
		const std::vector<int>* pList = pEdges->find(EDGEF_ENABLES, EDGEB_HERITAGES);
		if (pList == NULL) continue;
		for (size_t k = 0; k < pList->size(); ++k)
			if ((*pList)[k] == iThis) { m_iPrereqTech = t; break; }
	}

	// PrereqOrHeritage: every heritage whose enables.heritages includes this heritage (its predecessor succession).
	for (int h = 0; h < GC.getNumHeritageInfos(); ++h)
	{
		if (h == iThis) continue;
		const CvJsonEdges* pEdges = GC.getHeritageInfo((HeritageTypes)h).getEdges();
		if (pEdges == NULL) continue;
		const std::vector<int>* pList = pEdges->find(EDGEF_ENABLES, EDGEB_HERITAGES);
		if (pList == NULL) continue;
		for (size_t k = 0; k < pList->size(); ++k)
			if ((*pList)[k] == iThis) { m_prereqOrHeritage.push_back((HeritageTypes)h); break; }
	}
}

int CvHeritageInfo::getPrereqTech() const
{
	resolvePrereqs();
	return m_iPrereqTech;
}

const std::vector<HeritageTypes>& CvHeritageInfo::getPrereqOrHeritage() const
{
	resolvePrereqs();
	return m_prereqOrHeritage;
}

void CvHeritageInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c) m_aEraCommerce[c].clear();
	CvInfo::mapFrom(entity);   // core + availability (tech enables.heritages, this heritage's enables.heritages)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// PROPERTY_* per-turn SOURCES: a heritage's <PROPERTY_X>.city.flat (the tech-gated folklore education)
	// deposits in EVERY city while held -- the player gather walks heritages; RELATION_ASSOCIATED fans each
	// source to every owner city per the curated `city` scope. (The legacy XML carried NO GameObjectType, so
	// legacy deposited into the PLAYER's own property bag, which nothing reads -- the curated city scope is the
	// evident intent and the delivery here; property-audit.md carriers note.)
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_ASSOCIATED);

	// era-gated empire commerce -- {gold/research/culture/espionage}.empire.flat, each a bare number (one always-on
	// band, eraMin 0) OR a list of { value, enabled:{type:"ERA", min:N} } era-threshold bands.
	static const char* fam[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };   // COMMERCE_* order
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		const picojson::object* fo = jsonChildObj(o, fam[c]);       if (!fo) continue;
		const picojson::object* so = jsonChildObj(*fo, "empire");   if (!so) continue;
		picojson::object::const_iterator fl = so->find("flat");     if (fl == so->end()) continue;
		if (fl->second.is<double>())
		{
			EraBand b; b.eraMin = 0; b.value = jsonX100(fl->second.get<double>());   // human -> ×100 (was (int)h: truncated fractional heritage commerce to 0)
			m_aEraCommerce[c].push_back(b);
		}
		else if (fl->second.is<picojson::array>())
		{
			const picojson::array& a = fl->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				EraBand b; b.eraMin = 0; b.value = 0;
				if (a[i].is<double>()) { b.value = jsonX100(a[i].get<double>()); m_aEraCommerce[c].push_back(b); continue; }
				if (!a[i].is<picojson::object>()) continue;
				const picojson::object& e = a[i].get<picojson::object>();
				picojson::object::const_iterator ve = e.find("value");
				if (ve == e.end() || !ve->second.is<double>()) continue;
				b.value = jsonX100(ve->second.get<double>());
				const picojson::object* en = jsonChildObj(e, "enabled");
				if (en) { picojson::object::const_iterator mn = en->find("min"); if (mn != en->end() && mn->second.is<double>()) b.eraMin = (int)mn->second.get<double>(); }
				m_aEraCommerce[c].push_back(b);
			}
		}
	}

	if (const picojson::object* io = jsonChildObj(o, "identity"))
		m_bNeedsLanguage = jsonIdBool(*io, "needsLanguage");

	// archived getEraCommerceChanges100 reconstruction -- REAL: same bands just parsed above, ×100 (CentiCommerce),
	// keyed by the actual EraTypes (ordinal-1; see header comment). eraMin==0 is the "always-on" sentinel (bare/
	// unconditioned flat entries above) -> era 0, which an `>=`/`==` era-threshold consumer treats correctly.
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		const std::vector<EraBand>& bands = m_aEraCommerce[c];
		for (size_t i = 0; i < bands.size(); ++i)
		{
			const EraTypes eEra = (bands[i].eraMin > 0) ? (EraTypes)(bands[i].eraMin - 1) : (EraTypes)0;
			CommerceArray kArr; kArr.fill(0);
			kArr[c] = bands[i].value;   // value is already ×100 (jsonX100 at parse); no re-scale (was *100 over a truncated int -> 0)
			m_eraCommerceChanges100.addArrayValue(eEra, kArr);
		}
	}
}
