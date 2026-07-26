#pragma once
#ifndef INFO_REPO_H
#define INFO_REPO_H

#include "CvInfo.h"   // the payload base (the JSON-parsed info data); the Cascade layer is on /I -> bare include
#include "CvSimpleTraitInfo.h"    // the per-type payload subclasses (referenced by RepoPayload below); each pulls its base
#include "CvComplexTraitInfo.h"   // + CvComplexTraitTag (the complex-set repo discriminator)
#include "CvBuildingInfo.h"
#include "CvReligionInfo.h"
#include "CvCorporationInfo.h"
#include "CvUnitInfo.h"
#include "CvTechInfo.h"
#include "CvPromotionInfo.h"
#include "CvUnitCombatInfo.h"
#include "CvCivicInfo.h"
#include "CvTerrainInfo.h"        // plot-substrate + small/mid per-type payload subclasses (RepoPayload below)
#include "CvFeatureInfo.h"
#include "CvRouteInfo.h"
#include "CvBuildInfo.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvSpecialistInfo.h"
#include "CvProcessInfo.h"
#include "CvCivicOptionInfo.h"
#include "CvCultureLevelInfo.h"
#include "CvProjectInfo.h"
#include "CvHeritageInfo.h"
#include "CvPromotionLineInfo.h"
#include "CvCivilizationInfo.h"   // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvEraInfo.h"          // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvHandicapInfo.h"     // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvGameSpeedInfo.h"   // #430: consolidated onto the engine class (Infos/), JSON-fed via mapFrom; poco retired
#include "CvSpecialBuildingInfo.h" // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvPropertyInfo.h"
#include "CvLeaderHeadInfo.h"   // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvSpecialUnitInfo.h"   // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvVictoryInfo.h"       // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvVoteInfo.h"         // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvHurryInfo.h"        // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvBonusClassInfo.h"   // #430: consolidated onto the engine class (Infos/), JSON-fed; poco retired
#include "CvWorldInfo.h"        // #430 item 15: consolidated onto the engine class (Infos/), JSON-fed; pure config
// The FIVE EXE-bound shim leaves (cascade-engine-430.md §3): the payload IS the shim so the engine getters return
// it directly. The shim headers are thin (just their CvJson<X>Info poco, already included above + the art-info fwd decl).
#include "CvBonusInfo.h"
#include "CvImprovementInfo.h"
#include "CvTerrainInfo.h"
#include "CvFeatureInfo.h"
#include "CvBuildInfo.h"
#include <vector>

//
//	InfoRepo<TTag> -- the uniform, per-info-type repository for JSON-mapped info data (owner ruling 2026-06-30).
//
//	It establishes the proper repository pattern the codebase has lacked (the existing "repos" were early experiments;
//	the reality everywhere is bare arrays looped over). One template definition, instantiated per info type as a `get()`
//	singleton (the `TTag` is a phantom type that distinguishes the singletons -- use the engine info class, e.g.
//	`InfoRepo<CvBuildingInfo>::get()`). Each holds a `std::vector<CvInfo*>` held PARALLEL to the engine's
//	`GC.m_pa<X>Info`, indexed by the SAME id -> O(1) access.
//
//	Why a separate parallel layer (not on the info objects): it keeps the migration boundary clean (the engine's XML
//	info stays pure; the XML-vs-JSON shadow is two structures, swapped cleanly at cutover), the access is standardized,
//	and it never touches `CvInfoBase`. The repo OWNS its `CvInfo` entries (frees on `clear()` / at shutdown).
//
//	Scope (for now): the home for the JSON info data. Retrofitting the existing engine arrays+loops onto this pattern is
//	a separate, later initiative.
//
//	C++03 / VC7.1: a header-only template; the per-`TTag` `static` in `get()` gives one instance per info type.
//
// The engine info TAGS (phantom per-type discriminators) -- forward-declared for the RepoPayload map below.
class CvTraitInfo; class CvBuildingInfo; class CvReligionInfo; class CvCorporationInfo; class CvUnitInfo;
class CvTechInfo; class CvPromotionInfo; class CvUnitCombatInfo; class CvCivicInfo;
class CvTerrainInfo; class CvFeatureInfo; class CvRouteInfo; class CvBuildInfo; class CvImprovementInfo;
class CvBonusInfo; class CvSpecialistInfo; class CvProcessInfo; class CvCivicOptionInfo; class CvCultureLevelInfo;
class CvProjectInfo; class CvHeritageInfo; class CvPromotionLineInfo;
class CvCivilizationInfo; class CvEraInfo; class CvHandicapInfo; class CvGameSpeedInfo; class CvSpecialBuildingInfo;
class CvPropertyInfo; class CvLeaderHeadInfo; class CvSpecialUnitInfo; class CvVictoryInfo; class CvVoteInfo;
class CvHurryInfo; class CvBonusClassInfo; class CvWorldInfo;

