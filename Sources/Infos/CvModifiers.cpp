//
//	CvModifiers -- the load COMPILE pass over an entity's §6 modifier families (see the header). The walk
//	recurses the family tree exactly as authored (a unit keyword ends the address; a bare-number/array
//	non-unit key is the count-by-type leaf; any other key recurses one segment deeper), decodes every leaf's
//	address ONCE to typed ids (family / scope / kind / target -- [DEC-materialize-at-mapfrom]), and RETAINS
//	every §3.9 deposit as a typed entry (the COMPLETE list -- unconditioned entries included, ruling 29).
//	finalizeCompiled then derives the (family, kind, scope, unit) slot sums FROM that list at compile end --
//	one derivation, list -> sums. Strings exist only inside this load-time walk.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvModifiers.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition -- the ONE human->condition boundary
#include "CvJsonParse.h"            // jsonClassifyKey / jsonParseScope / jsonIsScopeToken / jsonX100 / jsonResolveId
#include <algorithm>

namespace
{
	//	⛔ A PERCENT IS NOT SCALED (owner). The ×100 exists so an AMOUNT -- a yield, a combat value -- can carry
	//	two decimals at the edge; a percentage has no decimals to carry, so scaling it buys nothing and costs a
	//	second identity constant (`100 + Σpercent` would become `10000 + Σpercent` at every site a percent is
	//	combined). `flat` and `multiplier` DO convert: a flat is an amount, and a multiplier is authored on the
	//	same two-decimal footing with identity 100 (×1.5 -> 150).
	int mod_valueForUnit(double dHuman, CvCascUnit eUnit)
	{
		// ⛔ Branch on the PERCENT SIDE, not the one enumerator: every downstream site treats rawPercent as a
		// percent (the gather's percent-side test, the deposit index, the entry text), so scaling it here would
		// land it ×100 in the percent plane and blow the `100 + Sigma-percent` identity by a hundred.
		if (eUnit == CASC_UNIT_PERCENT || eUnit == CASC_UNIT_RAW_PERCENT)
		{
			return (int)(dHuman >= 0 ? dHuman + 0.5 : dHuman - 0.5);
		}
		// ⛔ THERE IS NO COUNT EXEMPTION. The §2 unit table is the whole vocabulary -- flat (x100), percent
		// (unscaled), multiplier (x100) -- and the count-by-type leaf (CASC_UNIT_COUNT: freeSpecialists /
		// allowedSpecialists, modifier.md §6) is AUTHORED ON AN INFO, so it is an amount like any other.
		// ⚠ Its NAME is the trap: "count" reads like STATE, and state (population, era) genuinely is not scaled
		// -- but state is not from an info and never reaches this function. "Half a specialist does not exist"
		// is true and is not a reason: the rule's worth is that it has NO exceptions, because an exemption
		// argued well for one field is the precedent the next one is argued from
		// ([fixed-point-and-scales] §4c-zero).
		return jsonX100(dHuman);
	}
}

CvModifiers::~CvModifiers()
{
	clearParsed();
}

void CvModifiers::clearParsed()
{
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		delete m_entries[i];
	}
	m_entries.clear();
	m_conditioned.clear();
	m_keyed.clear();
	m_slots.clear();
	m_propertySlots.clear();
	m_slotsAiOnly.clear();
	m_propertySlotsAiOnly.clear();
}

// ============================ the compiled slot table ============================

// The packed unconditioned-sum slot key: (family, kind, scope, unit) -- the unit is part of the key, so a
// flat sum and a percent sum are SEPARATE slots (modifier.md §2).
static int mod_slotKey(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit)
{
	return (((int)eFamily & 0x7F) << 17) | ((iKind & 0xFF) << 9) | (((int)eScope & 0x1F) << 4) | ((int)eUnit & 0xF);
}

// The per-property plane's slot key (the property FK id replaces the family/kind pair).
static int mod_propertySlotKey(int iPropertyFk, CvCascScope eScope, CvCascUnit eUnit)
{
	return ((iPropertyFk & 0x7FFFFF) << 9) | (((int)eScope & 0x1F) << 4) | ((int)eUnit & 0xF);
}

static void mod_foldSlot(std::vector<std::pair<int, int> >& slots, int iKey, int iValue)
{
	for (size_t i = 0; i < slots.size(); ++i)
	{
		if (slots[i].first == iKey)
		{
			slots[i].second += iValue;
			return;
		}
	}
	slots.push_back(std::make_pair(iKey, iValue));
}

static int mod_readSlot(const std::vector<std::pair<int, int> >& slots, int iKey)
{
	// the table is sorted by finalizeCompiled -- one binary search, 0 calculation
	size_t iLow = 0;
	size_t iHigh = slots.size();
	while (iLow < iHigh)
	{
		const size_t iMid = (iLow + iHigh) / 2;
		if (slots[iMid].first < iKey)
		{
			iLow = iMid + 1;
		}
		else
		{
			iHigh = iMid;
		}
	}
	return (iLow < slots.size() && slots[iLow].first == iKey) ? slots[iLow].second : 0;
}

