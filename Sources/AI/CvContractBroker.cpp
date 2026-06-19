// unitAI.cpp


#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "Engine/CvCity.h"
#include "CvContractBroker.h"
#include "CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "Engine/CvMap.h"
#include "Infrastructure/CvPathGenerator.h"
#include "CvPlayerAI.h"
#include "Engine/CvPlot.h"
#include "Engine/CvSelectionGroup.h"
#include "Engine/CvUnit.h"
#include "Cascade/CvEventSpine.h" // #430 logging consolidation: route [CTB] lines through the event spine (shadow)

// ---------------------------------------------------------------------------
// #430 logging: [CTB] contract broker -> event spine (CvContractBroker). Self-registers its prefixes +
// ContractBroker.log; the spine never names CTB. Shadow discipline: these emits run ALONGSIDE the existing
// logContractBroker calls (diff on /events, then cut). Pre-composed CvString/%S lines cannot be field-ized
// under the raw-field model and are LEFT on the legacy path only -- see FLAG comments below.
// NOTE on log() helper: log() itself pre-composes into a stack buffer and passes that as a %s runtime string
// to logContractBroker (line 37). The spine emits are placed at each CALL SITE (the log(...) calls), not
// inside the helper, so they carry the original structured args directly.
namespace
{
	enum CtbEvent
	{
		// [CTB/turn]
		CTB_TURN_CLEANUP = 0,          // [CTB/turn] action=cleanup contractedUnits= advertisingTenders= advertisingUnits=
		CTB_TURN_WORKREQUESTS,         // [CTB/turn] workRequests=
		CTB_TURN_TENDERS_LAST,         // [CTB/turn] tendersPostedLastTurn=
		CTB_TURN_CLEARING,             // [CTB/turn] action=clearing contractedUnits= advertisingTenders= employedUnitsBefore=
		CTB_TURN_FULFILLED_SUMMARY,    // [CTB/turn] fulfilledContractsLastTurn= workRequestsRemaining=

		// [CTB/fulfilled]
		CTB_FULFILLED_CLEANUP,         // [CTB/fulfilled] action=eraseCleanup workRequest= priority= atX= atY= aiType=
		CTB_FULFILLED_ABANDONED,       // [CTB/fulfilled] action=abandonedJoin workRequest= joinUnit=
		CTB_FULFILLED_TENDER_WON,      // [CTB/fulfilled] action=tenderWon workRequest=
		CTB_FULFILLED_ALREADY_BUILT,   // [CTB/fulfilled] action=alreadyBuilding workRequest=
		CTB_FULFILLED_UNIT_SATISFIES,  // [CTB/fulfilled] action=unitSatisfies workRequest= unit=

		// [CTB/avail/dup]
		CTB_AVAIL_DUP_WORKLIST,        // [CTB/avail/dup] unitsRemainingInWorklist=

		// [CTB/avail]
		CTB_AVAIL_UNIT,                // [CTB/avail] unit= type= aiType= minPriority=
		CTB_AVAIL_DETAIL,              // [CTB/avail] unit= worker= healer= offValue= defValue= minPriority=
		CTB_AVAIL_WORKLIST,            // [CTB/avail] unitsRemainingInWorklist=

		// [CTB/remove]
		CTB_REMOVE_BY_ID,              // [CTB/remove] action=byId unit=
		CTB_REMOVE_INTERNAL,           // [CTB/remove] unit= contractedWorkRequest= employedUnits= unitsRemainingInWorklist=

		// [CTB/work]
		CTB_WORK_JOIN_DUPE,            // [CTB/work] action=joinDupe unit= existingWorkRequest=
		CTB_WORK_ADDED,                // [CTB/work] action=added index= priority= atX= atY= aiType= flags= requiredStrx100= maxPath= join=

		// [CTB/work/intransit]
		CTB_INTRANSIT_FULLNOSTR,       // [CTB/work/intransit] action=fullNoStr atX= atY= aiType= group=
		CTB_INTRANSIT_FULLCOVERED,     // [CTB/work/intransit] action=fullCovered atX= atY= aiType= group= groupStr= requiredStr=
		CTB_INTRANSIT_PARTIAL,         // [CTB/work/intransit] action=partial atX= atY= aiType= group= groupStr= requiredStr= priorityBefore=

		// [CTB/outstanding]
		CTB_OUTSTANDING,               // [CTB/outstanding] aiType= atCityOnly= plotX= plotY= outstandingRequests=

		// [CTB/tender/cand]  (numeric-only variant -- city=%d, no city name string)
		CTB_TENDER_CAND_PRIORITY,      // [CTB/tender/cand] action=lowPriority workRequest= tenderIdx= city= cityMinPriority= reqPriority=

		// [CTB/tender/alloc]  (numeric-only variants)
		CTB_TENDER_ALLOC_ALREADY,      // [CTB/tender/alloc] action=alreadyBuilding workRequest= allocKey= allocCount=
		CTB_TENDER_ALLOC_WON,          // [CTB/tender/alloc] action=tenderWon workRequest= allocKey= allocCount=

		// [CTB/finalize]
		CTB_FINALIZE,                  // [CTB/finalize] contractsSatisfied= total= unitsEmployed= unitsWithoutWork=

		// [CTB/match/abandon]
		CTB_MATCH_ABANDON,             // [CTB/match/abandon] workRequest= joinUnit=

		// [CTB/match]
		CTB_MATCH_ASSIGNED,            // [CTB/match] unit= workRequest=
		CTB_MATCH_SATISFIED,           // [CTB/match] workRequest= unit= unitStr= requiredStr=

		// [CTB/match/partial]
		CTB_MATCH_PARTIAL,             // [CTB/match/partial] workRequest= unit= providedStr= remainingStr= newPriority=

		// [CTB/contract/lost]
		CTB_CONTRACT_LOST,             // [CTB/contract/lost] unit= workRequest=

		// [CTB/contract]
		CTB_CONTRACT_DISPATCHED,       // [CTB/contract] unit= atX= atY= priority= aiType= joinUnit= workRequest=
		CTB_CONTRACT_NOMATCH,          // [CTB/contract] action=noMatch unit=
		CTB_CONTRACT_NOTLISTED,        // [CTB/contract] action=notListed unit=

		// [CTB/assess]
		CTB_ASSESS_NEWBEST,            // [CTB/assess] unit= workRequest= iValue= pathTurns=
		CTB_ASSESS_NOPATH,             // [CTB/assess] action=noPath unit= workRequest= targetX= targetY= maxPathTurns= thisPlotOnly=
		CTB_ASSESS_SUITABILITY,        // [CTB/assess] unit= workRequest= iValue= currentBest=
		CTB_ASSESS_CHOSEN,             // [CTB/assess] unit= workRequest= index= bestValue=
		CTB_ASSESS_NONE,               // [CTB/assess] action=none workRequest= advertiserCount=
		CTB_ASSESS_REJECT_GONE,        // [CTB/assess] action=rejectGone unit= workRequest=
		CTB_ASSESS_REJECT_WRONGAI,     // [CTB/assess] action=rejectWrongAI unit= workRequest=
		CTB_ASSESS_REJECT_CRITERIA,    // [CTB/assess] action=rejectCriteria unit= workRequest=
		CTB_ASSESS_REJECT_PRIORITY,    // [CTB/assess] action=rejectPriority unit= workRequest=

		// [CTB/postprocess]
		CTB_POSTPROCESS_BEGIN,         // [CTB/postprocess] advertisingUnits=
		CTB_POSTPROCESS_UNIT,          // [CTB/postprocess] unit= contractedWorkRequest=
		CTB_POSTPROCESS_GONE           // [CTB/postprocess] action=gone unit=
	};

	const char* contractLinePrefix(int iEventId)
	{
		switch (iEventId)
		{
		case CTB_TURN_CLEANUP:          return "[CTB/turn] action=cleanup";
		case CTB_TURN_WORKREQUESTS:     return "[CTB/turn] action=workRequestCount";
		case CTB_TURN_TENDERS_LAST:     return "[CTB/turn] action=tendersLastTurn";
		case CTB_TURN_CLEARING:         return "[CTB/turn] action=clearing";
		case CTB_TURN_FULFILLED_SUMMARY:return "[CTB/turn] action=fulfilledSummary";
		case CTB_FULFILLED_CLEANUP:     return "[CTB/fulfilled] action=eraseCleanup";
		case CTB_FULFILLED_ABANDONED:   return "[CTB/fulfilled] action=abandonedJoin";
		case CTB_FULFILLED_TENDER_WON:  return "[CTB/fulfilled] action=tenderWon";
		case CTB_FULFILLED_ALREADY_BUILT:return "[CTB/fulfilled] action=alreadyBuilding";
		case CTB_FULFILLED_UNIT_SATISFIES:return "[CTB/fulfilled] action=unitSatisfies";
		case CTB_AVAIL_DUP_WORKLIST:    return "[CTB/avail/dup] action=dupWorklist";
		case CTB_AVAIL_UNIT:            return "[CTB/avail] action=advertise";
		case CTB_AVAIL_DETAIL:          return "[CTB/avail] action=detail";
		case CTB_AVAIL_WORKLIST:        return "[CTB/avail] action=worklistCount";
		case CTB_REMOVE_BY_ID:          return "[CTB/remove] action=byId";
		case CTB_REMOVE_INTERNAL:       return "[CTB/remove] action=internal";
		case CTB_WORK_JOIN_DUPE:        return "[CTB/work] action=joinDupe";
		case CTB_WORK_ADDED:            return "[CTB/work] action=added";
		case CTB_INTRANSIT_FULLNOSTR:   return "[CTB/work/intransit] action=fullNoStr";
		case CTB_INTRANSIT_FULLCOVERED: return "[CTB/work/intransit] action=fullCovered";
		case CTB_INTRANSIT_PARTIAL:     return "[CTB/work/intransit] action=partial";
		case CTB_OUTSTANDING:           return "[CTB/outstanding]";
		case CTB_TENDER_CAND_PRIORITY:  return "[CTB/tender/cand] action=lowPriority";
		case CTB_TENDER_ALLOC_ALREADY:  return "[CTB/tender/alloc] action=alreadyBuilding";
		case CTB_TENDER_ALLOC_WON:      return "[CTB/tender/alloc] action=tenderWon";
		case CTB_FINALIZE:              return "[CTB/finalize]";
		case CTB_MATCH_ABANDON:         return "[CTB/match/abandon]";
		case CTB_MATCH_ASSIGNED:        return "[CTB/match] action=assigned";
		case CTB_MATCH_SATISFIED:       return "[CTB/match] action=satisfied";
		case CTB_MATCH_PARTIAL:         return "[CTB/match/partial]";
		case CTB_CONTRACT_LOST:         return "[CTB/contract/lost]";
		case CTB_CONTRACT_DISPATCHED:   return "[CTB/contract] action=dispatched";
		case CTB_CONTRACT_NOMATCH:      return "[CTB/contract] action=noMatch";
		case CTB_CONTRACT_NOTLISTED:    return "[CTB/contract] action=notListed";
		case CTB_ASSESS_NEWBEST:        return "[CTB/assess] action=newBest";
		case CTB_ASSESS_NOPATH:         return "[CTB/assess] action=noPath";
		case CTB_ASSESS_SUITABILITY:    return "[CTB/assess] action=suitability";
		case CTB_ASSESS_CHOSEN:         return "[CTB/assess] action=chosen";
		case CTB_ASSESS_NONE:           return "[CTB/assess] action=none";
		case CTB_ASSESS_REJECT_GONE:    return "[CTB/assess] action=rejectGone";
		case CTB_ASSESS_REJECT_WRONGAI: return "[CTB/assess] action=rejectWrongAI";
		case CTB_ASSESS_REJECT_CRITERIA:return "[CTB/assess] action=rejectCriteria";
		case CTB_ASSESS_REJECT_PRIORITY:return "[CTB/assess] action=rejectPriority";
		case CTB_POSTPROCESS_BEGIN:     return "[CTB/postprocess] action=begin";
		case CTB_POSTPROCESS_UNIT:      return "[CTB/postprocess] action=unit";
		case CTB_POSTPROCESS_GONE:      return "[CTB/postprocess] action=gone";
		default:                        return NULL;
		}
	}