// RepoPayload<TTag> -- the type-specific CvInfo subclass each repo creates (owner ruling 2026-06-30, mirroring
// StoneBase's per-type Domain/Infos). Default = the generic CvInfo; specialized for the types carrying type-specific
// data. Keeps creation CONSISTENT: InfoRepo<CvTraitInfo> ALWAYS makes a CvSimpleTraitInfo regardless of the caller,
// so readJson + the machines never disagree about the concrete type they delete/read.
template <class TTag> struct RepoPayload { typedef CvInfo type; };
template <> struct RepoPayload<CvTraitInfo>       { typedef CvSimpleTraitInfo  type; };
template <> struct RepoPayload<CvComplexTraitTag> { typedef CvComplexTraitInfo type; };
template <> struct RepoPayload<CvBuildingInfo>    { typedef CvBuildingInfo     type; };
template <> struct RepoPayload<CvReligionInfo>    { typedef CvReligionInfo     type; };
template <> struct RepoPayload<CvCorporationInfo> { typedef CvCorporationInfo  type; };
template <> struct RepoPayload<CvUnitInfo>         { typedef CvUnitInfo         type; };
template <> struct RepoPayload<CvTechInfo>         { typedef CvTechInfo         type; };
template <> struct RepoPayload<CvPromotionInfo>    { typedef CvPromotionInfo    type; };
template <> struct RepoPayload<CvUnitCombatInfo>   { typedef CvUnitCombatInfo   type; };
template <> struct RepoPayload<CvCivicInfo>        { typedef CvCivicInfo        type; };
// The five EXE-bound types: the payload IS the concrete Cv<X>Info leaf, so getBonusInfo/… return it directly.
template <> struct RepoPayload<CvTerrainInfo>      { typedef CvTerrainInfo          type; };
template <> struct RepoPayload<CvFeatureInfo>      { typedef CvFeatureInfo          type; };
template <> struct RepoPayload<CvRouteInfo>        { typedef CvRouteInfo        type; };
template <> struct RepoPayload<CvBuildInfo>        { typedef CvBuildInfo            type; };
template <> struct RepoPayload<CvImprovementInfo>  { typedef CvImprovementInfo      type; };
template <> struct RepoPayload<CvBonusInfo>        { typedef CvBonusInfo            type; };
template <> struct RepoPayload<CvSpecialistInfo>   { typedef CvSpecialistInfo   type; };
template <> struct RepoPayload<CvProcessInfo>      { typedef CvProcessInfo      type; };
template <> struct RepoPayload<CvCivicOptionInfo>  { typedef CvCivicOptionInfo  type; };
template <> struct RepoPayload<CvCultureLevelInfo> { typedef CvCultureLevelInfo type; };
template <> struct RepoPayload<CvProjectInfo>      { typedef CvProjectInfo      type; };
template <> struct RepoPayload<CvHeritageInfo>     { typedef CvHeritageInfo     type; };
template <> struct RepoPayload<CvPromotionLineInfo>{ typedef CvPromotionLineInfo type; };
template <> struct RepoPayload<CvCivilizationInfo> { typedef CvCivilizationInfo     type; };
template <> struct RepoPayload<CvEraInfo>          { typedef CvEraInfo              type; };
template <> struct RepoPayload<CvHandicapInfo>     { typedef CvHandicapInfo         type; };
template <> struct RepoPayload<CvGameSpeedInfo>    { typedef CvGameSpeedInfo        type; };
template <> struct RepoPayload<CvSpecialBuildingInfo> { typedef CvSpecialBuildingInfo type; };
template <> struct RepoPayload<CvPropertyInfo>     { typedef CvPropertyInfo     type; };
template <> struct RepoPayload<CvLeaderHeadInfo>   { typedef CvLeaderHeadInfo       type; };
template <> struct RepoPayload<CvSpecialUnitInfo>  { typedef CvSpecialUnitInfo      type; };
template <> struct RepoPayload<CvVictoryInfo>      { typedef CvVictoryInfo          type; };
template <> struct RepoPayload<CvVoteInfo>         { typedef CvVoteInfo             type; };
template <> struct RepoPayload<CvHurryInfo>        { typedef CvHurryInfo            type; };
template <> struct RepoPayload<CvBonusClassInfo>   { typedef CvBonusClassInfo       type; };
template <> struct RepoPayload<CvWorldInfo>        { typedef CvWorldInfo            type; };

