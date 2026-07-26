//
//	CvInfo::mapFrom -- the CORE JSON reading (shared type/identity/button onto CvInfoBase) + the ONE section
//	DISPATCH: each json.md section routes to the composing subclass's unit via the mut* write targets (write-once at
//	load). A section the type composes NO unit for is recorded via jsonNoteUnconsumed -- never silently dropped.
//	⛔ No section DATA lives here (owner 2026-07-08) and no cascade RUNTIME (no DepositIndex, no evaluator) --
//	[DEC-json-not-cascade]; the per-type data is the composed units + typed members on the subclasses.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson, CvString/CvWString
#include "CvInfo.h"
#include "CvJsonParse.h"            // jsonClassifyKey / jsonNoteUnconsumed + the shared walkers (jsonChildObj/jsonIdStr)
#include "Data/CvInfoValuation.h"   // InfoValuation -- the ONE per-group what-if calc unit the expected* delegate to

CvInfo::CvInfo() {}
CvInfo::~CvInfo() {}

// --- the compiled conditioned list + range (empty when the type composes no modifiers) ---
const std::vector<const CvModEntry*>& CvInfo::modifierConditioned() const
{
	static const std::vector<const CvModEntry*> s_empty;
	const CvModifiers* pModifiers = getModifiers();
	return pModifiers != NULL ? pModifiers->conditioned() : s_empty;
}

void CvInfo::modifierConditionedRange(ModifierFamily eFamily, size_t& iBeginOut, size_t& iEndOut) const
{
	const CvModifiers* pModifiers = getModifiers();
	if (pModifiers != NULL)
	{
		pModifiers->conditionedRange(eFamily, iBeginOut, iEndOut);
	}
	else
	{
		iBeginOut = 0;
		iEndOut = 0;
	}
}

// --- the per-group what-if endpoints: one-line delegations onto the ONE calc unit (InfoValuation) ---
void CvInfo::expectedFlatYields(const CityContext& cityContext, const EmpireContext& empireContext,
	const CvPlotGroup* plotGroup, int (&flatYields)[NUM_YIELD_TYPES]) const
{
	InfoValuation::expectedFlatYields(getModifiers(), cityContext, empireContext, plotGroup, flatYields);
}

void CvInfo::expectedYieldModifiers(const CityContext& cityContext, const EmpireContext& empireContext,
	const CvPlotGroup* plotGroup, int (&yieldModifiers)[NUM_YIELD_TYPES]) const
{
	InfoValuation::expectedYieldModifiers(getModifiers(), cityContext, empireContext, plotGroup, yieldModifiers);
}

void CvInfo::expectedPlotYields(const CityContext& cityContext, const EmpireContext& empireContext,
	const CvPlotGroup* plotGroup, int (&plotYields)[NUM_YIELD_TYPES]) const
{
	InfoValuation::expectedPlotYields(getModifiers(), cityContext, empireContext, plotGroup, plotYields);
}

void CvInfo::expectedFlatCommerce(const CityContext& cityContext, const EmpireContext& empireContext,
	const CvPlotGroup* plotGroup, int (&flatCommerce)[NUM_COMMERCE_TYPES]) const
{
	InfoValuation::expectedFlatCommerce(getModifiers(), cityContext, empireContext, plotGroup, flatCommerce);
}

void CvInfo::expectedWellbeing(const CityContext& cityContext, const EmpireContext& empireContext,
	const CvPlotGroup* plotGroup, int (&wellbeing)[NUM_WELLBEING_CHANNELS]) const
{
	InfoValuation::expectedWellbeing(getModifiers(), cityContext, empireContext, plotGroup, wellbeing);
}

int CvInfo::expectedModifier(ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup) const
{
	return InfoValuation::expectedSum(getModifiers(), eFamily, iKind, eUnit, cityContext, empireContext, plotGroup);
}

int CvInfo::expectedScalar(InfoScalar eScalar, CvCascUnit eUnit,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup) const
{
	ModifierFamily eFamily = MODFAM_NONE;
	int iKind = -1;
	infoScalarSlot(eScalar, eFamily, iKind);
	return InfoValuation::expectedSum(getModifiers(), eFamily, iKind, eUnit, cityContext, empireContext, plotGroup);
}

// #430: the 23 replaced infos load via LoadGlobalClassInfoJson (CvXMLLoadUtilitySet) -> mapFrom(json) directly; there
// is no XML read() on this path (reading a replaced info's XML into the game is HARD BANNED -- DEC-no-xml-into-game).
// mapFrom below is the sole load hook.

const std::vector<int>& CvInfo::dormantTriggers() const
{
	static const std::vector<int> s_empty;
	const CvRequires* r = getRequires();
	return r ? r->dormantTriggers : s_empty;
}

void CvInfo::mapFrom(const picojson::value& entity)
{
	clearSections();          // idempotency contract: a re-run (the full-registry pass) fully redefines the sections
	mapSections(entity);
}

