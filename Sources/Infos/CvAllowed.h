#pragma once
#ifndef CV_ALLOWED_H
#define CV_ALLOWED_H

//
//	CvAllowed -- the `allowed` caps as ONE composable unit (json.md §4.4): scope self-caps (world/team/empire =
//	"at most N of me at that scope") + the wonder-category per-city caps (worldWonders/teamWonders/nationalWonders,
//	on CultureLevel). Composed BY VALUE on the derived infos that author it (buildings, culturelevels, projects,
//	specialbuildings, techs, units). WRITE-ONCE AT LOAD. The enabler's cap gate compares count(me, scope) < cap.
//

#include <string>
#include <map>

namespace picojson { class value; }

class CvAllowed
{
public:
	CvAllowed() {}

	// The unit's single load-time writer: parse the `allowed` object ({capKind: N}).
	void parse(const picojson::value& v);

	// The cap at `kind` ("world"/"team"/"empire"/"worldWonders"/…), or -1 = uncapped/absent.
	int cap(const std::string& szKind) const
	{
		std::map<std::string, int>::const_iterator it = m_caps.find(szKind);
		return (it != m_caps.end()) ? it->second : -1;
	}
	const std::map<std::string, int>& all() const { return m_caps; }
	bool isEmpty() const { return m_caps.empty(); }
	void clearParsed() { m_caps.clear(); }   // the clear-first half of the full-registry section re-map

private:
	std::map<std::string, int> m_caps;
	CvAllowed(const CvAllowed&);              // noncopyable (held by-value on the noncopyable info)
	CvAllowed& operator=(const CvAllowed&);
};

#endif // CV_ALLOWED_H
