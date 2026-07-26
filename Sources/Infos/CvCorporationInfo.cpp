//
//	CvCorporationInfo -- the corporation poco's own typed reading on top of the base section dispatch (see the
//	header). mapFrom materializes the census identity set + the two value planes ONCE
//	([DEC-materialize-at-mapfrom]); the value planes scan the COMPILED entry list (the one sanctioned load-time
//	scan source, patterns.md § Materialize at mapFrom), never the raw parse. Idempotent by contract
//	(unconditional scalar assigns, clear-first containers).
//

#include "CvGameCoreDLL.h"
#include "CvCorporationInfo.h"
#include "CvJsonParse.h"   // jsonChildObj / jsonIdInt / jsonIdStr
#include "CvModEntry.h"    // the compiled entries -- the HQ-revenue + consumed-bonus planes
#include <set>

CvCorporationInfo::CvCorporationInfo()
	: m_iSpreadFactor(0)
	, m_iCompetingSpreadCostPercent(0)
	, m_iSpreadCost(0)
	, m_iTGAIndex(-1)   // -1 = the TGA-filler sentinel (RemoveTGAFiller)
	, m_eTechPrereq(NO_TECH)
	, m_eObsoleteTech(NO_TECH)
	, m_iChar(0)
	, m_iHeadquarterChar(-1)
	, m_iMissionType(-1)
{
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		m_aiHeadquartersCommerce[iCommerce] = 0;
	}
}

// GameFont glyph: DERIVED from the TGA index, offset PAST the religion block (corps follow religions in
// GameFont) -- reproduces the archived derivation exactly, else the corp icon lands on the wrong/empty slot.
// The symbol pass's sequential id argument is ignored (as legacy did); the HQ glyph is the +1 sibling.
void CvCorporationInfo::setChar(int /*iSymbol*/)
{
	m_iChar = 8550 + (GC.getGAMEFONT_TGA_RELIGIONS() + m_iTGAIndex) * 2;
}

void CvCorporationInfo::setHeadquarterChar(int /*iSymbol*/)
{
	m_iHeadquarterChar = 8551 + (GC.getGAMEFONT_TGA_RELIGIONS() + m_iTGAIndex) * 2;
}

namespace
{
	// Collect requires.spread BUILDING count atoms (json §4.3) into the per-building-id count map -- the
	// per-building COUNT need the executive-spread gate reads (min absent = presence = 1).
	void corpCollectSpreadBuildings(const CvCondition* pCondition, std::map<int, int>& countsOut)
	{
		if (pCondition == NULL)
		{
			return;
		}
		if (pCondition->kind == CASC_COND_PRESENCE)
		{
			if (pCondition->id >= 0 && pCondition->type.compare(0, 9, "BUILDING_") == 0)
			{
				countsOut[pCondition->id] = pCondition->min > 0 ? pCondition->min : 1;
			}
			return;
		}
		if (pCondition->kind == CASC_COND_GROUP)
		{
			for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
			{
				corpCollectSpreadBuildings(pCondition->all[iChild], countsOut);
			}
			for (size_t iChild = 0; iChild < pCondition->anyOf.size(); ++iChild)
			{
				corpCollectSpreadBuildings(pCondition->anyOf[iChild], countsOut);
			}
		}
	}
}

void CvCorporationInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers, fills provides/edges/requires)

	// SELF-collapse the CORPORATION_LEVEL per token onto this corp's own engine id (the resolveAboveToken
	// precedent, json §3.1): the count core resolves the level count as countCorporationLevels(perTypeId), so
	// the what-if valuation scales the HQ-revenue entries by the LIVE corp level. Idempotent (re-stamps the
	// same id on a re-map).
	const int iSelfCorporation = GC.getInfoTypeForString(getType(), true);
	if (iSelfCorporation >= 0)
	{
		m_modifiers.resolvePerToken("CORPORATION_LEVEL", iSelfCorporation);
	}

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member
	// (the reverse-pass-fed tech FKs are NOT reset -- they are written after the last mapFrom of a load)
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		m_aiHeadquartersCommerce[iCommerce] = 0;
	}
	m_aeConsumedBonuses.clear();
	m_spreadBuildingCounts.clear();
	m_iSpreadFactor = 0;
	m_iCompetingSpreadCostPercent = 0;
	m_iSpreadCost = 0;
	m_iTGAIndex = -1;
	m_szSound.clear();
	m_szMovieFile.clear();
	m_szMovieSound.clear();

	// --- the two value planes, scanned ONCE from the compiled entry list (the sanctioned load-time scan
	// source). HQ revenue = the per-CORPORATION_LEVEL commerce entries (rulings 4+10: {value, enabled:
	// {IS_HEADQUARTERS: SELF}, per: "CORPORATION_LEVEL"}); the consumed-bonus set = the union of every
	// entry's per.anyOf scaler ids, first-seen order (the authored bonus-list order). ---
	std::set<int> seenBonuses;
	const std::vector<CvModEntry*>& entries = m_modifiers.entries();
	for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
	{
		const CvModEntry* pEntry = entries[iEntry];
		if (pEntry->hasPer && pEntry->perType == "CORPORATION_LEVEL")
		{
			const int iCommerce = infoFamilyCommerce(pEntry->family);
			if (iCommerce >= 0 && iCommerce < NUM_COMMERCE_TYPES)
			{
				m_aiHeadquartersCommerce[iCommerce] += pEntry->value;
			}
		}
		for (size_t iBonus = 0; iBonus < pEntry->perAnyOf.size(); ++iBonus)
		{
			const int iBonusId = pEntry->perAnyOf[iBonus];
			if (iBonusId >= 0 && seenBonuses.insert(iBonusId).second)
			{
				m_aeConsumedBonuses.push_back(iBonusId);
			}
		}
	}

	// requires.spread -> the per-building count map (json §4.3; served while no corp authors it)
	corpCollectSpreadBuildings(m_requires.spread, m_spreadBuildingCounts);

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	if (const picojson::object* pCost = jsonChildObj(entityObj, "cost"))
	{
		m_iSpreadCost = jsonIdInt(*pCost, "spread");
	}
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iSpreadFactor = jsonIdInt(*pIdentity, "spreadFactor");
		m_iCompetingSpreadCostPercent = jsonIdInt(*pIdentity, "competingSpreadCostPercent");
	}
	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
		jsonIdStr(*pSound, "sound", m_szSound);
	}
	// ui.art: tgaIndex + movie.file/movie.sound (the religion mirror shape)
	if (const picojson::object* pUi = jsonChildObj(entityObj, "ui"))
	{
		if (const picojson::object* pArt = jsonChildObj(*pUi, "art"))
		{
			m_iTGAIndex = jsonIdInt(*pArt, "tgaIndex");
			if (const picojson::object* pMovie = jsonChildObj(*pArt, "movie"))
			{
				jsonIdStr(*pMovie, "file", m_szMovieFile);
				jsonIdStr(*pMovie, "sound", m_szMovieSound);
			}
		}
	}
}
