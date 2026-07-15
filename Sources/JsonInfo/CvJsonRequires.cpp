//
//	CvJsonRequires -- see the header. The parse is a faithful relocation of the former base walk_requires (json §4.3):
//	build/operate -> the info-owned typed CvJsonCondition (via the ONE human->data boundary, cascadeParseCondition);
//	the structural `dormant` key -- which the condition parser drops -- extracted separately into trigger ids.
//	Building operate.dormant = [BUILDING_…]; unit build.dormant = {all:[UNIT_…]}.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonRequires.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition -- curated JSON -> the typed condition tree
#include "CvJsonParse.h"            // jsonResolveId + jsonNoteUnconsumed

CvJsonRequires::~CvJsonRequires()
{
	clearParsed();
}

void CvJsonRequires::clearParsed()
{
	delete build;   build = NULL;
	delete operate; operate = NULL;
	dormantTriggers.clear();
}

void CvJsonRequires::parse(const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& ro = v.get<picojson::object>();
	for (picojson::object::const_iterator sub = ro.begin(); sub != ro.end(); ++sub)
	{
		CvJsonCondition* c = cascadeParseCondition(sub->second);
		if (sub->first == "build")        { delete build;   build = c; }
		else if (sub->first == "operate") { delete operate; operate = c; }
		else { jsonNoteUnconsumed("requires", sub->first); delete c; }   // unknown sub-section -> the census, never silent
		                                                                 // (the owning entity's type id is not reachable here)
		// dormant triggers: building operate.dormant = [BUILDING_…]; unit build.dormant = {all:[UNIT_…]}.
		if (!sub->second.is<picojson::object>()) continue;
		const picojson::object& clause = sub->second.get<picojson::object>();
		picojson::object::const_iterator dm = clause.find("dormant");
		if (dm == clause.end()) continue;
		const picojson::value* arr = NULL;
		if (dm->second.is<picojson::array>()) arr = &dm->second;
		else if (dm->second.is<picojson::object>())
		{
			picojson::object::const_iterator al = dm->second.get<picojson::object>().find("all");
			if (al != dm->second.get<picojson::object>().end() && al->second.is<picojson::array>()) arr = &al->second;
		}
		if (!arr) continue;
		const picojson::array& a = arr->get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) dormantTriggers.push_back(id); }
	}
}