	// CTB's LOCAL field tags (all plain ints unless noted).
	enum CtbField
	{
		CTBF_unit = 0,
		CTBF_workRequest,
		CTBF_contractedUnits,
		CTBF_advertisingTenders,
		CTBF_advertisingUnits,
		CTBF_workRequests,
		CTBF_employedUnitsBefore,
		CTBF_fulfilledContracts,
		CTBF_workRequestsRemaining,
		CTBF_priority,
		CTBF_atX,
		CTBF_atY,
		CTBF_aiType,
		CTBF_unitType,
		CTBF_minPriority,
		CTBF_worker,
		CTBF_healer,
		CTBF_offValue,
		CTBF_defValue,
		CTBF_contractedWorkRequest,
		CTBF_employedUnits,
		CTBF_unitsRemainingInWorklist,
		CTBF_existingWorkRequest,
		CTBF_index,
		CTBF_flags,
		CTBF_requiredStrx100,
		CTBF_maxPath,
		CTBF_join,
		CTBF_group,
		CTBF_groupStr,
		CTBF_requiredStr,
		CTBF_priorityBefore,
		CTBF_atCityOnly,
		CTBF_plotX,
		CTBF_plotY,
		CTBF_outstandingRequests,
		CTBF_tenderIdx,
		CTBF_city,
		CTBF_cityMinPriority,
		CTBF_reqPriority,
		CTBF_allocKey,
		CTBF_allocCount,
		CTBF_total,
		CTBF_unitsEmployed,
		CTBF_unitsWithoutWork,
		CTBF_contractsSatisfied,
		CTBF_joinUnit,
		CTBF_unitStr,
		CTBF_providedStr,
		CTBF_remainingStr,
		CTBF_newPriority,
		CTBF_iValue,
		CTBF_pathTurns,
		CTBF_currentBest,
		CTBF_bestValue,
		CTBF_targetX,
		CTBF_targetY,
		CTBF_maxPathTurns,
		CTBF_thisPlotOnly,
		CTBF_advertiserCount
	};

	const char* contractFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		*peType = SFT_INT;
		switch (iFieldTag)
		{
		case CTBF_unit:                   return "unit";
		case CTBF_workRequest:            return "workRequest";
		case CTBF_contractedUnits:        return "contractedUnits";
		case CTBF_advertisingTenders:     return "advertisingTenders";
		case CTBF_advertisingUnits:       return "advertisingUnits";
		case CTBF_workRequests:           return "workRequests";
		case CTBF_employedUnitsBefore:    return "employedUnitsBefore";
		case CTBF_fulfilledContracts:     return "fulfilledContracts";
		case CTBF_workRequestsRemaining:  return "workRequestsRemaining";
		case CTBF_priority:               return "priority";
		case CTBF_atX:                    return "atX";
		case CTBF_atY:                    return "atY";
		case CTBF_aiType:                 return "aiType";
		case CTBF_unitType:               *peType = SFT_UNIT; return "unitType";
		case CTBF_minPriority:            return "minPriority";
		case CTBF_worker:                 return "worker";
		case CTBF_healer:                 return "healer";
		case CTBF_offValue:               return "offValue";
		case CTBF_defValue:               return "defValue";
		case CTBF_contractedWorkRequest:  return "contractedWorkRequest";
		case CTBF_employedUnits:          return "employedUnits";
		case CTBF_unitsRemainingInWorklist:return "unitsRemainingInWorklist";
		case CTBF_existingWorkRequest:    return "existingWorkRequest";
		case CTBF_index:                  return "index";
		case CTBF_flags:                  return "flags";
		case CTBF_requiredStrx100:        return "requiredStrx100";
		case CTBF_maxPath:                return "maxPath";
		case CTBF_join:                   return "join";
		case CTBF_group:                  return "group";
		case CTBF_groupStr:               return "groupStr";
		case CTBF_requiredStr:            return "requiredStr";
		case CTBF_priorityBefore:         return "priorityBefore";
		case CTBF_atCityOnly:             return "atCityOnly";
		case CTBF_plotX:                  return "plotX";
		case CTBF_plotY:                  return "plotY";
		case CTBF_outstandingRequests:    return "outstandingRequests";
		case CTBF_tenderIdx:              return "tenderIdx";
		case CTBF_city:                   return "city";
		case CTBF_cityMinPriority:        return "cityMinPriority";
		case CTBF_reqPriority:            return "reqPriority";
		case CTBF_allocKey:               return "allocKey";
		case CTBF_allocCount:             return "allocCount";
		case CTBF_total:                  return "total";
		case CTBF_unitsEmployed:          return "unitsEmployed";
		case CTBF_unitsWithoutWork:       return "unitsWithoutWork";
		case CTBF_contractsSatisfied:     return "contractsSatisfied";
		case CTBF_joinUnit:               return "joinUnit";
		case CTBF_unitStr:                return "unitStr";
		case CTBF_providedStr:            return "providedStr";
		case CTBF_remainingStr:           return "remainingStr";
		case CTBF_newPriority:            return "newPriority";
		case CTBF_iValue:                 return "iValue";
		case CTBF_pathTurns:              return "pathTurns";
		case CTBF_currentBest:            return "currentBest";
		case CTBF_bestValue:              return "bestValue";
		case CTBF_targetX:                return "targetX";
		case CTBF_targetY:                return "targetY";
		case CTBF_maxPathTurns:           return "maxPathTurns";
		case CTBF_thisPlotOnly:           return "thisPlotOnly";
		case CTBF_advertiserCount:        return "advertiserCount";
		default:                        return NULL;
		}
	}

	struct ContractLogRegistrar
	{
		ContractLogRegistrar()
		{
			spineRegisterDomain(SD_CONTRACT, &contractLinePrefix, "ContractBroker.log", &contractFieldInfo);
		}
	};
	ContractLogRegistrar s_contractLogRegistrar; // static-init registration (g_domains is zero-init first; safe)
}

CvContractBroker::CvContractBroker() : m_eOwner(NO_PLAYER)
{
	reset();
}

CvContractBroker::~CvContractBroker()
{
}


void CvContractBroker::log(int level, char* format, ...)
{
	static char logString[2048];
	_vsnprintf(logString, 2048 - 4, format, (char*)(&format + 1));

	// Keep the structure consistent with the other tagged AI logs: every line leads
	// with its [CTB/*] tag and carries the player as an owner=%d field (the numeric
	// id, cross-referenceable via GameInfo.log), rather than a "<Name> - " prefix.
	logContractBroker(level, "%s owner=%d", logString, (int)m_eOwner);
}

//	Delete all work requests and looking for work records
void CvContractBroker::reset()
{
	//	No need to lock here as this is (currently) always run in a single-threaded context

	m_workRequests.clear();
	m_advertisingUnits.clear();
	m_advertisingTenders.clear();
	m_contractedUnits.clear();

	m_iNextWorkRequestId = 0;
	m_iEmployedUnits = 0;
}

