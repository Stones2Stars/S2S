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

///<summary>A group of AMOUNTS as display text, each reduced from the engine's x100 form with at most two
/// decimals: "12", "12.5", "-0.75". The text twin of cyToHumanList, for a value a screen prints.</summary>
template <int N>
inline python::list cyToHumanTextList(const int (&values)[N])
{
	python::list list = python::list();
	for (int i = 0; i < N; ++i)
	{
		const int iAbsValue = values[i] < 0 ? -values[i] : values[i];
		const wchar_t* szSign = values[i] < 0 ? L"-" : L"";
		const int iFraction = iAbsValue % 100;
		wchar_t szText[32];
		if (iFraction == 0)
		{
			swprintf(szText, L"%s%d", szSign, iAbsValue / 100);
		}
		else if (iFraction % 10 == 0)
		{
			swprintf(szText, L"%s%d.%d", szSign, iAbsValue / 100, iFraction / 10);
		}
		else
		{
			swprintf(szText, L"%s%d.%02d", szSign, iAbsValue / 100, iFraction);
		}
		list.append(std::wstring(szText));
	}
	return list;
}

#endif // CyPyList_h__
