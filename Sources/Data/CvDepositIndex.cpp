//
//	DepositIndex -- the #430 compiled deposit index (see the header). The interner + the push-time compile over
//	the spec model (CvModifiers families) + the compiled-record registry + the lazy per-info-type segment-id
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
#include <set>
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

static bool s_bDepsCompiled = false;

// The dense SOURCE INDEX (see the header): assigned on first push, stable for the load, dropped with the
// compiled registry. Append-only within a load, so an index handed to an owner's live-source record stays
// valid for as long as that record does.
static std::map<const CvInfo*, int> s_sourceIndex;


// The CONDITION-DEPENDENCY routes, carrying the DEPOSITS themselves -- what the apply path consumes: one
// global pass over every compiled record's gates, per scalers, and religion filters, keyed by the state the
// gate reads. Records are pointers INTO s_compiled, which is not mutated after the pass and is dropped
// wholesale by clearCompiled().
static std::map<std::string, std::vector<DepositIndex::GatedDeposit> > s_gatedByType;
static std::map<std::string, std::vector<DepositIndex::GatedDeposit> > s_gatedByToken;
static std::map<int, std::vector<DepositIndex::GatedDeposit> > s_gatedByPredicate;
static std::vector<DepositIndex::GatedDeposit> s_gatedReligionCounts;
// The PROPERTY thresholds the DEPOSIT gates declare (PROPERTY_ id -> boundary values), collected in the same
// condition scan that interns the gate. The band emit tests ONE registry of authored boundaries
// (EnablerKernel::propertyBandThresholds), and the operate bands are only half of what the data authors -- a
// deposit gated `{PROPERTY_X, min: N}` declares a boundary too, and a value sweep crossing ONLY that boundary
// must still announce, or the deposit's re-book never fires ([DEC-close-event-gaps-now]).
static std::map<int, std::set<int> > s_propertyGateThresholds;



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

// The unit enum's segment spelling -- the EXACT reverse of cascadeUnitFromString (CvModEntry.cpp); the two
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
	// The slot axes (family/kind/scope/channel/dictionary) are COPIED from the compiled CvModEntry at push --
	// the parse typed every axis once ([DEC-materialize-at-mapfrom]); nothing here re-interprets an address
	// string. An entry outside the vocabulary leaves channel = -1 and every slot consumer skips it -- how the
	// unit-plane families and any batch-pending member drop out with no special-casing.

	// FK-resolve the LAST segment when it is a keyed deposit's INFOTYPE target ("<chan>.<scope>.<member>.<KEY>").
	// Hide-assert: a non-key tail (a member name like "goldenAge"/"distance") simply doesn't resolve. Deliberately
	// NOT routed through jsonResolveId -- a member name must never be reported as an unresolved FK.
	d.targetFk = (d.nSeg >= 3 && last.find('_') != std::string::npos)
		? GC.getInfoTypeForString(last.c_str(), true) : -1;
}

// One CvModifiers unit's compiled entries -> compiled records: per entry, the record carries the entry's
// payload (value / enabled / disabled, borrowed) + the authored address spelled back from the entry's
// interned segments (render/diagnostics only) + the entry's unit spelled as the unit segment; compile()
// interns + FK-resolves into THIS index's own id space. Entry order is the authored walk order -- every
// consumer sums commutatively, so order carries no semantics. `j` = the SOURCE info (the SELF per token
// collapses onto it).
static void di_pushFamilies(const CvInfo* j, const CvModifiers* mods, std::vector<CascadeDeposit>& out)
{
	if (mods == NULL || mods->empty()) return;
	const std::vector<CvModEntry*>& entries = mods->entries();
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const CvModEntry* e = entries[i];
		if (e == NULL) continue;
		const char* szUnit = DepositIndex::unitSegment(e->unit);
		if (szUnit[0] == '\0') continue;   // UNKNOWN never reaches an entry (the leaf parse takes real units only)
		out.push_back(CascadeDeposit());
		CascadeDeposit& d = out.back();
		d.entry = e;   // the entry this record compiles -- the apply path's route back to the ONE resolve
		d.address = e->address();
		d.unit = szUnit;
		d.value = e->value;
		d.aiOnly = e->aiOnly;   // the §3.9 audience flag rides the record (the address carries NO ai segment)
		// the typed slot axes, copied straight off the compiled entry (parse typed them once); the channel is
		// minted through the registry, which also derives the per-scope channel SETS from this same push
		// (state-repositories.md KEYS ONLY WHERE NEEDED -- the layout falls out of the data).
		d.family = (short)e->family;
		d.kind = (short)e->kind;
		d.propertyFk = e->propertyFk;
		d.scopeIdx = (short)e->scope;
		d.isPercent = (e->unit == CASC_UNIT_PERCENT || e->unit == CASC_UNIT_RAW_PERCENT);
		if ((int)e->scope < CASCADE_PACKAGE_SCOPES)
		{
			d.channel = CascadeChannelRegistry::registerDeposit(e->scope, e->family, e->kind, e->propertyFk);
		}
		d.enabled = e->enabled;
		d.disabled = e->disabled;
		d.unitQual = e->unitQual;
		d.religionQual = e->religionQual;
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
		// the §3.7 `per.above` threshold (ruling 26): the base was source-resolved at mapFrom
		// (CvModifiers::resolveAboveToken); a token spelling interns so perScale's scaling leg compares ints.
		d.hasAbove = e->hasAbove;
		d.perAbove = e->perAbove;
		if (e->hasAbove && !e->perAboveToken.empty())
			d.perAboveSeg = DepositIndex::internSegment(e->perAboveToken);
		DepositIndex::compile(d);
	}
}