void CvContractBroker::cleanup()
{
	if (m_eOwner == NO_PLAYER || GET_PLAYER(m_eOwner).getName() == NULL) return;

	m_ownerName = GET_PLAYER(m_eOwner).getName();

	int fulfilledContracts = 0;

	log(1, "[CTB/turn] cleanup contractedUnits=%d advertisingTenders=%d advertisingUnits=%d",
		m_contractedUnits.size(),
		m_advertisingTenders.size(),
		m_advertisingUnits.size());
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_TURN_CLEANUP, 1)
		.addI(CTBF_contractedUnits, (int)m_contractedUnits.size())
		.addI(CTBF_advertisingTenders, (int)m_advertisingTenders.size())
		.addI(CTBF_advertisingUnits, (int)m_advertisingUnits.size()));

	log(1, "[CTB/turn] workRequests=%d", m_workRequests.size());
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_TURN_WORKREQUESTS, 1)
		.addI(CTBF_workRequests, (int)m_workRequests.size()));

	log(1, "[CTB/turn] tendersPostedLastTurn=%d", m_advertisingTenders.size());
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_TURN_TENDERS_LAST, 1)
		.addI(CTBF_advertisingTenders, (int)m_advertisingTenders.size()));


	log(2, "[CTB/turn] clearing contractedUnits=%d advertisingTenders=%d resetting employedUnits (was %d) to 0",
		m_contractedUnits.size(), m_advertisingTenders.size(), m_iEmployedUnits);
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_TURN_CLEARING, 2)
		.addI(CTBF_contractedUnits, (int)m_contractedUnits.size())
		.addI(CTBF_advertisingTenders, (int)m_advertisingTenders.size())
		.addI(CTBF_employedUnitsBefore, m_iEmployedUnits));

	m_contractedUnits.clear();
	m_advertisingTenders.clear();
	m_iEmployedUnits = 0;
	for (unsigned int iI = 0; iI < (int)m_workRequests.size(); iI++)
	{
		if (m_workRequests[iI].bFulfilled)
		{
			workRequest* wr = &m_workRequests[iI];

			log(2, "[CTB/fulfilled] erasing fulfilled workRequest=%d priority=%d at=(%d,%d) aiType=%d (cleanup)",
				wr->iWorkRequestId, wr->iPriority, wr->iAtX, wr->iAtY, (int)wr->eAIType);
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_FULFILLED_CLEANUP, 2)
				.addI(CTBF_workRequest, wr->iWorkRequestId)
				.addI(CTBF_priority, wr->iPriority)
				.addI(CTBF_atX, wr->iAtX)
				.addI(CTBF_atY, wr->iAtY)
				.addI(CTBF_aiType, (int)wr->eAIType));

			m_workRequests.erase(wr);
			fulfilledContracts++;
		}

	}
	log(1, "[CTB/turn] fulfilledContractsLastTurn=%d workRequestsRemaining=%d", fulfilledContracts, m_workRequests.size());
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_TURN_FULFILLED_SUMMARY, 1)
		.addI(CTBF_fulfilledContracts, fulfilledContracts)
		.addI(CTBF_workRequestsRemaining, (int)m_workRequests.size()));


}

//	Initialize
void CvContractBroker::init(PlayerTypes eOwner)
{
	m_eOwner = eOwner;
}


bool CvContractBroker::alreadyLookingForWork(const CvUnit* pUnit)
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < (int)m_advertisingUnits.size(); iI++)
	{
		if (m_advertisingUnits[iI].iUnitId == pUnit->getID())
		{

			log(1,
				"[CTB/avail/dup] unit=%S (%d) at=(%d,%d) already in worklist",
				pUnit->getDescription().GetCString(),
				pUnit->getID(),
				pUnit->getX(),
				pUnit->getY());
			// FLAG: pre-composed wide-string (%S unit description) -- left on legacy only

			log(1,
				"[CTB/avail/dup] unitsRemainingInWorklist=%d",
				m_advertisingUnits.size()
			);
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_AVAIL_DUP_WORKLIST, 1)
				.addI(CTBF_unitsRemainingInWorklist, (int)m_advertisingUnits.size()));
			return true;
		}
	}
	return false;
}
//	Note a unit looking for work
void CvContractBroker::lookingForWork(const CvUnit* pUnit, int iMinPriority)
{
	PROFILE_FUNC();

	if (alreadyLookingForWork(pUnit))
	{
		return;
	}

	// [CTB/avail] -- a unit advertises itself to the broker as available for work.
	log(2, "[CTB/avail] unit=%d type=%d aiType=%d minPriority=%d", pUnit->getID(), (int)pUnit->getUnitType(), (int)pUnit->AI_getUnitAIType(), iMinPriority);
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_AVAIL_UNIT, 2)
		.addI(CTBF_unit, pUnit->getID())
		.addI(CTBF_unitType, (int)pUnit->getUnitType())
		.addI(CTBF_aiType, (int)pUnit->AI_getUnitAIType())
		.addI(CTBF_minPriority, iMinPriority));

	advertisingUnit	unitDetails;
	unitDetails.eUnitType = pUnit->getUnitType();

	const int iUnitStr = GC.getGame().AI_combatValue(pUnit->getUnitType());

	unitDetails.bIsWorker = (pUnit->AI_getUnitAIType() == UNITAI_WORKER);
	unitDetails.bIsHealer = (pUnit->AI_getUnitAIType() == UNITAI_HEALER);

	//	Combat values are just the crude value of the unit type for now - should add in promotions
	//	here for sure
	if (pUnit->canDefend())
	{
		unitDetails.iDefensiveValue = iUnitStr;
	}
	//TB OOS Debug: Almost certainly a solid fix here
	else
	{
		unitDetails.iDefensiveValue = 0;
	}
	if (pUnit->canAttack())
	{
		unitDetails.iOffensiveValue = iUnitStr;
	}
	else
	{
		unitDetails.iOffensiveValue = 0;
	}

	unitDetails.iUnitId = pUnit->getID();
	unitDetails.iAtX = pUnit->getX();
	unitDetails.iAtY = pUnit->getY();
	unitDetails.iMinPriority = iMinPriority;

	//	Initially not assigned to a contract
	unitDetails.iContractedWorkRequest = -1;
	//	and no attempt has been made yet to match any work requests
	unitDetails.iMatchedToRequestSeqThisPlot = -1;
	unitDetails.iMatchedToRequestSeqAnyPlot = -1;

	m_advertisingUnits.push_back(unitDetails);

	log(1,
	"[CTB/avail] unit=%S (%d) at=(%d,%d) asking for work",
	pUnit->getDescription().GetCString(),
	pUnit->getID(),
	pUnit->getX(),
	pUnit->getY());
	// FLAG: pre-composed wide-string (%S unit description) -- left on legacy only

	log(2,
	"[CTB/avail] unit=%d worker=%d healer=%d offValue=%d defValue=%d minPriority=%d",
	unitDetails.iUnitId,
	unitDetails.bIsWorker ? 1 : 0,
	unitDetails.bIsHealer ? 1 : 0,
	unitDetails.iOffensiveValue,
	unitDetails.iDefensiveValue,
	unitDetails.iMinPriority);
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_AVAIL_DETAIL, 2)
		.addI(CTBF_unit, unitDetails.iUnitId)
		.addI(CTBF_worker, unitDetails.bIsWorker ? 1 : 0)
		.addI(CTBF_healer, unitDetails.bIsHealer ? 1 : 0)
		.addI(CTBF_offValue, unitDetails.iOffensiveValue)
		.addI(CTBF_defValue, unitDetails.iDefensiveValue)
		.addI(CTBF_minPriority, unitDetails.iMinPriority));

	log(1,
	"[CTB/avail] unitsRemainingInWorklist=%d",
	m_advertisingUnits.size()
	);
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_AVAIL_WORKLIST, 1)
		.addI(CTBF_unitsRemainingInWorklist, (int)m_advertisingUnits.size()));

}

//	Unit fulfilled its work and is no longer advertising as available
void CvContractBroker::removeUnit(const CvUnit* pUnit)
{
	PROFILE_EXTRA_FUNC();

	internalRemoveUnit(pUnit->getID());

	log(1,
		"[CTB/remove] unit=%S (%d) at=(%d,%d) removed from worklist",
		pUnit->getDescription().GetCString(),
		pUnit->getID(),
		pUnit->getX(),
		pUnit->getY());
	// FLAG: pre-composed wide-string (%S unit description) -- left on legacy only


}

void CvContractBroker::removeUnit(const int iUnitId)
{
	PROFILE_EXTRA_FUNC();

	internalRemoveUnit(iUnitId);

	log(1, "[CTB/remove] unit=%d removed from worklist", iUnitId);
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_REMOVE_BY_ID, 1)
		.addI(CTBF_unit, iUnitId));
}