int CvModifiers::sum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit, CvModAudience eAudience) const
{
	const int iKey = mod_slotKey(eFamily, iKind, eScope, eUnit);
	int iSum = 0;
	if (eAudience != MOD_AUDIENCE_AI_ONLY)
	{
		iSum += mod_readSlot(m_slots, iKey);
	}
	if (eAudience != MOD_AUDIENCE_HUMAN)
	{
		iSum += mod_readSlot(m_slotsAiOnly, iKey);   // the §3.9 ai audience rides ON TOP of the base for AI players
	}
	return iSum;
}

int CvModifiers::sum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit, bool bIncludeAiOnly) const
{
	return sum(eFamily, iKind, eScope, eUnit, bIncludeAiOnly ? MOD_AUDIENCE_INCLUSIVE : MOD_AUDIENCE_HUMAN);
}

int CvModifiers::propertySum(int iPropertyFk, CvCascScope eScope, CvCascUnit eUnit, CvModAudience eAudience) const
{
	if (iPropertyFk < 0)
	{
		return 0;
	}
	const int iKey = mod_propertySlotKey(iPropertyFk, eScope, eUnit);
	int iSum = 0;
	if (eAudience != MOD_AUDIENCE_AI_ONLY)
	{
		iSum += mod_readSlot(m_propertySlots, iKey);
	}
	if (eAudience != MOD_AUDIENCE_HUMAN)
	{
		iSum += mod_readSlot(m_propertySlotsAiOnly, iKey);
	}
	return iSum;
}

// The keyed entry-list read ([modifier.md §5]). Unconditioned entries only, mirroring the point sum: a
// conditioned keyed entry resolves against a live context and is the valuation's job, not a static sum.
void CvModifiers::targetedSums(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
	int iTargetSeg, std::vector<std::pair<int, int> >& kOut, CvModAudience eAudience) const
{
	kOut.clear();
	for (size_t iEntry = 0; iEntry < m_entries.size(); ++iEntry)
	{
		const CvModEntry* pEntry = m_entries[iEntry];
		if (pEntry->family != eFamily || pEntry->scope != eScope || pEntry->unit != eUnit) continue;
		//	⚠ NOT "untargeted, the point slot answers it" -- that was the assumption that left the plural plane
		//	unreadable. An entry here may be genuinely untargeted (the point slot does answer it) OR carry a
		//	PLURAL token with no FK, which the point slot rejects too; `pluralTargetSum` owns the second.
		if (pEntry->targetFk < 0) continue;                       // not the NAMED-target plane this read serves
		if (iTargetSeg >= 0 && pEntry->targetSeg != iTargetSeg) continue;   // the AXIS: never mix keyed planes
		if (iKind >= 0 && pEntry->kind != iKind) continue;
		if (pEntry->enabled != NULL || pEntry->disabled != NULL) continue;   // conditioned -> the valuation
		if (pEntry->aiOnly && eAudience == MOD_AUDIENCE_HUMAN) continue;
		if (!pEntry->aiOnly && eAudience == MOD_AUDIENCE_AI_ONLY) continue;
		size_t iFound = kOut.size();
		for (size_t iSeek = 0; iSeek < kOut.size(); ++iSeek)
		{
			if (kOut[iSeek].first == pEntry->targetFk) { iFound = iSeek; break; }
		}
		if (iFound == kOut.size()) kOut.push_back(std::make_pair(pEntry->targetFk, 0));
		kOut[iFound].second += pEntry->value;
	}
}

void CvModifiers::targetedSums(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
	int iTargetSeg, std::vector<std::pair<int, int> >& kOut, bool bIncludeAiOnly) const
{
	targetedSums(eFamily, iKind, eScope, eUnit, iTargetSeg, kOut, bIncludeAiOnly ? MOD_AUDIENCE_INCLUSIVE : MOD_AUDIENCE_HUMAN);
}

int CvModifiers::pluralTargetSum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
	int iTargetSeg, CvModAudience eAudience) const
{
	if (iTargetSeg < 0)
	{
		return 0;   // the token was never authored anywhere -- nothing can be keyed on it
	}
	int iSum = 0;
	for (size_t iEntry = 0; iEntry < m_entries.size(); ++iEntry)
	{
		const CvModEntry* pEntry = m_entries[iEntry];
		if (pEntry->family != eFamily || pEntry->scope != eScope || pEntry->unit != eUnit) continue;
		if (pEntry->targetSeg != iTargetSeg) continue;
		//	A resolved FK means a NAMED target (`buildings.{B}`) -- that is the keyed plane and targetedSum
		//	owns it. This read is the FK-LESS half: the plural token standing alone.
		if (pEntry->targetFk >= 0) continue;
		if (iKind >= 0 && pEntry->kind != iKind) continue;
		//	⛔ Anything QUALIFIED is the valuation's, not a scope-wide sum ([modifier.md] §5). A predicate on the
		//	target, a unit/religion filter or a `per` scaler each make the value conditional on state this read
		//	has none of, and folding one anyway is the plausible-wrong case that rule names.
		if (pEntry->enabled != NULL || pEntry->disabled != NULL) continue;
		if (pEntry->unitQual != NULL || pEntry->religionQual != NULL || pEntry->hasPer) continue;
		if (pEntry->aiOnly && eAudience == MOD_AUDIENCE_HUMAN) continue;
		if (!pEntry->aiOnly && eAudience == MOD_AUDIENCE_AI_ONLY) continue;
		iSum += pEntry->value;
	}
	return iSum;
}
int CvModifiers::pluralTargetSum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
	int iTargetSeg, bool bIncludeAiOnly) const
{
	return pluralTargetSum(eFamily, iKind, eScope, eUnit, iTargetSeg,
		bIncludeAiOnly ? MOD_AUDIENCE_INCLUSIVE : MOD_AUDIENCE_HUMAN);
}

