#pragma once
#ifndef CV_CONTEXT_DICT_H
#define CV_CONTEXT_DICT_H

//
//	ContextDict -- the ONE uniform keyed dictionary the per-scope live-state contexts share (CityContext /
//	EmpireContext / ... ): an id -> count/value map. `has` is the plain gate; `count` is the scale (a keyed or
//	plots-target deposit's output = flat x count). A dictionary, not a fixed struct, so each family's key set is
//	OPEN -- a new key, never a reshape. The info reads it directly: cx.<dict>.has(id) / .count(id).
//

#include <map>

struct ContextDict
{
	std::map<int, int> m;
	int  count(int id) const { std::map<int, int>::const_iterator it = m.find(id); return it != m.end() ? it->second : 0; }
	bool has(int id) const   { return count(id) > 0; }
	void add(int id, int d)  { m[id] += d; }
	void set(int id, int n)  { m[id] = n; }
	void clear()             { m.clear(); }
	bool empty() const       { return m.empty(); }
};

#endif // CV_CONTEXT_DICT_H
