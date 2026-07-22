//------------------------------------------------------------------------------------------------
//  FILE:    CvOutcomeInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvBuildingInfo.h"
#include "CvHeritageInfo.h"
#include "AI/CvGameAI.h"
#include "UI/CvGameTextMgr.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "AI/CvPlayerAI.h"
#include "Infrastructure/CvPython.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvXMLLoadUtilityModTools.h"
#include "Tools/CheckSum.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvOutcomeInfo.h"
#include "CvJsonParse.h"   // #430: jsonChildObj / jsonResolveId -- the JSON intake



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvOutcomeInfo
//
//  DESC:   Contains info about outcome types which can be the result of a kill or of actions
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CvOutcomeInfo::CvOutcomeInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvOutcomeInfo::~CvOutcomeInfo()
{
	GC.removeDelayedResolutionVector(m_aeReplaceOutcomes);
}


void CvOutcomeInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. Hand-written fields (see read()):
	// - m_szMessageText: CvWString, no CvInfoUtil wrapper exists (StringWrapper is CvString-only).
	// - m_aeiExtraChancePromotions: bespoke std::vector<std::pair> walk, no wrapper shape fits.
	// - m_aeReplaceOutcomes: self-referential FK list (SetOptionalVectorWithDelayedResolution).
	// getCheckSum stays explicit because those fields sit mid-order in the legacy checksum.
	util
		.addEnum(m_ePrereqTech, L"PrereqTech")
		.addEnum(m_eObsoleteTech, L"ObsoleteTech")
		.add(m_aePrereqBuildings, L"PrereqBuildings")
		.add(m_bToCoastalCity, L"bToCoastalCity")
		.add(m_bFriendlyTerritory, L"bFriendlyTerritory", true)
		.add(m_bNeutralTerritory, L"bNeutralTerritory", true)
		.add(m_bHostileTerritory, L"bHostileTerritory", true)
		.add(m_bBarbarianTerritory, L"bBarbarianTerritory")
		.add(m_bCity, L"bCity")
		.add(m_bNotCity, L"bNotCity")
		.add(m_bCapture, L"bCapture")
		.addEnum(m_ePrereqCivic, L"PrereqCivic")
	;
}


// #430: JSON intake from Assets/Data/outcomes/*.json (curate_outcome.py). Populates the SAME members the getters
// read (CvOutcome::isPossibleInPlot reads them), so the gate behaviour is unchanged -- only the read source flips.
void CvOutcomeInfo::mapFrom(const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	picojson::object::const_iterator it;

	if ((it = o.find("type")) != o.end() && it->second.is<std::string>()) m_szType = it->second.get<std::string>().c_str();
	if (const picojson::object* id = jsonChildObj(o, "identity"))
	{
		if ((it = id->find("description")) != id->end() && it->second.is<std::string>()) m_szTextKey     = CvWString(it->second.get<std::string>().c_str());
		if ((it = id->find("message"))     != id->end() && it->second.is<std::string>()) m_szMessageText = CvWString(it->second.get<std::string>().c_str());
	}

	// requires.all -> the prereq fields, routed by id prefix (bare strings from curate_outcome)
	if (const picojson::object* req = jsonChildObj(o, "requires"))
	{
		it = req->find("all");
		if (it != req->end() && it->second.is<picojson::array>())
		{
			const picojson::array& a = it->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (!a[i].is<std::string>()) continue;
				const std::string& s = a[i].get<std::string>();
				const int rid = jsonResolveId(s);
				if (rid < 0) continue;
				if      (s.compare(0, 5, "TECH_")     == 0) m_ePrereqTech  = (TechTypes)rid;
				else if (s.compare(0, 6, "CIVIC_")    == 0) m_ePrereqCivic = (CivicTypes)rid;
				else if (s.compare(0, 9, "BUILDING_") == 0) m_aePrereqBuildings.push_back((BuildingTypes)rid);
			}
		}
	}
	if ((it = o.find("obsoletedBy")) != o.end() && it->second.is<std::string>())
	{ const int rid = jsonResolveId(it->second.get<std::string>()); if (rid >= 0) m_eObsoleteTech = (TechTypes)rid; }

	if ((it = o.find("territory")) != o.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (!a[i].is<std::string>()) continue;
			const std::string& s = a[i].get<std::string>();
			if      (s == "friendly")  m_bFriendlyTerritory  = true;
			else if (s == "neutral")   m_bNeutralTerritory   = true;
			else if (s == "hostile")   m_bHostileTerritory   = true;
			else if (s == "barbarian") m_bBarbarianTerritory = true;
		}
	}
	if ((it = o.find("in")) != o.end() && it->second.is<std::string>())
	{
		const std::string& s = it->second.get<std::string>();
		if (s == "city") m_bCity = true; else if (s == "notCity") m_bNotCity = true;
	}
	if ((it = o.find("coastalCity")) != o.end() && it->second.is<bool>()) m_bToCoastalCity = it->second.get<bool>();
	if ((it = o.find("capture"))     != o.end() && it->second.is<bool>()) m_bCapture       = it->second.get<bool>();

	if (const picojson::object* odds = jsonChildObj(o, "odds"))
		for (picojson::object::const_iterator p = odds->begin(); p != odds->end(); ++p)
			if (p->second.is<double>())
			{
				const int rid = jsonResolveId(p->first);
				if (rid >= 0) m_aeiExtraChancePromotions.push_back(std::make_pair((PromotionTypes)rid, (int)p->second.get<double>()));
			}

	if ((it = o.find("replaces")) != o.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>()) { const int rid = jsonResolveId(a[i].get<std::string>()); if (rid >= 0) m_aeReplaceOutcomes.push_back((OutcomeTypes)rid); }
	}
}


void CvOutcomeInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_ePrereqTech);
	CheckSum(iSum, m_eObsoleteTech);
	CheckSumC(iSum, m_aeiExtraChancePromotions);
	CheckSumC(iSum, m_aePrereqBuildings);
	CheckSum(iSum, m_bToCoastalCity);
	CheckSum(iSum, m_bFriendlyTerritory);
	CheckSum(iSum, m_bNeutralTerritory);
	CheckSum(iSum, m_bHostileTerritory);
	CheckSum(iSum, m_bBarbarianTerritory);
	CheckSum(iSum, m_bCity);
	CheckSum(iSum, m_bNotCity);
	CheckSum(iSum, m_bCapture);
	CheckSumC(iSum, m_aeReplaceOutcomes);
	CheckSum(iSum, m_ePrereqCivic);
}


CvWString CvOutcomeInfo::getMessageText() const
{
	return m_szMessageText;
}


bool CvOutcomeInfo::getToCoastalCity() const
{
	return m_bToCoastalCity;
}


bool CvOutcomeInfo::getFriendlyTerritory() const
{
	return m_bFriendlyTerritory;
}


bool CvOutcomeInfo::getNeutralTerritory() const
{
	return m_bNeutralTerritory;
}


bool CvOutcomeInfo::getHostileTerritory() const
{
	return m_bHostileTerritory;
}


bool CvOutcomeInfo::getBarbarianTerritory() const
{
	return m_bBarbarianTerritory;
}


bool CvOutcomeInfo::getCity() const
{
	return m_bCity;
}


bool CvOutcomeInfo::getNotCity() const
{
	return m_bNotCity;
}


bool CvOutcomeInfo::isCapture() const
{
	return m_bCapture;
}


TechTypes CvOutcomeInfo::getObsoleteTech() const
{
	return m_eObsoleteTech;
}


CivicTypes CvOutcomeInfo::getPrereqCivic() const
{
	return m_ePrereqCivic;
}


int CvOutcomeInfo::getNumExtraChancePromotions() const
{
	return m_aeiExtraChancePromotions.size();
}


PromotionTypes CvOutcomeInfo::getExtraChancePromotion(int i) const
{
	FASSERT_BOUNDS(0, getNumExtraChancePromotions(), i);
	return m_aeiExtraChancePromotions[i].first;
}


int CvOutcomeInfo::getExtraChancePromotionChance(int i) const
{
	FASSERT_BOUNDS(0, getNumExtraChancePromotions(), i);
	return m_aeiExtraChancePromotions[i].second;
}

