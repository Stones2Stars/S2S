//
//	DepositIndex -- the #430 compiled deposit index (see the header). The interner + the push-time compile over
//	the spec model (CvJsonModifiers families) + the compiled-record registry + the lazy per-info-type segment-id
//	caches.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeDepositIndex.h"
#include "CvJsonInfo.h"               // CvJsonInfo::getModifiers()/getWhenObsolete() -- the push's read surface
#include "Defines/CvGlobals.h"
#include "Infos/CvTerrainInfo.h"
#include "Infos/CvFeatureInfo.h"
#include "Infos/CvBonusInfo.h"
#include "Infos/CvImprovementInfo.h"
#include "Infos/CvBuildingInfo.h"
#include <map>
#include <vector>

static std::map<std::string, int> s_segs;    // segment string -> id (append-only)
static std::map<std::string, int> s_addrs;   // whole-address string -> id (append-only)

// The compiled-record registry: source info -> its compiled deposits (+ the whenObsolete tree's). Cascade-side
// ONLY ([DEC-json-not-cascade]); rebuilt by the readJson push, dropped by clearCompiled() before a re-map.
struct DiCompiledSet
{
	std::vector<CascadeDeposit> main;
	std::vector<CascadeDeposit> whenObsolete;
};
static std::map<const CvJsonInfo*, DiCompiledSet> s_compiled;
static const std::vector<CascadeDeposit> s_noDeposits;   // the shared empty answer (NULL / family-less infos)

int DepositIndex::internSegment(const std::string& s)
{
	const std::map<std::string, int>::const_iterator it = s_segs.find(s);
	if (it != s_segs.end()) return it->second;
	const int id = (int)s_segs.size();
	s_segs.insert(std::make_pair(s, id));
	return id;
}

int DepositIndex::internAddress(const std::string& s)
{
	const std::map<std::string, int>::const_iterator it = s_addrs.find(s);
	if (it != s_addrs.end()) return it->second;
	const int id = (int)s_addrs.size();
	s_addrs.insert(std::make_pair(s, id));
	return id;
}

int DepositIndex::lookupSegment(const std::string& s)
{
	const std::map<std::string, int>::const_iterator it = s_segs.find(s);
	return it == s_segs.end() ? -1 : it->second;
}

int DepositIndex::lookupAddress(const std::string& s)
{
	const std::map<std::string, int>::const_iterator it = s_addrs.find(s);
	return it == s_addrs.end() ? -1 : it->second;
}

// The unit enum's segment spelling -- the EXACT reverse of cascadeUnitFromString (CvJsonModEntry.cpp); the two
// must stay in lock-step ("" = UNKNOWN, never authored as a leaf).
const char* DepositIndex::unitSegment(CvCascUnit u)
{
	switch (u)
	{
	case CASC_UNIT_FLAT:                  return "flat";
	case CASC_UNIT_PERCENT:               return "percent";
	case CASC_UNIT_MULTIPLIER:            return "multiplier";
	case CASC_UNIT_POST_MULTIPLIER:       return "postMultiplier";
	case CASC_UNIT_RAW_PERCENT:           return "rawPercent";
	case CASC_UNIT_COUNT:                 return "count";   // the §6 count-by-type leaf (synthesized by the walk)
	case CASC_UNIT_PER_POPULATION:        return "perPopulation";
	case CASC_UNIT_PER_SPECIALIST:        return "perSpecialist";
	case CASC_UNIT_PER_CORPORATION_LEVEL: return "perCorporationLevel";
	default:                              return "";
	}
}

void DepositIndex::compile(CascadeDeposit& d)
{
	d.addressId = internAddress(d.address);
	d.unitId = internSegment(d.unit);
	d.nSeg = 0;
	for (int i = 0; i < CascadeDeposit::CASC_DEP_SEGS; ++i) d.seg[i] = -1;
	std::string last;
	size_t start = 0;
	for (;;)
	{
		const size_t dot = d.address.find('.', start);
		const std::string segStr = (dot == std::string::npos)
			? d.address.substr(start) : d.address.substr(start, dot - start);
		if (!segStr.empty())
		{
			if (d.nSeg < CascadeDeposit::CASC_DEP_SEGS) d.seg[d.nSeg] = internSegment(segStr);
			++d.nSeg;
			last = segStr;
		}
		if (dot == std::string::npos) break;
		start = dot + 1;
	}
	// FK-resolve the LAST segment when it is a keyed deposit's INFOTYPE target ("<chan>.<scope>.<member>.<KEY>").
	// Hide-assert: a non-key tail (a member name like "goldenAge"/"distance") simply doesn't resolve. Deliberately
	// NOT routed through jsonResolveId -- a member name must never be reported as an unresolved FK.
	d.targetFk = (d.nSeg >= 3 && last.find('_') != std::string::npos)
		? GC.getInfoTypeForString(last.c_str(), true) : -1;
}