// Make a work request
//	iPriority should be in the range 0-100 ideally
//	eUnitFlags indicate the type(s) of unit sought
//	(iAtX,iAtY) is (roughly) where the work will be
//	pJoinUnit may be NULL but if not it is a request to join that unit's group
void CvContractBroker::advertiseWork(int iPriority, unitCapabilities eUnitFlags, int iAtX, int iAtY, const CvUnit* pJoinUnit, UnitAITypes eAIType, int iUnitStrength, const CvUnitSelectionCriteria* criteria, int iMaxPath)
{
	PROFILE_FUNC();

	int iUnitStrengthTimes100 = (iUnitStrength == -1 ? -1 : iUnitStrength * 100);
	if (pJoinUnit != NULL)
	{
		workRequest* wr = findWorkRequestByUnitId(pJoinUnit->getID());
		if (wr != NULL)
		{
			log(2, "[CTB/work] join request for unit=%d ignored - already an outstanding request (workRequest=%d) for that unit",
				pJoinUnit->getID(), wr->iWorkRequestId);
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_WORK_JOIN_DUPE, 2)
				.addI(CTBF_unit, pJoinUnit->getID())
				.addI(CTBF_existingWorkRequest, wr->iWorkRequestId));
			return;
		}
	}

	// [CTB/work] -- a job is advertised to the broker (a need for a unit).
	if (gPlayerLogLevel >= 2)
	{
		CvString szWorkCriteria = (criteria != NULL ? criteria->getDescription() : CvString(""));
		if (!szWorkCriteria.empty())
		{
			szWorkCriteria.Format(" criteria=(%s)", szWorkCriteria.c_str());
		}
		log(2, "[CTB/work] priority=%d at=(%d,%d) aiType=%d flags=0x%x strength=%d(x100=%d) maxPath=%d join=%d%s",
			iPriority, iAtX, iAtY, (int)eAIType, (int)eUnitFlags, iUnitStrength, iUnitStrengthTimes100,
			iMaxPath, pJoinUnit ? pJoinUnit->getID() : -1, szWorkCriteria.c_str());
		// FLAG: runtime CvString (%s szWorkCriteria) -- left on legacy only
	}

	//	First check that there are not already units on the way to meet this need
	//	else concurrent builds will get queued while they are in transit
	foreach_(const CvSelectionGroup * pLoopSelectionGroup, GET_PLAYER(m_eOwner).groups())
	{
		const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

		if (pMissionPlot == GC.getMap().plot(iAtX, iAtY) && !pLoopSelectionGroup->atPlot(pMissionPlot)
		&& pLoopSelectionGroup->AI_getMissionAIType() == (pJoinUnit == NULL ? MISSIONAI_CONTRACT : MISSIONAI_CONTRACT_UNIT)
		//	Allow for the last unit having died so that this group is about to vanish
		&& pLoopSelectionGroup->getNumUnits() > 0
		&& (eAIType == NO_UNITAI || pLoopSelectionGroup->getHeadUnitAI() == eAIType)
		&& pLoopSelectionGroup->meetsUnitSelectionCriteria(criteria))
		{
			std::map<int, bool>::const_iterator itr = m_contractedUnits.find(pLoopSelectionGroup->getID());

			if (itr == m_contractedUnits.end())
			{
				m_contractedUnits[pLoopSelectionGroup->getID()] = true;
				if (gUnitLogLevel > 2)
				{
					log(1,
						"[CTB/work/intransit] unit=%S (%d) at=(%d,%d) already responding to contract at=(%d,%d)",
						pLoopSelectionGroup->getHeadUnit()->getDescription().GetCString(),
						pLoopSelectionGroup->getHeadUnit()->getID(),
						pLoopSelectionGroup->getX(),
						pLoopSelectionGroup->getY(),
						iAtX, iAtY
					);
					// FLAG: pre-composed wide-string (%S unit description) -- left on legacy only
				}

				if (iUnitStrengthTimes100 == -1)
				{
					// Request already handled by existing mission
					log(2, "[CTB/work/intransit] request at=(%d,%d) aiType=%d fully handled by in-transit group=%d (no strength requirement) - not adding",
						iAtX, iAtY, (int)eAIType, pLoopSelectionGroup->getID());
					eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_INTRANSIT_FULLNOSTR, 2)
						.addI(CTBF_atX, iAtX)
						.addI(CTBF_atY, iAtY)
						.addI(CTBF_aiType, (int)eAIType)
						.addI(CTBF_group, pLoopSelectionGroup->getID()));
					return;
				}
				const int iMissionGroupStrengthTimes100 = pLoopSelectionGroup->AI_getGenericValueTimes100(unitCapabilities2UnitValueFlags(eUnitFlags));
				if (iMissionGroupStrengthTimes100 >= iUnitStrengthTimes100)
				{
					// Request is entirely fulfilled by existing mission
					log(2, "[CTB/work/intransit] request at=(%d,%d) aiType=%d fully covered by in-transit group=%d (groupStr=%d >= requiredStr=%d) - not adding",
						iAtX, iAtY, (int)eAIType, pLoopSelectionGroup->getID(), iMissionGroupStrengthTimes100, iUnitStrengthTimes100);
					eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_INTRANSIT_FULLCOVERED, 2)
						.addI(CTBF_atX, iAtX)
						.addI(CTBF_atY, iAtY)
						.addI(CTBF_aiType, (int)eAIType)
						.addI(CTBF_group, pLoopSelectionGroup->getID())
						.addI(CTBF_groupStr, iMissionGroupStrengthTimes100)
						.addI(CTBF_requiredStr, iUnitStrengthTimes100));
					return;
				}
				//	It's partially fulfilled so lower the priority of the remainder
				log(2, "[CTB/work/intransit] request at=(%d,%d) aiType=%d partially covered by in-transit group=%d (groupStr=%d < requiredStr=%d) - lowering priority %d and reducing remaining strength",
					iAtX, iAtY, (int)eAIType, pLoopSelectionGroup->getID(), iMissionGroupStrengthTimes100, iUnitStrengthTimes100, iPriority);
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_INTRANSIT_PARTIAL, 2)
					.addI(CTBF_atX, iAtX)
					.addI(CTBF_atY, iAtY)
					.addI(CTBF_aiType, (int)eAIType)
					.addI(CTBF_group, pLoopSelectionGroup->getID())
					.addI(CTBF_groupStr, iMissionGroupStrengthTimes100)
					.addI(CTBF_requiredStr, iUnitStrengthTimes100)
					.addI(CTBF_priorityBefore, iPriority));
				iPriority = lowerPartiallyFulfilledRequestPriority(iPriority, iUnitStrengthTimes100, iMissionGroupStrengthTimes100);
				iUnitStrengthTimes100 -= iMissionGroupStrengthTimes100;
			}
		}
	}
	workRequest newRequest;
	newRequest.iPriority = iPriority;
	newRequest.eUnitFlags = eUnitFlags;
	newRequest.eAIType = eAIType;
	newRequest.iAtX = iAtX;
	newRequest.iAtY = iAtY;
	newRequest.iMaxPath = iMaxPath;
	newRequest.iWorkRequestId = ++m_iNextWorkRequestId;
	newRequest.bFulfilled = false;
	newRequest.iRequiredStrengthTimes100 = iUnitStrengthTimes100;
	if (criteria != NULL)
	{
		newRequest.criteria = *criteria;
	}

	OutputDebugString(CvString::format("Adding new work request, index %d with priority %d\n", newRequest.iWorkRequestId, iPriority).c_str());
	log(1, "[CTB/work] added request index=%d priority=%d at=(%d,%d) aiType=%d flags=0x%x requiredStrx100=%d maxPath=%d join=%d",
		newRequest.iWorkRequestId, iPriority, iAtX, iAtY, (int)eAIType, (int)eUnitFlags,
		iUnitStrengthTimes100, iMaxPath, pJoinUnit ? pJoinUnit->getID() : -1);
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_WORK_ADDED, 1)
		.addI(CTBF_index, newRequest.iWorkRequestId)
		.addI(CTBF_priority, iPriority)
		.addI(CTBF_atX, iAtX)
		.addI(CTBF_atY, iAtY)
		.addI(CTBF_aiType, (int)eAIType)
		.addI(CTBF_flags, (int)eUnitFlags)
		.addI(CTBF_requiredStrx100, iUnitStrengthTimes100)
		.addI(CTBF_maxPath, iMaxPath)
		.addI(CTBF_join, pJoinUnit ? pJoinUnit->getID() : -1));

	if (pJoinUnit == NULL)
	{
		newRequest.iUnitId = -1;
	}
	else
	{
		newRequest.iUnitId = pJoinUnit->getID();
	}

	//	Insert in priority order, highest first
	std::vector<workRequest>::iterator insertAt;

	for (insertAt = m_workRequests.begin(); insertAt != m_workRequests.end(); ++insertAt)
	{
		if (iPriority > (*insertAt).iPriority)
		{
			break;
		}
	}
	m_workRequests.insert(insertAt, newRequest);
}

//	Advertise a tender to build units
//		iMinPriority indicates the lowest priority request this tender is appropriate for
void CvContractBroker::advertiseTender(const CvCity* pCity, int iMinPriority)
{
	PROFILE_FUNC();

	int iNumTenders = 1; // par d�faut

	iNumTenders = 1 + (pCity->getPopulation() / 10) + (pCity->getYieldRate(YIELD_PRODUCTION) / 100);
	if (pCity->isCapital())
	{
		iNumTenders+=2;
	}

	
	iNumTenders = std::min(iNumTenders, 4); // max 6

	log(1, "[CTB/tender] city=%S (%d) advertising %d tender slot(s) for unit builds minPriority=%d (pop=%d prod=%d capital=%d)",
		pCity->getName().GetCString(), pCity->getID(), iNumTenders, iMinPriority,
		pCity->getPopulation(), pCity->getYieldRate(YIELD_PRODUCTION), pCity->isCapital() ? 1 : 0);
	// FLAG: pre-composed wide-string (%S city name) -- left on legacy only

	for (int i = 0; i < iNumTenders; i++)
	{
		cityTender newTender;
		newTender.iMinPriority = iMinPriority;
		newTender.iCityId = pCity->getID();
		m_advertisingTenders.push_back(newTender);

		log(3, "[CTB/tender] city=%S (%d) tenders (slot %d/%d) for unit builds minPriority=%d totalTendersNow=%d",
			pCity->getName().GetCString(), pCity->getID(), i + 1, iNumTenders, iMinPriority, m_advertisingTenders.size());
		// FLAG: pre-composed wide-string (%S city name) -- left on legacy only
	}

}

//	Find out how many requests have already been made for units of a specified AI type
//	This is used by cities requesting globally needed units like settlers to avoid multiple
//	tenders all occurring at once
//	Find out how many requests have already been made for units of a specified AI type
//	This is used by cities requesting globally needed units like settlers to avoid multiple
//	tenders all occurring at once
int CvContractBroker::numRequestsOutstanding(UnitAITypes eUnitAI, bool bAtCityOnly, const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	for (int iI = 0; iI < (int)m_workRequests.size(); iI++)
	{
		if (!m_workRequests[iI].bFulfilled)
		{
			if (m_workRequests[iI].eAIType == eUnitAI)
			{
				const CvPlot* pDestPlot = GC.getMap().plot(m_workRequests[iI].iAtX, m_workRequests[iI].iAtY);
				const CvCity* targetCity = pDestPlot->getPlotCity();

				if (!bAtCityOnly || (targetCity != NULL && targetCity->getOwner() == m_eOwner))
				{
					if (pPlot == NULL || pPlot == pDestPlot)
					{
						iCount++;
					}
				}
			}
		}
	}

	const_cast<CvContractBroker*>(this)->log(3, "[CTB/outstanding] aiType=%d atCityOnly=%d plot=(%d,%d) outstandingRequests=%d",
		(int)eUnitAI, bAtCityOnly ? 1 : 0,
		pPlot ? pPlot->getX() : -1, pPlot ? pPlot->getY() : -1, iCount);
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_OUTSTANDING, 3)
		.addI(CTBF_aiType, (int)eUnitAI)
		.addI(CTBF_atCityOnly, bAtCityOnly ? 1 : 0)
		.addI(CTBF_plotX, pPlot ? pPlot->getX() : -1)
		.addI(CTBF_plotY, pPlot ? pPlot->getY() : -1)
		.addI(CTBF_outstandingRequests, iCount));

	return iCount;
}