int CvModifiers::targetedSum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
	int iTargetSeg, int iTargetFk, CvModAudience eAudience) const
{
	if (iTargetFk < 0) return 0;
	std::vector<std::pair<int, int> > kPairs;
	targetedSums(eFamily, iKind, eScope, eUnit, iTargetSeg, kPairs, eAudience);
	for (size_t i = 0; i < kPairs.size(); ++i)
	{
		if (kPairs[i].first == iTargetFk) return kPairs[i].second;
	}
	return 0;
}

int CvModifiers::targetedSum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
	int iTargetSeg, int iTargetFk, bool bIncludeAiOnly) const
{
	return targetedSum(eFamily, iKind, eScope, eUnit, iTargetSeg, iTargetFk, bIncludeAiOnly ? MOD_AUDIENCE_INCLUSIVE : MOD_AUDIENCE_HUMAN);
}

int CvModifiers::propertySum(int iPropertyFk, CvCascScope eScope, CvCascUnit eUnit, bool bIncludeAiOnly) const
{
	return propertySum(iPropertyFk, eScope, eUnit, bIncludeAiOnly ? MOD_AUDIENCE_INCLUSIVE : MOD_AUDIENCE_HUMAN);
}

void CvModifiers::keyedRange(ModifierFamily eFamily, size_t& iBeginOut, size_t& iEndOut) const
{
	// m_keyed is family-sorted by finalizeCompiled; the range is contiguous.
	size_t i = 0;
	while (i < m_keyed.size() && m_keyed[i]->family != eFamily)
	{
		++i;
	}
	iBeginOut = i;
	while (i < m_keyed.size() && m_keyed[i]->family == eFamily)
	{
		++i;
	}
	iEndOut = i;
}

void CvModifiers::conditionedRange(ModifierFamily eFamily, size_t& iBeginOut, size_t& iEndOut) const
{
	// m_conditioned is family-sorted by finalizeCompiled; the range is contiguous.
	size_t i = 0;
	while (i < m_conditioned.size() && m_conditioned[i]->family != eFamily)
	{
		++i;
	}
	iBeginOut = i;
	while (i < m_conditioned.size() && m_conditioned[i]->family == eFamily)
	{
		++i;
	}
	iEndOut = i;
}

// ============================ the load-time compile walk ============================

//
//	The node-level §3.9 qualifier shorthand (json §3.9: "a qualifier written at the target-node level is
//	shorthand applying to every entry that carries none of its own"): the counted-kind filters (`unit:` /
//	`religion:`) + the ranked-selection pair (`max:` / `orderedBy(Descending):`, ruling 25). Gathered per walk
//	node, consumed as qualifiers -- never address segments, never count-by-type leaves. Value-shape
//	discriminated: an OBJECT-valued `max` stays a MEMBER segment (tradeRoutes.<scope>.max.{unit}); only a
//	number/token `max` is the ranked qualifier.
//
struct ModNodeQuals
{
	const picojson::value* unitQual;       // string predicate
	const picojson::value* religionQual;   // string predicate
	const picojson::value* maxQual;        // number, or a token string (TARGET_NUM_CITIES)
	const picojson::value* orderedQual;    // string metric (CITY_SIZE)
	bool orderedDescending;
	ModNodeQuals() : unitQual(NULL), religionQual(NULL), maxQual(NULL), orderedQual(NULL), orderedDescending(false) {}
};

namespace
{
	// Stamp the ranked-selection qualifiers onto an entry from raw picojson values (entry-form and node-form
	// share this). CARRY-only: the ranked SELECTION evaluation is the parked ranked-target-selection plan.
	void mod_stampRankQuals(CvModEntry* pEntry, const picojson::value* pMax, const picojson::value* pOrdered, bool bDescending)
	{
		if (pMax != NULL)
		{
			pEntry->hasRankQual = true;
			if (pMax->is<double>())
			{
				pEntry->rankMax = (int)pMax->get<double>();
			}
			else if (pMax->is<std::string>())
			{
				pEntry->rankMaxToken = pMax->get<std::string>();
			}
		}
		if (pOrdered != NULL && pOrdered->is<std::string>())
		{
			pEntry->hasRankQual = true;
			pEntry->orderedBySeg = modSegmentIntern(pOrdered->get<std::string>());
			pEntry->orderedDescending = bDescending;
		}
	}

	// The per-LEAF address decode -- computed once, stamped on every entry the leaf produces.
	struct ModLeafDecode
	{
		ModifierFamily family;
		int propertyFk;
		CvCascScope scope;
		int kind;
		int memberSeg;
		int targetSeg;
		int targetFk;
		int nSeg;
		int seg[CvModEntry::MOD_ENTRY_SEGS];
		ModLeafDecode()
			: family(MODFAM_NONE), propertyFk(-1), scope(CASC_SCOPE_CITY), kind(-1), memberSeg(-1),
			  targetSeg(-1), targetFk(-1), nSeg(0)
		{
			for (int i = 0; i < CvModEntry::MOD_ENTRY_SEGS; ++i)
			{
				seg[i] = -1;
			}
		}
	};

