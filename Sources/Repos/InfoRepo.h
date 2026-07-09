#pragma once
#ifndef INFO_REPO_H
#define INFO_REPO_H

#include "CvJsonInfo.h"   // the payload base (the JSON-parsed info data); the Cascade layer is on /I -> bare include
#include "CvJsonSimpleTraitInfo.h"    // the per-type payload subclasses (referenced by JsonPayload below); each pulls its base
#include "CvJsonComplexTraitInfo.h"   // + CvComplexTraitTag (the complex-set repo discriminator)
#include "CvJsonBuildingInfo.h"
#include "CvJsonReligionInfo.h"
#include "CvJsonCorporationInfo.h"
#include "CvJsonUnitInfo.h"
#include "CvJsonTechInfo.h"
#include "CvJsonPromotionInfo.h"
#include "CvJsonUnitCombatInfo.h"
#include "CvJsonCivicInfo.h"
#include "CvJsonTerrainInfo.h"        // plot-substrate + small/mid per-type payload subclasses (JsonPayload below)
#include "CvJsonFeatureInfo.h"
#include "CvJsonRouteInfo.h"
#include "CvJsonBuildInfo.h"
#include "CvJsonImprovementInfo.h"
#include "CvJsonBonusInfo.h"
#include "CvJsonSpecialistInfo.h"
#include "CvJsonProcessInfo.h"
#include "CvJsonCivicOptionInfo.h"
#include "CvJsonCultureLevelInfo.h"
#include "CvJsonProjectInfo.h"
#include "CvJsonHeritageInfo.h"
#include "CvJsonPromotionLineInfo.h"
#include "CvJsonCivilizationInfo.h"   // the uniformity set (owner ruling: every type gets its own subclass)
#include "CvJsonEraInfo.h"
#include "CvJsonHandicapInfo.h"
#include "CvJsonGameSpeedInfo.h"
#include "CvJsonSpecialBuildingInfo.h"
#include "CvJsonPropertyInfo.h"
#include "CvJsonLeaderHeadInfo.h"
#include "CvJsonSpecialUnitInfo.h"
#include "CvJsonVictoryInfo.h"
#include "CvJsonVoteInfo.h"
#include "CvJsonHurryInfo.h"
#include "CvJsonBonusClassInfo.h"
// The FIVE EXE-bound shim leaves (cascade-engine-430.md §3): the payload IS the shim so the engine getters return
// it directly. The shim headers are thin (just their CvJson<X>Info poco, already included above + the art-info fwd decl).
#include "Infos/CvBonusInfo.h"
#include "Infos/CvImprovementInfo.h"
#include "Infos/CvTerrainInfo.h"
#include "Infos/CvFeatureInfo.h"
#include "Infos/CvBuildInfo.h"
#include <vector>

//
//	InfoRepo<TTag> -- the uniform, per-info-type repository for JSON-mapped info data (owner ruling 2026-06-30).
//
//	It establishes the proper repository pattern the codebase has lacked (the existing "repos" were early experiments;
//	the reality everywhere is bare arrays looped over). One template definition, instantiated per info type as a `get()`
//	singleton (the `TTag` is a phantom type that distinguishes the singletons -- use the engine info class, e.g.
//	`InfoRepo<CvJsonBuildingInfo>::get()`). Each holds a `std::vector<CvJsonInfo*>` held PARALLEL to the engine's
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
// The engine info TAGS (phantom per-type discriminators) -- forward-declared for the JsonPayload map below.
class CvJsonTraitInfo; class CvJsonBuildingInfo; class CvJsonReligionInfo; class CvJsonCorporationInfo; class CvJsonUnitInfo;
class CvJsonTechInfo; class CvJsonPromotionInfo; class CvJsonUnitCombatInfo; class CvJsonCivicInfo;
class CvTerrainInfo; class CvFeatureInfo; class CvJsonRouteInfo; class CvBuildInfo; class CvImprovementInfo;
class CvBonusInfo; class CvJsonSpecialistInfo; class CvJsonProcessInfo; class CvJsonCivicOptionInfo; class CvJsonCultureLevelInfo;
class CvJsonProjectInfo; class CvJsonHeritageInfo; class CvJsonPromotionLineInfo;
class CvCivilizationInfo; class CvEraInfo; class CvHandicapInfo; class CvGameSpeedInfo; class CvSpecialBuildingInfo;
class CvJsonPropertyInfo; class CvLeaderHeadInfo; class CvSpecialUnitInfo; class CvVictoryInfo; class CvVoteInfo;
class CvHurryInfo; class CvBonusClassInfo;

