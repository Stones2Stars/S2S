#pragma once

#ifndef CyPyList_h__
#define CyPyList_h__

#include <boost/python/list.hpp>
#include "UI/CvEntryText.h"   // amountText -- the one amount format

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

///<summary>A container of ids (a vector or set of an id type) as the list Python iterates, in the container's
/// order.</summary>
template <typename IdContainer>
inline python::list cyToIdList(const IdContainer& ids)
{
	python::list list = python::list();
	for (typename IdContainer::const_iterator entry = ids.begin(); entry != ids.end(); ++entry)
	{
		list.append((int)*entry);
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

///<summary>A group of amounts as display text, in the one amount format (amountText): "12", "12.5", "-0.75".
/// The text twin of cyToHumanList, for a value a screen prints.</summary>
template <int N>
inline python::list cyToHumanTextList(const int (&values)[N])
{
	python::list list = python::list();
	for (int i = 0; i < N; ++i)
	{
		list.append(std::wstring(amountText(values[i]).GetCString()));
	}
	return list;
}

#endif // CyPyList_h__