void DepositIndex::pushInfo(const CvInfo* j)
{
	if (j == NULL) return;
	const CvModifiers* mods = j->getModifiers();
	const CvModifiers* obs = j->getWhenObsolete();
	if ((mods == NULL || mods->empty()) && (obs == NULL || obs->empty())) return;
	if (s_sourceIndex.find(j) == s_sourceIndex.end())
	{
		const int iNext = (int)s_sourceIndex.size();
		s_sourceIndex[j] = iNext;
	}
	DiCompiledSet& set = s_compiled[j];
	set.main.clear();          // re-push-safe: a re-mapped info compiles fresh, never doubles
	set.whenObsolete.clear();
	di_pushFamilies(j, mods, set.main);
	di_pushFamilies(j, obs, set.whenObsolete);
}

void DepositIndex::clearCompiled()
{
	s_compiled.clear();
	s_sourceIndex.clear();
	s_gatedByType.clear();
	s_gatedByToken.clear();
	s_gatedByPredicate.clear();
	s_gatedReligionCounts.clear();
	s_propertyGateThresholds.clear();
	s_bDepsCompiled = false;
}

// ---- the condition-dependency compile: ONE global pass over every compiled record's gates (modifier.md §3:
// ---- a conditioned deposit re-resolves when the state its gate reads moves, routed by these tables).

// Classify one condition-tree node's state reads into the dependency tables, crediting them with the carrying
// record's reach. GROUP nodes recurse; PRESENCE atoms key their TYPE string; parameterized predicates key the
// param TYPE; bare predicates key their kind.
static void di_scanConditionTree(const CvCondition* node, const CvInfo* pSource, const CascadeDeposit& record)
{
	if (node == NULL)
	{
		return;
	}
	if (node->kind == CASC_COND_GROUP)
	{
		for (size_t i = 0; i < node->all.size(); ++i)
		{
			di_scanConditionTree(node->all[i], pSource, record);
		}
		for (size_t i = 0; i < node->anyOf.size(); ++i)
		{
			di_scanConditionTree(node->anyOf[i], pSource, record);
		}
		for (size_t i = 0; i < node->noneOf.size(); ++i)
		{
			di_scanConditionTree(node->noneOf[i], pSource, record);
		}
		di_scanConditionTree(node->enabled, pSource, record);
		di_scanConditionTree(node->disabled, pSource, record);
		return;
	}
	if (node->kind == CASC_COND_PRESENCE)
	{
		if (!node->type.empty())
		{
			s_gatedByType[node->type].push_back(DepositIndex::GatedDeposit(pSource, &record, DepositIndex::sourceIndexOf(pSource)));
			// A PROPERTY gate's bounds are BOUNDARIES the band emit must test (see s_propertyGateThresholds).
			// The boundary is the authored value itself for both senses: a `min: T` clause flips between T-1
			// and T and a `max: T` clause between T and T+1, and the emit's closed-interval sweep test catches
			// each (CvProperties.cpp). hasMin/hasMax are the authored-bound flags -- a property bound is
			// legitimately NEGATIVE, so the value's sign says nothing ([enabler.md] §3).
			if (node->id >= 0 && node->type.compare(0, 9, "PROPERTY_") == 0)
			{
				if (node->hasMin) { s_propertyGateThresholds[node->id].insert(node->min); }
				if (node->hasMax) { s_propertyGateThresholds[node->id].insert(node->max); }
			}
		}
		return;
	}
	// PREDICATE: a parameterized predicate depends on the named INFOTYPE's state; a bare one on its own fact.
	if (!node->param.empty())
	{
		s_gatedByType[node->param].push_back(DepositIndex::GatedDeposit(pSource, &record, DepositIndex::sourceIndexOf(pSource)));
	}
	if (node->predKind != CASC_PRED_UNKNOWN)
	{
		s_gatedByPredicate[(int)node->predKind].push_back(DepositIndex::GatedDeposit(pSource, &record, DepositIndex::sourceIndexOf(pSource)));
	}
}

