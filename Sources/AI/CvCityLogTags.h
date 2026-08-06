#pragma once
#ifndef CV_CITY_LOG_TAGS_H
#define CV_CITY_LOG_TAGS_H

//
//	[CIT] event-spine logging tags (#430). The [CIT] domain spans TWO files (CvCityAI.cpp + CvCity.cpp), which share a
//	FastBuild unity batch -- so these tag enums must be defined ONCE in a shared header, not duplicated in each .cpp's
//	anonymous namespace (that is an enum-redefinition error in the concatenated unity TU). The prefix/field-info/registrar
//	live in CvCityAI.cpp; both files reference these enum values at their emit sites. (Per-domain isolation preserved --
//	this header is [CIT]-only.)
//

enum CitEvent
{
	CIT_GARRCONS = 0,        // [CIT/garrcons]
	CIT_BEGIN,               // [CIT/begin]
	CIT_STRANDED,            // [CIT/stranded]
	CIT_STRANDED_TRY,        // [CIT/stranded/try]
	CIT_STRANDED_DECLINED,   // [CIT/stranded/declined]
	CIT_DANGER,              // [CIT/danger]
	CIT_ORDER_CONSTRUCT,     // [CIT/order] action=construct
	CIT_ORDER_PROJECT,       // [CIT/order] action=createProject
	CIT_ORDER_PROCESS,       // [CIT/order] action=maintainProcess
	CIT_PROP,                // [CIT/prop]
	CIT_PROPLEVEL,           // [CIT/proplevel]
	CIT_PUSH_REJECT_UNIT,    // [CIT/push/reject] kind=unit reason=spamGuard
	CIT_PUSH_REJECT_BUILDING,// [CIT/push/reject] kind=building reason=dupGuard
	CIT_PUSH_UNIT,           // [CIT/push] kind=unit
	CIT_PUSH_BUILDING,       // [CIT/push] kind=building
	CIT_PUSH_PROJECT,        // [CIT/push] kind=project
	CIT_PUSH_PROCESS,        // [CIT/push] kind=process
	CIT_PUSH_LIST,           // [CIT/push] kind=list
	CIT_PUSH_OTHER,          // [CIT/push] kind=other
	CIT_CANCEL_UNIT,         // [CIT/cancel] kind=unit
	CIT_CANCEL_BUILDING,     // [CIT/cancel] kind=building
	CIT_CANCEL_PROJECT,      // [CIT/cancel] kind=project
	CIT_CANCEL_OTHER,        // [CIT/cancel] kind=other
	CIT_PRODUCED_UNIT,       // [CIT/produced] kind=unit
	CIT_PRODUCED_BUILDING,   // [CIT/produced] kind=building
	CIT_PRODUCED_PROJECT,    // [CIT/produced] kind=project
	CIT_SPIN_LOOP_CAP,       // [CIT/spin] reason=produceLoopCap
	CIT_SPIN_NO_PROD,        // [CIT/spin] reason=noProductionChosen
	CIT_WASTE,               // [CIT/waste]
	CIT_ASSIGN_DIRTY,        // [CIT/assign/dirty] -- assignWork false->true transition + the caller's RVA (the churn-storm attribution instrument)
	CIT_ASSIGN_FAN,          // [CIT/assign/fan] -- a WHOLE-SCOPE assign-dirty fan + the RVA of whoever asked for it (see the header note)
	CIT_ASSIGN_RUN,          // [CIT/assign/run] -- one AI_assignWorkingPlots run completed (runs/city/turn = the storm shape)
	CIT_BILLBOARD_POLL,      // [CIT/billboard] -- an EXE billboard entry point was called (fn = the census index; the exhaustive billboard-feed trace)
	CIT_ASSIGN_CAND,         // [CIT/assign/cand] -- one citizen placement decided: the best specialist and best plot with their VALUES, and which won
	//	The ATTRIBUTION half of the specialist-vs-plot question. [CIT/assign/cand] shows THAT one side wins and by
	//	how much; it cannot show WHERE that side's number comes from, so the cause stayed a matter of opinion.
	//	Each of these splits ONE candidate's score into the YIELD term (the AI_yieldValue return, the only part the
	//	two kinds compute the same way) and the FINAL value. The difference is the non-yield contribution --
	//	specialist: GPP + XP + wellbeing + property + underworld; plot: the improvement blend, the potential-plot
	//	penalty and the bonus-discovery adds. Which term carries the advantage is then a reading, not a theory.
	CIT_ASSIGN_SPECVAL,      // [CIT/assign/specval] -- one specialist's score, yield term vs final
	CIT_ASSIGN_PLOTVAL       // [CIT/assign/plotval] -- one plot's score, yield term vs final
};

