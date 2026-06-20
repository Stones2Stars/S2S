#pragma once
#ifndef CV_CASCADE_MOVEMENT_H
#define CV_CASCADE_MOVEMENT_H

#include <vector>

class CvUnit;
class CvPlot;

//
//	CvCascadeMovement -- the MOVEMENT RESOLVER (owner ruling 2026-06-20: moveCost is a RESOLVER SUBSYSTEM, not a
//	modifier family). Base move cost is INTRINSIC plot data read per-(unit, edge): terrain/feature/route OWN their
//	cost; only cascading DELTAS (route tech changes, promotion discount/credit) are modifier families. This resolver
//	reproduces CvPlot::movementCost -- the branch tree mapped in `modifier.md` §6.6 / `legacy-value-calc-map.md`
//	§10.1 -- but sources the PLOT-SUBSTRATE cost from the migrated JSON (terrain/feature/route
//	`identity.movementCost` + route `identity.flatMovementCost`), so the movement SHADOW can diff the cascade's
//	number against the FRESH legacy decomposition (`MoveCostParts::iFinal` in CvHttpServer, NOT the AI-cached
//	`engineCost`) and license cutting the legacy terrain/feature/route `getMovementCost()` reads.
//
//	⛔ CUT 1 (this build) = the PLOT-SUBSTRATE channel only. terrain/feature/route costs come from the cascade JSON;
//	the UNIT-SIDE aggregate (baseMoves, moveDiscount, the double-move / ignore-terrain / flat-cost predicates) AND
//	the hills/river/peak/denominator globals are read from the engine / GC -- they are the unit-plane MODIFIER
//	FAMILY (the self-accumulator, "largest surface, last") and the Tier-G config globals, NEITHER migrated yet. A
//	divergence therefore localises to the terrain/feature/route JSON migration -- exactly the deletable plot-
//	substrate reads. When the unit-plane lands, the unit-side swaps engine->cascade behind THIS interface and the
//	shadow re-validates (the per-channel build, `shadow.md` §8). TEMPORARY harness; folds into full readJson.
//

// The plot-substrate intrinsic move costs, indexed by engine Type. terrain/feature: `identity.movementCost`.
// route: `identity.movementCost` + `identity.flatMovementCost`. Values are the HUMAN ints == the legacy small ints
// (moveCost is NOT the ×100 fixed-point of the modifier cascade -- the resolver multiplies by MOVE_DENOMINATOR,
// exactly like legacy). A slot left at -1 = no cascade datum found (the resolver falls back to legacy + flags it).
struct CvMovementSubstrate
{
	std::vector<int> aiTerrainMoveCost;  // [TerrainTypes]
	std::vector<int> aiFeatureMoveCost;  // [FeatureTypes]
	std::vector<int> aiRouteMoveCost;    // [RouteTypes]
	std::vector<int> aiRouteFlatCost;    // [RouteTypes]
	int  iTerrainParsed, iFeatureParsed, iRouteParsed, iMissing; // diagnostics
	bool bLoaded;
	CvMovementSubstrate()
		: iTerrainParsed(0), iFeatureParsed(0), iRouteParsed(0), iMissing(0), bLoaded(false) {}
};

// Lazily parse + cache the plot-substrate move costs from the terrain/feature/route JSON. Game thread (reads GC
// type indices + the live mod path). Parse-once per process, like the other readJson indices -- harmless for the
// diagnostic shadow (a reload keeps the prior parse; the data is static).
const CvMovementSubstrate& cascadeMovementSubstrate();