// JsonPayload<TTag> -- the type-specific CvJson*Info subclass each repo creates (owner ruling 2026-06-30, mirroring
// StoneBase's per-type Domain/Infos). Default = the generic CvJsonInfo; specialized for the types carrying type-specific
// data. Keeps creation CONSISTENT: InfoRepo<CvJsonTraitInfo> ALWAYS makes a CvJsonSimpleTraitInfo regardless of the caller,
// so readJson + the machines never disagree about the concrete type they delete/read.
template <class TTag> struct JsonPayload { typedef CvJsonInfo type; };
template <> struct JsonPayload<CvJsonTraitInfo>       { typedef CvJsonSimpleTraitInfo  type; };
template <> struct JsonPayload<CvComplexTraitTag> { typedef CvJsonComplexTraitInfo type; };
template <> struct JsonPayload<CvJsonBuildingInfo>    { typedef CvJsonBuildingInfo     type; };
template <> struct JsonPayload<CvJsonReligionInfo>    { typedef CvJsonReligionInfo     type; };
template <> struct JsonPayload<CvJsonCorporationInfo> { typedef CvJsonCorporationInfo  type; };
template <> struct JsonPayload<CvJsonUnitInfo>         { typedef CvJsonUnitInfo         type; };
template <> struct JsonPayload<CvJsonTechInfo>         { typedef CvJsonTechInfo         type; };
template <> struct JsonPayload<CvJsonPromotionInfo>    { typedef CvJsonPromotionInfo    type; };
template <> struct JsonPayload<CvJsonUnitCombatInfo>   { typedef CvJsonUnitCombatInfo   type; };
template <> struct JsonPayload<CvJsonCivicInfo>        { typedef CvJsonCivicInfo        type; };
// The five EXE-bound types: the payload IS the shim leaf (Cv<X>Info : public CvJson<X>Info) so getBonusInfo/… return it.
template <> struct JsonPayload<CvTerrainInfo>      { typedef CvTerrainInfo          type; };
template <> struct JsonPayload<CvFeatureInfo>      { typedef CvFeatureInfo          type; };
template <> struct JsonPayload<CvJsonRouteInfo>        { typedef CvJsonRouteInfo        type; };
template <> struct JsonPayload<CvBuildInfo>        { typedef CvBuildInfo            type; };
template <> struct JsonPayload<CvImprovementInfo>  { typedef CvImprovementInfo      type; };
template <> struct JsonPayload<CvBonusInfo>        { typedef CvBonusInfo            type; };
template <> struct JsonPayload<CvJsonSpecialistInfo>   { typedef CvJsonSpecialistInfo   type; };
template <> struct JsonPayload<CvJsonProcessInfo>      { typedef CvJsonProcessInfo      type; };
template <> struct JsonPayload<CvJsonCivicOptionInfo>  { typedef CvJsonCivicOptionInfo  type; };
template <> struct JsonPayload<CvJsonCultureLevelInfo> { typedef CvJsonCultureLevelInfo type; };
template <> struct JsonPayload<CvJsonProjectInfo>      { typedef CvJsonProjectInfo      type; };
template <> struct JsonPayload<CvJsonHeritageInfo>     { typedef CvJsonHeritageInfo     type; };
template <> struct JsonPayload<CvJsonPromotionLineInfo>{ typedef CvJsonPromotionLineInfo type; };
template <> struct JsonPayload<CvCivilizationInfo> { typedef CvJsonCivilizationInfo type; };
template <> struct JsonPayload<CvEraInfo>          { typedef CvJsonEraInfo          type; };
template <> struct JsonPayload<CvHandicapInfo>     { typedef CvJsonHandicapInfo     type; };
template <> struct JsonPayload<CvGameSpeedInfo>    { typedef CvJsonGameSpeedInfo    type; };
template <> struct JsonPayload<CvSpecialBuildingInfo> { typedef CvJsonSpecialBuildingInfo type; };
template <> struct JsonPayload<CvJsonPropertyInfo>     { typedef CvJsonPropertyInfo     type; };
template <> struct JsonPayload<CvLeaderHeadInfo>   { typedef CvJsonLeaderHeadInfo   type; };
template <> struct JsonPayload<CvSpecialUnitInfo>  { typedef CvJsonSpecialUnitInfo  type; };
template <> struct JsonPayload<CvVictoryInfo>      { typedef CvJsonVictoryInfo      type; };
template <> struct JsonPayload<CvVoteInfo>         { typedef CvJsonVoteInfo         type; };
template <> struct JsonPayload<CvHurryInfo>        { typedef CvJsonHurryInfo        type; };
template <> struct JsonPayload<CvBonusClassInfo>   { typedef CvJsonBonusClassInfo   type; };