	void mod_decodeLeaf(const std::vector<std::string>& segments, ModLeafDecode& decode)
	{
		decode.nSeg = (int)segments.size();
		for (size_t i = 0; i < segments.size() && (int)i < CvModEntry::MOD_ENTRY_SEGS; ++i)
		{
			decode.seg[i] = modSegmentIntern(segments[i]);
		}
		if (segments.empty())
		{
			return;
		}
		decode.family = infoFamilyFromKey(segments[0]);
		if (decode.family == MODFAM_PROPERTY)
		{
			decode.propertyFk = jsonResolveId(segments[0]);
		}
		size_t iTailStart = 1;
		if (segments.size() > 1 && jsonIsScopeToken(segments[1]))
		{
			decode.scope = jsonParseScope(segments[1], CASC_SCOPE_CITY);
			iTailStart = 2;
		}
		std::string szMemberPath;
		for (size_t i = iTailStart; i < segments.size(); ++i)
		{
			const std::string& szSegment = segments[i];
			// `any` is the UNTYPED bucket -- N slots whose specialist type the ENGINE picks at placement
			// ("the AI chooses whatever specialist is best", owner), which is modifier.md §6's two-part seam:
			// the cascade owns the AMOUNT, placement stays the engine's. So it is authoring sugar for the
			// MEMBERLESS scope-wide amount, never a named target -- routing it to targetSeg would make the
			// entry unfoldable (isPointFoldable) and strand the amount outside the package plane, where no
			// scope roll-up can reach it and every reader would have to walk the live sources instead.
			if (szSegment == "any")
			{
				continue;
			}
			if (infoIsTargetToken(szSegment) || infoIsFamilyTargetToken(decode.family, szSegment))
			{
				decode.targetSeg = modSegmentIntern(szSegment);
				continue;
			}
			if (szSegment.find('_') != std::string::npos)
			{
				// an INFOTYPE / count-token key segment (member spellings are camelCase, never underscored);
				// an unresolved id surfaces via jsonResolveId's diagnostic and re-resolves on the full-registry re-map
				decode.targetFk = jsonResolveId(szSegment);
				continue;
			}
			if (!szMemberPath.empty())
			{
				szMemberPath += ".";
			}
			szMemberPath += szSegment;
		}
		if (szMemberPath.empty())
		{
			decode.kind = (decode.family == MODFAM_NONE) ? -1 : 0;   // kind 0 = the scope-wide amount
		}
		else
		{
			decode.memberSeg = modSegmentIntern(szMemberPath);
			decode.kind = infoResolveKind(decode.family, szMemberPath);
			if (decode.kind < 0)
			{
				infoNoteUnkindedMember(segments[0], szMemberPath);
			}
		}
	}

	void mod_stampDecode(CvModEntry* pEntry, const ModLeafDecode& decode)
	{
		pEntry->family = decode.family;
		pEntry->propertyFk = decode.propertyFk;
		pEntry->scope = decode.scope;
		pEntry->kind = decode.kind;
		pEntry->memberSeg = decode.memberSeg;
		pEntry->targetSeg = decode.targetSeg;
		pEntry->targetFk = decode.targetFk;
		pEntry->nSeg = decode.nSeg;
		for (int i = 0; i < CvModEntry::MOD_ENTRY_SEGS; ++i)
		{
			pEntry->seg[i] = decode.seg[i];
		}
	}

	// One `{value, unit?, religion?, max?, orderedBy(Descending)?, per?, enabled?, disabled?}` entry object
	// (§3.9; `unit:`/`religion:` = the §3.7 counted-kind filters; max/orderedBy* = the §3.3 ranked qualifiers,
	// parse-CARRIED per ruling 25).
	CvModEntry* mod_parseEntryObject(const picojson::object& o, CvCascUnit eUnit, CvCascScope eScope)
	{
		picojson::object::const_iterator valueIt = o.find("value");
		if (valueIt == o.end() || !valueIt->second.is<double>())
		{
			return NULL;   // not an entry object
		}
		CvModEntry* pEntry = new CvModEntry();
		pEntry->value = mod_valueForUnit(valueIt->second.get<double>(), eUnit);
		pEntry->unit = eUnit;
		pEntry->scope = eScope;
		picojson::object::const_iterator it;
		if ((it = o.find("unit")) != o.end())
		{
			pEntry->unitQual = cascadeParseCondition(it->second);
		}
		if ((it = o.find("religion")) != o.end())
		{
			pEntry->religionQual = cascadeParseCondition(it->second);
		}
		{
			picojson::object::const_iterator maxIt = o.find("max");
			const picojson::value* pMax = (maxIt != o.end() && !maxIt->second.is<picojson::object>()) ? &maxIt->second : NULL;
			picojson::object::const_iterator ordAscIt = o.find("orderedBy");
			picojson::object::const_iterator ordDescIt = o.find("orderedByDescending");
			const picojson::value* pOrdered = NULL;
			bool bDescending = false;
			if (ordDescIt != o.end())
			{
				pOrdered = &ordDescIt->second;
				bDescending = true;
			}
			else if (ordAscIt != o.end())
			{
				pOrdered = &ordAscIt->second;
			}
			mod_stampRankQuals(pEntry, pMax, pOrdered, bDescending);
		}
		if ((it = o.find("per")) != o.end())
		{
			jsonParsePer(pEntry, it->second);
		}
		if ((it = o.find("enabled")) != o.end())
		{
			pEntry->enabled = cascadeParseCondition(it->second);
		}
		if ((it = o.find("disabled")) != o.end())
		{
			pEntry->disabled = cascadeParseCondition(it->second);
		}
		return pEntry;
	}

