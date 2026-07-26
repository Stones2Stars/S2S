#pragma once
#ifndef CV_HURRY_INFO_H
#define CV_HURRY_INFO_H

//
//	CvHurryInfo -- the HURRY poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP: the four
//	read categories, nothing else). A rush type (gold-rush / population-rush): the bespoke §9 `conversion`
//	block (json.md §9) held as ONE typed unit mirroring the authored keys -- the two rush rates are mutually
//	exclusive in the data, each hurry authors exactly one -- plus the top-level `causesAnger` flag intrinsic.
//	JSON-fed (Assets/Data/hurries/*.json via mapFrom); no XML read (DEC-no-xml-into-game). A hurry authors no
//	availability/classification/modifier sections; type + identity text + the ui.art.icon button ride the base
//	CvInfo reading.
//

#include "CvInfo.h"   // the JSON-info base (mapFrom); on /I -> bare include

class CvHurryInfo : public CvInfo
{
public:

	CvHurryInfo();

	virtual void mapFrom(const picojson::value& entity);

	// The bespoke §9 `conversion` block as ONE typed unit mirroring the authored keys.
	struct Conversion
	{
		Conversion();
		void reset();

		int goldPerProduction;        // conversion.goldPerProduction -- gold cost per hammer of remaining production
		int productionPerPopulation;  // conversion.productionPerPopulation -- hammers yielded per population point
	};

	// ======================= 1. SECTIONS -- the whole typed `conversion` unit (json.md §9) ===================
	// (categories 2/3 are absent by the data: hurries author no §8 classification and no §6 modifier families)
	const Conversion& getConversion() const { return m_conversion; }

	// ======================= 4. INTRINSIC -- bare typed reads (genuine lone values) ==========================
	int getGoldPerProduction() const { return m_conversion.goldPerProduction; }
	int getProductionPerPopulation() const { return m_conversion.productionPerPopulation; }
	bool causesAnger() const { return m_bCausesAnger; }   // causesAnger -- the population-rush anger flag

private:
	// --- the bespoke §9 unit + the intrinsic members (materialized once at mapFrom) ---
	Conversion m_conversion;
	bool m_bCausesAnger;
};

#endif // CV_HURRY_INFO_H
