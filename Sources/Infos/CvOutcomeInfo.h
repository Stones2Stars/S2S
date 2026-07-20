#pragma once

#ifndef CV_OUTCOME_INFO_H
#define CV_OUTCOME_INFO_H

#include "CvInfoBase.h"

namespace picojson { class value; }   // #430: mapFrom -- JSON intake (replaces the XML read path)

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvOutcomeInfo
//
//  DESC:   Contains info about outcome types which can be the result of a kill or of actions
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvOutcomeInfo
	: public CvInfoBase
	, private bst::noncopyable
{
public:

	CvOutcomeInfo();
	virtual ~CvOutcomeInfo();

	CvWString getMessageText() const;
	TechTypes getPrereqTech() const	{ return m_ePrereqTech; }
	TechTypes getObsoleteTech() const;
	CivicTypes getPrereqCivic() const;
	bool getToCoastalCity() const;
	bool getFriendlyTerritory() const;
	bool getNeutralTerritory() const;
	bool getHostileTerritory() const;
	bool getBarbarianTerritory() const;
	bool getCity() const;
	bool getNotCity() const;
	bool isCapture() const;
	const std::vector<BuildingTypes>& getPrereqBuildings() const { return m_aePrereqBuildings; }
	int getNumExtraChancePromotions() const;
	PromotionTypes getExtraChancePromotion(int i) const;
	int getExtraChancePromotionChance(int i) const;
	const std::vector<OutcomeTypes>& getReplaceOutcomes() const { return m_aeReplaceOutcomes; }

	void getDataMembers(CvInfoUtil& util);
	void mapFrom(const picojson::value& v);   // #430: JSON intake from Assets/Data/outcomes/*.json (the XML read is dead)
	void getCheckSum(uint32_t& iSum) const;

protected:

	TechTypes m_ePrereqTech;
	TechTypes m_eObsoleteTech;
	CivicTypes m_ePrereqCivic;
	bool m_bToCoastalCity;
	bool m_bFriendlyTerritory;
	bool m_bNeutralTerritory;
	bool m_bHostileTerritory;
	bool m_bBarbarianTerritory;
	bool m_bCity;
	bool m_bNotCity;
	bool m_bCapture;
	std::vector<BuildingTypes> m_aePrereqBuildings;
	std::vector<std::pair<PromotionTypes, int> > m_aeiExtraChancePromotions;
	std::vector<OutcomeTypes> m_aeReplaceOutcomes;
	CvWString m_szMessageText;
};

#endif // CV_OUTCOME_INFO_H