	// The §3.9 ENTRY-FORM `ai` sibling ({ <payload>, ..., "ai"? } -- same inner shape, AI audience): a bare
	// number or a nested entry object riding beside the payload INSIDE one entry object. Parsed into its OWN
	// aiOnly-flagged entry -- the audience is a FLAG on the compiled entry, never an address segment. (The
	// node-level `ai` hop -- the handicap dual-leaf shape -- is the walk's bAiOnly recursion; this covers the
	// entry-internal spelling so the §3.9 mechanism is UNREDUCED.)
	CvModEntry* mod_parseEntryAiSibling(const picojson::object& o, CvCascUnit eUnit, CvCascScope eScope);

	CvModEntry* mod_makeBareEntry(int iValue, CvCascUnit eUnit, CvCascScope eScope)
	{
		CvModEntry* pEntry = new CvModEntry();
		pEntry->value = iValue;
		pEntry->unit = eUnit;
		pEntry->scope = eScope;
		return pEntry;
	}

	CvModEntry* mod_parseEntryAiSibling(const picojson::object& o, CvCascUnit eUnit, CvCascScope eScope)
	{
		picojson::object::const_iterator aiIt = o.find("ai");
		if (aiIt == o.end())
		{
			return NULL;
		}
		CvModEntry* pAiEntry = NULL;
		if (aiIt->second.is<double>())
		{
			pAiEntry = mod_makeBareEntry(mod_valueForUnit(aiIt->second.get<double>(), eUnit), eUnit, eScope);
		}
		else if (aiIt->second.is<picojson::object>())
		{
			pAiEntry = mod_parseEntryObject(aiIt->second.get<picojson::object>(), eUnit, eScope);
		}
		if (pAiEntry != NULL)
		{
			pAiEntry->aiOnly = true;
		}
		return pAiEntry;
	}

	bool mod_conditionedFamilyBefore(const CvModEntry* pLeft, const CvModEntry* pRight)
	{
		return (int)pLeft->family < (int)pRight->family;
	}

	bool mod_slotKeyBefore(const std::pair<int, int>& kLeft, const std::pair<int, int>& kRight)
	{
		return kLeft.first < kRight.first;
	}
}

// Route a point-foldable entry into its AUDIENCE's slot table (json §3.9 `ai`: the aiOnly twin tables keep the
// human point sums audience-clean; sum's explicit bIncludeAiOnly adds the twin back for an AI-player read).
void CvModifiers::foldPointEntry(const CvModEntry* pEntry)
{
	std::vector<std::pair<int, int> >& slots = pEntry->aiOnly ? m_slotsAiOnly : m_slots;
	std::vector<std::pair<int, int> >& propertySlots = pEntry->aiOnly ? m_propertySlotsAiOnly : m_propertySlots;
	if (pEntry->family == MODFAM_PROPERTY)
	{
		if (pEntry->propertyFk >= 0)
		{
			mod_foldSlot(propertySlots, mod_propertySlotKey(pEntry->propertyFk, pEntry->scope, pEntry->unit), pEntry->value);
		}
	}
	else
	{
		mod_foldSlot(slots, mod_slotKey(pEntry->family, pEntry->kind, pEntry->scope, pEntry->unit), pEntry->value);
	}
}

