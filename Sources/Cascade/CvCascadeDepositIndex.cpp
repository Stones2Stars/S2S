//
//	DepositIndex -- the #430 compiled deposit index (see the header). The interner + the push-time compile +
//	the lazy per-info-type segment-id caches.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeDepositIndex.h"
#include "CvJsonInfo.h"               // CvCascadeDeposit -- the compile target
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

void DepositIndex::compile(CvCascadeDeposit& d)
{
	d.addressId = internAddress(d.address);
	d.unitId = internSegment(d.unit);
	d.nSeg = 0;
	for (int i = 0; i < CvCascadeDeposit::CASC_DEP_SEGS; ++i) d.seg[i] = -1;
	std::string last;
	size_t start = 0;
	for (;;)
	{
		const size_t dot = d.address.find('.', start);
		const std::string segStr = (dot == std::string::npos)
			? d.address.substr(start) : d.address.substr(start, dot - start);
		if (!segStr.empty())
		{
			if (d.nSeg < CvCascadeDeposit::CASC_DEP_SEGS) d.seg[d.nSeg] = internSegment(segStr);
			++d.nSeg;
			last = segStr;
		}
		if (dot == std::string::npos) break;
		start = dot + 1;
	}
	// FK-resolve the LAST segment when it is a keyed deposit's INFOTYPE target ("<chan>.<scope>.<member>.<KEY>").
	// Hide-assert: a non-key tail (a member name like "goldenAge"/"distance") simply doesn't resolve. Deliberately
	// NOT routed through cascadeJsonResolveId -- a member name must never be reported as an unresolved FK.
	d.targetFk = (d.nSeg >= 3 && last.find('_') != std::string::npos)
		? GC.getInfoTypeForString(last.c_str(), true) : -1;
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
