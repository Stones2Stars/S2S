//
//	CvCascadeChannels -- the LOAD-TIME address resolution (strings -> the channel/position/scope ints).
//
//	This file is the ONE place a modifier address string is interpreted. It runs at readJson load
//	(DepositIndex::compile) and NEVER on a read or rebuild path: the runtime carries ints only
//	([DEC-materialize-at-mapfrom] applied to the cascade's own gather). The per-source walkers that used to
//	re-derive an address per channel per city (two string lookups + a full rescan of the info's deposit vector,
//	MMKernel::sumUnitFrom) are what this replaces.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeChannels.h"
#include <string>

int cascadeScopeFromSegment(const char* scope)
{
	if (scope == NULL || *scope == '\0') return -1;
	const std::string s(scope);
	if (s == "world")  return CSC_WORLD;
	if (s == "team")   return CSC_TEAM;
	if (s == "empire") return CSC_EMPIRE;
	if (s == "area")   return CSC_AREA;
	if (s == "city")   return CSC_CITY;
	if (s == "plot")   return CSC_PLOT;
	if (s == "unit")   return CSC_UNIT;
	if (s == "self")   return CSC_SELF;
	return -1;
}

// The family -> channel map for the families whose channel needs no member. Returns NUM_CASCADE_CHANNELS when
// the family is not a (member-less) channel -- the caller then tries the member-qualified families below.
static CascadeChannel ch_plainFamily(const std::string& f)
{
	if (f == "food")            return CH_FOOD;
	if (f == "production")      return CH_PRODUCTION;
	if (f == "commerce")        return CH_COMMERCE;
	if (f == "gold")            return CH_GOLD;
	if (f == "research")        return CH_RESEARCH;
	if (f == "culture")         return CH_CULTURE;
	if (f == "espionage")       return CH_ESPIONAGE;
	if (f == "greatPeopleRate") return CH_GREAT_PEOPLE;
	if (f == "maintenance")     return CH_MAINTENANCE;
	if (f == "tradeRoutes")     return CH_TRADE_ROUTES;
	if (f == "freeSpecialists") return CH_FREE_SPECIALISTS;
	if (f == "happiness")       return CH_HAPPINESS;
	if (f == "health")          return CH_HEALTH;
	return NUM_CASCADE_CHANNELS;
}

// The GROUPED families, whose channel is (family, member): `defense` and `buildRate` (json.md §6 "grouped
// families keep <member> parts"). An unknown member is NOT a channel -- resolve fails and the deposit is
// skipped rather than silently folded into the wrong slot.
static CascadeChannel ch_groupedFamily(const std::string& f, const std::string& m)
{
	if (f == "defense")
	{
		if (m == "amount")         return CH_DEFENSE;
		if (m == "bombardDefense") return CH_DEFENSE_BOMBARD;
		if (m == "min")            return CH_DEFENSE_MIN;
		return NUM_CASCADE_CHANNELS;
	}
	if (f == "buildRate")
	{
		if (m == "military")       return CH_BUILDRATE_MILITARY;
		if (m == "space")          return CH_BUILDRATE_SPACE;
		if (m == "worldWonder")    return CH_BUILDRATE_WORLD_WONDER;
		if (m == "teamWonder")     return CH_BUILDRATE_TEAM_WONDER;
		if (m == "nationalWonder") return CH_BUILDRATE_NATIONAL_WONDER;
		// the KEYED members (units/buildings/domains/unitCombats/...) route to the keyed ledger, not a slot
		return NUM_CASCADE_CHANNELS;
	}
	return NUM_CASCADE_CHANNELS;
}

// The `stateReligion.<scope>.<channel>` prefix: a state-religion-GATED deposit of an ordinary channel. The
// channel is the MEMBER; the gate is the position. Stored UNGATED (the SR-in-city test is live at read), so a
// state-religion change never invalidates the slot -- exactly like the coastal/connected/golden-age gates.
static CascadeChannel ch_stateReligionMember(const std::string& m)
{
	if (m == "greatPeopleRate")    return CH_GREAT_PEOPLE;
	if (m == "happiness")          return CH_HAPPINESS;
	if (m == "unitProduction")     return CH_BUILDRATE_MILITARY;   // the SR unit-production percent
	if (m == "buildingProduction") return CH_BUILDRATE_BUILDING;   // the SR building-production percent
	return NUM_CASCADE_CHANNELS;
}

bool cascadeResolveAddress(const char* family, const char* member, const char* unit,
                           CascadeChannel& outChannel, CascadePosition& outGatePos, bool& outIsPercent)
{
	if (family == NULL) return false;
	const std::string f(family);
	const std::string m(member != NULL ? member : "");
	const std::string u(unit != NULL ? unit : "");

	outIsPercent = (u == "percent");
	outGatePos = NUM_CASCADE_POSITIONS;   // "no gate" -- the caller uses the source-kind position

	// (1) the state-religion GATED prefix
	if (f == "stateReligion")
	{
		const CascadeChannel c = ch_stateReligionMember(m);
		if (c == NUM_CASCADE_CHANNELS) return false;
		outChannel = c;
		outGatePos = outIsPercent ? POS_PCT_STATE_RELIGION : POS_STATE_RELIGION;
		return true;
	}

	// (2) the grouped families (channel = family + member)
	CascadeChannel c = ch_groupedFamily(f, m);
	if (c != NUM_CASCADE_CHANNELS) { outChannel = c; return true; }

	// (3) the plain families (channel = family; the member, where present, selects a GATE)
	c = ch_plainFamily(f);
	if (c == NUM_CASCADE_CHANNELS) return false;   // not a cascade channel (the unit-plane families land here)
	outChannel = c;

	if (!m.empty())
	{
		if (m == "coastal")         outGatePos = POS_COASTAL;
		else if (m == "connectedCity") outGatePos = POS_PCT_CONNECTED;
		else if (m == "goldenAge")  outGatePos = POS_GOLDEN_AGE;
		else if (m == "perPopulation") outGatePos = POS_PER_POPULATION;
		// `maintenance.empire.all` is the plain empire sum (no gate); `homeArea`/`otherArea` are AREA-scope
		// splits the area package owns, and `nonStateReligion` is a policy-conditioned entry -- none of them
		// select a gate position here, so they fall through to the source-kind position.
	}
	return true;
}
