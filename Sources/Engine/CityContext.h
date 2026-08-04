#pragma once
#ifndef CV_CITY_CONTEXT_H
#define CV_CITY_CONTEXT_H

//
//	CityContext -- the per-city READ SURFACE the (cityContext, plotGroup) building-output getters + the one condition evaluator use.
//	Bound to its CvCity by pointer (never a value copy -- passing a bound reference is far cheaper than snapshotting
//	values, owner). The CITY-scope half of the symmetric per-scope contexts (EmpireContext = empire scope), so a
//	reader always knows where to go: city state here, empire state on the owner's EmpireContext.
//
//	A context is an EVENT-BUILT STORE, not a forwarding facade (contexts.md). The split is by DERIVED vs RAW:
//	 - STORE every DERIVED fact evaluation reads -- a scan, a union, an aggregate, a multi-hop resolution -- computed
//	   ONCE by the ONE derivation and maintained by the spine facts, never recomputed at read.
//	 - FORWARD only the RAW data CvCity already holds O(1) (population, a presence flag, a single counter). Forwarding
//	   raw data is not duplication; storing a second copy of it would be.
//	A forwarded read that COMPUTES is the defect this rule exists to kill.
//
//	⚖ THE HAVE AXIS LIVES HERE (contexts.md): every evaluator atom / enabler gate read of a CITY-scope fact goes
//	through this surface -- the context is the ONE responsibility home for the city's changeable state; the
//	evaluator never reaches into CvCity ad hoc.
//
//	MAINTENANCE IS EXTERNAL AND EVENT-DRIVEN -- a read NEVER recomputes, and NOTHING heals a missed fact. There is no
//	periodic refresh, no recompute-on-read fallback, no staleness stamp: a fact that fails to fire leaves the stored
//	value visibly wrong, which is how it gets found (DEC-no-self-heal; LOAD is the only full build). The refresh entry
//	points are const with the stores `mutable` (the CvDerivedCache / PlotContext shape) so the maintainer can drive
//	them through the bound city's const accessor without a second, mutable path onto the city.
//
//	NEVER SERIALIZED (DEC-derived-never-trusted): every store rebuilds from the save read's own in-read DOMAIN emits
//	(DEC-spine-reseed).
//
//	⛔ THE VICINITY SPLIT -- the context holds the MAP half, the enabler holds the BUILDING half, neither duplicates
//	the other (contexts.md: the enabler's precomputed sets stay the ENABLER's derived output, fed to the evaluator as
//	ctx members, never per-scope live state). Concretely, the json par.5a in-vicinity supply is the union of two
//	independently-owned halves:
//	  - MAP providers (a bonus on a radius tile providing itself) -- per-scope live state with no other home, so it
//	    lives HERE, tiered by the par.3.4 ownership discriminator.
//	  - ACTIVE BUILDING providers (`provides.bonuses`) -- the operate/provides LEAST FIXPOINT, which only the enabler
//	    can resolve (an operate condition may consume a bonus another active building provides). It stays
//	    EnablerKernel's `OperatingBuildings::provided`, reached through CvCascadeEvalCtx::vicinityProvidedBonuses.
//	The evaluator unions the two at the read. A second copy of the building half here would be the banned duplication
//	AND would drift, since the enabler mutates its set in place while the fixpoint ripples.
//

#include "ContextDict.h"
#include "CvCondition.h"   // CvCascVicinity -- the json par.3.4 ownership tiers the stored vicinity dicts key on
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / NUM_COMMERCE_TYPES -- the realized group forwards' out-array extents

class CvPlot;
class CvCity;
class CvClassificationBlock;   // a grantor's §8 `amenities` block, folded by pointer (never included here)
struct CvCascadeEvalCtx;
struct OperatingBuildings;   // the enabler's per-city ACTIVE set, forwarded (CvCity owns the storage)

// The city-side twin of CLS_HAS: a one-line getter body reading the city's amenity FOLD by key, with the
// load-minted id memoized per call site. The city is what a gate asks (contexts.md) -- never each grantor.
#define CITY_HAS_AMENITY(ctx, key) { static int s_amenityId = -1; return (ctx).hasAmenityKey(s_amenityId, key); }

