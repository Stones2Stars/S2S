#pragma once
#ifndef CV_JSON_BOOL_BLOCK_H
#define CV_JSON_BOOL_BLOCK_H

//
//	CvJsonBoolBlock -- the shared `{name:true}` boolean-flag block as ONE composable unit: the §8/§9 classification
//	sections (unit `skills`/`tags`, building `attributes`, tech `capabilities`, civic/trait `policies`) all share
//	this exact shape, so ONE class serves all five -- instantiated per section on the derived infos that author it.
//	KEYED skill extras (terrainDoubleMove:{TERRAIN_X:true}, targets:[…]) are NOT this block -- they stay typed
//	members on the owning subclass. WRITE-ONCE AT LOAD.
//

#include <string>
#include <set>

namespace picojson { class value; }

class CvJsonBoolBlock
{
public:
	CvJsonBoolBlock() {}

	// The unit's single load-time writer: parse the section's {name:true} object (false/non-bool ignored).
	void parse(const picojson::value& v);

	bool has(const char* szName) const { return m_names.count(szName) != 0; }
	bool has(const std::string& szName) const { return m_names.count(szName) != 0; }
	const std::set<std::string>& all() const { return m_names; }
	bool isEmpty() const { return m_names.empty(); }

private:
	std::set<std::string> m_names;
	CvJsonBoolBlock(const CvJsonBoolBlock&);              // noncopyable (held by-value on the noncopyable info)
	CvJsonBoolBlock& operator=(const CvJsonBoolBlock&);
};

#endif // CV_JSON_BOOL_BLOCK_H
