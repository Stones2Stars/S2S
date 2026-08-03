#pragma once

#ifndef CyState_h__
#define CyState_h__

#include <string>

//
//	CyState -- the Python LIVE-STATE surface: the "what do I HAVE, right now?" half of the GAME-OBJECT read role
//	(patterns.md § THE TWO READ ROLES), exposed to script. Sibling of CyEnabler, which answers the "can I?" half.
//
//	⛔ THIS IS A NEW SURFACE, NOT A WIDENED BINDING ([DEC-cy-not-fixed]). The legacy per-type wrappers
//	(CyCity::getYieldRate, CyPlayer::getCommerceRate, ...) are NOT extended and NOT reused. Like CyEnabler it is
//	deliberately ID-BASED -- it takes plain player / city ids and holds no CyCity or CyPlayer -- so the legacy
//	wrappers can be CUT AWAY without touching it.
//	⚑ THAT IS WHY A CALLBACK HANDS OVER AN IDENTITY, NOT A HANDLE. The engine->Python CALLBACK direction is KEPT
//	(patterns.md: the cut is DIRECTIONAL), and a cut wrapper carries ZERO defs -- so a script handed one could ask
//	it nothing, not even which object it is. The event reports therefore push the (owner, id) PAIR this surface is
//	addressed by (Cy::PyIdentity, CvPython.h), which is what makes the kept direction usable at all.
//
//	THE GRAMMAR is the game-object half's, unchanged:
//	  - ONE READ PER GROUP, and the getter IS the group. There is NO scalar getter per channel; a script wanting
//	    one value indexes the returned list. The surface therefore grows by GROUPS (a handful), never by channels
//	    (hundreds) -- which is the whole point of the standardization.
//	  - THE EXISTING ENGINE ENUM INDEXES THE RESULT, never the call: getYields()[YieldTypes.YIELD_FOOD]. A family
//	    with no engine enum is indexed by its own kind enum, and the name says so (get<Family>Kinds).
//	  - A group read fills a caller-owned array on the C++ side; in script that is a plain Python LIST, returned
//	    whole. One call in, the whole group out.
//
//	⛔ SCOPE IS A SELECTOR, NEVER A NAME FRAGMENT ([DEC-scope-is-an-axis]). A group both scopes carry takes
//	(iPlayer, iCity) and reads the CITY when iCity >= 0, the EMPIRE when iCity < 0 -- the same selector shape the
//	cache endpoints already use (?player=N[&city=M], http-endpoints.md). A group only ONE scope carries takes only
//	the ids that scope needs, so a script cannot ask a scope for a group it does not have.
//
//	⛔ EVERY VALUE IS x100 NATIVE ([DEC-fixedpoint-x100]) -- there is no `100` in any name, no scale variant, and
//	no getter reduces. A script divides by 100 at the point it displays or compares against a whole game count.
//	A PERCENT is NOT scaled, so a percent-unit channel is already the number you want.
//
//	⛔ NO FINAL-STATE VALUE IS IN ANY GROUP (patterns.md rule 6). angryPopulation and healthRate are calculations
//	DOWNSTREAM of the four wellbeing channels, not slots in the array -- folding one in would put a computed
//	outcome in a slot meaning "a channel a source deposited into" and hide the opposing-pair structure.
//
//	⛔ NO WHAT-IF ARGUMENTS, and every read is a BARE FETCH. Nothing here gates, ensures or recomputes, exactly as
//	on the C++ side -- so a missed invalidation shows up in script as a visibly wrong number rather than being
//	silently repaired at the boundary (state-repositories.md; [DEC-no-self-heal]).
//
//	BOOST: this file uses ONLY the `python::` alias, never a bare `boost::` and never a using-directive -- the
//	tree carries TWO Boosts and an unqualified name can resolve to the wrong one through the PCH (engine.md).
//

class CyState
{
public:
	CyState() {}

	// ---- GROUPS both scopes carry: iCity >= 0 reads the CITY, iCity < 0 reads the EMPIRE ----
	python::list getYields(int iPlayer, int iCity) const;
	python::list getCommerces(int iPlayer, int iCity) const;
	python::list getWellbeing(int iPlayer, int iCity) const;
	python::list getDefenseKinds(int iPlayer, int iCity) const;
	python::list getMaintenanceKinds(int iPlayer, int iCity) const;
	python::list getBuildRateKinds(int iPlayer, int iCity) const;
	python::list getCombatKinds(int iPlayer, int iCity) const;
	python::list getExperienceKinds(int iPlayer, int iCity) const;
	python::list getRevolutionKinds(int iPlayer, int iCity) const;
	python::list getTradeRouteKinds(int iPlayer, int iCity) const;
	python::list getScalars(int iPlayer, int iCity) const;