class CityContext
{
public:
	CityContext()
		: m_city(NULL), m_areaId(-1), m_areaTileCount(0), m_maxAdjacentWaterTiles(0), m_holyCityCount(0),
		  m_governmentCenterDistance(0) {}
	void bind(const CvCity* c) { m_city = c; }   // set once by the owning CvCity; the pointer IS the owner (never dangles)

	// --- STORED: the uniquely-owned aggregate -- the HAS_/IS_ plot-predicate COUNTS, event-maintained (onPlotChanged) ---
	// `mutable` like every other store here, so the const maintenance entry points can reach it.
	mutable ContextDict plotAttrs;
	// A plot ENTERED (sign +1) / LEFT (sign -1) the city's owned worked-radius set: fold its stable HAS_/IS_ attributes
	// (+/-1) into plotAttrs. COUNTS only; the plot is never stored. Fired from the CvPlot::updateWorkingCity choke
	// point at play; the load reseed folds the same fact from the in-read SEVT_WORKING_CITY_CHANGED DOMAIN events
	// (Engine/ContextConsumer -- DEC-spine-reseed).
	void onPlotChanged(const CvPlot* plot, int sign);
	// ⛔ THE VICINITY TIERS HAVE NO APPLIER. The radius walk is deleted and nothing has replaced it yet, so the
	// tier dictionaries below stay EMPTY -- deliberately, and visibly wrong, rather than filled by something that
	// re-reads the plot. They are rebuilt driven by the plot facts' own payloads (bonus +-1, owner old->new,
	// worked 0/1, the network fact), each of which already names which tier moves and in which direction; an
	// applier that asks the plot instead is the legacy read path on an event clock ([contexts.md]).

	// --- STORED: the city's AMENITIES -- what its grantors CONFER ON IT (json §8) -----------------------------------
	// The clean feature list every gate checks (owner): a consumer asks the CITY, never each building in turn.
	// ⛔ An id->COUNT dictionary, NOT a bitset, and the count is load-bearing: SEVERAL grantors can confer the same
	// amenity, so a removal DECREMENTS -- losing one power plant must not darken a city that has two. (The same
	// refcount shape as OperatingBuildings::providedCount, for the same reason.) `has` is therefore count > 0.
	// ⚑ It also makes VOLUMETRIC free: the slot is already an int, so an amenity that later becomes a QUANTITY
	// needs no reshape, only a change in what the number means.
	// ⛔ THE AMENITY STATE IS NOT HELD HERE, and it is not reached THROUGH here either. It lives in its own
	// context, owned by `CvCity` exactly as `m_operatingBuildings` is, and that context owns its storage, its
	// maintenance AND the declared set of facts that drives it -- one place responsible (owner;
	// Engine/AmenityContext.h, [DEC-dict-is-a-consumer]). This context FORWARDS the reads below, the same
	// STORES-vs-FORWARDS split every other object-owned aggregate takes.
	// ⚠ Its maintainer reaches `pCity->amenity()` DIRECTLY -- never `getCityContext().amenity()`, which would
	// make this class a pass-through facade for state it does not own.

	// --- MAINTENANCE: called ONLY by the contexts' spine consumer (Engine/ContextConsumer) --------------------------
	// Each re-derives its WHOLE store from the bound city through the SAME engine accessors a read used to call --
	// ONCE per change instead of once per read, and one uniform derivation rather than a bespoke per-event delta.
	// CONSTRAINT: no choke point may call these directly. Every mutation that moves a stored fact emits its own DOMAIN
	// fact, so the consumer is the single trigger path; a direct call beside the event would be a second maintenance
	// surface for the same state.
	void refreshAreaFacts() const;         // the city's area ID + the coastal water-body size
	void refreshHolyCity() const;          // how many religions hold this city as their holy city
	// Distance to the owner's nearest government centre. Re-derived for EVERY city of a player when a
	// government centre appears or goes, and for one city when it is founded or changes hands -- the fan-out is
	// bounded by the city count and the facts driving it are rare.
	void refreshGovernmentCenterDistance() const;
	// (The amenity fold is NOT here: it is a DELTA, not a re-derivation -- see onBuildingChanged above.)

