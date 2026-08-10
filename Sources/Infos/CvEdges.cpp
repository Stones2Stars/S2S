//
//	CvEdges -- see the header. The load-time writer interns the spec's fixed edge/bucket vocabulary to the
//	enums; runtime reads are int-keyed. A faithful relocation of the former base walk_edge (json par.4.1/par.4.2).
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvEdges.h"
#include "CvJsonParse.h"     // jsonResolveId

// The intern tables -- ONE place the spec vocabulary is spelled (order matches the enums).
static const char* EDGE_FAMILY_NAMES[NUM_EDGEF] =
{
	"enables", "obsoletes", "replaces", "disables", "obsoletedBy", "related", "requiredBy", "enabledBy"
};
static const char* EDGE_BUCKET_NAMES[NUM_EDGEB] =
{
	"buildings", "units", "builds", "techs", "civics", "religions", "corporations", "projects", "processes",
	"promotions", "promotionLines", "heritages", "specialBuildings", "specialBuildingsWaived", "improvements",
	"bonuses", "routes", "routesAnd", "votes", "hurries", "traits", "traitsAnd", "traitsOr", "specialists",
	"leaders"
};

EnEdgeFamily CvEdges::familyFromString(const std::string& szFamily)
{
	for (int i = 0; i < NUM_EDGEF; ++i)
		if (szFamily == EDGE_FAMILY_NAMES[i]) return (EnEdgeFamily)i;
	return NUM_EDGEF;
}

EnEdgeBucket CvEdges::bucketFromString(const std::string& szBucket)
{
	for (int i = 0; i < NUM_EDGEB; ++i)
		if (szBucket == EDGE_BUCKET_NAMES[i]) return (EnEdgeBucket)i;
	return NO_EDGEB;
}

const char* CvEdges::bucketName(EnEdgeBucket eBucket)
{
	return (eBucket >= 0 && eBucket < NUM_EDGEB) ? EDGE_BUCKET_NAMES[eBucket] : "?";
}

const char* CvEdges::familyName(EnEdgeFamily eFamily)
{
	return (eFamily >= 0 && eFamily < NUM_EDGEF) ? EDGE_FAMILY_NAMES[eFamily] : "?";
}

void CvEdges::sortUnique()
{
	// ONLY the load-DERIVED families (RELATED/REQUIRED_BY) -- an AUTHORED list's order is data (first-element
	// getters like getObsoleteTech() read [0]; sorting it would silently change their answer).
	for (std::map<short, std::vector<int> >::iterator it = m_edges.begin(); it != m_edges.end(); ++it)
	{
		if ((it->first >> 5) < (short)EDGEF_RELATED) continue;
		std::vector<int>& v = it->second;
		std::sort(v.begin(), v.end());
		v.erase(std::unique(v.begin(), v.end()), v.end());
	}
}

void CvEdges::parse(const std::string& szEdge, const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const EnEdgeFamily eFamily = familyFromString(szEdge);
	FAssertMsg(eFamily != NUM_EDGEF, "CvEdges::parse -- unknown edge family (jsonClassifyKey should gate this)");
	if (eFamily == NUM_EDGEF) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		if (!it->second.is<picojson::array>()) continue;
		const EnEdgeBucket eBucket = bucketFromString(it->first);
		// FAIL LOUD on a bucket outside the spec vocabulary -- a silently-dropped edge is an unreachable entity
		// (the fails-closed philosophy); extend the enum + json.md par.4.1 together when the spec grows.
		FAssertMsg(eBucket != NO_EDGEB, "CvEdges::parse -- bucket key outside the spec vocabulary (json.md par.4.1); edge dropped");
		if (eBucket == NO_EDGEB) continue;
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>())
			{
				const int id = jsonResolveId(a[i].get<std::string>());
				if (id >= 0) m_edges[key(eFamily, eBucket)].push_back(id);
			}
	}
}