	// ---- CITY-only groups ----
	python::list getHealKinds(int iPlayer, int iCity) const;
	python::list getUnderworldKinds(int iPlayer, int iCity) const;
	python::list getVisionKinds(int iPlayer, int iCity) const;
	// The REALIZED wellbeing: the deposits above PLUS the raw-state inputs no deposit produces (the anger
	// percents, the espionage counters, event anger -- modifier.md §2b). Distinct from getWellbeing on purpose;
	// a script that adds the two double-counts.
	python::list getRealizedWellbeing(int iPlayer, int iCity) const;
	int getSight(int iPlayer, int iCity) const;   // the city's sight BUDGET (vision.md)

	// ---- ENUMERATION: which cities a player HAS, by id. ----
	// ⛔ The prerequisite of every id-based city read on this surface, and it is structural rather than
	// convenience: this library is deliberately ID-BASED so the legacy wrappers can be cut, but the cut leaves
	// CyCity carrying ZERO defs -- a registration-only marshalling handle. So a script handed a CyCity cannot
	// ask it anything, not even its own id, and player.cities() therefore hands back a list nothing can read.
	// Without an id enumeration the whole surface is unreachable for any screen that LISTS cities, which is most
	// of them. The ids are stable within a game and are what every other read here takes.
	python::list getCityIds(int iPlayer) const;

	// ---- CITY RANK groups: where this city places among its OWNER'S cities for each channel. ----
	// ⛔ The engine enum indexes the RESULT, never the call (grammar rule 2 above), so a rank read hands back the
	// WHOLE group and a script indexes it -- getYieldRateRanks(p, c)[YieldTypes.YIELD_FOOD]. A per-channel
	// findYieldRateRank(p, c, eYield) would put the channel in the CALL, which is the per-channel scalar shape
	// this surface exists to delete.
	// ⚠ A rank is an ORDINAL (1 = highest), not an amount -- so it is NOT x100 and a reader never divides it.
	python::list getYieldRateRanks(int iPlayer, int iCity) const;
	python::list getBaseYieldRateRanks(int iPlayer, int iCity) const;
	python::list getCommerceRateRanks(int iPlayer, int iCity) const;

	// ---- CITY plain FACTS: genuine lone values, so they stay bare typed reads (patterns.md category 4) rather
	// than being forced into a group that would mean nothing -- the getSight precedent. ----
	// A coordinate is the one PAIR here: x and y are meaningless apart, so they cross as one [x, y] list rather
	// than as two getters.
	python::list getCityPosition(int iPlayer, int iCity) const;
	int getCityPopulation(int iPlayer, int iCity) const;          // citizens; a whole game count, NOT x100
	int64_t getCityRealPopulation(int iPlayer, int iCity) const;  // the displayed head-count; exceeds 32 bits
	int getGreatPeopleRate(int iPlayer, int iCity) const;
	int getGreatPeopleProgress(int iPlayer, int iCity) const;
	int getMilitaryHappinessUnits(int iPlayer, int iCity) const;
	// Post-conquest resistance: whether it is running, and for how many more turns. Live city state, so it is
	// here and not on the info surface -- occupation is something a city HAS, never something a type CARRIES.
	bool isOccupation(int iPlayer, int iCity) const;
	int getOccupationTimer(int iPlayer, int iCity) const;

	// ---- EMPIRE-only groups ----
	python::list getUpkeepKinds(int iPlayer) const;
	python::list getCostKinds(int iPlayer) const;
	python::list getStateReligionKinds(int iPlayer) const;
	python::list getDiplomacyKinds(int iPlayer) const;
	python::list getDurationKinds(int iPlayer) const;
	python::list getAirKinds(int iPlayer) const;
	python::list getCaptureKinds(int iPlayer) const;
	python::list getCargoKinds(int iPlayer) const;
	python::list getExtraYieldThresholds(int iPlayer) const;
	python::list getLessYieldThresholds(int iPlayer) const;

	// ---- Plain live FACTS: genuine lone values, so they stay bare typed reads (patterns.md category 4), never
	// forced into a group that would mean nothing. ----
	int getActivePlayer() const;
	int getGameTurn() const;
	bool isFinalInitialized() const;   // is the game up enough to be asked / shown a message
	// The closed CONSTANTS block (python-read-map: a small closed set, trivially served by the library).
	// Compile-time engine limits, so they are bare reads with no owner and no scope.
	int getMAX_PLAYERS() const;
	int getMAX_PC_PLAYERS() const;
	int getMAX_TEAMS() const;
	int getMAX_PC_TEAMS() const;
	int getBARBARIAN_PLAYER() const;

	// The global DEFINES. They sit on the live-state half rather than the vocabulary because a define is a
	// LIVE option -- user-changeable mid-game through the BUG bridge, which is exactly why nothing STATIC may be
	// gated on one (python-read-map). The READS are in scope; the writes are not.
	int getDefineINT(const std::string& szName) const;
	float getDefineFLOAT(const std::string& szName) const;
	int getAIAutoPlay(int iPlayer) const;
	std::wstring getPlayerName(int iPlayer) const;
	std::wstring getCityName(int iPlayer, int iCity) const;

	static void pythonPublish();
};

#endif // CyState_h__