	void clear() const;   // m_city is a binding, not cleared

	// --- STORED READS: O(1) fetches of the blocks above; each replaces a per-check scan/union/multi-hop --------------
	// The city's MAP-provided in-vicinity presence of a bonus at the json par.3.4 ownership tier. `CASC_VIC_NONE` is
	// the DEFAULT tier (owned+neutral, NOT foreign). ⚠ This is the MAP half only -- a caller answering a
	// `connection:"vicinity"` atom must ALSO consult the enabler's active-building supply
	// (CvCascadeEvalCtx::vicinityProvidedBonuses); see the header note on the vicinity split.
	bool hasVicinityBonusAt(int eBonus, CvCascVicinity eTier) const;
	// The CONNECTED (json par.3.4 "obtained": owned + valid + connected) tier, the name/signature the evaluator's
	// connected leg already calls. Semantics are unchanged; it is now a bare fetch instead of a radius scan.
	bool hasVicinityBonus(int eBonus) const;
	// The city's TRADED bonus count -- the plot-group-backed network count with the TechCityTrade + minted gates
	// applied (enabler.md par.8 the residency/counting rule: "a maintained number, added and subtracted on spine
	// events, never calculated per read"). Traded MEMBERSHIP still belongs to CvPlotGroup (contexts.md) -- what is
	// held here is only this city's own gated COUNT, which no other object owns.
	int  tradedBonusCount(int eBonus) const;
	int  areaId() const;                      // the city's area ID -- never a CvArea* and never a per-read area() chase
	int  areaSize() const;                    // the AREA_SIZE counter, served from the maintained area facts
	// The DISTANCE_TO_GOVERNMENT_CENTER counter: plot distance to the NEAREST government centre of this city's
	// owner, 0 in a government centre itself. STORED rather than forwarded because deriving it walks the owner's
	// cities -- precisely the per-read scan contexts.md exists to delete, and it is asked once per maintenance
	// rebuild per city. A city with no government centre anywhere answers 0 (nothing to be distant from).
	int  governmentCenterDistance() const;
	bool isCoastal(int iMinWaterSize) const;  // the HAS_COAST minArea city form: the largest ADJACENT water body >= iMinWaterSize
	bool isHolyCityAny() const;               // bare IS_HOLY_CITY -- this city is some religion's holy city
	// The city's amenity reads -- O(1) over the fold. `hasAmenity` is the gate every consumer wants;
	// `amenityCount` exposes HOW MANY grantors confer it (the refcount, and the volumetric slot).
	bool hasAmenity(int iAmenityId) const;
	int  amenityCount(int iAmenityId) const;
	// The BY-KEY read, for a caller that has a name rather than an id. Same shape as CLS_HAS on the info side and
	// for the same reason: amenity ids are MINTED AT LOAD, so there is no compile-time id to pass
	// ([DEC-classification-infos]) -- the caller keeps a static cache the registry fills once.
	//   bool CvCity::isGovernmentCenter() const CITY_HAS_AMENITY(getCityContext(), "governmentCenter")
	bool hasAmenityKey(int& iIdCache, const char* szKey) const;