// One CvJsonModifiers unit's families -> compiled records: per (address, entry), the record carries the entry's
// payload (value100 / enabled / disabled, borrowed) + the entry's unit spelled as the unit segment; compile()
// interns + FK-resolves. Family map order (std::map, address-sorted) is the record order -- every consumer sums
// commutatively, so order carries no semantics.
static void di_pushFamilies(const CvJsonModifiers* mods, std::vector<CascadeDeposit>& out)
{
	if (mods == NULL || mods->empty()) return;
	const std::map<std::string, CvJsonModFamily*>& fams = mods->all();
	for (std::map<std::string, CvJsonModFamily*>::const_iterator it = fams.begin(); it != fams.end(); ++it)
	{
		const CvJsonModFamily* fam = it->second;
		if (fam == NULL) continue;
		for (size_t i = 0; i < fam->entries.size(); ++i)
		{
			const CvJsonModEntry* e = fam->entries[i];
			if (e == NULL) continue;
			const char* szUnit = DepositIndex::unitSegment(e->unit);
			if (szUnit[0] == '\0') continue;   // UNKNOWN never reaches an entry (parseLeaf takes real units only)
			out.push_back(CascadeDeposit());
			CascadeDeposit& d = out.back();
			d.address = it->first;
			d.unit = szUnit;
			d.value100 = e->value100;
			d.enabled = e->enabled;
			d.disabled = e->disabled;
			d.unitQual = e->unitQual;
			d.hasPer = e->hasPer;
			d.perTypeId = e->perTypeId;
			d.perEach = e->perEach;
			d.perAnyOf = e->perAnyOf.empty() ? NULL : &e->perAnyOf;
			DepositIndex::compile(d);
		}
	}
}

void DepositIndex::pushInfo(const CvJsonInfo* j)
{
	if (j == NULL) return;
	const CvJsonModifiers* mods = j->getModifiers();
	const CvJsonModifiers* obs = j->getWhenObsolete();
	if ((mods == NULL || mods->empty()) && (obs == NULL || obs->empty())) return;
	DiCompiledSet& set = s_compiled[j];
	set.main.clear();          // re-push-safe: a re-mapped info compiles fresh, never doubles
	set.whenObsolete.clear();
	di_pushFamilies(mods, set.main);
	di_pushFamilies(obs, set.whenObsolete);
}

void DepositIndex::clearCompiled()
{
	s_compiled.clear();
}

const std::vector<CascadeDeposit>& DepositIndex::depositsFor(const CvJsonInfo* j)
{
	if (j == NULL) return s_noDeposits;
	const std::map<const CvJsonInfo*, DiCompiledSet>::const_iterator it = s_compiled.find(j);
	return it == s_compiled.end() ? s_noDeposits : it->second.main;
}

const std::vector<CascadeDeposit>& DepositIndex::whenObsoleteFor(const CvJsonInfo* j)
{
	if (j == NULL) return s_noDeposits;
	const std::map<const CvJsonInfo*, DiCompiledSet>::const_iterator it = s_compiled.find(j);
	return it == s_compiled.end() ? s_noDeposits : it->second.whenObsolete;
}

// The shared lazy cache body: per info index, TYPE string -> segment id. Hits (>=0) cache forever; misses (-1)
// RE-LOOKUP each call (append-only interner -- a re-map can turn a miss into a hit; a hit's id never changes).
// -2 marks a never-looked-up slot.
static int di_segForType(std::vector<int>& cache, int i, int n, const char* szType)
{
	if (i < 0 || i >= n) return -1;
	if ((int)cache.size() != n) cache.assign(n, -2);
	if (cache[i] < 0) cache[i] = DepositIndex::lookupSegment(std::string(szType));
	return cache[i];
}

int DepositIndex::segIdForTerrain(int i)
{
	static std::vector<int> c;
	const int n = GC.getNumTerrainInfos();
	return (i < 0 || i >= n) ? -1 : di_segForType(c, i, n, GC.getTerrainInfo((TerrainTypes)i).getType());
}

int DepositIndex::segIdForFeature(int i)
{
	static std::vector<int> c;
	const int n = GC.getNumFeatureInfos();
	return (i < 0 || i >= n) ? -1 : di_segForType(c, i, n, GC.getFeatureInfo((FeatureTypes)i).getType());
}

int DepositIndex::segIdForBonus(int i)
{
	static std::vector<int> c;
	const int n = GC.getNumBonusInfos();
	return (i < 0 || i >= n) ? -1 : di_segForType(c, i, n, GC.getBonusInfo((BonusTypes)i).getType());
}

int DepositIndex::segIdForImprovement(int i)
{
	static std::vector<int> c;
	const int n = GC.getNumImprovementInfos();
	return (i < 0 || i >= n) ? -1 : di_segForType(c, i, n, GC.getImprovementInfo((ImprovementTypes)i).getType());
}

int DepositIndex::segIdForBuilding(int i)
{
	static std::vector<int> c;
	const int n = GC.getNumBuildingInfos();
	return (i < 0 || i >= n) ? -1 : di_segForType(c, i, n, GC.getBuildingInfo((BuildingTypes)i).getType());
}
