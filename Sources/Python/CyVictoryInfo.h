#pragma once

#ifndef CyVictoryInfo_h__
#define CyVictoryInfo_h__

//
//	CyVictoryInfo -- the VICTORY accessor, a sibling of CyWorldInfo / CyGameSpeedInfo / CyEspionageMissionInfo on
//	the PER-INFO plane ([patterns.md] THE PYTHON READ BOUNDARY). The id is the victory, passed per call.
//
//	⚑ IT IS THE WHOLE REGISTRY IN FOUR READS, because it MIRRORS THE INFO'S OWN SHAPE rather than the legacy
//	getter set. CvVictoryInfo already groups its `condition` block ([json.md] §9) behind two parameterized reads
//	over the block's authored keys, so this accessor does the same: one test over the boolean rules, one read
//	over the numeric thresholds, plus the two intrinsics. Nothing here is a per-field getter.
//	⛔ The alternative -- isConquest / isDiploVote / isTargetScore / isEndScore / isTotalVictory /
//	getReligionPercent / getNumCultureCities / ... -- is the ~300-hand-named-getter shape
//	([DEC-new-getter-surface]) rebuilt one screen at a time, and it grows by CHANNEL where this grows by GROUP.
//	A newly authored condition key is then a new enum entry the data already carries, never a new function.
//
//	⚠ The flag/value ids are the engine's own `VictoryConditionFlag` / `VictoryConditionValue` enums, published
//	as the caller's vocabulary ([patterns.md]: the existing enum indexes the RESULT, the call carries no channel
//	argument of its own invention).
//
class CyVictoryInfo
{
public:
	CyVictoryInfo() {}

	// The `condition` block's boolean win rules (conquest / diploVote / targetScore / endScore / permanent /
	// totalVictory) -- ONE test, parameterized over the block's keys.
	bool conditionFlag(int iVictory, int iFlag) const;
	// The `condition` block's numeric thresholds (landPercent / minLandPercent / populationPercentLead /
	// religionPercent / numCultureCities / delayTurns) -- ONE read over the same block.
	int conditionValue(int iVictory, int iValue) const;

	// INTRINSIC -- the CULTURELEVEL_ FK the culture rule names (-1 = no culture-level rule), and the win movie.
	int getCityCulture(int iVictory) const;
	std::string getMovie(int iVictory) const;

	static void pythonPublish();
};

#endif // CyVictoryInfo_h__