void CvContractBroker::finalizeTenderContracts()
{
	PROFILE_FUNC();

	std::map<int, int> tenderAllocations;

	for (int iI = 0; iI < (int)m_workRequests.size(); iI++)
	{
		if (!m_workRequests[iI].bFulfilled)
		{
			int iBestValue = 0;
			int iBestCityTenderKey = 0;
			UnitTypes eBestUnit = NO_UNIT;
			UnitAITypes eBestAIType = NO_UNITAI;
			CvCity* pBestCity = NULL;

			const CvUnit* pTargetUnit = findUnit(m_workRequests[iI].iUnitId);
			CvPlot* pDestPlot;

			if (pTargetUnit != NULL)
			{
				pDestPlot = pTargetUnit->plot();
			}
			else
			{
				pDestPlot = GC.getMap().plot(m_workRequests[iI].iAtX, m_workRequests[iI].iAtY);
			}

			if (gPlayerLogLevel >= 2)
			{
				CvString unitAIType = m_workRequests[iI].eAIType == NO_UNITAI ? "NO_UNITAI" : GC.getUnitAIInfo(m_workRequests[iI].eAIType).getType();

				CvString szCriteriaDescription = m_workRequests[iI].criteria.getDescription();

				if (!szCriteriaDescription.empty())
				{
					szCriteriaDescription.Format(" (%s)", szCriteriaDescription.c_str());
				}
				log(2,
					"[CTB/tender/bids] processing bids workRequest=%d unitAI=%s at=(%d,%d) priority=%d flags=0x%x advertisingTenders=%d%s",
					m_workRequests[iI].iWorkRequestId,
					unitAIType.c_str(),
					m_workRequests[iI].iAtX, m_workRequests[iI].iAtY,
					m_workRequests[iI].iPriority,
					(int)m_workRequests[iI].eUnitFlags,
					m_advertisingTenders.size(),
					szCriteriaDescription.c_str()
				);
				// FLAG: runtime CvString (%s unitAIType and %s szCriteriaDescription) -- left on legacy only
			}

			for (unsigned int iJ = 0; iJ < m_advertisingTenders.size(); iJ++)
			{
				if (m_advertisingTenders[iJ].iMinPriority <= m_workRequests[iI].iPriority)
				{
					CvCity* pCity = GET_PLAYER(m_eOwner).getCity(m_advertisingTenders[iJ].iCityId);

					if (pCity != NULL && pDestPlot != NULL
					&& (pCity->area() == pDestPlot->area() || pDestPlot->getPlotCity() != NULL && pCity->waterArea() == pDestPlot->getPlotCity()->waterArea()))
					{
						log(3, "[CTB/tender/cand] workRequest=%d tender[%d] city=%S (%d) considering (cityMinPriority=%d <= reqPriority=%d, area ok)",
							m_workRequests[iI].iWorkRequestId, iJ,
							pCity->getName().GetCString(), pCity->getID(),
							m_advertisingTenders[iJ].iMinPriority, m_workRequests[iI].iPriority);
						// FLAG: pre-composed wide-string (%S city name) -- left on legacy only
						int	iTendersAlreadyInProcess = pCity->numQueuedUnits(m_workRequests[iI].eAIType, pTargetUnit == NULL ? pDestPlot : NULL);
						int iTenderAllocationKey = 0;

						CvChecksum xSum;

						xSum.add(pCity->getID());

						if (pTargetUnit != NULL)
						{
							// Units move around, so can't use destination plot
							xSum.add(pTargetUnit->getID());
						}
						else
						{
							xSum.add(GC.getMap().plotNum(m_workRequests[iI].iAtX, m_workRequests[iI].iAtY));
						}
						xSum.add((int)m_workRequests[iI].eAIType);

						iTenderAllocationKey = xSum.get();

						std::map<int, int>::const_iterator itr = tenderAllocations.find(iTenderAllocationKey);
						if (itr != tenderAllocations.end())
						{
							iTendersAlreadyInProcess -= itr->second;
						}
						else
						{
							tenderAllocations[iTenderAllocationKey] = 0;
							log(3, "[CTB/tender/alloc] workRequest=%d city=%S (%d) allocKey=%d initialized to 0",
								m_workRequests[iI].iWorkRequestId, pCity->getName().GetCString(), pCity->getID(), iTenderAllocationKey);
							// FLAG: pre-composed wide-string (%S city name) -- left on legacy only
						}

						FASSERT_NOT_NEGATIVE(iTendersAlreadyInProcess);

						log(3, "[CTB/tender/cand] workRequest=%d city=%S (%d) allocKey=%d tendersAlreadyInProcess=%d",
							m_workRequests[iI].iWorkRequestId, pCity->getName().GetCString(), pCity->getID(),
							iTenderAllocationKey, iTendersAlreadyInProcess);
						// FLAG: pre-composed wide-string (%S city name) -- left on legacy only

						if (iTendersAlreadyInProcess <= 0)
						{
							int iValue = 0;
							UnitTypes eUnit = NO_UNIT;
							UnitAITypes eAIType = NO_UNITAI;

							if (m_workRequests[iI].eAIType == NO_UNITAI)
							{
								UnitAITypes* pUnitAIs = NULL;
								int iNumAIs = -1;

								if ((m_workRequests[iI].eUnitFlags & DEFENSIVE_UNITCAPABILITIES) != 0)
								{
									static UnitAITypes defensiveAIs[] = { UNITAI_CITY_DEFENSE, UNITAI_ATTACK, UNITAI_CITY_COUNTER, UNITAI_COUNTER };
									pUnitAIs = defensiveAIs;
									iNumAIs = sizeof(defensiveAIs) / sizeof(UnitAITypes);
								}
								if ((m_workRequests[iI].eUnitFlags & OFFENSIVE_UNITCAPABILITIES) != 0)
								{
									static UnitAITypes offensiveAIs[] = { UNITAI_ATTACK, UNITAI_ATTACK_CITY, UNITAI_COUNTER };
									pUnitAIs = offensiveAIs;
									iNumAIs = sizeof(offensiveAIs) / sizeof(UnitAITypes);
								}
								if ((m_workRequests[iI].eUnitFlags & WORKER_UNITCAPABILITIES) != 0)
								{
									static UnitAITypes workerAIs[] = { UNITAI_WORKER };
									pUnitAIs = workerAIs;
									iNumAIs = sizeof(workerAIs) / sizeof(UnitAITypes);
								}
								if ((m_workRequests[iI].eUnitFlags & HEALER_UNITCAPABILITIES) != 0)
								{
									static UnitAITypes workerAIs[] = { UNITAI_HEALER };
									pUnitAIs = workerAIs;
									iNumAIs = sizeof(workerAIs) / sizeof(UnitAITypes);
								}
								eUnit = pCity->AI_bestUnit(iValue, iNumAIs, pUnitAIs, false, &eAIType, false, true, &m_workRequests[iI].criteria);
							}
							else
							{
								eAIType = m_workRequests[iI].eAIType;

								if (pCity->area() != pDestPlot->area() && !IS_NAVAL_AITYPE(eAIType) && !IS_AIR_AITYPE(eAIType))
								{
									log(4, "[CTB/tender/cand] workRequest=%d city=%S (%d) skipped (land unit aiType=%d cannot cross from city area to dest area)",
										m_workRequests[iI].iWorkRequestId, pCity->getName().GetCString(), pCity->getID(), (int)eAIType);
								// FLAG: pre-composed wide-string (%S city name) -- left on legacy only
									continue;
								}
								eUnit = pCity->AI_bestUnitAI(eAIType, iValue, false, false, &m_workRequests[iI].criteria);
							}
							if (eUnit != NO_UNIT)
							{
								int iBaseValue = iValue;

								// Adjust value for production time and distance
								const int iTurns =
									(
										pCity->isProduction() && pCity->getOrderData(0).eOrderType == ORDER_TRAIN
										?
										pCity->getTotalProductionQueueTurnsLeft() + pCity->getProductionTurnsLeft(eUnit, 1)
										:
										pCity->getProductionTurnsLeft(eUnit, 1)
									);
								iValue *= GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent();
								iValue /= iTurns;

								//if the nb of turns is too high for a small city, don't consider the unit, unless there are no other options
								if (iTurns > 20 && pCity->getPopulation() < 5 && m_advertisingTenders.size() > 3)
								{
									log(4,
										"[CTB/tender/giveup] workRequest=%d city=%S (%d) unit=%S baseValue=%d turns=%d depreciatedValue=%d (too slow for small city) - skipped",
										m_workRequests[iI].iWorkRequestId,
										pCity->getName().GetCString(),
										pCity->getID(),
										GC.getUnitInfo(eUnit).getDescription(),
										iBaseValue,
										iTurns,
										iValue
									);
								// FLAG: pre-composed wide-string (%S city name, %S unit description) -- left on legacy only
									continue;
								}

								// generate the path to the destination, if it is too long the value of the unit drops. If no path, then can't supply the unit
								if (CvSelectionGroup::getPathGenerator()->generatePathForHypotheticalUnit(pCity->plot(), pDestPlot, m_eOwner, eUnit, MOVE_NO_ENEMY_TERRITORY, m_workRequests[iI].iMaxPath))
								{
									const int iDistance = CvSelectionGroup::getPathGenerator()->getLastPath().length();
									iValue /= (1 + intSqrt(iDistance));

									log(3,
										"[CTB/tender/bid] workRequest=%d city=%S (%d) unit=%S aiType=%d baseValue=%d turns=%d distance=%d depreciatedValue=%d (prevBest=%d)",
										m_workRequests[iI].iWorkRequestId,
										pCity->getName().GetCString(),
										pCity->getID(),
										GC.getUnitInfo(eUnit).getDescription(),
										(int)eAIType,
										iBaseValue,
										iTurns,
										iDistance,
										iValue,
										iBestValue
									);
									// FLAG: pre-composed wide-string (%S city name, %S unit description) -- left on legacy only

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										iBestCityTenderKey = iTenderAllocationKey;
										eBestUnit = eUnit;
										eBestAIType = eAIType;
										pBestCity = pCity;

										log(3, "[CTB/tender/bid] workRequest=%d city=%S (%d) is new best bid value=%d unit=%S",
											m_workRequests[iI].iWorkRequestId, pCity->getName().GetCString(), pCity->getID(),
											iValue, GC.getUnitInfo(eUnit).getDescription());
										// FLAG: pre-composed wide-string (%S city name, %S unit description) -- left on legacy only
									}
								}
								else
								{
									log(4, "[CTB/tender/cand] workRequest=%d city=%S (%d) unit=%S has no usable path to dest (maxPath=%d) - cannot supply",
										m_workRequests[iI].iWorkRequestId, pCity->getName().GetCString(), pCity->getID(),
										GC.getUnitInfo(eUnit).getDescription(), m_workRequests[iI].iMaxPath);
									// FLAG: pre-composed wide-string (%S city name, %S unit description) -- left on legacy only
								}
							}
							else
							{
								log(4, "[CTB/tender/nosuit] workRequest=%d city=%S (%d) has no suitable unit to offer for aiType=%d",
									m_workRequests[iI].iWorkRequestId, pCity->getName().GetCString(), pCity->getID(), (int)m_workRequests[iI].eAIType);
								// FLAG: pre-composed wide-string (%S city name) -- left on legacy only
							}
						}
						else // Already being built
						{
							m_workRequests[iI].bFulfilled = true;
							eBestUnit = NO_UNIT;

							tenderAllocations[iTenderAllocationKey] += 1;

							log(2, "[CTB/tender/already] city=%S (%d) is already building a unit for this request (tendersInProcess=%d)",
								pCity->getName().GetCString(), pCity->getID(), iTendersAlreadyInProcess);
							// FLAG: pre-composed wide-string (%S city name) -- left on legacy only
							log(2, "[CTB/fulfilled] workRequest=%d marked fulfilled - city=%S (%d) already building required unit",
								m_workRequests[iI].iWorkRequestId, pCity->getName().GetCString(), pCity->getID());
							// FLAG: pre-composed wide-string (%S city name) -- left on legacy only
							log(3, "[CTB/tender/alloc] workRequest=%d allocKey=%d incremented to %d (already-building)",
								m_workRequests[iI].iWorkRequestId, iTenderAllocationKey, tenderAllocations[iTenderAllocationKey]);
							eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_FULFILLED_ALREADY_BUILT, 2)
								.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId));
							eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_TENDER_ALLOC_ALREADY, 3)
								.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId)
								.addI(CTBF_allocKey, iTenderAllocationKey)
								.addI(CTBF_allocCount, tenderAllocations[iTenderAllocationKey]));
							break;
						}
					}
					else if (gPlayerLogLevel >= 4)
					{
						CvCity* pSkipCity = GET_PLAYER(m_eOwner).getCity(m_advertisingTenders[iJ].iCityId);
						log(4, "[CTB/tender/cand] workRequest=%d tender[%d] city=%S (%d) skipped (city=%s destPlot=%s wrong-area/null)",
							m_workRequests[iI].iWorkRequestId, iJ,
							pSkipCity ? pSkipCity->getName().GetCString() : L"<null>",
							m_advertisingTenders[iJ].iCityId,
							pSkipCity ? "ok" : "null",
							pDestPlot ? "ok" : "null");
						// FLAG: pre-composed wide-string (%S city name) and runtime %s strings ("ok"/"null") -- left on legacy only
					}
				}
				else
				{
					log(4, "[CTB/tender/cand] workRequest=%d tender[%d] city=%d skipped (cityMinPriority=%d > reqPriority=%d)",
						m_workRequests[iI].iWorkRequestId, iJ, m_advertisingTenders[iJ].iCityId,
						m_advertisingTenders[iJ].iMinPriority, m_workRequests[iI].iPriority);
					eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_TENDER_CAND_PRIORITY, 4)
						.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId)
						.addI(CTBF_tenderIdx, (int)iJ)
						.addI(CTBF_city, m_advertisingTenders[iJ].iCityId)
						.addI(CTBF_cityMinPriority, m_advertisingTenders[iJ].iMinPriority)
						.addI(CTBF_reqPriority, m_workRequests[iI].iPriority));
				}
			}

			if (eBestUnit != NO_UNIT)
			{
				if (gPlayerLogLevel >= 1)
				{
					CvString unitAIType = m_workRequests[iI].eAIType == NO_UNITAI ? "NO_UNITAI" : GC.getUnitAIInfo(m_workRequests[iI].eAIType).getType();

					if (pBestCity != NULL)
					{
						log(1, "[CTB/tender/win] workRequest=%d city=%S (%d) wins business for unitAI build %s (training %S) value=%d",
								m_workRequests[iI].iWorkRequestId,
								pBestCity->getName().GetCString(),
								pBestCity->getID(),
								unitAIType.c_str(),
								GC.getUnitInfo(eBestUnit).getDescription(),
								iBestValue);
					// FLAG: pre-composed wide-string (%S city name, %S unit description) and runtime CvString (%s unitAIType) -- left on legacy only
					}
					else
					{
						log(1, "[CTB/tender/none] workRequest=%d problem - no city wins business for unitAI build %s (training %S)",
								m_workRequests[iI].iWorkRequestId,
								unitAIType.c_str(),
								GC.getUnitInfo(eBestUnit).getDescription());
					// FLAG: runtime CvString (%s unitAIType) and pre-composed wide-string (%S unit description) -- left on legacy only
					}
				}
				m_workRequests[iI].bFulfilled = true;
				tenderAllocations[iBestCityTenderKey] += 1;

				log(2, "[CTB/fulfilled] workRequest=%d marked fulfilled - tender won, city will build the unit", m_workRequests[iI].iWorkRequestId);
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_FULFILLED_TENDER_WON, 2)
					.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId));
				log(3, "[CTB/tender/alloc] workRequest=%d allocKey=%d incremented to %d (tender won)",
					m_workRequests[iI].iWorkRequestId, iBestCityTenderKey, tenderAllocations[iBestCityTenderKey]);
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_TENDER_ALLOC_WON, 3)
					.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId)
					.addI(CTBF_allocKey, iBestCityTenderKey)
					.addI(CTBF_allocCount, tenderAllocations[iBestCityTenderKey]));

				// Queue up the build. Add to queue head if the current build is not a unit,
				//	implies a local build below the priority of work the city tendered for.
				const bool bDanger = pBestCity->AI_isDanger();
				const int iHammerCostPercent = GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent();
				//if a production is nearly finished, don't insert a unit, add it to the queue.
				const int iMaxTurntoLeave = (bDanger && pBestCity->getProductionUnit() == NO_UNIT ? 1 + GC.getGame().getGameSpeedType() / 4 : 1 + iHammerCostPercent / 50);
				const bool bNearlyFinished = (pBestCity->getProductionTurnsLeft() <= iMaxTurntoLeave || (pBestCity->getProductionTurnsLeft()+3) <= pBestCity->getProductionTurnsLeft(eBestUnit, 1));
				bool bAppend = (pBestCity->isProduction() && pBestCity->getOrderData(0).eOrderType == ORDER_TRAIN) || bNearlyFinished;

				pBestCity->pushOrder(
					ORDER_TRAIN,
					eBestUnit, eBestAIType,
					false, !bAppend, bAppend, false,
					pDestPlot, m_workRequests[iI].eAIType,
					(m_workRequests[iI].iUnitId == -1 ? 0 : AUX_CONTRACT_FLAG_IS_UNIT_CONTRACT)
				);

				log(1, "[CTB/tender/build] city=%S (%d) queued ORDER_TRAIN unit=%S unitAI=%d for workRequest=%d at=(%d,%d) append=%d danger=%d",
					pBestCity->getName().GetCString(), pBestCity->getID(),
					GC.getUnitInfo(eBestUnit).getDescription(), (int)eBestAIType,
					m_workRequests[iI].iWorkRequestId,
					m_workRequests[iI].iAtX, m_workRequests[iI].iAtY,
					bAppend ? 1 : 0, bDanger ? 1 : 0);
				// FLAG: pre-composed wide-string (%S city name, %S unit description) -- left on legacy only
			}
		}
	}

	if (gPlayerLogLevel >= 1)
	{
		int	iIdleUnits = 0;
		int iEmployedUnits = 0;
		int	iSatisfiedContracts = 0;

		for (int iI = 0; iI < (int)m_workRequests.size(); iI++)
		{
			if (m_workRequests[iI].bFulfilled)
			{
				iSatisfiedContracts++;
			}
		}

		log(1, "[CTB/finalize] contractsSatisfied=%d/%d unitsEmployed=%d unitsWithoutWork=%d", iSatisfiedContracts, m_workRequests.size(), m_iEmployedUnits, m_advertisingUnits.size());
		eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_FINALIZE, 1)
			.addI(CTBF_contractsSatisfied, iSatisfiedContracts)
			.addI(CTBF_total, (int)m_workRequests.size())
			.addI(CTBF_unitsEmployed, m_iEmployedUnits)
			.addI(CTBF_unitsWithoutWork, (int)m_advertisingUnits.size()));
	}
}

