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
//	points are const with the stores `mutable` (the PlotContext shape) so the maintainer can drive
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
#include <vector>
#include "CvCondition.h"   // CvCascVicinity -- the json par.3.4 ownership tiers the stored vicinity dicts key on
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / NUM_COMMERCE_TYPES -- the realized group forwards' out-array extents

class CvPlot;
class CvCity;
class CvClassificationBlock;   // a grantor's §8 `amenities` block, folded by pointer (never included here)
struct CvCascadeEvalCtx;
struct CvSpineEvent;
struct OperatingBuildings;   // the enabler's per-city ACTIVE set, forwarded (CvCity owns the storage)

// The city-side twin of CLS_HAS: a one-line getter body reading the city's amenity FOLD by key, with the
// load-minted id memoized per call site. The city is what a gate asks (contexts.md) -- never each grantor.
#define CITY_HAS_AMENITY(ctx, key) { static int s_amenityId = -1; return (ctx).hasAmenityKey(s_amenityId, key); }

//	The STORED vicinity partitions. ⛔ NEUTRAL is deliberately absent -- it is the DEFAULT (no owner), answered as
//	the residual `all − owned − foreign`, so it needs no storage and no fact (see the members).
enum CityVicinityPartition
{
	CITYVIC_ALL,       // on a radius tile at all -- the total the ownership bands are carved out of
	CITYVIC_OWNED,     // that tile is owned by this city's owner
	CITYVIC_FOREIGN,   // that tile is owned by another player
	CITYVIC_WORKED,    // a citizen of this city works that tile
	// ⚖ ON SITE -- the resource is actually AVAILABLE here: an owned radius tile whose improvement TRADES it.
	// ⛔ NOT an ownership band and NOT derivable from one: `owned` is raw presence on an owned tile, improved or
	// not, so onSite is strictly stronger ([json.md] par.3.4). Two owned radius tiles can carry the same resource
	// with only one of them improved, which is why it is counted separately rather than filtered at the read.
	CITYVIC_ONSITE
};

class CityContext
{
public:
	CityContext()
		: m_city(NULL), m_areaId(-1), m_areaTileCount(0), m_maxAdjacentWaterTiles(0), m_holyCityCount(0), m_headquartersCount(0),
		  m_governmentCenterDistance(0) {}
	void bind(const CvCity* c) { m_city = c; }   // set once by the owning CvCity; the pointer IS the owner (never dangles)

	// --- STORED: the uniquely-owned aggregate -- the HAS_/IS_ plot-predicate COUNTS, event-maintained (onPlotChanged) ---
	// `mutable` like every other store here, so the const maintenance entry points can reach it.
	mutable ContextDict plotAttrs;
	// A plot ENTERED (sign +1) / LEFT (sign -1) the city's owned worked-radius set: fold its stable HAS_/IS_ attributes
	// (+/-1) into plotAttrs. COUNTS only; the plot is never stored. Reached ONLY from this store's own consumer, off
	// the SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED membership fact -- at play as it fires, and at load from the
	// buffered in-read facts once the stream has ended (DEC-spine-reseed).
	void onPlotChanged(const CvPlot* plot, int sign);
	// A radius tile's BONUS arrived / left, or its OWNERSHIP moved it between partitions. ±1, applied by this
	// store's own consumer off the plot facts -- never a radius walk, and never a fill pass
	// ([contexts.md]: the objection the dictionary answers is the REWALK).
	void applyVicinityBonus(int iBonus, CityVicinityPartition ePartition, int iSign) const;
	// ⚖ A MEMBER PLOT'S OWN BIT MOVED -- the ±1 the per-bit fact exists for. The PLOT announces the crossing
	// (SEVT_PLOT_PREDICATE_ADDED / _REMOVED) and this applies it; the city never reaches down to re-read the
	// plot's block ([contexts.md]). ⛔ THE ALTERNATIVE CANNOT WORK, which is why the fact was minted: a city-side
	// maintainer that "unfolds the old bits and refolds the new" runs after the plot already holds the NEW value,
	// so the old bits it needs are gone. ⚠ The failure if this route is missing is NOT a stale gate but a
	// COMPOUNDING MAGNITUDE -- plotAttrs is plane B's COUNT, so a bit never withdrawn leaves every deposit scaled
	// on it inflated permanently, and further inflated on every later substrate change.
	void onPlotPredicateChanged(int iPredicateId, int iSign) const;
	// The VICINITY store is fed by `applyVicinityBonus` off the plot facts' own payloads -- each already names
	// which band moves and in which direction, so nothing re-reads the plot to find out ([contexts.md]).

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