void CvModifiers::parseLeaf(const std::vector<std::string>& segments, const picojson::value& leaf,
                            CvCascUnit eUnit, const ModNodeQuals& nodeQuals, bool bAiOnly)
{
	ModLeafDecode decode;
	mod_decodeLeaf(segments, decode);

	const size_t iFirst = m_entries.size();
	if (leaf.is<double>())   // a bare, always-on value
	{
		m_entries.push_back(mod_makeBareEntry(mod_valueForUnit(leaf.get<double>(), eUnit), eUnit, decode.scope));
	}
	else if (leaf.is<bool>())
	{
		// a §8 keyed-container MEMBERSHIP flag (the combat targets/unitTargets/defenders {UNITCOMBAT_X: true}
		// maps -- json §8: keyed targeting/immunity rides the combat family): compiles as a TARGETED entry
		// (value 1 ×100, synthesized COUNT unit -- never point-foldable, an entry-list read) ONLY under a
		// recognized keyed-container token. `true` asserts membership, the jsonBoolSet convention (a false is
		// no entry); a bool OUTSIDE a keyed container stays uncompiled -- its unkinded member path already
		// surfaced through mod_decodeLeaf's diagnostic, never silently dropped.
		if (decode.targetSeg >= 0 && leaf.get<bool>())
		{
			m_entries.push_back(mod_makeBareEntry(mod_valueForUnit(1, eUnit), eUnit, decode.scope));
		}
	}
	else if (leaf.is<picojson::object>())   // a single `{value, unit?, per?, enabled?, disabled?, ai?}` entry (§3.9)
	{
		CvModEntry* pSingle = mod_parseEntryObject(leaf.get<picojson::object>(), eUnit, decode.scope);
		if (pSingle != NULL)
		{
			m_entries.push_back(pSingle);
		}
		CvModEntry* pAiSibling = mod_parseEntryAiSibling(leaf.get<picojson::object>(), eUnit, decode.scope);
		if (pAiSibling != NULL)
		{
			m_entries.push_back(pAiSibling);
		}
	}
	else if (leaf.is<picojson::array>())
	{
		const picojson::array& entryList = leaf.get<picojson::array>();
		for (size_t i = 0; i < entryList.size(); ++i)
		{
			if (entryList[i].is<double>())   // a bare number in a list = an always-on entry
			{
				m_entries.push_back(mod_makeBareEntry(mod_valueForUnit(entryList[i].get<double>(), eUnit), eUnit, decode.scope));
				continue;
			}
			if (!entryList[i].is<picojson::object>())
			{
				continue;
			}
			CvModEntry* pListed = mod_parseEntryObject(entryList[i].get<picojson::object>(), eUnit, decode.scope);
			if (pListed != NULL)
			{
				m_entries.push_back(pListed);
			}
			CvModEntry* pListedAiSibling = mod_parseEntryAiSibling(entryList[i].get<picojson::object>(), eUnit, decode.scope);
			if (pListedAiSibling != NULL)
			{
				m_entries.push_back(pListedAiSibling);
			}
		}
	}
	for (size_t i = iFirst; i < m_entries.size(); ++i)
	{
		CvModEntry* pEntry = m_entries[i];
		// the §3.9 AUDIENCE flag: inherited from the walk's `ai` hop -- every entry this leaf produced under an
		// `ai` node is AI-players-only (mod_parseEntryObject's entry-form `ai` sibling stamps its own).
		if (bAiOnly)
		{
			pEntry->aiOnly = true;
		}
		// the NODE-form §3.9 qualifier shorthand (unit:/religion: filters + the ranked pair): applies to every
		// entry this leaf produced that carries no entry-form qualifier of its own kind.
		if (nodeQuals.unitQual != NULL && pEntry->unitQual == NULL)
		{
			pEntry->unitQual = cascadeParseCondition(*nodeQuals.unitQual);
		}
		if (nodeQuals.religionQual != NULL && pEntry->religionQual == NULL)
		{
			pEntry->religionQual = cascadeParseCondition(*nodeQuals.religionQual);
		}
		if (!pEntry->hasRankQual && (nodeQuals.maxQual != NULL || nodeQuals.orderedQual != NULL))
		{
			mod_stampRankQuals(pEntry, nodeQuals.maxQual, nodeQuals.orderedQual, nodeQuals.orderedDescending);
		}
		mod_stampDecode(pEntry, decode);
		// the slot sums are NOT folded here: finalizeCompiled derives them from the retained entry list at
		// compile end -- ONE derivation (list -> sums), never a second fill beside the list that can drift
	}
}

void CvModifiers::walk(std::vector<std::string>& segments, const picojson::value& node, bool bAiOnly)
{
	if (!node.is<picojson::object>())
	{
		return;   // a family/segment node is always object-valued (json §1/§6)
	}
	const picojson::object& o = node.get<picojson::object>();
	// gather the NODE-form §3.9 qualifier shorthand -- siblings of the magnitude leaves, consumed as
	// qualifiers, never address segments / count-by-type leaves. Shape-discriminated: an OBJECT-valued `max`
	// stays a member segment (tradeRoutes.<scope>.max.{unit}); string-valued unit/religion/orderedBy* only.
	ModNodeQuals nodeQuals;
	for (picojson::object::const_iterator qualIt = o.begin(); qualIt != o.end(); ++qualIt)
	{
		if (qualIt->first == "unit" && qualIt->second.is<std::string>())
		{
			nodeQuals.unitQual = &qualIt->second;
		}
		else if (qualIt->first == "religion" && qualIt->second.is<std::string>())
		{
			nodeQuals.religionQual = &qualIt->second;
		}
		else if (qualIt->first == "max" && !qualIt->second.is<picojson::object>())
		{
			nodeQuals.maxQual = &qualIt->second;
		}
		else if (qualIt->first == "orderedByDescending" && qualIt->second.is<std::string>())
		{
			nodeQuals.orderedQual = &qualIt->second;
			nodeQuals.orderedDescending = true;
		}
		else if (qualIt->first == "orderedBy" && qualIt->second.is<std::string>())
		{
			nodeQuals.orderedQual = &qualIt->second;
			nodeQuals.orderedDescending = false;
		}
	}
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		if (&it->second == nodeQuals.unitQual || &it->second == nodeQuals.religionQual
		 || &it->second == nodeQuals.maxQual || &it->second == nodeQuals.orderedQual)
		{
			continue;   // a qualifier key itself
		}
		if (it->first == "ai" && it->second.is<picojson::object>())
		{
			// the §3.9 AUDIENCE HOP (the ai sibling block, same inner shape -- the handicap human/AI dual-leaf
			// pattern: upkeep.empire.unit.{percent, ai:{percent}}): recurse WITHOUT pushing a segment, so the
			// member path below kind-resolves cleanly and the audience compiles as the entries' aiOnly FLAG --
			// never an address segment.
			walk(segments, it->second, true);
			continue;
		}
		const CvCascUnit eUnit = cascadeUnitFromString(it->first);
		if (eUnit != CASC_UNIT_UNKNOWN)   // a unit keyword ends the address -- this is a magnitude LEAF
		{
			parseLeaf(segments, it->second, eUnit, nodeQuals, bAiOnly);
		}
		else if (it->second.is<double>() || it->second.is<picojson::array>() || it->second.is<bool>())
		{
			// the modifier.md §6 COUNT-BY-TYPE leaf (freeSpecialists/allowedSpecialists: the key IS the type/`any`
			// and the value the count) -- the one sanctioned non-unit leaf; synthesized unit COUNT, key in the
			// address. A BOOL value is the §8 keyed-container MEMBERSHIP flag (combat targets/unitTargets/
			// defenders) -- parseLeaf compiles it only under a recognized keyed-container token.
			segments.push_back(it->first);
			parseLeaf(segments, it->second, CASC_UNIT_COUNT, nodeQuals, bAiOnly);
			segments.pop_back();
		}
		else
		{
			segments.push_back(it->first);
			walk(segments, it->second, bAiOnly);   // a scope/target/member segment -- one level deeper
			segments.pop_back();
		}
	}
}

