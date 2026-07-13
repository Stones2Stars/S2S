#pragma once
#ifndef CV_JSON_EDGES_H
#define CV_JSON_EDGES_H

//
//	CvJsonEdges -- the `enables`-family edges as ONE composable unit (json.md §4.1/§4.2): the source-side
//	enables/obsoletes/replaces/disables buckets + the target-side obsoletedBy, each a per-kind FK id list.
//	Composed BY VALUE on the derived infos that author any edge (the data-grounded table). WRITE-ONCE AT LOAD.
//
//	Storage is the consumer-proven dotted key "<edge>.<bucket>" -> [ids] ("enables.units", "obsoletedBy.techs") --
//	the exact query surface the enabler's GENERATE pass reads (EnablerKernel/BuildingEnabler/UnitEnabler). One
//	find(), no per-edge classes: the edge/bucket vocabulary is DATA (json §4.1), not structure.
//

#include <string>
#include <vector>
#include <map>

namespace picojson { class value; }

class CvJsonEdges
{
public:
	CvJsonEdges() {}

	// The unit's single load-time writer: parse ONE edge section ("enables"/"obsoletes"/"replaces"/"disables"/
	// "obsoletedBy") -- a per-kind bucket object {bucket:[INFOTYPE_ids]} -- into the dotted map, FK-resolving each id.
	void parse(const std::string& szEdge, const picojson::value& v);

	// The GENERATE-pass read: the id list at "<edge>.<bucket>", or NULL if the edge/bucket is not authored.
	const std::vector<int>* find(const std::string& szEdgeDotBucket) const
	{
		std::map<std::string, std::vector<int> >::const_iterator it = m_edges.find(szEdgeDotBucket);
		return (it != m_edges.end()) ? &it->second : NULL;
	}
	const std::map<std::string, std::vector<int> >& all() const { return m_edges; }
	bool isEmpty() const { return m_edges.empty(); }

private:
	std::map<std::string, std::vector<int> > m_edges;   // "<edge>.<bucket>" -> FK ids
	CvJsonEdges(const CvJsonEdges&);                    // noncopyable (held by-value on the noncopyable info)
	CvJsonEdges& operator=(const CvJsonEdges&);
};

#endif // CV_JSON_EDGES_H