	// --- FORWARDED: the RAW data CvCity already holds O(1) -- a stored copy would duplicate it. Out-of-line (.cpp) ---
	int  population() const;                  // CvCity::getPopulation            (m_iPopulation)
	// The ENABLER's per-city derived output (enabler.md §3.2), FORWARDED because the city OWNS the storage
	// (`CvCity::m_operatingBuildings`) exactly as it owns population -- this context neither computes nor
	// mirrors it. The eval seam points the ctx's three set members at it, so a predicate asking "is this
	// building ACTIVE / is this bonus provided in vicinity" reads the cascade-computed verdict rather than the
	// engine's ([DEC-calc-zero-ride-in]). ⛔ Never STORE a copy: the enabler mutates its set in place as the
	// operate fixpoint ripples, so a mirror would drift (the contexts.md vicinity-split reasoning).
	const OperatingBuildings* operatingBuildings() const;
	int  power() const;                       // the `providesPower` AMENITY fold (owner: power IS an amenity)
	bool isPowered() const;                   // CvCity::isPower -- the HAS_POWER verdict; see the CONTEXT GAP note below
	bool hasReligion(int eReligion) const;    // CvCity::isHasReligion            (the presence array)
	bool isHolyCityOf(int eReligion) const;   // CvCity::isHolyCity(eReligion)    ({IS_HOLY_CITY: R}; one game-level lookup)
	bool hasCorporation(int eCorp) const;     // CvCity::isHasCorporation (presence; spread state)
	bool hasActiveCorporation(int eCorp) const;   // CvCity::isActiveCorporation ({HAS_CORPORATION: X} = ACTIVE, json §3.5).
	                                          // Engine-driven per-turn spread state -- an owner-ruled ENGINE-OWNED INPUT
	                                          // the cascade reads, NOT a cascade-computed verdict.
	bool isHeadquartersOf(int eCorp) const;   // CvCity::isHeadquarters(eCorp)    ({IS_HEADQUARTERS: X}; one game-level lookup)
	bool isHeadquartersAny() const;           // CvCity::isHeadquarters() -- see the CONTEXT GAP note below
	bool isCapital() const;                   // CvCity::isCapital (IS_CAPITAL)
	bool isGovernmentCenter() const;          // CvCity::isGovernmentCenter (the owner-sanctioned engine counter)
	bool hasFreshWaterAccess() const;         // CvCity::hasFreshWater -- the provider-building-fed ACCESS counter (m_iFreshWater)
	int  propertyValue(int eProperty) const;  // CvProperties::getValueByProperty (the PROPERTY_ band read)
	// The §3.7 `per` COUNT domains this scope answers (tally.md: let the OBJECT own the aggregate, the context
	// forwards it -- never a tally side-store). A per-scaler over these read 0/1 through the presence fallback
	// before they were wired, so every deposit scaled by one silently contributed nothing.
	int  specialistCount() const;             // CvCity::getSpecialistPopulation -- the SPECIALIST count token
	int  improvedPlotCount(int eImprovement) const;   // CvCity::countNumImprovedPlots -- the IMPROVEMENT_ count
	int  ownCulturePercent() const;           // CULTURE_PERCENTAGE -- see the CONTEXT GAP note below
	int  owner() const;                       // CvCity::getOwner
	int  team() const;                        // CvCity::getTeam (the plot-bonus reveal axis)
	const CvPlot* cityPlot() const;           // CvCity::plot -- the centre tile
	const CvPlot* radiusPlot(int iRingIndex) const;   // CvCity::getCityIndexPlot -- O(1) coordinate arithmetic
	bool hasBuilding(int eBuilding) const;    // CvCity::hasBuilding -- the §7 raw-presence has-list (m_bHasBuildings)
	// The GAME YEAR a present building was built (CvCity::getBuildingData().iTimeBuilt -- the serialized building
	// ledger, raw object-owned state, so FORWARDED and never stored). Serves the `existedFor` age gate
	// (json.md §3.5): the authored thresholds are YEAR counts, which is what the tooltip has always promised.
	// ⛔ Returns MIN_INT for a building this city does NOT hold -- the caller must test that BEFORE subtracting,
	// since a game year is legitimately NEGATIVE (BC) and `getGameTurnYear() - MIN_INT` overflows.
	int  buildingBuildYear(int eBuilding) const;
	int  stateReligion() const;               // owner CvPlayer::getStateReligion  (STATE_RELIGION_IN_CITY = hasReligion(stateReligion()))
	bool hasPolicy(int ePolicy) const;        // owner EmpireContext::policies.has  (empire aggregate, not mirrored here)
	// The city's CURRENT REALIZED YIELDS -- CvCity::getYields, the city's own O(1) group read, handed on unchanged.
	// FORWARDED, never stored: it is the bound object's own maintained data, so a copy here would be the banned
	// duplication AND would need an invalidation the forward does not have. This is the base a percent deposit
	// resolves against (contexts.md: THE CONTEXT *IS* THE CURRENT VALUE -- a valuation reads it HERE rather than
	// taking current amounts as a separate parameter). x100 native, indexed by YieldTypes.
	void yields(int (&realizedYields)[NUM_YIELD_TYPES]) const;
	// The city's CURRENT REALIZED COMMERCE -- CvCity::getCommerces, the city's own group read, handed on
	// unchanged: the per-commerce SPLIT of the commerce yield by the empire's sliders, plus each channel's own
	// deposits (InfoValuation::commerceSplit). The commerce twin of yields() above and forwarded for the same
	// reason -- it is the base a city-scope commerce percent deposit resolves against (contexts.md: THE CONTEXT
	// *IS* THE CURRENT VALUE), and a stored copy would duplicate maintained data with no invalidation.
	// ×100 native, indexed by CommerceTypes.
	void commerces(int (&realizedCommerces)[NUM_COMMERCE_TYPES]) const;