// The recomposed cascade per-edge cost + its decomposition (the cascade twin of MoveCostParts in CvHttpServer).
struct CvCascadeMoveCost
{
	int  iFinal;          // the recomposed cascade cost -- the value the shadow diffs vs the FRESH legacy iFinal
	bool bRouteBranch;    // took the route-override branch
	bool bEarlyFlat;      // flat-cost / air early return (or a non-substrate early return -- reads engine)
	bool bIgnoreTerrain;  // ignoreTerrainCost, or the discount drove the terrain stack to <= 1
	int  iTerrain, iFeature, iHills, iRiver, iPeak; // additive regular-branch adders (terrain/feature from cascade)
	int  iRouteCost, iRouteFlatCost;                // route branch (route cost/flat from cascade)
	int  iDiscount, iRegularPreDenom, iDoubleDiv;
	bool bSubstrateMiss;  // a terrain/feature/route had no cascade datum -> fell back to legacy for that term
	CvCascadeMoveCost()
		: iFinal(0), bRouteBranch(false), bEarlyFlat(false), bIgnoreTerrain(false),
		  iTerrain(0), iFeature(0), iHills(0), iRiver(0), iPeak(0), iRouteCost(0), iRouteFlatCost(0),
		  iDiscount(0), iRegularPreDenom(0), iDoubleDiv(1), bSubstrateMiss(false) {}
};

// The RESOLVER: compute the cascade per-edge move cost for (unit, from -> to), reproducing the legacy branch tree
// with the PLOT-SUBSTRATE cost sourced from the cascade JSON (unit-side + globals from engine/GC -- cut 1). The
// early returns + the unit-side are read identically to legacy, so a divergence ISOLATES to the plot-substrate
// migration. Game thread; const reads only.
void cascadeResolveMoveCost(const CvPlot* pTo, const CvUnit* pUnit, const CvPlot* pFrom, CvCascadeMoveCost& out);

// Classify a cascade-vs-legacy moveCost divergence into a cause-tag + provisional care rung (the movement slice of
// the shadow care scale, `shadow.md` §4 -- reuses the ModifierCareLevel enum). iDelta = cascade - legacyFresh.
// ONE definition so the endpoint + any per-turn line agree. Writes the care rung (0..5) into iCareOut.
const char* cascadeMoveClassify(int iDelta, const CvCascadeMoveCost& cc, int& iCareOut);

// ===================== the UNIT-PLANE movement/range channel (the modifier-family slice) =====================
// The unit-side CREDIT (baseMoves), the -cost DISCOUNT, the RANGE radius, and the movement CAPABILITIES ARE
// modifier families -- the unit-plane self-accumulator (`modifier.md` §5/§6.6, "largest surface, last"). This is
// their MOVEMENT slice: parsed per source (unit TYPE / promotion / unitcombat) and aggregated per live unit by
// summing the type + the promotions/unitcombats it currently has -- the cascade's own reconstruction of the
// engine's m_iExtra* stack. The per-unit shadow diffs this against the engine's MIGRATED parts (UnitInfo +
// getExtra*), and the per-SOURCE attribution (each contributing promo/unitcombat's exact contribution) is the
// META observability rung -- reconstruct WHY a unit moves as it does, from the wire alone.
//
// Verified engine model (CvUnit.cpp, 2026-06-20): baseMoves = UnitInfo.getMoves() + getExtraMoves() (own +
// commander/commodore) + (domain != AIR ? team.getExtraMoves(domain)); getExtraMoveDiscount = m_iExtraMoveDiscount
// (own + commander); airRange = UnitInfo.getAirRange() + getExtraAirRange() + team.getExtraMoves(AIR) + national.
// ignoreTerrainCost / flatMovementCost are UNIT-INFO-only flags (NOT promo-fed) + a runtime canFliesToMove() OR;
// terrain/feature/hills double-move counts ARE promo/unitcombat-fed. So the cascade aggregates moves/discount/range
// over type+promo+combat, ignore/flat from the TYPE only, double-move over type+promo+combat. The team/national
// scopes + commander cross-edge + flying runtime are NOT migrated -- they are the shadow's expected residue.