// Make a contract
//	This will attempt to make the best contracts between currently advertising units and work,
//	then search the resulting set for the work of the requested unit
//	returns true if a contract is made along with the details of what to do
bool CvContractBroker::makeContract(CvUnit* pUnit, int& iAtX, int& iAtY, CvUnit*& pJoinUnit, bool bThisPlotOnly)
{
	PROFILE_FUNC();

	// Satisfy the highest priority requests first (sort order of m_workRequests)
	for (int iI = 0; iI < (int)m_workRequests.size(); iI++)
	{
		if (!m_workRequests[iI].bFulfilled)
		{
			// If this is a request to join a unit check the unit is still in a joinable state
			//	ie - it has not joined someone else!
			if (m_workRequests[iI].iUnitId != -1)
			{
				const CvUnit* pTargetUnit = findUnit(m_workRequests[iI].iUnitId);

				if (pTargetUnit == NULL || pTargetUnit->getGroup()->getHeadUnit() != pTargetUnit)
				{
					log(1, "[CTB/match/abandon] workRequest=%d (join unit=%d) no longer joinable, marking fulfilled",
						m_workRequests[iI].iWorkRequestId, m_workRequests[iI].iUnitId);
					eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_MATCH_ABANDON, 1)
						.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId)
						.addI(CTBF_joinUnit, m_workRequests[iI].iUnitId));
					log(2, "[CTB/fulfilled] workRequest=%d marked fulfilled - join target unit=%d gone or no longer group head (abandoned)",
						m_workRequests[iI].iWorkRequestId, m_workRequests[iI].iUnitId);
					eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_FULFILLED_ABANDONED, 2)
						.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId)
						.addI(CTBF_joinUnit, m_workRequests[iI].iUnitId));
					m_workRequests[iI].bFulfilled = true;
					continue;
				}
			}
			bool bFound;
			do
			{
				bFound = false;
				advertisingUnit* suitableUnit = findBestUnit(m_workRequests[iI], bThisPlotOnly);

				if (NULL != suitableUnit)
				{
					CvUnit* unitX = findUnit(suitableUnit->iUnitId);

					if (unitX != NULL)
					{
						bFound = true;

						suitableUnit->iContractedWorkRequest = m_workRequests[iI].iWorkRequestId;

						log(3, "[CTB/match] unit=%d assigned to workRequest=%d (contracted)", suitableUnit->iUnitId, m_workRequests[iI].iWorkRequestId);
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_MATCH_ASSIGNED, 3)
							.addI(CTBF_unit, suitableUnit->iUnitId)
							.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId));

						const int iUnitStrengthTimes100 = std::max(1, unitX->AI_genericUnitValueTimes100(unitCapabilities2UnitValueFlags(m_workRequests[iI].eUnitFlags)));
						if (m_workRequests[iI].iRequiredStrengthTimes100 == -1 || iUnitStrengthTimes100 >= m_workRequests[iI].iRequiredStrengthTimes100)
						{
							//	Request is entirely fulfilled by this unit
							m_workRequests[iI].bFulfilled = true;

							log(1, "[CTB/match] workRequest=%d satisfied by unit=%d (strength %d >= required %d)", m_workRequests[iI].iWorkRequestId, suitableUnit->iUnitId, iUnitStrengthTimes100, m_workRequests[iI].iRequiredStrengthTimes100);
							eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_MATCH_SATISFIED, 1)
								.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId)
								.addI(CTBF_unit, suitableUnit->iUnitId)
								.addI(CTBF_unitStr, iUnitStrengthTimes100)
								.addI(CTBF_requiredStr, m_workRequests[iI].iRequiredStrengthTimes100));
							log(2, "[CTB/fulfilled] workRequest=%d marked fulfilled - unit=%d fully satisfies it", m_workRequests[iI].iWorkRequestId, suitableUnit->iUnitId);
							eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_FULFILLED_UNIT_SATISFIES, 2)
								.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId)
								.addI(CTBF_unit, suitableUnit->iUnitId));
							OutputDebugString(CvString::format("work request %d satisfied by unit %d\n", m_workRequests[iI].iWorkRequestId, suitableUnit->iUnitId).c_str());
						}
						else
						{
							if (m_workRequests[iI].iRequiredStrengthTimes100 < 1)
							{
								m_workRequests[iI].iRequiredStrengthTimes100 = 1;
							}
							// It's partially fulfilled so lower the priority of the remainder
							m_workRequests[iI].iPriority = lowerPartiallyFulfilledRequestPriority(m_workRequests[iI].iPriority, m_workRequests[iI].iRequiredStrengthTimes100, iUnitStrengthTimes100);
							m_workRequests[iI].iRequiredStrengthTimes100 -= iUnitStrengthTimes100;

							// SanityCheck needed!
							if (m_workRequests[iI].iRequiredStrengthTimes100 > (iUnitStrengthTimes100 * 10))
							{
								m_workRequests[iI].iRequiredStrengthTimes100 = iUnitStrengthTimes100;
							}

							log(1, "[CTB/match/partial] workRequest=%d partially satisfied by unit=%d (providedStr=%d, remainingRequiredStr=%d, newPriority=%d)",
								m_workRequests[iI].iWorkRequestId, suitableUnit->iUnitId,
								iUnitStrengthTimes100, m_workRequests[iI].iRequiredStrengthTimes100, m_workRequests[iI].iPriority);
							eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_MATCH_PARTIAL, 1)
								.addI(CTBF_workRequest, m_workRequests[iI].iWorkRequestId)
								.addI(CTBF_unit, suitableUnit->iUnitId)
								.addI(CTBF_providedStr, iUnitStrengthTimes100)
								.addI(CTBF_remainingStr, m_workRequests[iI].iRequiredStrengthTimes100)
								.addI(CTBF_newPriority, m_workRequests[iI].iPriority));

							OutputDebugString(CvString::format("work request %d partially satisfied by unit %d\n", m_workRequests[iI].iWorkRequestId, suitableUnit->iUnitId).c_str());
						}
					}
				}
			} while (bFound && !m_workRequests[iI].bFulfilled);
		}
	}

	for (int iI = 0; iI < (int)m_advertisingUnits.size(); iI++)
	{
		// Note that all existing advertising units have attempted to match against existing work requests
		if (bThisPlotOnly)
		{
			m_advertisingUnits[iI].iMatchedToRequestSeqThisPlot = m_iNextWorkRequestId;
		}
		else
		{
			m_advertisingUnits[iI].iMatchedToRequestSeqAnyPlot = m_iNextWorkRequestId;
		}
	}

	// Now see if this unit has work assigned
	for (int iI = 0; iI < (int)m_advertisingUnits.size(); iI++)
	{
		if (m_advertisingUnits[iI].iUnitId == pUnit->getID())
		{
			const int iWorkRequest = m_advertisingUnits[iI].iContractedWorkRequest;

			if (-1 != iWorkRequest)
			{
				const workRequest* contractedRequest = findWorkRequest(iWorkRequest);
				if(contractedRequest == NULL)
				{
					log(1, "[CTB/contract/lost] unit=%d contracted workRequest=%d no longer exists, releasing",
						pUnit->getID(), iWorkRequest);
					eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_CONTRACT_LOST, 1)
						.addI(CTBF_unit, pUnit->getID())
						.addI(CTBF_workRequest, iWorkRequest));
					m_advertisingUnits[iI].iContractedWorkRequest = -1;
					return false;
				}
				FAssert(NULL != contractedRequest);

				iAtX = contractedRequest->iAtX;
				iAtY = contractedRequest->iAtY;

				pJoinUnit = findUnit(contractedRequest->iUnitId);
				FAssert(NULL != pJoinUnit);
				// [CTB/contract] -- the broker matches a unit to a work request (the
				// key decision: this unit is dispatched to satisfy that need).
				log(1, "[CTB/contract] unit=%d -> work at (%d,%d) priority=%d aiType=%d joinUnit=%d (workRequest=%d)",
					pUnit->getID(), iAtX, iAtY, contractedRequest->iPriority, (int)contractedRequest->eAIType,
					pJoinUnit ? pJoinUnit->getID() : -1, iWorkRequest);
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_CONTRACT_DISPATCHED, 1)
					.addI(CTBF_unit, pUnit->getID())
					.addI(CTBF_atX, iAtX)
					.addI(CTBF_atY, iAtY)
					.addI(CTBF_priority, contractedRequest->iPriority)
					.addI(CTBF_aiType, (int)contractedRequest->eAIType)
					.addI(CTBF_joinUnit, pJoinUnit ? pJoinUnit->getID() : -1)
					.addI(CTBF_workRequest, iWorkRequest));
				return true;
			}
			log(4, "[CTB/contract] unit=%d has no contracted work this pass (no match found)", pUnit->getID());
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_CONTRACT_NOMATCH, 4)
				.addI(CTBF_unit, pUnit->getID()));
			return false;
		}
	}
	log(4, "[CTB/contract] unit=%d not present in advertising list (no contract)", pUnit->getID());
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_CONTRACT_NOTLISTED, 4)
		.addI(CTBF_unit, pUnit->getID()));
	return false;
}