	// --- THE MAINTENANCE: reached ONLY through this store's own spine consumer -------------------------------------
	// ⚖ THE DECLARED INTEREST SET -- the facts that maintain this store, stated AT the store rather than in a
	// central switch that fans out to whichever store a case happens to name ([DEC-dict-is-a-consumer]). A fact
	// absent from this list does not reach the store, and that is readable HERE.
	static bool wantsEvent(int iEventId);

	// DIAGNOSTIC: how many distinct resources each of this city's bonus stores actually holds, emitted at load
	// end. ⛔ It exists because every one of these stores is INVISIBLE on every served surface, so "the lists are
	// empty" and "the lists are full but the deposits refuse" produce the SAME observable -- a short yield -- and
	// the difference between them is the difference between a broken store and a broken route. Reading the wiring
	// cannot settle it: the interest set, the appliers and the drain all read correct while the answer stayed
	// wrong ([DEC-no-guessing]: at a gap, EMIT the decomposition rather than infer it).
	// ⚠ Reports what the stores CONTAIN, never what they should -- it says nothing about correctness on its own,
	// and it is not state anything may fold on ([event-spine.md] § THE RECEIVED LINE).
	void reportBonusStoreCensus() const;

	// The two bonus lists, by id, for a SERVED census. ⛔ Read live at request time -- a load-end snapshot and a
	// mid-game read are different questions, and conflating them is what let a store that fills at load and
	// drains afterwards look correct. The two lists are ORTHOGONAL and are handed back separately for the same
	// reason they are stored separately: neither is derivable from the other.
	void collectBonusStores(std::vector<int>& tradedOut, std::vector<int>& onSiteOut) const;
	static void onSpineEvent(const CvSpineEvent& kEvent);

