//
//	DepositIndex -- the #430 compiled deposit index (see the header). The interner + the push-time compile over
//	the spec model (CvJsonModifiers families) + the compiled-record registry + the lazy per-info-type segment-id
//	caches.
//

#include "CvGameCoreDLL.h"
#include "Data/CvDepositIndex.h"
#include "CvInfo.h"               // CvInfo::getModifiers()/getWhenObsolete() -- the push's read surface
#include "Defines/CvGlobals.h"
#include "CvTerrainInfo.h"
#include "CvFeatureInfo.h"
#include "CvBonusInfo.h"
#include "CvImprovementInfo.h"
#include "CvBuildingInfo.h"
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
static std::map<const CvInfo*, DiCompiledSet> s_compiled;
static const std::vector<CascadeDeposit> s_noDeposits;   // the shared empty answer (NULL / family-less infos)

// The lazy reverse-route cache (F0 R2): source info -> its compiled cross-scope route. Filled on first routeFor
// query, dropped with s_compiled by clearCompiled() (its keys are the about-to-be-freed infos).
static std::map<const CvInfo*, SourceRoute> s_routes;


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
	std::string segStrs[3];   // family / scope / member -- kept for the one-time channel resolution below
	size_t start = 0;
	for (;;)
	{
		const size_t dot = d.address.find('.', start);
		const std::string segStr = (dot == std::string::npos)
			? d.address.substr(start) : d.address.substr(start, dot - start);
		if (!segStr.empty())
		{
			if (d.nSeg < CascadeDeposit::CASC_DEP_SEGS) d.seg[d.nSeg] = internSegment(segStr);
			if (d.nSeg < 3) segStrs[d.nSeg] = segStr;
			++d.nSeg;
			last = segStr;
		}
		if (dot == std::string::npos) break;
		start = dot + 1;
	}
	// THE ONE-TIME SLOT RESOLUTION: (family, member, unit) -> channel + which dictionary, and the scope
	// segment -> its index. Done HERE so no runtime path ever interprets an address string. An address that is
	// not a cascade channel leaves chan = -1 and the gather skips it -- how the unit-plane families and any
	// retired system drop out with no special-casing.
	{
		CascadeChannel ch;
		bool bPct = false;
		if (cascadeResolveAddress(segStrs[0].c_str(), segStrs[2].c_str(), d.unit.c_str(), ch, bPct))
		{
			d.chan = (short)ch;
			d.isPercent = bPct;
		}
		d.scopeIdx = (short)cascadeScopeFromSegment(segStrs[1].c_str());
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
// commutatively, so order carries no semantics. `j` = the SOURCE info (the SELF per token collapses onto it).
static void di_pushFamilies(const CvInfo* j, const CvJsonModifiers* mods, std::vector<CascadeDeposit>& out)
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
			d.perType = e->perType;
			d.perTypeId = e->perTypeId;
			d.perEach = e->perEach;
			// resolve the §3.7 scope DEFAULT at push: the authored per scope, else the deposit's OWN scope --
			// the resolver (MMKernel::perScale) then never needs the record's own scope back.
			d.perScope = (e->perScope >= 0) ? e->perScope : (int)e->scope;
			d.perAnyOf = e->perAnyOf.empty() ? NULL : &e->perAnyOf;
			d.perAnyOfTypes = e->perAnyOfTypes.empty() ? NULL : &e->perAnyOfTypes;
			// SELF ("per how many of me exist", json §3.1) collapses at PUSH time onto the SOURCE info's own
			// type -- the resolver then counts it like any typed per (a unit's world count = lifetime-created,
			// empire = live, via cascadeCountOf). Unresolvable stays "SELF": the resolver SKIPS the multiply
			// (a bogus 0 count would zero the contribution).
			if (d.hasPer && d.perType == "SELF")
			{
				const int iSelf = GC.getInfoTypeForString(j->getType(), true);
				if (iSelf >= 0) { d.perType = j->getType(); d.perTypeId = iSelf; }
			}
			// intern a CATCH-ALL token (perTypeId stayed -1: POPULATION/TURN/SELF/...) -- the hot-path guard
			// compares ints, never strings (append-only interner; ids survive a re-map).
			if (d.hasPer && d.perTypeId < 0 && !d.perType.empty())
				d.perTokenSeg = DepositIndex::internSegment(d.perType);
			DepositIndex::compile(d);
		}
	}
}

void DepositIndex::pushInfo(const CvInfo* j)
{
	if (j == NULL) return;
	const CvJsonModifiers* mods = j->getModifiers();
	const CvJsonModifiers* obs = j->getWhenObsolete();
	if ((mods == NULL || mods->empty()) && (obs == NULL || obs->empty())) return;
	DiCompiledSet& set = s_compiled[j];
	set.main.clear();          // re-push-safe: a re-mapped info compiles fresh, never doubles
	set.whenObsolete.clear();
	di_pushFamilies(j, mods, set.main);
	di_pushFamilies(j, obs, set.whenObsolete);
}

void DepositIndex::clearCompiled()
{
	s_compiled.clear();
	s_routes.clear();
}