template <class TTag>
class InfoRepo
{
public:
	// ⛔ DECLARED here, DEFINED per-tag as explicit specializations in InfoRepo.cpp (fix 2026-07-02). The original
	// header-inline body (`static InfoRepo s_instance;` in an inline template member) hit the classic VC7.1
	// inline-static DUPLICATION defect under unity batching: each unity TU inlined its OWN s_instance, so
	// CvCascadeReadJson populated one instance while the machines (a different batch) read another, EMPTY one --
	// proven live by [MODIFIER/repo] probeFiles=13444 / mapped=0. Out-of-line specializations give exactly ONE
	// instance per tag; a NEW tag use without an InfoRepo.cpp entry fails at LINK (add it there -- one line).
	static InfoRepo& get();

	// #430 collapse (owner ruling 2026-07-08, option B): ALIAS the engine's GC.m_pa<X>Info array. When bound, this repo
	// is a thin VIEW over that array -- the SAME objects SetGlobalClassInfo->read() loads (XML hotkey/base) and read()'s
	// mapFrom populates (JSON data). So getXInfo + every cascade consumer read ONE object, no separate InfoRepo store,
	// no seam. bind() is called once per tag from InfoRepo.cpp's CASCADE_INFOREPO_ALIAS rows. Unbound tags (the JSON-only
	// handful with no XML shell -- Heritage/Build/complex-traits) keep the owned m_data home below.
	void bind(std::vector<CvJsonInfo*>* pEngineArray) { m_pBacking = pEngineArray; }
	bool isAliased() const { return m_pBacking != NULL; }

	// get-or-create the JSON info at id. Grows the (aliased or owned) array to fit; aliased slots are normally already
	// created by SetGlobalClassInfo (read()), so the create is only a fallback.
	CvJsonInfo& edit(int iId)
	{
		std::vector<CvJsonInfo*>& vec = m_pBacking ? *m_pBacking : m_data;
		if (iId >= (int)vec.size())
		{
			vec.resize(iId + 1, (CvJsonInfo*)NULL);
		}
		if (vec[iId] == NULL)
		{
			vec[iId] = new typename JsonPayload<TTag>::type();   // the per-type subclass (StoneBase-mirrored), upcast to base
		}
		return *vec[iId];
	}

	// pointer form of edit() (uniform with get() for prefix dispatch); never NULL.
	CvJsonInfo* editPtr(int iId) { return &edit(iId); }

	// the JSON info at id, or NULL if none mapped (consumers read this).
	const CvJsonInfo* get(int iId) const
	{
		const std::vector<CvJsonInfo*>& vec = m_pBacking ? *m_pBacking : m_data;
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

	std::vector<CvJsonInfo*>* m_pBacking;   // #430: when set, ALIAS the engine array (view, not owner); else use m_data
	std::vector<CvJsonInfo*> m_data;        // [id] -> owned CvJsonInfo* (NULL if none); the JSON-only (unbound) home
};

#endif // INFO_REPO_H
