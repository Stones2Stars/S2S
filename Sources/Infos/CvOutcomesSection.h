#pragma once
#ifndef CV_OUTCOMES_SECTION_H
#define CV_OUTCOMES_SECTION_H

//
//	CvOutcomesSection -- the json.md par.8 `outcomes` bespoke block as ONE composable typed section object
//	(patterns.md par. THE GETTER SETUP category 1: sections are whole typed objects). The CvOutcome system's
//	intake: units and unitcombats author the exact key vocabulary { kill:[...], actions:[...] }, so ONE unit
//	serves both (the CvHideAndSeekSection composition precedent) -- the combat-kill outcome list, the heap-owned
//	action outcome-missions, and the by-index / by-mission runtime queries the composing pocos forward.
//	WRITE-ONCE AT LOAD (parse is the sole writer; clear-first, idempotent under the full-registry re-map).
//	Members stay private -- unlike the plain-data open-member sections, this unit OWNS heap objects (the
//	action missions, freed in the dtor), so the read surface is accessors.
//

#include "Defines/CvEnums.h"    // MissionTypes
#include "UI/CvOutcomeList.h"   // the kill-outcome list value member (its dtor frees its CvOutcome*)
#include <vector>

namespace picojson { class value; }

class CvOutcomeMission;   // Sources/Engine/CvOutcomeMission.h -- the actions[] outcome-missions (heap-owned)

class CvOutcomesSection
{
public:
	CvOutcomesSection();
	~CvOutcomesSection();   // frees the heap-owned action missions

	// The unit's single load-time writer: locate + parse the entity's `outcomes` block (absent = stays empty).
	void parse(const picojson::value& entity);
	void clearParsed();

	// --- the runtime queries the composing pocos forward ---
	const CvOutcomeList* getKillOutcomeList() const { return &m_killOutcomes; }
	int getNumActionOutcomes() const { return (int)m_actionMissions.size(); }
	const CvOutcomeList* getActionOutcomeList(int iIndex) const;                      // CvOutcomeMission deref -> .cpp
	MissionTypes getActionOutcomeMission(int iIndex) const;                          // CvOutcomeMission deref -> .cpp
	const CvOutcomeList* getActionOutcomeListByMission(MissionTypes eMission) const;
	const CvOutcomeMission* getOutcomeMission(int iIndex) const { return m_actionMissions[iIndex]; }
	const CvOutcomeMission* getOutcomeMissionByMission(MissionTypes eMission) const;

private:
	CvOutcomeList m_killOutcomes;                     // outcomes.kill[]
	std::vector<CvOutcomeMission*> m_actionMissions;  // outcomes.actions[] (heap-owned; freed in the dtor)

	CvOutcomesSection(const CvOutcomesSection&);              // noncopyable (held by-value on the noncopyable info)
	CvOutcomesSection& operator=(const CvOutcomesSection&);
};

#endif // CV_OUTCOMES_SECTION_H