// THE REVERSE ROUTE (F0 R2). The body is a VERBATIM transcription of CascadeAccumulator::buildingProcessed's former
// inline per-deposit loop -- generalized to any source info and computed once (cached) instead of re-derived every
// event. So it is derivation-IDENTICAL to the proven building path: a percent empire/area deposit fans to every
// city's percent stacks (+ the player gp/maint/buildRate sums); a flat one to the player flat/wb/keyed sums (+ the
// sibling CBASE/WB keyed realization, or the specialist packages); a world deposit sets the world flag. The
// per-CHANNEL narrowing (which yield) is the R2b follow-on; this is the scope x unit x member level.
const SourceRoute& DepositIndex::routeFor(const CvInfo* j)
{
	static const SourceRoute s_empty;
	if (j == NULL) return s_empty;
	const std::map<const CvInfo*, SourceRoute>::const_iterator cit = s_routes.find(j);
	if (cit != s_routes.end()) return cit->second;

	// The interned segment ids the routing compares against (all authored in any loaded game; a real deposit's
	// address is always >= 2 segments, so seg[1] is never -1 and an unauthored id can never false-match).
	const int segArea       = lookupSegment("area");
	const int segEmpire     = lookupSegment("empire");
	const int segWorld      = lookupSegment("world");
	const int segPercent    = lookupSegment("percent");
	const int segBuildings  = lookupSegment("buildings");
	const int segSpecialist = lookupSegment("specialist");
	// R2b PER-CHANNEL narrowing (the family segment seg[0], grounded from the fills' channel strings:
	// BuildingPackage/PercentStack yields = {food,production,commerce}, CommerceCalc = {gold,research,culture,
	// espionage}, ScalarChannels = greatPeopleRate/maintenance/buildRate). A yield/commerce empire PERCENT is
	// purely CITY-realized (yPctCity/cPct) -- the player scope holds NO yield/commerce percent (verified against
	// CascadePlayerScope), so it marks ONE city bit and NO player bit. gp/maint keep their player scalar half
	// (gpModPlayer/maintPlayerAll = PSC_SC); buildRate keeps PSC_BR. The GROUPED families (defense, stateReligion --
	// seg[0] not a plain channel) + any unrecognized family fall to the COARSE-SAFE percent mask (never under-mark).
	const int segFood = lookupSegment("food"), segProd = lookupSegment("production"), segCommY = lookupSegment("commerce");
	const int segGold = lookupSegment("gold"), segResearch = lookupSegment("research"),
	          segCulture = lookupSegment("culture"), segEsp = lookupSegment("espionage");
	const int segGp = lookupSegment("greatPeopleRate"), segMaint = lookupSegment("maintenance"), segBr = lookupSegment("buildRate");

	SourceRoute r;
	const std::vector<CascadeDeposit>& deps = depositsFor(j);
	for (size_t i = 0; i < deps.size(); ++i)
	{
		const CascadeDeposit& dep = deps[i];
		if (dep.seg[1] == segWorld) { r.world = true; r.playerBits |= PSC_SC; continue; }
		if (dep.seg[1] != segEmpire && dep.seg[1] != segArea) continue;
		if (dep.unitId == segPercent)
		{
			// empire/area PERCENTS enter the CITY-REALIZED stacks (the owned-type walk); the player half is only the
			// scalar sums (gp/maint = PSC_SC, buildRate = PSC_BR) -- NOT the yield/commerce percents (city-only).
			const int f = dep.seg[0];
			if (f == segFood || f == segProd || f == segCommY)                            r.cityBits |= CPK_YPCT;
			else if (f == segGold || f == segResearch || f == segCulture || f == segEsp)  r.cityBits |= CPK_CPCT;
			else if (f == segGp)    { r.cityBits |= CPK_SCPCT; r.playerBits |= PSC_SC; }
			else if (f == segMaint) { r.cityBits |= CPK_SCPCT; r.playerBits |= PSC_SC; }
			else if (f == segBr)    { r.cityBits |= CPK_BR;    r.playerBits |= PSC_BR; }
			else   // defense / stateReligion (grouped) / unrecognized -> coarse-safe (never under-mark)
			{
				r.cityBits   |= CPK_YPCT | CPK_CPCT | CPK_SCPCT | CPK_BR;
				r.playerBits |= PSC_SC | PSC_BR;
			}
		}
		else
		{
			// empire/area FLATS feed the player building sums (trade) + the wb fold maps + the keyed ledgers
			r.playerBits |= PSC_SC | PSC_CFLAT | PSC_WB;
			if (dep.nSeg == 4 && dep.seg[2] == segBuildings)
				r.cityBits |= CPK_CBASE | CPK_WB;   // the guild-grant + Royal-Tomb classes: every city's keyed realization re-fills
			else if (dep.nSeg >= 3 && dep.seg[2] == segSpecialist)
				r.cityBits |= CPK_YSPEC | CPK_CSPEC;   // <ch>.empire.specialist.perSpecialist -> every city's specialist package (G4)
		}
	}
	return s_routes[j] = r;
}

const std::vector<CascadeDeposit>& DepositIndex::depositsFor(const CvInfo* j)
{
	if (j == NULL) return s_noDeposits;
	const std::map<const CvInfo*, DiCompiledSet>::const_iterator it = s_compiled.find(j);
	return it == s_compiled.end() ? s_noDeposits : it->second.main;
}

const std::vector<CascadeDeposit>& DepositIndex::whenObsoleteFor(const CvInfo* j)
{
	if (j == NULL) return s_noDeposits;
	const std::map<const CvInfo*, DiCompiledSet>::const_iterator it = s_compiled.find(j);
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