	// ⚠ ONE FORWARD STILL COMPUTES, recorded rather than papered over, because a store with no trigger is
	// permanently wrong -- which is worse than a cheap forward (the PlotContext::isCity precedent):
	//  - ownCulturePercent(): plot culture moves EVERY turn for every plot, so a store would be rewritten as often
	//    as it is read; it is correctly a live read, not a missing fact.
	// isPowered() and isHeadquartersAny() stay FORWARDS by the STORES-vs-FORWARDS split (both are data CvCity
	// already answers O(1)), but all three of isPowered()'s legs now announce -- the power COUNT
	// (SEVT_POWER_CHANGED), the disabled-power timer (SEVT_CITY_POWER_DISABLED_CHANGED) and the area clean-power
	// flag (SEVT_AREA_CLEAN_POWER_CHANGED) -- and the headquarters designation announces
	// SEVT_HEADQUARTERS_CHANGED, so a reader that needs to react to either has a fact to hang on.

	// Fill the CITY half of a condition-eval context (this silo + its centre plot's + the enabler's operating
	// set) -- the context IS the
	// eval state (the evaluator reads through the ctx it fills). Paired with EmpireContext::fillEvalCtx (player/team).
	void fillEvalCtx(CvCascadeEvalCtx& ec) const;

private:
	// The ONE tier-dictionary selector, shared by the derivation and the read so the two cannot disagree about which
	// tier a plot lands in (DEC-single-implementation).
	const ContextDict& vicinityTier(CvCascVicinity eTier) const;

	const CvCity* m_city;   // the bound game object; the derivation reads it -- never a value copy

	// --- the stored blocks; derived state, so NEVER serialized ----------------------------------------------------
	// The MAP-provider vicinity presence, one dictionary per json par.3.4 ownership tier, all folded by the ONE
	// derivation (the onPlotChanged fold). The tiers NEST -- owned ⊂ owned+neutral ⊂ crossBorder, worked ⊂ owned --
	// so the read composes them rather than each carrying a redundant copy.
	mutable ContextDict m_vicinityOwned;       // a radius tile this city owns (the centre tile included)
	mutable ContextDict m_vicinityNeutral;     // an UNOWNED radius tile
	mutable ContextDict m_vicinityForeign;     // a radius tile owned by another player (the crossBorder opt-in only)
	mutable ContextDict m_vicinityWorked;      // a radius tile a citizen works this turn (the centre tile included)
	mutable ContextDict m_vicinityConnected;   // owned + a valid bonus + connected to this city (the "obtained" tier)
	mutable int m_areaId;                      // the city's area ID (-1 = unassigned)
	mutable int m_areaTileCount;               // that area's tile count -- AREA_SIZE served without dereferencing CvArea
	// The largest ADJACENT water body, in tiles -- ONE int that answers isCoastal at EVERY threshold
	// (isCoastal(N) == m_maxAdjacentWaterTiles >= N), so a new authored minArea needs no new store.
	mutable int m_maxAdjacentWaterTiles;
	mutable int m_holyCityCount;               // how many religions hold this city as their holy city
	// Plot distance to the owner's NEAREST government centre (0 here, or with none anywhere). One int answers
	// the whole distance-maintenance leg; it moves only on a government-centre crossing or a city gained/lost.
	mutable int m_governmentCenterDistance;
};

#endif // CV_CITY_CONTEXT_H
