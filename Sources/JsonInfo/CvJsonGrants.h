#pragma once
#ifndef CV_JSON_GRANTS_H
#define CV_JSON_GRANTS_H

//
//	CvJsonGrants -- the `grants` section as ONE composable unit (json.md §5, the WHOLE post-classification shape):
//	id-list buckets, numeric pulses, bool flags, scoped pulses, the settler `foundBuildings` seeds, and the
//	STRUCTURED `repeatable` provisions (per-turn unit spawn / unitCombat heal / full-heal / property pulse with its
//	#429 spatial intent). Composed BY VALUE on the derived infos that author grants (the data-grounded table).
//	WRITE-ONCE AT LOAD. Owns its entries/conditions.
//
//	Scale: numeric pulse values are ×100 at parse (the one human->fixed-point boundary); readers take
//	pulse100()/100 for the human count -- the shape the grants machine's ÷100 reads were written against.
//

#include "CvJsonCondition.h"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace picojson { class value; }

// One settler-seed entry (json §5 foundBuildings): the building placed at settle-time, optionally conditioned.
class CvJsonFoundBuilding
{
public:
	int building;                // BUILDING_* FK
	CvJsonCondition* enabled;    // NULL = always placed
	CvJsonFoundBuilding() : building(-1), enabled(NULL) {}
	~CvJsonFoundBuilding() { delete enabled; }
private:
	CvJsonFoundBuilding(const CvJsonFoundBuilding&);            // noncopyable -- owns the condition
	CvJsonFoundBuilding& operator=(const CvJsonFoundBuilding&);
};

// One structured `repeatable` provision (json §5): exactly ONE payload group is live per entry --
// unit spawn (unitId), per-unitCombat heal (unitCombatId+heal100), full-heal (healFull+count), or a
// property pulse (propertyId+amount100 with the on/relation/distance spatial intent the #429 distribution reads).
class CvJsonGrantRepeatable
{
public:
	// payload (one group live)
	int unitId;                  // {"unit": UNIT_X}                     -- spawned unit
	int unitCombatId;            // {"unitCombat": UNITCOMBAT_X}         -- per-turn heal target class
	int heal100;                 //   its "heal": N amount (×100)
	bool healFull;               // {"heal": "full"}                     -- full-heal provision
	int count;                   //   its "count": N (raw -- a unit count, not a magnitude)
	int propertyId;              // {"PROPERTY_X": N}                    -- property pulse target
	int amount100;               //   its per-turn amount (×100, signed)
	// recurrence + chance (json §3.8 / §3.7 -- the per parses via the shared jsonParsePer)
	int intervalPerTurn;         // "perTurn" = 1; {"perTurn": N} = N
	int chanceValue100;          // "chance": N (flat, ×100); 0 = none
	int chancePerId;             // "chance": {"per": TYPE} -- the count-scaler type FK (-1 = none)
	std::string chancePerToken;  //   the raw CATCH-ALL token when the type is no FK (POPULATION/...; "" = typed/none) -- carry-only
	int chancePerEach;           //   its "each" quantum (default 1)
	int chancePerScope;          //   its AUTHORED scope (CvCascScope; -1 = absent -> the deposit's own scope, json §3.7)
	std::vector<int> chancePerAnyOf;   //   its per.anyOf summed-count FK ids (json §3.7)
	// spatial intent (the #429 distribution's target -- json §5 property pulses)
	std::string on;              // "plot" / "" (the emitting object)
	std::string relation;        // "near" / "same" / ""
	int distance;                // radius for "near"
	CvJsonCondition* enabled;    // optional per-entry gate (NULL = always)

	CvJsonGrantRepeatable()
		: unitId(-1), unitCombatId(-1), heal100(0), healFull(false), count(0), propertyId(-1), amount100(0),
		  intervalPerTurn(1), chanceValue100(0), chancePerId(-1), chancePerEach(1), chancePerScope(-1), distance(0), enabled(NULL) {}
	~CvJsonGrantRepeatable() { delete enabled; }
private:
	CvJsonGrantRepeatable(const CvJsonGrantRepeatable&);            // noncopyable -- owns the condition
	CvJsonGrantRepeatable& operator=(const CvJsonGrantRepeatable&);
};

class CvJsonGrants
{
public:
	CvJsonGrants() {}
	~CvJsonGrants();

	// The unit's single load-time writer: parse the whole `grants` object (every §5 shape; no key is "unknown").
	void parse(const picojson::value& v);

	// --- readers (const-only; the grants machine's query surface) ---
	const std::vector<int>* list(const std::string& szBucket) const
	{
		std::map<std::string, std::vector<int> >::const_iterator it = m_lists.find(szBucket);
		return (it != m_lists.end()) ? &it->second : NULL;
	}
	int firstListId(const std::string& szBucket) const
	{
		const std::vector<int>* l = list(szBucket);
		return (l != NULL && !l->empty()) ? (*l)[0] : -1;
	}
	int pulse100(const std::string& szChannel) const
	{
		std::map<std::string, int>::const_iterator it = m_pulses100.find(szChannel);
		return (it != m_pulses100.end()) ? it->second : 0;
	}
	int scopedPulse100(const std::string& szChannel, const std::string& szScope) const;
	int scopedPulseSumAllScopes100(const std::string& szChannel) const;   // one channel summed over its scopes
	bool flag(const std::string& szName) const { return m_flags.count(szName) != 0; }
	int pulseCount() const { return (int)m_pulses100.size(); }            // census read

	const std::map<std::string, std::vector<int> >& lists() const { return m_lists; }
	const std::vector<CvJsonFoundBuilding*>& foundBuildings() const { return m_foundBuildings; }
	const std::vector<CvJsonGrantRepeatable*>& repeatables() const { return m_repeatables; }

	bool isEmpty() const
	{
		return m_lists.empty() && m_pulses100.empty() && m_scopedPulses100.empty() && m_flags.empty()
			&& m_foundBuildings.empty() && m_repeatables.empty();
	}

private:
	void parseRepeatable(const picojson::value& v);      // the "repeatable" array
	void parseFoundBuildings(const picojson::value& v);  // the "foundBuildings" array

	std::map<std::string, std::vector<int> > m_lists;              // bucket -> FK ids (techs/units/freePromotions/…)
	std::map<std::string, int> m_pulses100;                        // channel -> value ×100 (population/revolution/…)
	std::map<std::string, std::map<std::string, int> > m_scopedPulses100;   // channel -> {scope -> value ×100}
	std::set<std::string> m_flags;                                 // bool flags ("goldenAge")
	std::vector<CvJsonFoundBuilding*> m_foundBuildings;            // owned
	std::vector<CvJsonGrantRepeatable*> m_repeatables;             // owned

	CvJsonGrants(const CvJsonGrants&);              // noncopyable -- owns its entries
	CvJsonGrants& operator=(const CvJsonGrants&);
};

#endif // CV_JSON_GRANTS_H
