//
//	CvJsonInfo::mapFrom -- the CORE JSON reading (shared type/identity/button onto CvInfoBase) + the ONE section
//	DISPATCH: each json.md section routes to the composing subclass's unit via the mut* write targets (write-once at
//	load). A section the type composes NO unit for is recorded via jsonNoteUnconsumed -- never silently dropped.
//	⛔ No section DATA lives here (owner 2026-07-08) and no cascade RUNTIME (no DepositIndex, no evaluator) --
//	[DEC-json-not-cascade]; the per-type data is the composed units + typed members on the subclasses.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson, CvString/CvWString
#include "CvJsonInfo.h"
#include "CvJsonParse.h"            // jsonClassifyKey / jsonNoteUnconsumed + the shared walkers (jsonChildObj/jsonIdStr)

CvJsonInfo::CvJsonInfo() {}
CvJsonInfo::~CvJsonInfo() {}

const std::vector<int>& CvJsonInfo::dormantTriggers() const
{
	static const std::vector<int> s_empty;
	const CvJsonRequires* r = getRequires();
	return r ? r->dormantTriggers : s_empty;
}

void CvJsonInfo::mapFrom(const picojson::value& entity)
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
		if (k == "skills")            { if (CvJsonBoolBlock* u = mutSkills())       u->parse(v); continue; }
		if (k == "tags")              { if (CvJsonBoolBlock* u = mutTags())         u->parse(v); continue; }
		if (k == "attributes")        { if (CvJsonBoolBlock* u = mutAttributes())   u->parse(v); continue; }
		if (k == "capabilities")      { if (CvJsonBoolBlock* u = mutCapabilities()) u->parse(v); continue; }
		if (k == "policies")          { if (CvJsonBoolBlock* u = mutPolicies())     u->parse(v); continue; }

		switch (jsonClassifyKey(k, v.is<picojson::object>()))
		{
		case CJK_EDGE:
			if (CvJsonEdges* u = mutEdges()) u->parse(k, v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_PROVIDES:
			if (CvJsonProvides* u = mutProvides()) u->parse(v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_ALLOWED:
			if (CvJsonAllowed* u = mutAllowed()) u->parse(v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_GRANTS:
			if (CvJsonGrants* u = mutGrants()) u->parse(v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_REQUIRES:
			if (CvJsonRequires* u = mutRequires()) u->parse(v); else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_WHEN_OBSOLETE:
			if (CvJsonModifiers* u = mutWhenObsolete())
			{
				if (v.is<picojson::object>()) u->parseEntity(v.get<picojson::object>());
			}
			else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_GATE:
			if (CvJsonGate* u = mutGate()) { if (k == "enabled") u->parseEnabled(v); else u->parseDisabled(v); }
			else jsonNoteUnconsumed(m_szType.GetCString(), k);
			break;
		case CJK_FAMILY:
			bHasFamilyKeys = true;   // parsed in ONE pass below (CvJsonModifiers::parseEntity self-skips reserved keys)
			break;
		case CJK_RETIRED:
			jsonNoteUnconsumed(m_szType.GetCString(), k);   // purged vocabulary (loadPrune) -- a straggler must SURFACE
			break;
		case CJK_INTRINSIC:
		case CJK_FLAG:
		default:
			break;   // subclass / other-system concern (typed members; the reader census counts them)
		}
	}

	// --- the §6 modifier families: one walk over the whole entity (parseEntity self-skips the reserved keys) ---
	if (bHasFamilyKeys)
	{
		if (CvJsonModifiers* u = mutModifiers()) u->parseEntity(o);
		else jsonNoteUnconsumed(m_szType.GetCString(), "(modifier families)");
	}
}
