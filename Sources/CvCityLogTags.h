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
	CIT_WASTE                // [CIT/waste]
};

// CIT LOCAL field tags. city/owner are ints (city ID / PlayerTypes); prop is a PropertyTypes index (rendered as int).
enum CitField
{
	CF_city = 0, CF_owner, CF_turn, CF_prop,
	CF_val, CF_change, CF_merges, CF_strLeft, CF_need,
	CF_pop, CF_danger, CF_dangerVal, CF_finTrouble, CF_critGold, CF_foodProd,
	CF_wHave, CF_wNeed, CF_areaHave, CF_areaNeed, CF_inhibit, CF_turtle, CF_bestBuildVal,
	CF_minAtk, CF_defShortfall, CF_sqrtCities, CF_ownedAtk, CF_ownedAtkRaw, CF_fire,
	CF_pct, CF_eval, CF_check, CF_proj, CF_getting, CF_good, CF_maxed, CF_propPct,
	CF_unitType, CF_unitAI,
	CF_building,
	CF_score, CF_rank, CF_count, CF_focus,
	CF_project, CF_process, CF_commerce,
	CF_alreadyQueued, CF_append, CF_force,
	CF_progressLost, CF_willChoose,
	CF_overflow, CF_lost, CF_ownerHas, CF_aiRoleHas,
	CF_lostProd, CF_gold
};

#endif // CV_CITY_LOG_TAGS_H