template <class TTag>
class InfoRepo
{
public:
	// ⛔ DECLARED here, DEFINED per-tag as explicit specializations in InfoRepo.cpp (fix 2026-07-02). The original
	// header-inline body (`static InfoRepo s_instance;` in an inline template member) hit the classic VC7.1
	// inline-static DUPLICATION defect under unity batching: each unity TU inlined its OWN s_instance, so
	// readJson populated one instance while the machines (a different batch) read another, EMPTY one --
	// proven live by [MODIFIER/repo] probeFiles=13444 / mapped=0. Out-of-line specializations give exactly ONE
	// instance per tag; a NEW tag use without an InfoRepo.cpp entry fails at LINK (add it there -- one line).
	static InfoRepo& get();

	// #430 collapse (owner ruling 2026-07-08, option B): ALIAS the engine's GC.m_pa<X>Info array. When bound, this repo
	// is a thin VIEW over that array -- the SAME objects SetGlobalClassInfo->read() loads (XML hotkey/base) and read()'s
	// mapFrom populates (JSON data). So getXInfo + every cascade consumer read ONE object, no separate InfoRepo store,
	// no seam. bind() is called once per tag from InfoRepo.cpp's CASCADE_INFOREPO_ALIAS rows. Unbound tags (the JSON-only
	// handful with no XML shell -- Heritage/Build/complex-traits) keep the owned m_data home below.
	void bind(std::vector<CvInfo*>* pEngineArray) { m_pBacking = pEngineArray; }
	bool isAliased() const { return m_pBacking != NULL; }

	// get-or-create the JSON info at id. Grows the (aliased or owned) array to fit; aliased slots are normally already
	// created by SetGlobalClassInfo (read()), so the create is only a fallback.
	CvInfo& edit(int iId)
	{
		std::vector<CvInfo*>& vec = m_pBacking ? *m_pBacking : m_data;
		if (iId >= (int)vec.size())
		{
			vec.resize(iId + 1, (CvInfo*)NULL);
		}
		if (vec[iId] == NULL)
		{
			vec[iId] = new typename RepoPayload<TTag>::type();   // the per-type subclass (StoneBase-mirrored), upcast to base
		}
		return *vec[iId];
	}

	// pointer form of edit() (uniform with get() for prefix dispatch); never NULL.
	CvInfo* editPtr(int iId) { return &edit(iId); }

	// the JSON info at id, or NULL if none mapped (consumers read this).
	const CvInfo* get(int iId) const
	{
		const std::vector<CvInfo*>& vec = m_pBacking ? *m_pBacking : m_data;
		return (iId >= 0 && iId < (int)vec.size()) ? vec[iId] : NULL;
	}

	// free every OWNED entry (before a re-map / at shutdown). When aliased, the engine (GC.m_pa<X>Info) owns the
	// objects -- do NOT free them here (double-free); only the owned m_data is freed.
	void clear()
	{
		if (m_pBacking) return;
		for (size_t i = 0; i < m_data.size(); ++i)
		{
			delete m_data[i];
		}
		m_data.clear();
	}

private:
	InfoRepo() : m_pBacking(NULL) {}
	~InfoRepo() { clear(); }
	InfoRepo(const InfoRepo&);
	InfoRepo& operator=(const InfoRepo&);

	std::vector<CvInfo*>* m_pBacking;   // #430: when set, ALIAS the engine array (view, not owner); else use m_data
	std::vector<CvInfo*> m_data;        // [id] -> owned CvInfo* (NULL if none); the JSON-only (unbound) home
};

#endif // INFO_REPO_H
