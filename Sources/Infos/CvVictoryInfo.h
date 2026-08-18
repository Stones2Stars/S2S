#pragma once
#ifndef CV_VICTORY_INFO_H
#define CV_VICTORY_INFO_H

//
//	CvVictoryInfo -- the VICTORY poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP: the four
//	read categories, nothing else). A victory condition: the whole payload is the bespoke §9 `condition` block
//	(json.md §9) -- the win-rule flags and numeric thresholds the victory-test system reads -- held as ONE
//	typed unit mirroring the authored keys, plus the completion-movie intrinsic. JSON-fed
//	(Assets/Data/victories/*.json via mapFrom); no XML read (AGENTS.md §Build And Test (no XML-into-game for replaced infos)). A victory authors no
//	availability/classification/modifier sections; type + identity text keys ride the base CvInfo reading.
//

#include "CvInfo.h"   // the JSON-info base (mapFrom); on /I -> bare include

// The `condition` block's boolean win rules -- ONE parameterized test over the block's authored keys
// (the coherent-surface rule: a groupable boolean set is never N hand-named getters).
enum VictoryConditionFlag
{
	VICTORY_CONDITION_CONQUEST,        // condition.conquest
	VICTORY_CONDITION_DIPLO_VOTE,      // condition.diploVote
	VICTORY_CONDITION_TARGET_SCORE,    // condition.targetScore
	VICTORY_CONDITION_END_SCORE,       // condition.endScore
	VICTORY_CONDITION_PERMANENT,       // condition.permanent
	VICTORY_CONDITION_TOTAL_VICTORY,   // condition.totalVictory
	NUM_VICTORY_CONDITION_FLAGS
};

// The `condition` block's numeric thresholds -- ONE parameterized read over the block's authored keys.
enum VictoryConditionValue
{
	VICTORY_CONDITION_LAND_PERCENT,              // condition.landPercent
	VICTORY_CONDITION_MIN_LAND_PERCENT,          // condition.minLandPercent
	VICTORY_CONDITION_POPULATION_PERCENT_LEAD,   // condition.populationPercentLead
	VICTORY_CONDITION_RELIGION_PERCENT,          // condition.religionPercent
	VICTORY_CONDITION_NUM_CULTURE_CITIES,        // condition.numCultureCities
	VICTORY_CONDITION_DELAY_TURNS,               // condition.delayTurns
	NUM_VICTORY_CONDITION_VALUES
};

class CvVictoryInfo : public CvInfo
{
public:

	CvVictoryInfo();

	virtual void mapFrom(const picojson::value& entity);

	// The bespoke §9 `condition` block as ONE typed unit mirroring the authored keys.
	struct Condition
	{
		Condition();
		void reset();

		bool flags[NUM_VICTORY_CONDITION_FLAGS];
		int values[NUM_VICTORY_CONDITION_VALUES];
		int cityCultureLevel;   // condition.cityCulture -- CULTURELEVEL_* FK id; -1 = no culture-level rule
	};

	// ======================= 1. SECTIONS -- the whole typed `condition` unit (json.md §9) ====================
	// (categories 2/3 are absent by the data: victories author no §8 classification and no §6 modifier families)
	const Condition& getCondition() const { return m_condition; }
	// the grouped reads over the unit -- one getter per group, parameterized over the block's keys
	bool conditionFlag(VictoryConditionFlag eFlag) const { return m_condition.flags[eFlag]; }
	int conditionValue(VictoryConditionValue eValue) const { return m_condition.values[eValue]; }

	// ======================= 4. INTRINSIC -- bare typed reads ================================================
	int getCityCulture() const { return m_condition.cityCultureLevel; }   // condition.cityCulture (FK; -1 none)
	const char* getMovie() const { return m_szMovie.c_str(); }            // ui.art.movie.file
	DllExport bool isPermanent() const;   // EXE-bound export (the closed .exe imports it) -- kept out-of-line

private:
	// --- the bespoke §9 unit + the intrinsic members (materialized once at mapFrom) ---
	Condition m_condition;
	std::string m_szMovie;
};

#endif // CV_VICTORY_INFO_H