static void di_scanRecordDependencies(const CvInfo* pSource, const CascadeDeposit& record)
{
	di_scanConditionTree(record.enabled, pSource, record);
	di_scanConditionTree(record.disabled, pSource, record);
	// the §3.7 per count-scaler: the counted state's changes rescale the deposit
	if (record.hasPer && !record.perType.empty())
	{
		if (record.perTypeId >= 0)
		{
			s_gatedByType[record.perType].push_back(DepositIndex::GatedDeposit(pSource, &record, DepositIndex::sourceIndexOf(pSource)));
		}
		else
		{
			s_gatedByToken[record.perType].push_back(DepositIndex::GatedDeposit(pSource, &record, DepositIndex::sourceIndexOf(pSource)));
		}
	}
	if (record.perAnyOfTypes != NULL)
	{
		for (size_t i = 0; i < record.perAnyOfTypes->size(); ++i)
		{
			s_gatedByType[(*record.perAnyOfTypes)[i]].push_back(DepositIndex::GatedDeposit(pSource, &record, DepositIndex::sourceIndexOf(pSource)));
		}
	}
	// the legacy per-unit spellings are population/specialist-count dependencies by construction
	if (record.unit == "perPopulation")
	{
		s_gatedByToken["POPULATION"].push_back(DepositIndex::GatedDeposit(pSource, &record, DepositIndex::sourceIndexOf(pSource)));
	}
	// the §3.7 religion: counted-kind filter re-counts on any city religion change
	if (record.religionQual != NULL)
	{
		s_gatedReligionCounts.push_back(DepositIndex::GatedDeposit(pSource, &record, DepositIndex::sourceIndexOf(pSource)));
		di_scanConditionTree(record.religionQual, pSource, record);
	}
}

void DepositIndex::compileDependencies()
{
	if (s_bDepsCompiled)
	{
		return;
	}
	s_bDepsCompiled = true;
	for (std::map<const CvInfo*, DiCompiledSet>::const_iterator it = s_compiled.begin(); it != s_compiled.end(); ++it)
	{
		const DiCompiledSet& set = it->second;
		for (size_t i = 0; i < set.main.size(); ++i)
		{
			di_scanRecordDependencies(it->first, set.main[i]);
		}
		for (size_t i = 0; i < set.whenObsolete.size(); ++i)
		{
			di_scanRecordDependencies(it->first, set.whenObsolete[i]);
		}
	}
}

// The gated-deposit accessors -- compiled by compileDependencies(); NULL means nothing anywhere depends on
// that state.
int DepositIndex::sourceIndexOf(const CvInfo* j)
{
	if (j == NULL)
	{
		return -1;
	}
	const std::map<const CvInfo*, int>::const_iterator it = s_sourceIndex.find(j);
	return (it == s_sourceIndex.end()) ? -1 : it->second;
}

const std::vector<DepositIndex::GatedDeposit>* DepositIndex::gatedByType(const std::string& szType)
{
	const std::map<std::string, std::vector<GatedDeposit> >::const_iterator it = s_gatedByType.find(szType);
	return (it == s_gatedByType.end()) ? NULL : &it->second;
}

const std::vector<DepositIndex::GatedDeposit>* DepositIndex::gatedByToken(const char* szToken)
{
	if (szToken == NULL)
	{
		return NULL;
	}
	const std::map<std::string, std::vector<GatedDeposit> >::const_iterator it = s_gatedByToken.find(std::string(szToken));
	return (it == s_gatedByToken.end()) ? NULL : &it->second;
}

const std::vector<DepositIndex::GatedDeposit>* DepositIndex::gatedByPredicate(CvCascPredKind ePredicate)
{
	const std::map<int, std::vector<GatedDeposit> >::const_iterator it = s_gatedByPredicate.find((int)ePredicate);
	return (it == s_gatedByPredicate.end()) ? NULL : &it->second;
}

const std::vector<DepositIndex::GatedDeposit>* DepositIndex::gatedByReligionCounts()
{
	return s_gatedReligionCounts.empty() ? NULL : &s_gatedReligionCounts;
}

const std::map<int, std::set<int> >& DepositIndex::propertyGateThresholds()
{
	return s_propertyGateThresholds;
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