void CvModifiers::landReverseEntry(CvModEntry* pEntry)
{
	if (pEntry == NULL)
	{
		return;
	}
	m_entries.push_back(pEntry);
	// finalizeCompiled re-derives the compiled forms from the (now grown) entry list, so a landed entry is
	// indistinguishable from an authored one to every reader -- incl. the slot fold, should a landed entry
	// ever be point-foldable (today every landing carries the source-presence condition).
	finalizeCompiled();
}

//	LOAD-TIME ONLY -- the PURE_TRAITS alignment filter (modifier.md §4): under
//	GAMEOPTION_LEADER_PURE_TRAITS a positive trait's DOWNSIDE values drop and a negative trait's UPSIDE values
//	drop. It runs as a PARSE TRANSFORM, between the trait being read and its entries LANDING, which is the same
//	class as the reverse pass's "land it on the target" (modifier.md §4: a parse transform, never an authored
//	shape). The curator deliberately does NOT pre-filter -- "the JSON carries ALL values ... so the runtime gates
//	below have the full data to act on" -- so this is the CLEAN gate that ruling hands to the cascade.
//
//	⛔ IT GATES; IT DOES NOT DROP. The option is per-GAME while an info is loaded once per PROCESS and shared by
//	every game in it, so baking the verdict into the compiled data would make a shared immutable object mutable
//	per game rather than per load ([DEC-json-not-cascade]). Composing the option onto the entry's `disabled`
//	keeps the decision where it belongs -- evaluated live, by the ONE evaluator.
//
//	⚑ WHY THIS PLACE COSTS NOTHING ELSEWHERE, which is the whole point of doing it here: attaching a condition
//	moves the entry OUT of the compiled unconditioned point sum and INTO the conditioned list (patterns.md: a
//	null-condition entry folds straight into the sum, a conditioned one lands in the conditioned list). So the
//	point reads become correct by construction -- a compiled sum can never be sign-filtered at read time, its
//	positives and negatives are already added together -- and every entry-walking consumer honours it through
//	the ONE MMKernel::applies they all already call. No parameter is threaded to any call site.
//	⚠ This is the ONLY place the rule is applied. No consumer filters by alignment, and none may: a private
//	filter in the oracle would be a rule the stored-vs-oracle diff could never report on.
void CvModifiers::applyPureTraitGate(bool bNegativeTrait)
{
	bool bChanged = false;
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		CvModEntry* pEntry = m_entries[i];
		if (pEntry == NULL || pEntry->value == 0)
		{
			continue;   // a zero carries no alignment to oppose
		}
		// ⛔ ALIGNMENT IS NOT THE SIGN -- ask the FAMILY which way its numbers point
		// ([DEC-single-implementation]: the one polarity table, `infoKindAlignmentInverted`). On an INVERTED
		// (family, kind) a positive value is the DOWNSIDE, so the test flips with it.
		// ⚑ Getting this wrong is silent in both directions and was: `lessYieldThreshold: +5` on a POSITIVE
		// trait survived the gate as though it were a gain, while `maintenance.distance: -10%` -- a genuine
		// upside -- was dropped as though it were a penalty. Every inverted family was gated backwards, and
		// the resulting numbers stay entirely plausible.
		const bool bInverted = infoKindAlignmentInverted(pEntry->family, pEntry->kind);
		const bool bValueIsUpside = bInverted ? (pEntry->value < 0) : (pEntry->value > 0);
		const bool bOffAlignment = bNegativeTrait ? bValueIsUpside : !bValueIsUpside;
		if (!bOffAlignment)
		{
			continue;
		}
		// The ONE typed-condition parser builds it, never a hand-rolled node ([DEC-single-implementation],
		// enabler.md §3.1). One per entry: an entry OWNS its trees and frees them in its dtor.
		const picojson::value kGateLeaf(std::string("GAMEOPTION_LEADER_PURE_TRAITS"));
		CvCondition* pGate = cascadeParseCondition(kGateLeaf);
		if (pGate == NULL)
		{
			continue;
		}
		if (pEntry->disabled == NULL)
		{
			pEntry->disabled = pGate;
		}
		else
		{
			// ⛔ NEVER an overwrite -- an authored `disabled` must keep suppressing what it was written to
			// suppress. Two suppressors is an OR, and an OR is `any` over its direct children (json.md §3.4);
			// the group node owns both, so the authored tree is re-parented rather than copied or leaked.
			CvCondition* pEither = new CvCondition();
			pEither->anyOf.push_back(pEntry->disabled);
			pEither->anyOf.push_back(pGate);
			pEntry->disabled = pEither;
		}
		bChanged = true;
	}
	if (bChanged)
	{
		// Re-derive the compiled forms from the (now gated) entry list -- exactly as a landed reverse entry
		// does. This is what moves the gated entries out of the point sums and into the conditioned list.
		finalizeCompiled();
	}
}

