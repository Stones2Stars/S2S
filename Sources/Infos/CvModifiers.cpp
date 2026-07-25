//
//	CvModifiers -- the load COMPILE pass over an entity's §6 modifier families (see the header). The walk
//	recurses the family tree exactly as authored (a unit keyword ends the address; a bare-number/array
//	non-unit key is the count-by-type leaf; any other key recurses one segment deeper), decodes every leaf's
//	address ONCE to typed ids (family / scope / kind / target -- [DEC-materialize-at-mapfrom]), and folds each
//	unconditioned untargeted entry straight into its (family, kind, scope, unit) slot sum. Strings exist only
//	inside this load-time walk.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvModifiers.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition -- the ONE human->condition boundary
#include "CvJsonParse.h"            // jsonClassifyKey / jsonParseScope / jsonIsScopeToken / jsonX100 / jsonResolveId
#include <algorithm>

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
	m_slots.clear();
	m_propertySlots.clear();
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

static void mod_foldSlot(std::vector<std::pair<int, int> >& slots, int iKey, int iValue100)
{
	for (size_t i = 0; i < slots.size(); ++i)
	{
		if (slots[i].first == iKey)
		{
			slots[i].second += iValue100;
			return;
		}
	}
	slots.push_back(std::make_pair(iKey, iValue100));
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

int CvModifiers::sum100(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit) const
{
	return mod_readSlot(m_slots, mod_slotKey(eFamily, iKind, eScope, eUnit));
}

int CvModifiers::propertySum100(int iPropertyFk, CvCascScope eScope, CvCascUnit eUnit) const
{
	if (iPropertyFk < 0)
	{
		return 0;
	}
	return mod_readSlot(m_propertySlots, mod_propertySlotKey(iPropertyFk, eScope, eUnit));
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

namespace
{
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
			if (infoIsTargetToken(szSegment))
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

	// One `{value, unit?, per?, enabled?, disabled?}` entry object (§3.9; `unit:` = the §3.7 predicate qualifier).
	CvModEntry* mod_parseEntryObject(const picojson::object& o, CvCascUnit eUnit, CvCascScope eScope)
	{
		picojson::object::const_iterator valueIt = o.find("value");
		if (valueIt == o.end() || !valueIt->second.is<double>())
		{
			return NULL;   // not an entry object
		}
		CvModEntry* pEntry = new CvModEntry();
		pEntry->value100 = jsonX100(valueIt->second.get<double>());
		pEntry->unit = eUnit;
		pEntry->scope = eScope;
		picojson::object::const_iterator it;
		if ((it = o.find("unit")) != o.end())
		{
			pEntry->unitQual = cascadeParseCondition(it->second);
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

	CvModEntry* mod_makeBareEntry(int iValue100, CvCascUnit eUnit, CvCascScope eScope)
	{
		CvModEntry* pEntry = new CvModEntry();
		pEntry->value100 = iValue100;
		pEntry->unit = eUnit;
		pEntry->scope = eScope;
		return pEntry;
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

void CvModifiers::parseLeaf(const std::vector<std::string>& segments, const picojson::value& leaf,
                            CvCascUnit eUnit, const picojson::value* pNodeQual)
{
	ModLeafDecode decode;
	mod_decodeLeaf(segments, decode);

	const size_t iFirst = m_entries.size();
	if (leaf.is<double>())   // a bare, always-on value
	{
		m_entries.push_back(mod_makeBareEntry(jsonX100(leaf.get<double>()), eUnit, decode.scope));
	}
	else if (leaf.is<picojson::object>())   // a single `{value, unit?, per?, enabled?, disabled?}` entry (§3.9)
	{
		CvModEntry* pSingle = mod_parseEntryObject(leaf.get<picojson::object>(), eUnit, decode.scope);
		if (pSingle != NULL)
		{
			m_entries.push_back(pSingle);
		}
	}
	else if (leaf.is<picojson::array>())
	{
		const picojson::array& entryList = leaf.get<picojson::array>();
		for (size_t i = 0; i < entryList.size(); ++i)
		{
			if (entryList[i].is<double>())   // a bare number in a list = an always-on entry
			{
				m_entries.push_back(mod_makeBareEntry(jsonX100(entryList[i].get<double>()), eUnit, decode.scope));
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
		}
	}
	for (size_t i = iFirst; i < m_entries.size(); ++i)
	{
		CvModEntry* pEntry = m_entries[i];
		// the NODE-form `unit:` qualifier (json §3.7 -- a sibling of the magnitude leaves, e.g. cargo.space.
		// {unit: IS_AIR, flat: N}): applies to every entry this leaf produced that carries no entry-form
		// qualifier of its own.
		if (pNodeQual != NULL && pEntry->unitQual == NULL)
		{
			pEntry->unitQual = cascadeParseCondition(*pNodeQual);
		}
		mod_stampDecode(pEntry, decode);
		// fold the point-foldable entries straight into their compiled slot sums
		if (pEntry->isPointFoldable())
		{
			if (pEntry->family == MODFAM_PROPERTY)
			{
				if (pEntry->propertyFk >= 0)
				{
					mod_foldSlot(m_propertySlots, mod_propertySlotKey(pEntry->propertyFk, pEntry->scope, pEntry->unit), pEntry->value100);
				}
			}
			else
			{
				mod_foldSlot(m_slots, mod_slotKey(pEntry->family, pEntry->kind, pEntry->scope, pEntry->unit), pEntry->value100);
			}
		}
	}
}

void CvModifiers::walk(std::vector<std::string>& segments, const picojson::value& node)
{
	if (!node.is<picojson::object>())
	{
		return;   // a family/segment node is always object-valued (json §1/§6)
	}
	const picojson::object& o = node.get<picojson::object>();
	// the NODE-form `unit:` predicate qualifier (json §3.7 -- a sibling of the magnitude leaves: cargo.space.
	// {unit: IS_AIR, ...}); consumed here, applied to this node's leaves, never an address segment.
	picojson::object::const_iterator nodeQualIt = o.find("unit");
	const picojson::value* pNodeQual = (nodeQualIt != o.end() && nodeQualIt->second.is<std::string>()) ? &nodeQualIt->second : NULL;
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		if (pNodeQual != NULL && it == nodeQualIt)
		{
			continue;   // the qualifier key itself
		}
		const CvCascUnit eUnit = cascadeUnitFromString(it->first);
		if (eUnit != CASC_UNIT_UNKNOWN)   // a unit keyword ends the address -- this is a magnitude LEAF
		{
			parseLeaf(segments, it->second, eUnit, pNodeQual);
		}
		else if (it->second.is<double>() || it->second.is<picojson::array>())
		{
			// the modifier.md §6 COUNT-BY-TYPE leaf (freeSpecialists/allowedSpecialists: the key IS the type/`any`
			// and the value the count) -- the one sanctioned non-unit leaf; synthesized unit COUNT, key in the address.
			segments.push_back(it->first);
			parseLeaf(segments, it->second, CASC_UNIT_COUNT, pNodeQual);
			segments.pop_back();
		}
		else
		{
			segments.push_back(it->first);
			walk(segments, it->second);   // a scope/target/member segment -- one level deeper
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
	// A landed entry always carries the source-presence condition, so it never point-folds; the point-foldable
	// branch stays for structural completeness (a fold-eligible entry would belong in the slot sums like any
	// authored one).
	if (pEntry->isPointFoldable())
	{
		if (pEntry->family == MODFAM_PROPERTY)
		{
			if (pEntry->propertyFk >= 0)
			{
				mod_foldSlot(m_propertySlots, mod_propertySlotKey(pEntry->propertyFk, pEntry->scope, pEntry->unit), pEntry->value100);
			}
		}
		else
		{
			mod_foldSlot(m_slots, mod_slotKey(pEntry->family, pEntry->kind, pEntry->scope, pEntry->unit), pEntry->value100);
		}
	}
	finalizeCompiled();
}

void CvModifiers::finalizeCompiled()
{
	std::sort(m_slots.begin(), m_slots.end(), mod_slotKeyBefore);
	std::sort(m_propertySlots.begin(), m_propertySlots.end(), mod_slotKeyBefore);
	m_conditioned.clear();
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		if (m_entries[i]->isConditioned())
		{
			m_conditioned.push_back(m_entries[i]);
		}
	}
	std::stable_sort(m_conditioned.begin(), m_conditioned.end(), mod_conditionedFamilyBefore);
}

void CvModifiers::parseEntity(const picojson::object& entity)
{
	std::vector<std::string> segments;
	for (picojson::object::const_iterator it = entity.begin(); it != entity.end(); ++it)
	{
		if (jsonClassifyKey(it->first, it->second.is<picojson::object>()) == CJK_FAMILY)
		{
			segments.push_back(it->first);
			walk(segments, it->second);
			segments.pop_back();
		}
	}
	finalizeCompiled();
}
