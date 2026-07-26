#pragma once
#ifndef CV_VISION_SECTION_H
#define CV_VISION_SECTION_H

//
//	CvVisionSection -- the json.md par.9 `vision` bespoke block as ONE composable typed section object
//	(patterns.md par. THE GETTER SETUP category 1: sections are whole typed objects). The LOS-resolver data the
//	unit plane authors (units / promotions / unitcombats share the exact key vocabulary, so ONE unit serves all
//	three -- the CvRequires/CvGrants composition precedent): the invisibility type, the extra visibility range,
//	the negated invisibility types, the four intensity pair-maps (keyed by INVISIBLE_* id), and the nine
//	invisible/visible substrate struct-row lists. WRITE-ONCE AT LOAD (parse is the sole writer; clear-first,
//	idempotent under the full-registry re-map). Values are engine-native counts/intensities (not magnitudes;
//	no x100 -- the building-exemplar "a config is human" convention).
//

#include "Defines/CvStructs.h"   // InvisibleTerrainChanges / InvisibleFeatureChanges / InvisibleImprovementChanges
#include <map>
#include <vector>

namespace picojson { class value; }

class CvVisionSection
{
public:
	CvVisionSection();

	// The unit's single load-time writer: locate + parse the entity's `vision` block (absent = stays empty).
	void parse(const picojson::value& entity);
	void clearParsed();

	// --- the typed section data (public members -- the CvRequires open-section style) ---
	int invisible;                                        // vision.invisible: INVISIBLE_* FK; -1 = not invisible
	int range;                                            // vision.range.flat: extra visibility range (plain count)
	std::vector<int> negates;                             // vision.negates: INVISIBLE_* FKs this negates outright
	std::map<int, int> visibilityIntensity;               // vision.visibilityIntensity.{INVISIBLE}: intensity
	std::map<int, int> invisibilityIntensity;             // vision.invisibilityIntensity.{INVISIBLE}: intensity
	std::map<int, int> visibilityIntensityRange;          // vision.visibilityIntensityRange.{INVISIBLE}: intensity
	std::map<int, int> visibilityIntensitySameTile;       // vision.visibilityIntensitySameTile.{INVISIBLE}: intensity
	std::vector<InvisibleTerrainChanges> invisibleTerrain;          // vision.invisibleTerrain rows
	std::vector<InvisibleFeatureChanges> invisibleFeature;          // vision.invisibleFeature rows
	std::vector<InvisibleImprovementChanges> invisibleImprovement;  // vision.invisibleImprovement rows
	std::vector<InvisibleTerrainChanges> visibleTerrain;            // vision.visibleTerrain rows
	std::vector<InvisibleFeatureChanges> visibleFeature;            // vision.visibleFeature rows
	std::vector<InvisibleImprovementChanges> visibleImprovement;    // vision.visibleImprovement rows
	std::vector<InvisibleTerrainChanges> visibleTerrainRange;       // vision.visibleTerrainRange rows
	std::vector<InvisibleFeatureChanges> visibleFeatureRange;       // vision.visibleFeatureRange rows
	std::vector<InvisibleImprovementChanges> visibleImprovementRange;  // vision.visibleImprovementRange rows

private:
	CvVisionSection(const CvVisionSection&);              // noncopyable (held by-value on the noncopyable info)
	CvVisionSection& operator=(const CvVisionSection&);
};

#endif // CV_VISION_SECTION_H
