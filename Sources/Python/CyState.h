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
//	⛔ EVERY READ IS A BARE FETCH -- nothing here gates, ensures or recomputes, exactly as on the C++ side, so a
//	missed invalidation shows up in script as a visibly wrong number rather than being silently repaired at the
//	boundary (state-repositories.md; [DEC-no-self-heal]).
//	⚖ A PROJECTION OVER A KNOWN IMMINENT CHANGE IS NOT A WHAT-IF (owner). getRealizedWellbeing takes the
//	population the city is ABOUT TO HAVE, because the engine's own realizedWellbeing does and because the anger
//	side is non-linear in population -- no consumer can derive it from the current-population answer. That is a
//	different thing from the banned shape: the AI's "what if I BUILT this?" is the cascade's expected* read on
//	the what-if plane (patterns.md), never a widened live-state getter. The test is whether the argument names a
//	CANDIDATE (banned here) or a state the object is already headed for (fine).
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
	// iExtraPopulation projects the answer onto a population the city is about to reach (+1 growing, -1
	// starving, 0 for the current state) -- the overcrowding and per-citizen anger terms are non-linear in
	// population, so this cannot be derived from the 0 answer.
	python::list getRealizedWellbeing(int iPlayer, int iCity, int iExtraPopulation) const;
	// The two FINAL-STATE values that sit DOWNSTREAM of that group -- calculations over the four channels,
	// never slots in them ([patterns.md] rule 6, which is why they are not in the list above). They are
	// published as their own named reads, delegating to the ONE engine calc, precisely so a script does not
	// re-derive `min(0, health - unhealth)` / `clamp(anger - happiness, 0, pop)` and become a second
	// implementation of a final-state rule ([DEC-single-implementation]).
	// iExtraPopulation projects them exactly as it does the group read.
	int getHealthRate(int iPlayer, int iCity, int iExtraPopulation) const;
	int getAngryPopulation(int iPlayer, int iCity, int iExtraPopulation) const;
	// The city's yield MODIFIER percents, indexed by YieldTypes -- the multiplier a base yield is scaled by.
	// A PERCENT, so it is NOT x100 and a reader never divides it ([DEC-fixedpoint-x100]).
	python::list getYieldModifiers(int iPlayer, int iCity) const;

	// THE CITY YIELD CENSUS, for ONE channel -- the same decomposition InfoValuation::cityReceiverRate fills
	// and the /computed census renders, published so the TOOLTIP reads the SAME DOCUMENT.
	// â A tooltip IS a census (owner): if the panel recomputes its own breakdown it is a second answer to
	// one question, and the two drift. This is the read that stops a screen hand-rolling yield arithmetic.
	// Indexed by CityYieldTerm -- the reader names its slot, never a literal position.
	python::list getCityYieldTerms(int iPlayer, int iCity, int iYield) const;
	int getSight(int iPlayer, int iCity) const;   // the city's sight BUDGET (vision.md)
	// Which player would receive this city if it were liberated, or -1 for nobody. A lone id, so it stays a
	// bare typed read (patterns.md category 4) rather than being forced into a group that would mean nothing.
	int getLiberationPlayer(int iPlayer, int iCity) const;
	// The city's REALIZED maintenance -- the computed total with the disorder / we-love-the-king suppression
	// applied, which the raw getMaintenanceKinds deposits do NOT carry. x100 native like every amount, so the
	// name says the VALUE and never the scale ([DEC-fixedpoint-x100]); a reader divides at the point of use.
	int64_t getMaintenance(int iPlayer, int iCity) const;

	// ---- THE CURRENT SELECTION, as an IDENTITY. ----
	// ⛔ Asked of the library, NOT of the EXE's CyInterface. getHeadSelectedCity/Unit there hand back a Cy*
	// HANDLE, and those wrappers carry ZERO defs -- so a script is given an object it cannot ask anything, not
	// even which one it is. That return type is the closed EXE's and cannot be changed, so the only way the
	// selection is reachable at all is to answer it HERE, in the (owner, id) pair every read on this surface
	// takes. It is the same identity a callback hands over (Cy::PyIdentity), from the other direction.
	// ⚠ Answers [-1, -1] when nothing is selected -- a real owner/id pair is never negative, so a caller tests
	// the id rather than inferring emptiness from a missing value.
	python::list getHeadSelectedCityId() const;
	python::list getHeadSelectedUnitId() const;
	// The WHOLE selection, as [owner, id] pairs -- what a stack panel iterates. CyInterface::getSelectionUnit
	// hands back a CyUnit the script cannot read, so the list is answered here for the same reason the head
	// selection is.
	python::list getSelectedUnitIds() const;

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
	// The city's potential work area as [(x, y), …], RING-ORDERED from the centre outward ([contexts.md]).
	// Plots the map does not hold are skipped, so a caller never meets a hole.
	python::list getCityPlots(int iPlayer, int iCity) const;
	// Two COUNTS over that same work area. They are their own reads rather than something a caller folds out
	// of getCityPlots, because the predicate is engine state the coordinate list does not carry -- deriving
	// them in script would mean a per-plot boundary crossing per city per turn to re-answer what the city
	// already counts O(1).
	int getImprovedPlotCount(int iPlayer, int iCity) const;
	int getWaterPlotCount(int iPlayer, int iCity) const;

	// ---- THE AI PLANE. ----
	// ⛔ These are the AI's OWN HEURISTIC SCORES, not state -- the sanctioned residual that belongs to the
	// asking side ([superseded-ideas] par.1). They sit on this surface because it is the (player, city)
	// address every city read already takes, and the accessor HOMING is the scheduled later pass
	// ([patterns.md] § THE PYTHON READ BOUNDARY -- the organizing pass is wholesale, never negotiated per
	// endpoint). Named, findable, and cheap to move.
	// ⚠ Read them as ADVICE, never as a fact about the city: a heuristic answers what the AI would weigh,
	// and two AI implementations may weigh it differently.
	int getAiCityValue(int iPlayer, int iCity) const;
	// The count of worthwhile worker builds the AI sees in this city's own AREA -- the area is resolved from
	// the city rather than passed, because the only question anyone asks is about the city's own.
	int getAiBestBuildCount(int iPlayer, int iCity) const;
	// The UNIT twin. ⚠ A unit's position is part of the IDENTITY SET on the handle
	// ([patterns.md] THE IDENTITY SET: owner, id, POSITION), but an EVENT PAYLOAD carries only (owner, id) --
	// so a handler that was handed a payload has no handle to ask and needs this. Answers (-1, -1) for a unit
	// that does not resolve OR is off-map, which is a real state and not only a save defect
	// ([unit-lifecycle.md] THE OFF-MAP UNIT).
	python::list getUnitPosition(int iPlayer, int iUnit) const;
	int getCityPopulation(int iPlayer, int iCity) const;          // citizens; a whole game count, NOT x100
	int64_t getCityRealPopulation(int iPlayer, int iCity) const;  // the displayed head-count; exceeds 32 bits
	int getGreatPeopleRate(int iPlayer, int iCity) const;
	int getGreatPeopleProgress(int iPlayer, int iCity) const;
	// ⚠ Takes the unit id in the CALL, unlike a group read, and deliberately: this is not a channel family but a
	// SPARSE id-keyed quantity over the whole unit registry (thousands of entries, of which a handful are great
	// people). Handing back a list indexed by UnitTypes would cross thousands of zeros to answer about five.
	int getGreatPeopleUnitProgress(int iPlayer, int iCity, int iUnit) const;
	int getGreatPeopleThresholdNonMilitary(int iPlayer) const;
	int getMilitaryHappinessUnits(int iPlayer, int iCity) const;

	// ---- The city's RAW-STATE groups: live engine counters and the current order. ----
	// These are the state no deposit produces, so they answer off the city directly rather than off a package --
	// but they obey the same grammar as the deposit groups: one read per group, the group's own kind enum
	// indexes the RESULT (CityCountdownKind / CityOrderRead), and the surface grows by groups, not counters.
	// ⚠ NOT x100. Every slot is a whole game count -- a turn number, a citizen count, a hammer total -- so a
	// reader never divides one ([DEC-fixedpoint-x100] scales AMOUNTS; these are counts).
	python::list getCountdowns(int iPlayer, int iCity) const;
	python::list getOrder(int iPlayer, int iCity) const;
	//	Is this unit TYPE already somewhere in the city's build queue? A POINTED question about one candidate,
	//	the `canUnitAcquirePromotion` shape -- getOrder answers only the HEAD order, so the queue cannot be
	//	scanned from script.
	//	⛔ IT IS NOT AN AVAILABILITY QUESTION, AND THE ENABLER MUST NOT ANSWER IT. A unit STAYS TRAINABLE while
	//	queued, because you can build MANY copies ([enabler.md] par.7.1: "a building leaves the frontier when
	//	built; a unit stays trainable") -- so the frontier rightly keeps offering it. What a RECOMMENDER wants is
	//	the different question "am I about to nag about something already ordered", which is city STATE.
	bool isUnitQueued(int iPlayer, int iCity, int iUnit) const;

	//	---- The city's ONE-SHOT GRANTED state, and the scenario reads that go with it ----
	//	⚑ These exist for the SCENARIO SERIALIZER, and that is not a second-class consumer: an event/vote grant
	//	is genuine non-derivable state kept in its own persisted store ([state-repositories.md]), so a scenario
	//	that could not carry it would silently drop it on every round trip. Nothing else can re-derive it.
	std::string  getCityScriptData(int iPlayer, int iCity) const;
	python::list getCityGrantedExtras(int iPlayer, int iCity) const;            // CityGrantedExtra
	python::list getBuildingGrantedWellbeing(int iPlayer, int iCity, int iBuilding) const;  // BuildingGrantedKind
	python::list getBuildingGrantedYields(int iPlayer, int iCity, int iBuilding) const;     // YieldTypes
	python::list getBuildingGrantedCommerces(int iPlayer, int iCity, int iBuilding) const;  // CommerceTypes
	//	When this building was BUILT here (the ledger's own record), and how many free specialists were ADDED to
	//	this city beyond what its live sources supply.
	//	⚠ The added count is NOT CITY_SPECIALIST_FREE: that one is the engine-derived total, so writing it into a
	//	scenario and reading it back would grant the derived half a second time.
	int getBuildingBuiltTime(int iPlayer, int iCity, int iBuilding) const;
	int getAddedFreeSpecialists(int iPlayer, int iCity, int iSpecialist) const;
	python::list getGrowth(int iPlayer, int iCity) const;
	python::list getCulture(int iPlayer, int iCity) const;
	// The city's parameterless predicates, as ONE group (CityFlagKind) -- occupation, disorder, capital,
	// power, the two automation toggles and the conscript verdict. A lone bool getter beside this group
	// would be a second surface for the same question, which is what the grammar exists to prevent.
	python::list getCityFlags(int iPlayer, int iCity) const;
	// What this city holds and is doing about ONE building / unit type. The entity is a SELECTOR in the call
	// because the question is about a PAIR, and the registries are sparse -- a slot per building would cross
	// thousands of zeros to answer about one.
	python::list getBuildingInCity(int iPlayer, int iCity, int iBuilding) const;
	python::list getUnitInCity(int iPlayer, int iCity, int iUnit) const;
	// Turns to finish ONE queued item. iOrder is an OrderTypes value naming which registry iType indexes, and
	// iNum is the item's position in the build QUEUE -- the estimate differs per position, which is why the node
	// is a parameter and not a slot on the groups above.
	int getProductionTurnsLeft(int iPlayer, int iCity, int iOrder, int iType, int iNum) const;
	python::list getSpecialistInCity(int iPlayer, int iCity, int iSpecialist) const;
	python::list getCityCounts(int iPlayer, int iCity) const;
	// The city's TRADE ROUTES as rows: [partnerOwner, partnerCity, profitTimes100]. Routes are live STATE, so
	// they are walked rather than looked up -- nothing authors a route.
	python::list getTradeRoutes(int iPlayer, int iCity) const;
	// Which religions / corporations this city HAS, as rows: [id, bIsHolyCity] and [id, bIsHeadquarters].
	python::list getCityReligions(int iPlayer, int iCity) const;
	python::list getCityCorporations(int iPlayer, int iCity) const;
	// Culture accumulated here BY a given player, and each player's share of the plot's culture.
	int64_t getCultureForPlayer(int iPlayer, int iCity, int iForPlayer) const;
	int getCulturePercent(int iPlayer, int iCity, int iForPlayer) const;
	// One yield's share of a ROUTE's profit, run through the city's trade-yield rule. The profit comes from
	// getTradeRoutes above, so the caller pairs the two rather than the engine re-deriving the route here.
	int getTradeYield(int iPlayer, int iCity, int iYield, int iProfitTimes100) const;
	// The city screen's RECOMMENDATION -- what the governor would build. It is the AI's own pick, so it is a
	// read of a verdict rather than a re-derivation; a script must never re-implement the choice.
	int getBestUnit(int iPlayer, int iCity) const;
	int getBestUnitForRole(int iPlayer, int iCity, int iUnitAI) const;
	// The city's recent OUTPUT history: one row per remembered turn, [iTurn, [[iOrder, iType], ...]]. Served as
	// rows because the history object itself is not something script can hold.
	python::list getCityOutputHistory(int iPlayer, int iCity) const;
	// The lone per-entity facts that belong to no group.
	int getNumBonuses(int iPlayer, int iCity, int iBonus) const;
	bool hasCorporation(int iPlayer, int iCity, int iCorporation) const;
	int getProjectProduction(int iPlayer, int iCity, int iProject) const;
	int getHandicap(int iPlayer, int iCity) const;
	// The city's PROPERTY rows: one [propertyId, value, changeThisTurn] per property it carries. A list of rows
	// rather than an object, so nothing hands script a handle it cannot read.
	python::list getCityProperties(int iPlayer, int iCity) const;
	// The city screen's BUILD LISTS, already filtered / grouped / sorted by the engine's own view model: a list
	// of GROUPS, each a list of entity ids. One crossing for the whole list rather than one per entry -- a
	// boost::python call costs far more than the lookup inside it, and this is redrawn constantly.
	// ⚠ A BARE fetch, like everything here: it reports the list as it currently stands and never rebuilds it.
	// Rebuilding is an explicit ACT verb, because it is the screen ASKING for work, not a read repairing itself.
	python::list getUnitListGroups(int iPlayer, int iCity) const;
	python::list getBuildingListGroups(int iPlayer, int iCity) const;
	// Has this team SEEN the city. Fog state, so it is live and per-team -- an unrevealed city is one a screen
	// may know of but must not name.
	bool isCityRevealed(int iPlayer, int iCity, int iTeam) const;
	// Is the city on a water body of at least iMinWaterSize tiles -- the `{HAS_COAST:{minArea:N}}` CITY form.
	// ⛔ NOT the bare coastal flag getCityFlags already carries: that one asks at threshold 0, so it cannot
	// answer a caller that needs an OCEAN rather than a lake. The two are different questions on one axis.
	bool isCityCoastal(int iPlayer, int iCity, int iMinWaterSize) const;
	// The city governor's EMPHASIS flags -- what the player told this city to prioritise. Per-city view state
	// the engine stores, so it reads here; eEmphasize selects one flag, the sparse-selector shape again.
	bool isEmphasize(int iPlayer, int iCity, int iEmphasize) const;
	// The hurry QUOTE for one method: may I, and at what price. eHurry is a SELECTOR in the call because the
	// hurry registry is sparse and a city is asked about one method at a time -- the getGreatPeopleUnitProgress
	// shape, for the same reason.
	python::list getHurryQuote(int iPlayer, int iCity, int iHurry) const;

	// ---- The city screen's own VIEW state: which filter/sort/grouping the player left its lists on. ----
	// ⚠ Genuinely per-city state that the engine stores, not authored data and not a derived value -- so it is
	// live state and belongs here. ⛔ These are the READS only. The matching setters are a WRITE, and no write
	// surface exists yet ([roadmap] scope decision 6), so the lists READ correctly and do not yet re-sort on a
	// click. Reads run on every redraw; the writes fire only on user action, which is why the split is usable.
	int getOrderQueueLength(int iPlayer, int iCity) const;
	// Is the city's HEAD order a unit? The advisor nags on a city building something OTHER than a unit, so the
	// queue LENGTH alone cannot answer it -- what is at the front is a separate question.
	bool isProductionUnit(int iPlayer, int iCity) const;
	// How many of a building the empire holds. ⚠ It replaces the DELETED CvPlayer::getBuildingCountWithUpgrades,
	// and the difference is a stated behaviour change ([validation.md]: the spec leads, a change is named rather
	// than hidden): that one also counted the building's upgrade-chain predecessors, this one counts the building.
	// The engine accessor it wraps is the surviving plain count.
	int getBuildingCount(int iPlayer, int iBuilding) const;
	bool getBuildingListFilterActive(int iPlayer, int iCity, int iFilter) const;
	int getBuildingListSorting(int iPlayer, int iCity) const;
	bool getUnitListFilterActive(int iPlayer, int iCity, int iFilter) const;
	int getUnitListGrouping(int iPlayer, int iCity) const;
	int getUnitListSorting(int iPlayer, int iCity) const;

	// ---- THE UNIT PLANE ----
	// ⛔ Addressed by the same (owner, id) pair as everything else. CyUnit carries ZERO defs, so a script handed
	// one -- by a callback or by CyPlot::getUnit -- can ask it nothing; the ids are the only way a unit is
	// reachable at all, which is why the plot enumeration below is a prerequisite rather than a convenience.
	python::list getUnitRead(int iPlayer, int iUnit) const;
	python::list getUnitFlags(int iPlayer, int iUnit) const;
	std::wstring getUnitName(int iPlayer, int iUnit) const;
	// The unit's CUSTOM name only -- empty when it carries none, unlike getUnitName which falls back to the
	// type's description. A scenario writes this one, because a fallback name is not authored data.
	std::wstring getUnitNameNoDesc(int iPlayer, int iUnit) const;
	std::string getUnitScriptData(int iPlayer, int iUnit) const;
	// The units standing on a plot, as [owner, id] pairs, in the engine's own plot order -- what a plot list
	// iterates. ⚠ Answers EVERY unit present; the caller applies its own visibility test below, because
	// visibility is per-OBSERVER and the list is drawn for one team.
	python::list getPlotUnitIds(int iX, int iY) const;
	// The SELECTOR predicates: each asks about a PAIR, so the subject is in the call rather than a flag slot.
	bool isUnitInvisible(int iPlayer, int iUnit, int iTeam) const;
	bool hasUnitPromotion(int iPlayer, int iUnit, int iPromotion) const;
	// ⛔ The LIVE per-unit verdict, NOT the type's skill block. hiddenNationality is promotion-grantable
	// (PROMOTION_PROUD_PIRATE, [skills.md]), so `INFO.isHiddenNationality(unitType)` answers a DIFFERENT question
	// and silently misses every unit that earned it -- which is why this is a STATE read and named for the unit.
	bool isUnitHiddenNationality(int iPlayer, int iUnit) const;
	// ⛔ A unit whose death is DECIDED but not yet performed still answers TRUE here, and a consumer iterating
	// a player's units must skip it: combat holds raw pointers across the exchange, so a killed unit stays a
	// live object until the delayed-death pass reaps it ([unit-lifecycle.md]). Anything listing or counting
	// units without this shows the dead ones.
	bool isUnitDead(int iPlayer, int iUnit) const;
	// The owner a VIEWER sees -- which differs from the real owner for a hidden-nationality unit. A log or a
	// message must use this one, or it names the civ the mechanic exists to conceal.
	int getUnitVisualOwner(int iPlayer, int iUnit) const;
	// Base combat strength on the HUMAN scale. ⛔ Deliberately NOT baseCombatStr(): that one returns x100 under
	// GAMEOPTION_COMBAT_SIZE_MATTERS and human without it, so its scale depends on live game state and no caller
	// can reason about it ([fixed-point-and-scales.md] §4c-ter). This is the boundary read, always human.
	int getUnitBaseCombatStr(int iPlayer, int iUnit) const;
	// A plot query asked RELATIVE TO A UNIT (whose owner decides who counts as an enemy). It takes the unit's
	// identity + the tile rather than a unit handle, because a unit reaches Python as (owner, id).
	int getNumVisiblePotentialEnemyDefenders(int iPlayer, int iUnit, int iX, int iY) const;
	// ⛔ The unit's HELD promotions, as ONE list. Asking per-promotion instead costs a boundary crossing per
	// PROMOTION per unit per redraw, and the registry is ~1500 entries -- a stack panel then spends tens of
	// thousands of crossings a frame. The per-promotion read above stays for a single POINTED question.
	// ⚠ Overridden promotions are excluded: a held-but-overridden promotion is not one the unit HAS in any sense
	// a display cares about, and every caller was already filtering them out by hand.
	python::list getUnitPromotions(int iPlayer, int iUnit) const;
	bool isUnitPromotionOverridden(int iPlayer, int iUnit, int iPromotion) const;
	// Does this unit hold that COMBAT CLASS? Its primary + subs + promotion-granted, composed -- the same
	// question CvUnit::isHasUnitCombat answers, asked by (owner, id) like every other read here.
	bool hasUnitCombat(int iPlayer, int iUnit, int iUnitCombat) const;
	// CAN this unit take that promotion right now? The per-unit applicability leg the enabler evaluates on
	// demand at level-up ([enabler.md] par.7.1 carve-out) -- a POINTED question about one candidate.
	bool canUnitAcquirePromotion(int iPlayer, int iUnit, int iPromotion) const;
	// ⚖ THE SIBLING, AND THE TWO ARE NOT INTERCHANGEABLE. canUnitAcquirePromotion answers the LEVEL-UP question;
	// this answers whether a promotion may be APPLIED to the unit at all, which is the gate a GRANT uses -- a
	// free promotion bypasses tech prereqs ([special-systems.md]). ⛔ Gating a grant on the acquire read instead
	// refuses every promotion an EVENT hands out, since those sit outside the normal list and cannot be taken by
	// XP at all (owner).
	bool isUnitPromotionValid(int iPlayer, int iUnit, int iPromotion) const;
	bool isUnitActionRecommended(int iPlayer, int iUnit, int iAction) const;
	// Can this unit become that one -- a PAIR question (this unit, that target type), so the target is in
	// the call. bTestVisible asks the display question ("show the button") rather than the strict one.
	bool canUnitUpgrade(int iPlayer, int iUnit, int iToUnit, bool bTestVisible) const;
	// ⛔ "Can this unit upgrade to ANYTHING?" is ONE question and therefore ONE read. Asking it by looping the
	// whole unit registry from script costs a boundary crossing PER UNIT TYPE, per unit, per redraw -- and a
	// boost::python call costs far more than the lookup inside it ([patterns.md]). The walk belongs here.
	bool canUnitUpgradeToAny(int iPlayer, int iUnit) const;

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
	// Is this player still in the game, and whose team are they on. Both are asked constantly by anything that
	// walks the player range, and neither belongs to a group -- they are lone facts about the slot itself.
	bool isPlayerAlive(int iPlayer) const;
	int getPlayerTeam(int iPlayer) const;
	//	What THIS TEAM actually pays for a tech, in beakers. COMPUTED GAME STATE, not info data: the authored
	//	base (CyInfo PYINT_COST) is scaled by gamespeed, the tech's era, the handicap and the team's member
	//	count ([culture-religion-research.md] Tech cost). ⛔ It belongs here rather than on the info plane
	//	precisely because an info never reads game state ([json.md] §9) -- the two sit side by side, and a
	//	consumer showing "what will this cost ME" wants this one ([pedia-read-map.md] finding 5).
	int getTechResearchCost(int iTeam, int iTech) const;
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
	// The empire's colour on the map (and on its unit sprites), resolved to a COLOR_ id in ONE crossing. The
	// legacy shape was a two-hop -- ask the player for its PLAYERCOLOR_, then ask that info for its primary --
	// which is plumbing rather than a question: every consumer wants the colour, none wants the intermediate.
	// ⚠ It is the PRIMARY only, because that is what the live consumers draw with; a secondary read is added
	// when something actually wants one, never mirrored ahead of demand.
	// ⛔ Deliberately NOT a slot on the generic intrinsic plane: this value belongs to ONE registry, and that
	// plane answers -1 for a prefix it was never wired for, which reads as a legitimate "no colour"
	// ([patterns.md] -- an opaque slot re-creates the fault it was meant to cure).
	int getPlayerColorPrimary(int iPlayer) const;
	std::wstring getPlayerName(int iPlayer) const;
	std::wstring getCityName(int iPlayer, int iCity) const;
	// What the city is building, already localized -- the engine composes it from the order, so a script that
	// rebuilt it from the order group would be a second implementation of the same sentence.
	std::wstring getProductionName(int iPlayer, int iCity) const;
	// The same thing as a TXT_KEY rather than resolved text -- the `*Key` suffix IS the contract
	// ([patterns.md]), so a caller always knows which of the two it is holding.
	std::wstring getProductionNameKey(int iPlayer, int iCity) const;

	static void pythonPublish();
};

#endif // CyState_h__
