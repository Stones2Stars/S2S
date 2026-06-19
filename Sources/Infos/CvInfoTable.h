#pragma once

#ifndef CV_INFO_TABLE_H
#define CV_INFO_TABLE_H

// Per-type info catalog ("table") — the static read-side STORAGE for one info class, and the home
// of the parse-then-link phase. This is the catalog layer; it sits with the info-data layer
// (CvInfoUtil's declarative fields), NOT with the derived-index repos in Sources/Repos/
// (BuildsRepo/BuildingsRepo), which are a separate, provisional layer that computes reverse-indices
// *from* a linked table.
//
// EXE identity is preserved: the row index IS the enum id the EXE passes to GC.getXInfo(), and
// rows() exposes the underlying vector<T*> so the existing generic loader (LoadGlobalClassInfo) and
// the DllExport accessors keep working unchanged — they just delegate here.
//
// The reason the abstraction exists is link(): after all rows are parsed, link() resolves every
// row's deferred foreign-key columns (the <Type>-string refs captured during read) in one pass.
// Because it runs after the whole catalog is loaded, FK targets are present regardless of XML load
// order — which is what lets us stop depending on the hand-maintained category order.
//
// Pure storage: the linking is done generically by cvInternalGlobals::linkAllInfos() over every
// registered info vector, so the table itself stays a thin, header-only owner with no dependency on
// CvInfoUtil (which would otherwise be a circular include — CvGlobals.h holds an InfoTable member).

template <typename T>
class InfoTable
	: private bst::noncopyable
{
public:
	InfoTable() {}

	// Underlying storage, handed to the generic loader and the getXInfos() accessor.
	std::vector<T*>&       rows()       { return m_rows; }
	const std::vector<T*>& rows() const { return m_rows; }

	int getNum() const     { return (int)m_rows.size(); }
	T&  get(int i) const   { return *m_rows[i]; }

private:
	std::vector<T*> m_rows;
};

#endif // CV_INFO_TABLE_H
