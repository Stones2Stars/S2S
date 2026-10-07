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

///<summary>A group of AMOUNTS as the list Python indexes, each reduced from the engine's x100 form to a
/// whole number. The values stay ints because Python hands them to the EXE's varargs getText, where an
/// 8-byte float takes two argument slots and shifts every later placeholder.</summary>
template <int N>
inline python::list cyToHumanList(const int (&values)[N])
{
	python::list list = python::list();
	for (int i = 0; i < N; ++i)
	{
		list.append(values[i] / 100);
	}
	return list;
}

///<summary>A group of AMOUNTS as the list Python indexes, each reduced from the engine's x100 form and
/// keeping its two decimals. A caller passing one to the EXE's varargs getText must hand it an int, since
/// an 8-byte float takes two argument slots there.</summary>
template <int N>
inline python::list cyToHumanFloatList(const int (&values)[N])
{
	python::list list = python::list();
	for (int i = 0; i < N; ++i)
	{
		list.append(values[i] / 100.0);
	}
	return list;
}

#endif // CyPyList_h__
