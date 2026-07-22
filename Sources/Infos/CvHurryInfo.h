#pragma once

#ifndef CV_HURRY_INFO_H
#define CV_HURRY_INFO_H

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvHurryInfo
//
//  DESC:   A rush type (gold-rush / population-rush). #430: JSON-fed
//          (Assets/Data/hurries/*.json via mapFrom); no XML read.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvHurryInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvHurryInfo();

	int getGoldPerProduction() const { return m_iGoldPerProduction; }
	int getProductionPerPopulation() const { return m_iProductionPerPopulation; }
	bool isAnger() const { return m_bAnger; }

	virtual void mapFrom(const picojson::value& entity);

	//---------------------------PROTECTED MEMBER VARIABLES-----------------------
protected:

	int m_iGoldPerProduction;
	int m_iProductionPerPopulation;
	bool m_bAnger;
};

#endif // CV_HURRY_INFO_H