// The clear-first half of the idempotent mapFrom: reset every composed unit this type carries (each unit's
// clearParsed is its dtor body -- single-sourced), so the full-registry re-run cannot double-accumulate.
void CvInfo::clearSections()
{
	if (CvEdges* u = mutEdges())            u->clearParsed();
	if (CvProvides* u = mutProvides())      u->clearParsed();
	if (CvAllowed* u = mutAllowed())        u->clearParsed();
	if (CvGrants* u = mutGrants())          u->clearParsed();
	if (CvTriggers* u = mutTriggers())          u->clearParsed();
	if (CvRequires* u = mutRequires())      u->clearParsed();
	if (CvGate* u = mutGate())              u->clearParsed();
	if (CvModifiers* u = mutModifiers())    u->clearParsed();
	if (CvModifiers* u = mutWhenObsolete()) u->clearParsed();
	if (CvClassificationBlock* u = mutSkills())       u->clearParsed();
	if (CvClassificationBlock* u = mutTags())         u->clearParsed();
	if (CvClassificationBlock* u = mutAttributes())   u->clearParsed();
	if (CvClassificationBlock* u = mutCapabilities()) u->clearParsed();
	if (CvClassificationBlock* u = mutPolicies())     u->clearParsed();
}

// §8/§9 classification id-plane resolve -- each carried block fills its by-id bitsets from the generated
// ClassificationRegistry. LOAD-ONLY (called by ClassificationRegistry::buildAndResolve after minting).
void CvInfo::resolveClassificationIds()
{
	if (CvClassificationBlock* u = mutSkills())       u->resolveIds(CLSD_SKILL);
	if (CvClassificationBlock* u = mutTags())         u->resolveIds(CLSD_TAG);
	if (CvClassificationBlock* u = mutAttributes())   u->resolveIds(CLSD_ATTRIBUTE);
	if (CvClassificationBlock* u = mutCapabilities()) u->resolveIds(CLSD_CAPABILITY);
	if (CvClassificationBlock* u = mutPolicies())     u->resolveIds(CLSD_POLICY);
}

void CvInfo::mapSections(const picojson::value& entity)
{
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	std::string s;

	// --- core reading: the shared CvInfoBase fields (type FIRST -- the diagnostics key on it) ---
	if (jsonIdStr(o, "type", s)) m_szType = s.c_str();
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		if (jsonIdStr(*io, "description", s)) m_szTextKey        = CvWString(s.c_str());
		if (jsonIdStr(*io, "civilopedia", s)) m_szCivilopediaKey = CvWString(s.c_str());
		if (jsonIdStr(*io, "help", s))        m_szHelpKey        = CvWString(s.c_str());
		if (jsonIdStr(*io, "strategy", s))    m_szStrategyKey    = CvWString(s.c_str());
	}
	if (const picojson::object* ui = jsonChildObj(o, "ui"))
		if (const picojson::object* art = jsonChildObj(*ui, "art"))
			if (jsonIdStr(*art, "icon", s)) m_szButton = s.c_str();

	// --- the ONE section dispatch: route each authored section to the type's composed unit (or record the gap) ---
	bool bHasFamilyKeys = false;
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		const std::string& k = it->first;
		const picojson::value& v = it->second;

		// the §8/§9 flat-bool classification blocks (keyed extras stay subclass-parsed from the same section)
		if (k == "skills")            { if (CvClassificationBlock* u = mutSkills())       u->parse(v); continue; }
		if (k == "tags")              { if (CvClassificationBlock* u = mutTags())         u->parse(v); continue; }
		if (k == "attributes")        { if (CvClassificationBlock* u = mutAttributes())   u->parse(v); continue; }
		if (k == "capabilities")      { if (CvClassificationBlock* u = mutCapabilities()) u->parse(v); continue; }
		if (k == "policies")          { if (CvClassificationBlock* u = mutPolicies())     u->parse(v); continue; }

		switch (jsonClassifyKey(k, v.is<picojson::object>()))
		{
		case CJK_EDGE:
			if (CvEdges* u = mutEdges()) u->parse(k, v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_PROVIDES:
			if (CvProvides* u = mutProvides()) u->parse(v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_ALLOWED:
			if (CvAllowed* u = mutAllowed()) u->parse(v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_GRANTS:
			if (CvGrants* u = mutGrants()) u->parse(v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_TRIGGERS:
			if (CvTriggers* u = mutTriggers()) u->parse(v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_REQUIRES:
			if (CvRequires* u = mutRequires()) u->parse(v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_WHEN_OBSOLETE:
			if (CvModifiers* u = mutWhenObsolete())
			{
				if (v.is<picojson::object>()) u->parseEntity(v.get<picojson::object>());
			}
			else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_GATE:
			if (CvGate* u = mutGate()) { if (k == "enabled") u->parseEnabled(v); else u->parseDisabled(v); }
			else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_FAMILY:
			bHasFamilyKeys = true;   // parsed in ONE pass below (CvModifiers::parseEntity self-skips reserved keys)
			break;
		case CJK_RETIRED:
			jsonNoteUnconsumed(m_szType.GetCString(), k);   // purged vocabulary (loadPrune) -- a straggler must SURFACE
			break;
		case CJK_UNKNOWN:
			jsonNoteUnknownKey(m_szType.GetCString(), k);   // outside the closed family vocabulary -- surfaces as a
			break;                                          // LOUD [READJSON] ERROR unknown-key line, never family-walked
		case CJK_INTRINSIC:
		case CJK_FLAG:
		default:
			break;   // subclass / other-system concern (typed members; the reader census counts them)
		}
	}

	// --- the §6 modifier families: one walk over the whole entity (parseEntity self-skips the reserved keys) ---
	if (bHasFamilyKeys)
	{
		if (CvModifiers* u = mutModifiers()) u->parseEntity(o);
		else jsonNoteUnconsumed(m_szType.GetCString(), "(modifier families)");
	}
}