void CvModifiers::resolveAboveToken(const char* szToken, int iBase)
{
	// LOAD-TIME ONLY (see the header): stamp the SOURCE-resolved base onto every entry carrying the token.
	// The token spelling stays -- it is the eval-time scaling marker (MMKernel::perScale). An entry left at
	// perAbove -1 (the source authored no config) is the unresolved case perScale skips, never zeroes.
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		CvModEntry* pEntry = m_entries[i];
		if (pEntry->hasAbove && pEntry->perAbove < 0 && pEntry->perAboveToken == szToken)
		{
			pEntry->perAbove = iBase;
		}
	}
}

void CvModifiers::resolvePerToken(const char* szToken, int iId)
{
	// LOAD-TIME ONLY (see the header): stamp the SOURCE's own engine id onto every entry counting the token.
	// An entry left at perTypeId -1 (no source resolution ran) is the count core's fail-visible 0 case.
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		CvModEntry* pEntry = m_entries[i];
		if (pEntry->hasPer && pEntry->perTypeId < 0 && pEntry->perType == szToken)
		{
			pEntry->perTypeId = iId;
		}
	}
}

void CvModifiers::finalizeCompiled()
{
	// THE ONE DERIVATION (patterns.md getter-setup category 5 / info-rebuild.md ruling 29): the retained
	// COMPLETE entry list is the single compiled source; the folded (family, kind, scope, unit) slot sums are
	// re-derived FROM it here, at compile end -- never a second parallel fill that can drift. Idempotent
	// (clear-first), so parseEntity and every landReverseEntry may re-finalize freely inside the load window.
	m_slots.clear();
	m_propertySlots.clear();
	m_slotsAiOnly.clear();
	m_propertySlotsAiOnly.clear();
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		if (m_entries[i]->isPointFoldable())
		{
			foldPointEntry(m_entries[i]);
		}
	}
	std::sort(m_slots.begin(), m_slots.end(), mod_slotKeyBefore);
	std::sort(m_propertySlots.begin(), m_propertySlots.end(), mod_slotKeyBefore);
	std::sort(m_slotsAiOnly.begin(), m_slotsAiOnly.end(), mod_slotKeyBefore);
	std::sort(m_propertySlotsAiOnly.begin(), m_propertySlotsAiOnly.end(), mod_slotKeyBefore);
	m_conditioned.clear();
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		if (m_entries[i]->isConditioned())
		{
			m_conditioned.push_back(m_entries[i]);
		}
	}
	std::stable_sort(m_conditioned.begin(), m_conditioned.end(), mod_conditionedFamilyBefore);
	// The KEYED-unconditioned view, built the same way and for the same reason. A keyed read is specified as
	// cheap because "it iterates the handful an entity AUTHORED" ([modifier.md] par.5) -- but it was iterating
	// m_entries, i.e. EVERY deposit of every family the entity carries, once per call. A trait holds hundreds,
	// and the read runs per city per candidate, which is what made it measurable.
	// ⚠ m_entries KEEPS ITS AUTHORED ORDER -- the per-entry text render walks it ([patterns.md] category 5) --
	// so this is a second borrowed view rather than a re-sort, exactly as m_conditioned is.
	m_keyed.clear();
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		const CvModEntry& kEntry = *m_entries[i];
		// ⚠ Membership is the RESOLVED TARGET FK and NOTHING ELSE, and both temptations here are wrong:
		// testing the SEGMENT would drop the direct-keyed shape, which sits straight under its scope with no
		// plural container token and so carries none ([modifier.md] par.5); and testing isConditioned() would
		// drop far more than a gate, since it also covers unitQual / religionQual / per / rank -- so every
		// per-scaled and unit-qualified keyed deposit would vanish from the readers below, silently.
		// ⇒ The view is EVERY keyed entry. Each reader keeps its own condition filter, which is where the
		// readers differ: the two point reads want ungated entries only, while the improvement leg deliberately
		// covers gated ones too.
		if (kEntry.targetFk >= 0)
		{
			m_keyed.push_back(m_entries[i]);
		}
	}
	std::stable_sort(m_keyed.begin(), m_keyed.end(), mod_conditionedFamilyBefore);
}

void CvModifiers::parseEntity(const picojson::object& entity)
{
	std::vector<std::string> segments;
	for (picojson::object::const_iterator it = entity.begin(); it != entity.end(); ++it)
	{
		if (jsonClassifyKey(it->first, it->second.is<picojson::object>()) == CJK_FAMILY)
		{
			segments.push_back(it->first);
			walk(segments, it->second, false);   // human audience until an `ai` hop flips it (json §3.9)
			segments.pop_back();
		}
	}
	finalizeCompiled();
}
