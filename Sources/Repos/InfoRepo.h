#pragma once
#ifndef INFO_REPO_H
#define INFO_REPO_H

#include "CvJsonInfo.h"   // the payload (the JSON-parsed info data); the Cascade layer is on /I -> bare include
#include <vector>

//
//	InfoRepo<TTag> -- the uniform, per-info-type repository for JSON-mapped info data (owner ruling 2026-06-30).
//
//	It establishes the proper repository pattern the codebase has lacked (the existing "repos" were early experiments;
//	the reality everywhere is bare arrays looped over). One template definition, instantiated per info type as a `get()`
//	singleton (the `TTag` is a phantom type that distinguishes the singletons -- use the engine info class, e.g.
//	`InfoRepo<CvBuildingInfo>::get()`). Each holds a `std::vector<CvJsonInfo*>` held PARALLEL to the engine's
//	`GC.m_pa<X>Info`, indexed by the SAME id -> O(1) access.
//
//	Why a separate parallel layer (not on the info objects): it keeps the migration boundary clean (the engine's XML
//	info stays pure; the XML-vs-JSON shadow is two structures, swapped cleanly at cutover), it is immune to the
//	`CvInfoReplacements` info-pointer swap (an array indexed by id stays put), the access is standardized, and it never
//	touches `CvInfoBase`. The repo OWNS its `CvJsonInfo` entries (frees on `clear()` / at shutdown).
//
//	Scope (for now): the home for the JSON info data. Retrofitting the existing engine arrays+loops onto this pattern is
//	a separate, later initiative.
//
//	C++03 / VC7.1: a header-only template; the per-`TTag` `static` in `get()` gives one instance per info type.
//
template <class TTag>
class InfoRepo
{
public:
	static InfoRepo& get()
	{
		static InfoRepo s_instance;
		return s_instance;
	}

	// get-or-create the JSON info at id (readJson populates it). Grows the mirrored array to fit the id.
	CvJsonInfo& edit(int iId)
	{
		if (iId >= (int)m_data.size())
		{
			m_data.resize(iId + 1, (CvJsonInfo*)NULL);
		}
		if (m_data[iId] == NULL)
		{
			m_data[iId] = new CvJsonInfo();
		}
		return *m_data[iId];
	}

	// pointer form of edit() (uniform with get() for prefix dispatch); never NULL.
	CvJsonInfo* editPtr(int iId) { return &edit(iId); }

	// the JSON info at id, or NULL if none mapped (consumers read this).
	const CvJsonInfo* get(int iId) const
	{
		return (iId >= 0 && iId < (int)m_data.size()) ? m_data[iId] : NULL;
	}

	// free every entry (before a re-map / at shutdown).
	void clear()
	{
		for (size_t i = 0; i < m_data.size(); ++i)
		{
			delete m_data[i];
		}
		m_data.clear();
	}

private:
	InfoRepo() {}
	~InfoRepo() { clear(); }
	InfoRepo(const InfoRepo&);
	InfoRepo& operator=(const InfoRepo&);

	std::vector<CvJsonInfo*> m_data;   // [id] -> owned CvJsonInfo* (NULL if none); mirrors the engine info array
};

#endif // INFO_REPO_H
