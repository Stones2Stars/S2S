#pragma once

#ifndef CyPyList_h__
#define CyPyList_h__

#include <boost/python/list.hpp>

//	The ONE array->python::list conversion for the group reads. Every group read answers a fixed-size int array
//	indexed by an engine enum, so turning it into the list Python indexes is a single shared operation rather
//	than a per-wrapper copy (docs/architecture/patterns/03-dry-one-implementation-per.md).
template <int N>
inline python::list cyToList(const int (&values)[N])
{
	python::list list = python::list();
	for (int i = 0; i < N; ++i)
	{
		list.append(values[i]);
	}
	return list;
}

#endif // CyPyList_h__