const workRequest* CvContractBroker::findWorkRequest(int iWorkRequestId) const
{
	PROFILE_FUNC();

	for (int iI = 0; iI < (int)m_workRequests.size(); iI++)
	{
		if (m_workRequests[iI].iWorkRequestId == iWorkRequestId)
		{
			return &m_workRequests[iI];
		}
	}
	return NULL;
}

workRequest* CvContractBroker::findWorkRequestByUnitId(int unitId)
{
	PROFILE_FUNC();

	for (int iI = 0; iI < (int)m_workRequests.size(); iI++)
	{
		if (m_workRequests[iI].iUnitId == unitId)
		{
			return &m_workRequests[iI];
		}
	}
	return NULL;
}

advertisingUnit* CvContractBroker::findBestUnit(const workRequest& request, bool bThisPlotOnly)
{
	PROFILE_FUNC();

	int	iBestValue = -1;
	int iBestUnitIndex = -1;

	for (int iI = 0; iI < (int)m_advertisingUnits.size(); iI++)
	{
		advertisingUnit& unitInfo = m_advertisingUnits[iI];

		if (unitInfo.iUnitId < 0) continue;
		// Don't bother recalculating this advertiser/requestor pair if they have already been calculated previously
		if ((bThisPlotOnly ? unitInfo.iMatchedToRequestSeqThisPlot : unitInfo.iMatchedToRequestSeqAnyPlot) < request.iWorkRequestId
		&& unitInfo.iContractedWorkRequest == -1)
		{
			const CvUnit* unitX = findUnit(unitInfo.iUnitId);
			if (unitX == NULL || request.eAIType != NO_UNITAI && unitX->AI_getUnitAIType() != request.eAIType)
			{
					log(4, "[CTB/assess] unit=%d rejected for workRequest=%d (%s)",
					unitInfo.iUnitId, request.iWorkRequestId,
					unitX == NULL ? "unit gone" : "wrong unitAI type");
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT,
						unitX == NULL ? CTB_ASSESS_REJECT_GONE : CTB_ASSESS_REJECT_WRONGAI, 4)
					.addI(CTBF_unit, unitInfo.iUnitId)
					.addI(CTBF_workRequest, request.iWorkRequestId));
				continue;
			}

			if (unitX->meetsUnitSelectionCriteria(&request.criteria) && unitInfo.iMinPriority <= request.iPriority)
			{
				int	iValue = 1;

				if ((request.eUnitFlags & WORKER_UNITCAPABILITIES) == 0 || (request.eUnitFlags & HEALER_UNITCAPABILITIES) == 0)
				{
					if (request.eAIType == NO_UNITAI || unitX->AI_getUnitAIType() == request.eAIType)
					{
						iValue += 10;

						if (unitInfo.iDefensiveValue > 0 && (request.eUnitFlags == 0 || (request.eUnitFlags & DEFENSIVE_UNITCAPABILITIES) != 0))
						{
							iValue += unitInfo.iDefensiveValue;
						}
						if (unitInfo.iOffensiveValue > 0 && (request.eUnitFlags == 0 || (request.eUnitFlags & OFFENSIVE_UNITCAPABILITIES) != 0))
						{
							iValue += unitInfo.iOffensiveValue;
						}
					}
				}
				else if (unitInfo.bIsWorker)
				{
					iValue = 100;
				}
				else if (unitInfo.bIsHealer)
				{
					iValue = 100;
				}

				iValue *= 1000;

				if (iValue > iBestValue)
				{
					const CvUnit* pTargetUnit = findUnit(request.iUnitId);
					CvPlot* pTargetPlot;

					if (pTargetUnit != NULL)
					{
						pTargetPlot = pTargetUnit->plot();
					}
					else
					{
						pTargetPlot = GC.getMap().plot(request.iAtX, request.iAtY);
					}
					int iPathTurns = 0;
					int iMaxPathTurns = std::min((request.iPriority > LOW_PRIORITY_ESCORT_PRIORITY ? MAX_INT : 10), (iBestValue < 1 ? MAX_INT : 5 * iValue / iBestValue));

					iValue = applyDistanceScoringFactor(iValue, unitX->plot(), pTargetPlot, 1);

					if (request.iMaxPath < iMaxPathTurns)
					{
						iMaxPathTurns = request.iMaxPath;
					}

					// For low priority work never try to satisfy it with a distant unit
					if (unitX->atPlot(pTargetPlot)
					|| !bThisPlotOnly
					&& unitX->generatePath(pTargetPlot, MOVE_SAFE_TERRITORY | MOVE_AVOID_ENEMY_UNITS, true, &iPathTurns, iMaxPathTurns))
					{
						//iValue /= (iPathTurns + 1);

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							iBestUnitIndex = iI;

							log(3, "[CTB/assess] unit=%d is new best for workRequest=%d iValue=%d pathTurns=%d",
								unitInfo.iUnitId, request.iWorkRequestId, iValue, iPathTurns);
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_ASSESS_NEWBEST, 3)
							.addI(CTBF_unit, unitInfo.iUnitId)
							.addI(CTBF_workRequest, request.iWorkRequestId)
							.addI(CTBF_iValue, iValue)
							.addI(CTBF_pathTurns, iPathTurns));
						}
					}
					else
					{
						log(4, "[CTB/assess] unit=%d rejected for workRequest=%d (no path to (%d,%d) within maxPathTurns=%d, thisPlotOnly=%d)",
							unitInfo.iUnitId, request.iWorkRequestId, pTargetPlot->getX(), pTargetPlot->getY(), iMaxPathTurns, bThisPlotOnly ? 1 : 0);
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_ASSESS_NOPATH, 4)
							.addI(CTBF_unit, unitInfo.iUnitId)
							.addI(CTBF_workRequest, request.iWorkRequestId)
							.addI(CTBF_targetX, pTargetPlot->getX())
							.addI(CTBF_targetY, pTargetPlot->getY())
							.addI(CTBF_maxPathTurns, iMaxPathTurns)
							.addI(CTBF_thisPlotOnly, bThisPlotOnly ? 1 : 0));
					}
				}
				OutputDebugString(CvString::format("Assessed unit %d suitability for work request %d (iValue = %d)\n", unitInfo.iUnitId, request.iWorkRequestId, iValue).c_str());
				log(4, "[CTB/assess] unit=%d suitability for workRequest=%d iValue=%d (currentBest=%d)", unitInfo.iUnitId, request.iWorkRequestId, iValue, iBestValue);
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_ASSESS_SUITABILITY, 4)
					.addI(CTBF_unit, unitInfo.iUnitId)
					.addI(CTBF_workRequest, request.iWorkRequestId)
					.addI(CTBF_iValue, iValue)
					.addI(CTBF_currentBest, iBestValue));
			}
			else if (gPlayerLogLevel >= 4)
			{
				const bool bFailsCriteria = !unitX->meetsUnitSelectionCriteria(&request.criteria);
				log(4, "[CTB/assess] unit=%d rejected for workRequest=%d (%s)",
					unitInfo.iUnitId, request.iWorkRequestId,
					bFailsCriteria ? "fails selection criteria" : "unit minPriority above request priority");
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT,
						bFailsCriteria ? CTB_ASSESS_REJECT_CRITERIA : CTB_ASSESS_REJECT_PRIORITY, 4)
					.addI(CTBF_unit, unitInfo.iUnitId)
					.addI(CTBF_workRequest, request.iWorkRequestId));
			}
		}
	}

	if (iBestUnitIndex > -1)
	{
		log(1, "[CTB/assess] unit=%d chosen for workRequest=%d index=%d bestValue=%d",
			m_advertisingUnits[iBestUnitIndex].iUnitId, request.iWorkRequestId, iBestUnitIndex, iBestValue);
		eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_ASSESS_CHOSEN, 1)
			.addI(CTBF_unit, m_advertisingUnits[iBestUnitIndex].iUnitId)
			.addI(CTBF_workRequest, request.iWorkRequestId)
			.addI(CTBF_index, iBestUnitIndex)
			.addI(CTBF_bestValue, iBestValue));
		return &m_advertisingUnits[iBestUnitIndex];
	}

	log(4, "[CTB/assess] no suitable unit found for workRequest=%d among %d advertisers", request.iWorkRequestId, m_advertisingUnits.size());
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_ASSESS_NONE, 4)
		.addI(CTBF_workRequest, request.iWorkRequestId)
		.addI(CTBF_advertiserCount, (int)m_advertisingUnits.size()));
	return NULL;
}