	// --- MAINTENANCE ENTRY POINTS: called ONLY from this store's own consumer ---------------------------------------
	// Each re-derives its WHOLE store from the bound city through the SAME engine accessors a read used to call --
	// ONCE per change instead of once per read, and one uniform derivation rather than a bespoke per-event delta.
	// CONSTRAINT: no choke point may call these directly. Every mutation that moves a stored fact emits its own DOMAIN
	// fact, so the consumer is the single trigger path; a direct call beside the event would be a second maintenance
	// surface for the same state.
	void refreshAreaFacts() const;         // the city's area ID + the coastal water-body size
	// ⚖ THE DESIGNATION COUNTS ARE DELTA STORES, applied ±1 by the fact that names the designation -- never
	// re-derived. Both used to be answered by asking CvGame once per entry of the whole religion / corporation
	// registry, which is read-time work that GROWS with a registry and therefore earns a store
	// ([contexts.md]: ask what the read WALKS; one pointer forwards, a scan stores). ⛔ There is deliberately
	// no `refresh*` twin: a fact that triggers a callback which goes and asks is the legacy read path moved
	// from read-time to event-time, not deleted.
	// ⚠ The authoritative assignment stays on CvGame, keyed by religion / corporation -- exactly one city each,
	// so uniqueness is STRUCTURAL there and a per-city bit could never guarantee it. The city holds only HOW
	// MANY name it, which is all any consumer of the bare verdict asks.
	void changeHolyCityCount(int iChange) const;
	void changeHeadquartersCount(int iChange) const;
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
	// ⛔ There is deliberately NO tier-less `hasVicinityBonus` here. A vicinity read WITHOUT a tier is the shape
	// that teaches vicinity == connected -- the one that existed answered `CASC_VIC_ONSITE`, so a caller
	// asking for vicinity silently got the far stricter OBTAINED verdict. VICINITY (is it on my ground) and
	// CONNECTED (does it reach me through the network) are ORTHOGONAL, not nested: a resource can be either
	// without the other. Ask `hasVicinityBonusAt` and NAME the tier ([json.md par.3.4](../../docs/specs/json.md)).
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
	bool isPowered() const;                   // CvCity::isPowered -- the HAS_POWER verdict; see the CONTEXT GAP note below
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
	bool hasFreshWaterAccess() const;         // CvCity::hasFreshWater -- the provider-building-fed ACCESS verdict
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
	// already answers O(1)).
	// ⚖ isPowered() forwards to CvCity::isPowered -- the ONE definition of "powered": a live grantor supplies it
	// AND no blackout gates delivery. What ANNOUNCES is that VERDICT's crossing, as SEVT_CITY_POWER_ADDED /
	// _REMOVED from the amenity fold; a status is middleware gating delivery, so it reaches the fold and stops
	// there, never becoming a store entry or a cascade input (state.md § A STATUS IS MIDDLEWARE).
	// ⛔ No LEG is the verdict -- a grantor arriving mid-blackout moves the store and delivers nothing -- so a
	// consumer routes on the verdict fact and never on a leg.
	// The headquarters designation announces SEVT_CITY_HEADQUARTERS_ADDED / _REMOVED, so a reader that needs to
	// react to it has a fact to hang on.

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
	// ⚖ NEUTRAL IS THE DEFAULT STATE -- IF THERE IS NO OWNER IT IS NEUTRAL (owner). So neutral is NOT a stored
	// partition and needs no fact of its own: it is the RESIDUAL, `all − owned − foreign`. That is what makes the
	// store maintainable at all, because the owner pair (`SEVT_PLOT_OWNER_ADDED / _REMOVED`) is guarded on
	// `!= NO_PLAYER` and therefore announces only the OWNED ends -- a stored neutral tier would have no announced
	// transition across `unowned ⇄ owned` and could not be kept correct by any delta.
	mutable ContextDict m_vicinityAll;         // the bonus is on a radius tile, whoever owns it -- moved ONLY by the bonus facts
	mutable ContextDict m_vicinityOwned;       // ...and that tile is owned by THIS city's owner
	mutable ContextDict m_vicinityForeign;     // ...and that tile is owned by ANOTHER player (the crossBorder opt-in only)
	mutable ContextDict m_vicinityWorked;      // a radius tile a citizen works this turn (the centre tile included)
	// ⚖ THE ON-SITE STORE -- the MAP half of the json par.3.4 `onSite` tier: an OWNED radius tile whose improvement
	// TRADES the resource. ⛔ Its own dictionary, not a filter over `m_vicinityOwned`: two owned radius tiles can
	// carry one resource with only one improved, so only a count answers it.
	// ⛔ ONSITE AND TRADED ARE TWO COMPLETELY SEPARATE LISTS, NEITHER DERIVABLE FROM THE OTHER (owner) -- you can
	// hold a resource on site and not in trade, having traded your only copy away. So this store answers on its
	// own and never consults the network count; a mounted unit needs horses ON SITE, a swordsman only needs iron
	// wares in the NETWORK ([json.md] par.3.4).
	// ⚠ The BUILDING half (a herd, a factory -- `provides.bonuses`) is NOT here: it is the enabler's operate/provides
	// fixpoint and the reader unions the two, exactly as it does for every other vicinity tier (the header's
	// VICINITY SPLIT). A herd building and an improved herd tile are the same act as far as the answer goes; they
	// simply have different owners.
	mutable ContextDict m_onSite;
	// ⚖ THE TRADED STORE -- what this city holds OVER THE NETWORK, maintained from the city's own acquisition
	// fact (SEVT_CITY_BONUS_ADDED/REMOVED, which CvCity::processBonus fires on a genuine 0<->non-zero crossing).
	mutable int m_areaId;                      // the city's area ID (-1 = unassigned)
	mutable int m_areaTileCount;               // that area's tile count -- AREA_SIZE served without dereferencing CvArea
	// The largest ADJACENT water body, in tiles -- ONE int that answers isCoastal at EVERY threshold
	// (isCoastal(N) == m_maxAdjacentWaterTiles >= N), so a new authored minArea needs no new store.
	mutable int m_maxAdjacentWaterTiles;
	mutable int m_holyCityCount;               // how many religions hold this city as their holy city
	mutable int m_headquartersCount;           // how many corporations are headquartered here
	// Plot distance to the owner's NEAREST government centre (0 here, or with none anywhere). One int answers
	// the whole distance-maintenance leg; it moves only on a government-centre crossing or a city gained/lost.
	mutable int m_governmentCenterDistance;
};

void cityContextRegisterConsumer();   // register on the event spine (from spineRegisterConsumers; idempotent)

#endif // CV_CITY_CONTEXT_H