// CIT LOCAL field tags. city/owner are ints (city ID / PlayerTypes); prop is a PropertyTypes index (rendered as int).
enum CitField
{
	CITF_city = 0, CITF_owner, CITF_turn, CITF_prop,
	CITF_val, CITF_change, CITF_merges, CITF_strLeft, CITF_need,
	CITF_pop, CITF_danger, CITF_dangerVal, CITF_finTrouble, CITF_critGold, CITF_foodProd,
	CITF_wHave, CITF_wNeed, CITF_areaHave, CITF_areaNeed, CITF_inhibit, CITF_turtle, CITF_bestBuildVal,
	CITF_minAtk, CITF_defShortfall, CITF_sqrtCities, CITF_ownedAtk, CITF_ownedAtkRaw, CITF_fire,
	CITF_pct, CITF_eval, CITF_check, CITF_proj, CITF_getting, CITF_good, CITF_maxed, CITF_propPct,
	CITF_unitType, CITF_unitAI,
	CITF_building,
	CITF_score, CITF_rank, CITF_count, CITF_focus,
	CITF_project, CITF_process, CITF_commerce,
	CITF_alreadyQueued, CITF_append, CITF_force,
	CITF_progressLost, CITF_willChoose,
	CITF_overflow, CITF_lost, CITF_ownerHas, CITF_aiRoleHas,
	CITF_lostProd, CITF_gold,
	CITF_callerRva,  // the dirty-setter caller's RVA (module-relative return address; resolve via the PDB: `ln CvGameCoreDLL+<val>`)
	CITF_cities,     // how many cities one assign-dirty FAN reached
	CITF_fn,         // the billboard entry-point census index (gPerfBillboardFnNames)
	//	The specialist-vs-plot COMPARISON, which is the one thing the assign instrument could not show: it
	//	recorded that a run happened, never what it decided or why. Both kinds' best REMAINING option on one
	//	line makes the ratio readable directly, and the shape is unchanged across the priority-list restructure
	//	so captures either side of it stay comparable.
	//	⚠ THE SCALE THEORY IS CLOSED -- do not re-open it. A side that always wins is normally the signature of
	//	a truncated or mis-scaled input ([fixed-point-and-scales] §5), and that was checked here: BOTH sides
	//	enter AI_yieldValue as HUMAN values (CvPlayer::specialistYield / specialistCommerce each reduce /100,
	//	matching pPlot->getYield), so the specialist advantage this log shows is genuine AI WEIGHTING and is a
	//	tuning question, not a scale defect to hunt again.
	CITF_specialist, CITF_specialistVal, CITF_plot, CITF_plotVal,
	//	The attribution split (CIT_ASSIGN_SPECVAL / CIT_ASSIGN_PLOTVAL): the shared YIELD term, and the FINAL
	//	score. final - yield IS the non-yield contribution, which is the axis the two kinds differ on.
	CITF_yieldPart, CITF_finalVal,
	//	The specialist tail, one level deeper (CIT_ASSIGN_SPECVAL). The tail is ~95% of a specialist's score and
	//	lands at nearly the SAME magnitude on unrelated specialist types, which is the signature of a shared term
	//	rather than of what any specialist produces -- so it is split into its five actual contributors instead
	//	of being reasoned about from the source.
	CITF_gppPart, CITF_xpPart, CITF_wellbeingPart, CITF_propertyPart, CITF_underworldPart
};

// ⛔ WHY THE FAN NEEDS ITS OWN TAG: the per-city [CIT/assign/dirty] line captures the return address inside
// AI_setAssignWorkDirty, which attributes a DIRECT caller correctly -- but every WHOLE-SCOPE fan arrives through
// one `algo::for_each(cities(), ...)`, so all of them report that one address and the instrument names the fan
// instead of whoever asked for it. The fan therefore captures its OWN caller, once, at the entry point.

// The billboard-feed trace (owner 2026-07-16): EVERY billboard entry point emits a [CIT/billboard] spine event
// per call (+ increments the census counter). Defined once in CvCityAI.cpp; called from CvCity.cpp + CvGameTextMgr.cpp.
void citEmitBillboardPoll(int iFn, int iCityId);

#endif // CV_CITY_LOG_TAGS_H
