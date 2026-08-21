#pragma once
#ifndef CV_SIZE_MATTERS_SECTION_H
#define CV_SIZE_MATTERS_SECTION_H

//
//	CvSizeMattersSection -- the json.md par.9 `sizeMatters` bespoke block as ONE composable typed section object
//	(patterns.md par. THE GETTER SETUP category 1). The Size-Matters combat system's data, gated by
//	GAMEOPTION_COMBAT_SIZE_MATTERS at the CONSUMING system (the info is pure data and reads no game state --
//	patterns.md INFO DATA-OUT contract). The keys are shared across the entity kinds; the base-vs-delta semantic
//	is carried by the ENTITY (unitcombat = the intrinsic base ranks; promotion = the runtime deltas; unit = its
//	own per-rank combat mods + cargo geometry). A base rank equal to the legacy -10 "unset" sentinel is authored
//	ABSENT and stays -10 here (0 is a real rank). Values are the SM engine's own rank/percent parameters,
//	engine-native human (system config, not compiled magnitudes). WRITE-ONCE AT LOAD.
//

namespace picojson { class value; }

class CvSizeMattersSection
{
public:
	CvSizeMattersSection();

	// The unit's single load-time writer: locate + parse the entity's `sizeMatters` block (absent = defaults).
	void parse(const picojson::value& entity);
	void clearParsed();

	// --- the typed section data (public members -- the open-section style) ---
	// UnitCombat base ranks (-10 = unset sentinel; absent in the data when unset).
	int qualityBase;
	int groupBase;
	int sizeBase;
	// Promotion delta scalars (0 = no change).
	int quality;
	int group;
	int sizeModifier;
	int maxHP;
	// The per-rank combat-modifier quartet (unit / unitcombat / promotion).
	int combatModifierPerSizeMore;
	int combatModifierPerSizeLess;
	int combatModifierPerVolumeMore;
	int combatModifierPerVolumeLess;
	// The SM cargo trio (cargo: { smSpace, volume, volumeModifier }).
	int cargoSmSpace;
	int cargoVolume;
	int cargoVolumeModifier;
	// Unit-authored geometry (json.md par.9: groupSize / baseCargoVolume where authored; 0 = unauthored).
	int groupSize;
	int baseCargoVolume;

private:
	CvSizeMattersSection(const CvSizeMattersSection&);              // noncopyable (held by-value on the info)
	CvSizeMattersSection& operator=(const CvSizeMattersSection&);
};

#endif // CV_SIZE_MATTERS_SECTION_H