// One parsed movement/range source (a unit TYPE, a promotion, or a unitcombat). Capabilities are TYPE-KEYED in
// legacy + the JSON (`capabilities.terrainDoubleMove: {TERRAIN_X: true}`) -> stored as sorted type-index vectors.
struct CvMoveSourceProfile
{
	int  iMoves;           // movement.unit.moves.flat (+ identity.base.moves for a unit TYPE)
	int  iMoveDiscount;    // movement.unit.moveDiscount.flat
	int  iRange;           // range.unit.flat (unit base) + air.unit.range.flat (promo/combat delta)
	bool bIgnoreTerrain;   // capabilities.ignoreTerrainCost (UnitInfo-only in the engine -> type sources only)
	bool bFlatMoveCost;    // capabilities.flatMovementCost  (UnitInfo-only)
	bool bHillsDoubleMove; // capabilities.hillsDoubleMove
	std::vector<int> aiTerrainDM; // capabilities.terrainDoubleMove keys (TerrainTypes)
	std::vector<int> aiFeatureDM; // capabilities.featureDoubleMove keys (FeatureTypes)
	bool bAny;             // any movement-relevant field present (else skipped in aggregation)
	CvMoveSourceProfile()
		: iMoves(0), iMoveDiscount(0), iRange(0), bIgnoreTerrain(false), bFlatMoveCost(false),
		  bHillsDoubleMove(false), bAny(false) {}
};

// The parse-once unit-plane move data. aMovePromoIdx / aMoveCombatIdx hold the indices whose profile is non-empty,
// so the per-unit aggregation iterates only the movement-relevant promotions/unitcombats (a few dozen), not all.
struct CvMovementUnitData
{
	std::vector<CvMoveSourceProfile> aUnit;   // [UnitTypes]
	std::vector<CvMoveSourceProfile> aPromo;  // [PromotionTypes]
	std::vector<CvMoveSourceProfile> aCombat; // [UnitCombatTypes]
	std::vector<int> aMovePromoIdx;
	std::vector<int> aMoveCombatIdx;
	int  iUnitParsed, iPromoParsed, iCombatParsed; // sources with any movement/range/cap datum
	bool bLoaded;
	CvMovementUnitData() : iUnitParsed(0), iPromoParsed(0), iCombatParsed(0), bLoaded(false) {}
};
const CvMovementUnitData& cascadeMovementUnitData();

// One contributing source in a live unit's aggregated profile (the META per-source attribution). pProfile points
// into the parse-once cache (stable for the process), so the endpoint can render the exact per-source contribution.
struct CvMoveSourceRef
{
	int iKind; // 0 = unit type, 1 = promotion, 2 = unitcombat
	int iType; // the engine Type index
	const CvMoveSourceProfile* pProfile;
	CvMoveSourceRef() : iKind(0), iType(-1), pProfile(0) {}
};

// A live unit's aggregated cascade movement/range profile + the per-source attribution. The *Migrated values are
// the cascade's reconstruction of the engine's MIGRATED parts (UnitInfo + own getExtra*), so a delta vs the engine
// isolates the migration error (the team/national/commander/flying residue is engine-only, deliberately excluded).
struct CvUnitMoveAgg
{
	int  iMovesMigrated;   // cascade(type base.moves + Σ held promo/combat moves)
	int  iMoveDiscount;    // cascade(Σ type/promo/combat moveDiscount)
	int  iRangeMigrated;   // cascade(type range + Σ held promo/combat range)
	bool bIgnoreTerrain;   // cascade(type only)
	bool bFlatMoveCost;    // cascade(type only)
	bool bHillsDoubleMove; // cascade(type + held promo/combat)
	std::vector<int> aiTerrainDM, aiFeatureDM; // aggregated keyed sets (sorted, deduped)
	std::vector<CvMoveSourceRef> sources;      // attribution: the type + each contributing held promo/combat
	CvUnitMoveAgg()
		: iMovesMigrated(0), iMoveDiscount(0), iRangeMigrated(0),
		  bIgnoreTerrain(false), bFlatMoveCost(false), bHillsDoubleMove(false) {}
};
// Aggregate a live unit's cascade movement/range profile from the parse-once data (type + held promotions/
// unitcombats), with the per-source attribution. Game thread (reads the live promotion/unitcombat set).
void cascadeUnitMoveAgg(const CvUnit* pUnit, CvUnitMoveAgg& out);

#endif // CV_CASCADE_MOVEMENT_H
