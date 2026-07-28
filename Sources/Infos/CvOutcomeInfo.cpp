//------------------------------------------------------------------------------------------------
//  FILE:    CvOutcomeInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "CvOutcomeInfo.h"
#include "CvJsonParse.h"        // jsonChildObj / jsonResolveId -- the JSON intake
#include "Tools/CheckSum.h"


CvOutcomeInfo::CvOutcomeInfo()
	: m_bCapture(false)
	, m_ePrereqTech(NO_TECH)
	, m_eObsoleteTech(NO_TECH)
	, m_ePrereqCivic(NO_CIVIC)
{
	for (int iTerritory = 0; iTerritory < NUM_OUTCOME_TERRITORIES; ++iTerritory)
	{
		m_abTerritory[iTerritory] = false;
	}
	for (int iPlacement = 0; iPlacement < NUM_OUTCOME_PLACEMENTS; ++iPlacement)
	{
		m_abPlacement[iPlacement] = false;
	}
}


// The ONE load hook. IDEMPOTENT BY CONTRACT (CvInfo::mapFrom) -- loadJson re-runs the full map once the registry
// is complete, so every member is FULLY REDEFINED here: the accumulating containers clear first, the scalars and
// flag arrays are assigned unconditionally.
void CvOutcomeInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // the CvInfoBase bridge: type + text keys (identity.description)

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& kEntity = entity.get<picojson::object>();
	picojson::object::const_iterator it;

	// --- fully redefine every member this parse owns ---
	for (int iTerritory = 0; iTerritory < NUM_OUTCOME_TERRITORIES; ++iTerritory)
	{
		m_abTerritory[iTerritory] = false;
	}
	for (int iPlacement = 0; iPlacement < NUM_OUTCOME_PLACEMENTS; ++iPlacement)
	{
		m_abPlacement[iPlacement] = false;
	}
	m_promotionOdds.clear();
	m_aePrereqBuildings.clear();
	m_aeReplaceOutcomes.clear();
	m_szMessageText.clear();
	m_bCapture = false;
	m_ePrereqTech = NO_TECH;
	m_eObsoleteTech = NO_TECH;
	m_ePrereqCivic = NO_CIVIC;

	// identity.message -- the outcome-specific popup line (identity.description is the base's job)
	if (const picojson::object* pkIdentity = jsonChildObj(kEntity, "identity"))
	{
		it = pkIdentity->find("message");
		if (it != pkIdentity->end() && it->second.is<std::string>())
		{
			m_szMessageText = CvWString(it->second.get<std::string>().c_str());
		}
	}

	// requires.build.all -- a flat list of bare type ids, routed to its typed member by id prefix
	// ([DEC-materialize-at-mapfrom]: resolved once here, never re-read on a gate call). The tree sits INSIDE the
	// `build` timing clause (owner): an outcome is a leaf action checked once when it fires, so `build` is its
	// timing, exactly as a unit carries build only.
	const picojson::object* pkRequires = jsonChildObj(kEntity, "requires");
	const picojson::object* pkRequiresBuild = (pkRequires != NULL) ? jsonChildObj(*pkRequires, "build") : NULL;
	if (pkRequiresBuild != NULL)
	{
		it = pkRequiresBuild->find("all");
		if (it != pkRequiresBuild->end() && it->second.is<picojson::array>())
		{
			const picojson::array& kAll = it->second.get<picojson::array>();
			for (size_t iEntry = 0; iEntry < kAll.size(); ++iEntry)
			{
				if (!kAll[iEntry].is<std::string>())
				{
					continue;
				}
				const std::string& szId = kAll[iEntry].get<std::string>();
				const int iResolved = jsonResolveId(szId);
				if (iResolved < 0)
				{
					continue;
				}
				if (szId.compare(0, 5, "TECH_") == 0)
				{
					m_ePrereqTech = (TechTypes)iResolved;
				}
				else if (szId.compare(0, 6, "CIVIC_") == 0)
				{
					m_ePrereqCivic = (CivicTypes)iResolved;
				}
				else if (szId.compare(0, 9, "BUILDING_") == 0)
				{
					m_aePrereqBuildings.push_back((BuildingTypes)iResolved);
				}
			}
		}
	}

	it = kEntity.find("obsoletedBy");
	if (it != kEntity.end() && it->second.is<std::string>())
	{
		const int iResolved = jsonResolveId(it->second.get<std::string>());
		if (iResolved >= 0)
		{
			m_eObsoleteTech = (TechTypes)iResolved;
		}
	}

	it = kEntity.find("territory");
	if (it != kEntity.end() && it->second.is<picojson::array>())
	{
		const picojson::array& kTerritory = it->second.get<picojson::array>();
		for (size_t iEntry = 0; iEntry < kTerritory.size(); ++iEntry)
		{
			if (!kTerritory[iEntry].is<std::string>())
			{
				continue;
			}
			const std::string& szTerritory = kTerritory[iEntry].get<std::string>();
			if (szTerritory == "friendly")
			{
				m_abTerritory[OUTCOME_TERRITORY_FRIENDLY] = true;
			}
			else if (szTerritory == "neutral")
			{
				m_abTerritory[OUTCOME_TERRITORY_NEUTRAL] = true;
			}
			else if (szTerritory == "hostile")
			{
				m_abTerritory[OUTCOME_TERRITORY_HOSTILE] = true;
			}
			else if (szTerritory == "barbarian")
			{
				m_abTerritory[OUTCOME_TERRITORY_BARBARIAN] = true;
			}
		}
	}

	it = kEntity.find("in");
	if (it != kEntity.end() && it->second.is<std::string>())
	{
		const std::string& szPlacement = it->second.get<std::string>();
		if (szPlacement == "city")
		{
			m_abPlacement[OUTCOME_PLACEMENT_CITY] = true;
		}
		else if (szPlacement == "notCity")
		{
			m_abPlacement[OUTCOME_PLACEMENT_NOT_CITY] = true;
		}
	}

	it = kEntity.find("coastalCity");
	if (it != kEntity.end() && it->second.is<bool>())
	{
		m_abPlacement[OUTCOME_PLACEMENT_COASTAL_CITY] = it->second.get<bool>();
	}

	it = kEntity.find("capture");
	if (it != kEntity.end() && it->second.is<bool>())
	{
		m_bCapture = it->second.get<bool>();
	}

	// odds -- PROMOTION_* id -> extra-chance percentage
	if (const picojson::object* pkOdds = jsonChildObj(kEntity, "odds"))
	{
		for (picojson::object::const_iterator itOdds = pkOdds->begin(); itOdds != pkOdds->end(); ++itOdds)
		{
			if (!itOdds->second.is<double>())
			{
				continue;
			}
			const int iPromotion = jsonResolveId(itOdds->first);
			if (iPromotion >= 0)
			{
				m_promotionOdds[iPromotion] = (int)itOdds->second.get<double>();
			}
		}
	}

	// replaces -- the outcomes this one supersedes when it survives the roll
	it = kEntity.find("replaces");
	if (it != kEntity.end() && it->second.is<picojson::array>())
	{
		const picojson::array& kReplaces = it->second.get<picojson::array>();
		for (size_t iEntry = 0; iEntry < kReplaces.size(); ++iEntry)
		{
			if (!kReplaces[iEntry].is<std::string>())
			{
				continue;
			}
			const int iResolved = jsonResolveId(kReplaces[iEntry].get<std::string>());
			if (iResolved >= 0)
			{
				m_aeReplaceOutcomes.push_back((OutcomeTypes)iResolved);
			}
		}
	}
}


void CvOutcomeInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_ePrereqTech);
	CheckSum(iSum, m_eObsoleteTech);
	for (std::map<int, int>::const_iterator itOdds = m_promotionOdds.begin(); itOdds != m_promotionOdds.end(); ++itOdds)
	{
		CheckSum(iSum, itOdds->first);
		CheckSum(iSum, itOdds->second);
	}
	CheckSumC(iSum, m_aePrereqBuildings);
	for (int iTerritory = 0; iTerritory < NUM_OUTCOME_TERRITORIES; ++iTerritory)
	{
		CheckSum(iSum, m_abTerritory[iTerritory]);
	}
	for (int iPlacement = 0; iPlacement < NUM_OUTCOME_PLACEMENTS; ++iPlacement)
	{
		CheckSum(iSum, m_abPlacement[iPlacement]);
	}
	CheckSum(iSum, m_bCapture);
	CheckSumC(iSum, m_aeReplaceOutcomes);
	CheckSum(iSum, m_ePrereqCivic);
}