CvUnit* CvContractBroker::findUnit(int iUnitId) const
{
	if (iUnitId == -1)
	{
		return NULL;
	}
	return GET_PLAYER((PlayerTypes)m_eOwner).getUnit(iUnitId);
}

int	CvContractBroker::lowerPartiallyFulfilledRequestPriority(int iPreviousPriority, int iPreviousRequestStrength, int iStrengthProvided) const
{
	return (iPreviousPriority * (iPreviousRequestStrength - iStrengthProvided)) / iPreviousRequestStrength;
}

UnitValueFlags CvContractBroker::unitCapabilities2UnitValueFlags(unitCapabilities eCapabilities) const
{
	UnitValueFlags	valueFlags = (UnitValueFlags)0;

	if ((eCapabilities & DEFENSIVE_UNITCAPABILITIES) != 0)
	{
		valueFlags |= UNITVALUE_FLAGS_DEFENSIVE;
	}
	if ((eCapabilities & OFFENSIVE_UNITCAPABILITIES) != 0)
	{
		valueFlags |= UNITVALUE_FLAGS_OFFENSIVE;
	}

	if (valueFlags == 0)
	{
		valueFlags = UNITVALUE_FLAGS_ALL;
	}

	return valueFlags;
}

void CvContractBroker::internalRemoveUnit(const int unitId)
{
	for (int iI = 0; iI < (int)m_advertisingUnits.size(); iI++)
	{
		if (m_advertisingUnits[iI].iUnitId == unitId)
		{
			advertisingUnit* aUnit = &m_advertisingUnits[iI];
			const int iContracted = aUnit->iContractedWorkRequest;
			m_advertisingUnits.erase(aUnit);

			m_iEmployedUnits++;
			log(2, "[CTB/remove] unit=%d removed from worklist (contractedWorkRequest=%d) -> employedUnits=%d unitsRemainingInWorklist=%d",
				unitId, iContracted, m_iEmployedUnits, m_advertisingUnits.size());
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_REMOVE_INTERNAL, 2)
				.addI(CTBF_unit, unitId)
				.addI(CTBF_contractedWorkRequest, iContracted)
				.addI(CTBF_employedUnits, m_iEmployedUnits)
				.addI(CTBF_unitsRemainingInWorklist, (int)m_advertisingUnits.size()));
			break;
		}
	}

}


void CvContractBroker::postProcessUnitsLookingForWork()
{
	PROFILE_EXTRA_FUNC();

	log(2, "[CTB/postprocess] processing contracts for %d advertising unit(s)", m_advertisingUnits.size());
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_POSTPROCESS_BEGIN, 2)
		.addI(CTBF_advertisingUnits, (int)m_advertisingUnits.size()));

	for (int iI = 0; iI < (int)m_advertisingUnits.size(); iI++)
	{
		CvUnit* unitX = findUnit(m_advertisingUnits[iI].iUnitId);

		if (unitX)
		{
			log(4, "[CTB/postprocess] unit=%d contractedWorkRequest=%d -> processContracts()",
				m_advertisingUnits[iI].iUnitId, m_advertisingUnits[iI].iContractedWorkRequest);
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_POSTPROCESS_UNIT, 4)
				.addI(CTBF_unit, m_advertisingUnits[iI].iUnitId)
				.addI(CTBF_contractedWorkRequest, m_advertisingUnits[iI].iContractedWorkRequest));
			unitX->getGroup()->getHeadUnit()->processContracts();
		}
		else
		{
			log(4, "[CTB/postprocess] unit=%d no longer exists, skipping", m_advertisingUnits[iI].iUnitId);
			eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_CONTRACT, CTB_POSTPROCESS_GONE, 4)
				.addI(CTBF_unit, m_advertisingUnits[iI].iUnitId));
		}
	}
}

