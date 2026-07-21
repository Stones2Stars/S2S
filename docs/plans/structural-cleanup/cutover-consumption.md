# Cutover consumption + derived-state recompute-on-load

> The consumer/UI + derived-state half of the #430 cutover: once the cascade gate is authoritative, the surviving
> consumers (Python city screen, engine getters) must read the cascade, and every derived cache must recompute on
> load rather than be trusted from the save. Emerged from the enabler-consumption debugging; the cascade FRONTIER
> itself reads game state correctly (tech/bonus/building HAVE) — these are the CONSUMER + LOAD-PATH gaps around it.

## ⚖ Rulings (owner)

- **The Python city screen relies PURELY on the cascade.** No legacy-getter bridge for the buildability gates —
  the screen calls the flipped `canConstruct`/`canCreate`/`canTrain`, never legacy getters ("otherwise we end up
  with a ton of extra shoehorning and calcs"). Legacy `can*Legacy` survive only as the AI's per-building data
  source (to be trimmed in a later getter pass).
- **Manufactured resources are TRADE-NETWORK + vicinity, not vicinity-only.** A building-produced bonus enters the
  empire-wide trade network (the engine free-bonus mechanism) AND the in-vicinity supply.
- **Never serialize a cache; recompute derived state fresh on EVERY load** ([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted)).
  The "serialize the cache, recalc when the checksum says it is stale" model is DEAD: with the XML gone the asset
  checksum no longer mismatches, so `recalculateModifiers` never auto-fired on load and every load trusted stale
  derived state. The fix is to delete that model — never serialize a derived cache, recompute it on load — after
  which the checksum is unnecessary.
- **`-1` is the universal UNSET sentinel** (every engine int is signed for exactly this). A poco getter stub that
  returns `0` where the field is unset is a bug (the `getMaxPopulationAllowed` help-text-on-every-building class).

## Landed fixes (working tree)

- **Visible/greyed build-list frontier** ([enabler.md §6](../../specs/enabler.md)): the flip previously dodged
  `bTestVisible=true`, so the Python list ran on legacy. Added `CvCascadeEvalFlags::testVisible` (relaxes the
  GREYABLE clauses — connectable `BONUS_`, unadopted `CIVIC_` — keeping hard hides: tech, `notConstructible`,
  obsolete, terrain, building prereqs), a `bVisible` fill on `BuildingCascade::buildable` / `UnitCascade::trainable`
  / `EnablerKernel::gateSet`+`requiresMet`, parallel `enBuildableVisible`/… sets + `enConstructVisible`/… accessors,
  and routed `canConstruct`/`canCreate`/`canTrain(bTestVisible=true)` to them.
- **`trade|vicinity` eval bug**: `ev_bonusPresent` `CASC_CONN_TRADE_OR_VICINITY` only checked `hasBonus` (trade),
  dropping the vicinity leg → building-supplied bonuses invisible. Now `hasBonus OR ev_vicinityHas`.
- **`getFreeBonuses()` ← `provides.bonuses`** (`CvBuildingInfo::reconstructFromComposed`): the poco stub was empty,
  so `processBuilding`'s free-bonus loop (`CvCity.cpp:4739` → `changeFreeBonus` → `m_paiFreeBonus` → the plot-group
  TRADE network + the vicinity caches) added nothing → manufactured/culture bonuses were vicinity-only. Sourced from
  `provides.bonuses` (both are the legacy `ExtraFreeBonuses`); count 1 (the provides model dropped `iNumFreeBonuses`).
- **Project consumption** (`CvMainInterface.py`): the project list called `canCreate(iType, True, False)` —
  `bContinue=True` bypassed the cascade flip → legacy → every project shown. Now `canCreate(iType, False, False)`.
- **`getMaxPopulationAllowed()`** poco stub `return 0` → `return -1` (unset), killing the "Sets base max population
  at 0" help text on every building.
- **`recalculateModifiers`-on-load — REMOVED.** The load-branch `sendRecalculateModifiers()` post is deleted. It
  re-ran `processBuilding`/`processBonus`/`changePowerCount` across every city on every load, spraying ~100k+ spurious
  "changed" DOMAIN events — re-application, not real state changes: the broken-cascade churn the owner flagged — and it
  rebuilt legacy accumulators the cascade replaces anyway. The cascade is built from events (the read-driven reseed —
  [DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed)), never a recompute; legacy accumulators that
  still need correct-on-load die at their channel's cascade cut. Its legacy-correctness pass is moot pre-cutover (the
  game is not playable, parity invalid). ⚠ The retire-serialization audit below was written *around* this recompute
  path and is largely superseded — reconcile it to the reseed model when the read-driven load lands.

## The retire-serialization audit (chosen scope — the real end-state)

The derived-state-on-load fix, done properly rather than a recalc-on-load band-aid.

### The grounded mechanism (recalculateModifiers is NOT a load path — never was)

⛔ **`recalculateModifiers` has NEVER fired on a normal load (owner-confirmed) — any code comment claiming it
"repopulates X fresh on load" is FALSE.** It runs ONLY via the human-accepted `BUTTONPOPUP_MODIFIER_RECALCULATION`
popup, which `CvInitCore::checkVersions()` (`CvGame.cpp:588`, load branch) raises ONLY when
`m_uiSavegameAssetCheckSum != m_uiAssetCheckSum` — i.e. a manual, human-triggered **asset-change repair** path, not
part of the normal load flow. It is additionally **network-synced** (`GAMEMESSAGE_RECALCULATE_MODIFIERS` →
`CvNetRecalculateModifiers`) and rewrites synced state wholesale (plot groups, trade routes, building enable/disable,
sight, graphics, area power), so it *cannot* be auto-fired on load anyway. With the replaced-info XML gone the checksum
stopped mismatching, so the popup no longer appears — which is **FINE**: the destination was never "revive the popup."

### The model (owner): ALWAYS rebuild all caches on load

Because **no cache serialization is allowed**, the derived-state recompute **must run on EVERY load** — the *content*
of `recalculateModifiers` becomes the mandatory "build all caches on load" step, not an asset-change repair. What is
deleted is the **serialization** (nothing derived is written) and the **checksum/popup GATE** that made the recompute
conditional (`checkVersions` / `calculateAssetCheckSum` / `m_uiSavegameAssetCheckSum` / `BUTTONPOPUP_MODIFIER_RECALCULATION`) —
NOT the recompute itself. The recompute stops being popup/network-bound because that binding was a **mid-game** concern
(a mid-game state mutation must be MP-synced, hence `GAMEMESSAGE_RECALCULATE_MODIFIERS`); on **load** every client
rebuilds deterministically and independently, so the load-time rebuild needs no popup and no network message.

**Consequences for the audit:**

- The CASCADE's load build is the **event reseed** — NOT the recompute-from-state warm-up (`playerSliceRebuild` +
  `worldRebuild` are REMOVED — [f0-eventspine-invalidation.md](f0-eventspine-invalidation.md) /
  [DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed)): the save read fires the DOMAIN events for
  every fact at load and the cache-build consumer populates every cascade package (the plot-yield cache, a
  game-object cache, still warms from its own state). The **legacy** accumulators are a SEPARATE half: the reseed's events feed the cascade
  consumer, not the legacy apply-loops, so each legacy accumulator that must be correct-on-load keeps its
  `recalculateModifiers`-content recompute UNTIL its channel is cut to the cascade — at which point the reseed
  subsumes it. ⚠ Whether `recalculateModifiers` can be removed WHOLESALE (i.e. the reseed already re-runs the legacy
  apply-loops it drove) vs. staying until the last legacy channel cuts is an open build-time question — verify, do
  not assume.
- Any code comment claiming `recalculateModifiers` "repopulates X fresh on load" describes a path that never ran on a
  normal load — so `m_aiSpecialistCommerce100` (skip-read) and `m_paiFreeBonus` (serialized-trusted) are BOTH latently
  wrong on load today, fixed only when a human accepted the (asset-change) popup. The unconditional load rebuild fixes
  both.
- **Mechanical item to verify (not assume — [DEC-no-guessing]):** WHY the current entry is popup/network-only, and
  whether the recompute content is sound to invoke directly at `onFinalInitialized` on load for all players (the
  network path is a mid-game-sync artifact vs a load-time state-readiness requirement). Confirm before wiring.

### ⚠ The per-cache side-effect split (map, never assume — [engine.md] changer-body audit rule)

A legacy accumulator's recompute is NOT uniformly a pure read-cache warm. Two kinds, and the kind decides the hook:

- **Pure read-cache** (no synced side effect) — recompute anywhere post-load; drop serialization outright. The cascade
  read-caches are already here.
- **Recompute-with-synced-side-effect** — e.g. `m_paiFreeBonus`: its changer `changeFreeBonus` ripples into
  `updatePlotGroupBonus` (the **synced** trade network) and the vicinity caches. Recomputing it is a targeted subset of
  `recalculateModifiers` (clear, then re-apply every present building's `getFreeBonuses`←`provides.bonuses`), and it
  must be **sequenced consistently with plot-group establishment** (`RecalculatePlotGroupHashes`/`updatePlotGroups`),
  not dropped into the read-cache warm arbitrarily. Each such field's changer body is audited for its rider effects
  before its recompute hook is chosen.

### The steps

1. **Enumerate the serialized derived-state surface** — the free-bonus has-list (`m_paiFreeBonus`), the accumulators
   `recalculateModifiers` (game/team/player/city) rebuilds, and any other cache written to the save. (The `:17617`/`:17923`
   "recalculateModifiers repopulates it fresh on load" comments in `CvCity.cpp` mark instances.) Classify each:
   **DERIVED** (retire + recompute), **GENUINE STATE keep-serialized** (raw inputs, timers, and the event/vote
   one-shot grants that are NOT derivable — [state-repositories.md](../../architecture/state-repositories.md)
   "Event/vote grants are a SEPARATELY PERSISTED store"), or **already-handled** (cascade caches, never serialized).
2. **Respect the cutover boundary** — the wellbeing/modifier accumulators are slated to die at their channel's cascade
   cut ([modifier.md §2b](../../specs/modifier.md), "repaired wholesale at the cutover"). For those the interim is
   correct **recompute-on-load**, not a premature serialization retirement that duplicates the cut. Only caches that
   are purely derived and NOT owned by a pending cascade cut (the `m_paiFreeBonus` class) get the full two-stage
   retirement now.
3. **Retire each retiring cache's serialization** ([save.md §3](../../specs/save.md)): FULL-DELETE the `WRAPPER_WRITE`
   AND the read (no `WRAPPER_SKIP_ELEMENT`) and name the field's tag in `savemigration.txt`, which drains the orphan
   tag from old saves so nothing desyncs. Removing a read *without* the ledger entry is the one hard desync — the tag
   MUST be listed.
4. **Recompute each on load** from live state at the load-safe hook its side-effect profile allows (step ⚠), sequenced
   BEFORE any consumer (the plot-group trade network, the operating/dormancy set — "the operating-buildings set should
   not be computed until the has-list is loaded").
5. **Drop the checksum-recalc path** once nothing derived is serialized — `checkVersions`/`calculateAssetCheckSum`/
   `m_uiSavegameAssetCheckSum` + the `BUTTONPOPUP_MODIFIER_RECALCULATION` popup existed only to detect stale serialized
   caches; with none serialized there is nothing to detect.

## Poco-getter sweep (the #430 getter audit → fix)

Systematic sibling of `getMaxPopulationAllowed`: audit the poco getter STUBS (the `CURATOR-GAP` / `DROP_DEAD`
`return`-literal getters) for wrong defaults — `0` (or an unsigned assumption) where `-1`/unset is meant, which the
UI/help/AI then renders or mis-gates. The unset convention is `-1` everywhere.

**The audit ran (2026-07-11, 23 pocos × audit+adversarial-verify):** every getter classified
REAL / RECOVERABLE / DEAD / UNCERTAIN with evidence + a concrete data source. Counts: REAL 125 · **RECOVERABLE 44**
(wire) · DEAD ~103 (owner-confirm) · UNCERTAIN ~22 (owner-rule). The RECOVERABLE 44 is the fix worklist; DEAD +
UNCERTAIN go to the owner for confirmation.

**⚖ Ruling (owner 2026-07-11) — an option-gated getter's logic moves to the CONSUMER, never the poco.** Some audit
"RECOVERABLE" recs would have a pure-data poco read `GC.getGame().isOption(...)`, which [DEC-json-not-cascade] bars.
The resolution is NOT to relax the rule: the option-gated logic moves to the consumer, which reads the poco's static
flags + the live options. **Landed exemplar:** `CvTraitInfo::isValidTrait` (which *unconditionally returned `false`,
blocking every `setHasTrait`*) → **`CvGame::isTraitValid(eTrait, bGameStart)`** (faithful transcription of the archived
body minus the retired per-trait OnGameOptions loop — the simple/complex folder split replaces that gate); all 11
call sites (CvPlayer ×8, CvGameTextMgr ×3) rewired; the poco getter retained as a documented dead stub.

**Landed getter fixes (working tree, compiling):** `CvFeatureInfo::getNumVarieties`/`canBeSecondary` (delegate to the
art define) · `CvHeritageInfo::getEraCommerceChanges100` (`jsonX100` at parse — was truncating fractional heritage
commerce to 0) · `CvCorporationInfo::getMilitaryProductionModifier` (read the array form) ·
`CvBuildingInfo::getExtraPlayerInstances` (`identity.maxPlayerInstancesExtra`) · `CvTechInfo::getDCMAirBombTech1/2`
(`capabilities.has`) · `CvGame::isTraitValid` (the consumer move above) · the `CvBuildingInfo` **counter-damage
combat family** (`getDamageToAttacker`/`getDamageAttackerChance`/`isDamageAllAttackers`/`isDamageAttackerCapable`/
`getMayDamageAttackingUnitCombatType`+Num/is — a bespoke `mapFrom` read of `defense.city.counterDamage.{damage,chance,
units.unitCombats}`, replacing dead modifier-address reads) · `CvBuildingInfo::isForceTeamVoteEligible` (raw read of
the `enables.votes` "FORCE_TEAM_ELIGIBLE" marker the edge parser drops) · the **10 `cy*` Civilopedia wrappers**
(`cyGetTechYieldChanges100`/`cyGetTerrainYieldChanges`/`cyGetFreePromoTypes`/… — the backing `IDValueMap`s are
populated by `reconstructFromComposed` and iterate via `foreach_` with `value_type` == the archived pair typedefs, so
the archived bodies port verbatim) · `CvBuildingInfo` **plot-yields** (`getRiverPlotYieldChange` + `getPlotYieldChanges`
— a `mapFrom` predicate parse of `<yield>.city.plots.flat`: HAS_RIVER→river array, IS_WATER/HAS_HILLS/HAS_PEAK/IS_LAND→
per-PlotType `m_aPlotYieldChanges`; this also activates `cyGetPlotYieldChanges`). **✅ `CvBuildingInfo` RECOVERABLE
items are COMPLETE.** **Remaining 44-list work** splits into
C++-only reader/`mapFrom` fixes and curator+regen fixes (emit a datum, regenerate `Assets/Data`); the full
per-getter worklist with data sources is the audit's distilled table.

**⚠ The audit is a mechanical classifier — apply the ARCHITECTURE lens per getter.** A "RECOVERABLE" verdict is a
data-availability finding; whether the fix belongs on the poco (vs a consumer, vs a curator regen) is a separate
judgment the fixer makes against the specs (`isValidTrait` was the first collision). Do not blind-apply.

**The consumer-move batch (option-gated getters, per the ruling above) — ✅ DONE.** `isValidTrait` →
`CvGame::isTraitValid`; **`CvCivicInfo::getCityLimit`** → **`CvGame::getCivicCityLimit`** (the
`GAMEOPTION_EXP_OVEREXPANSION_PENALTIES` × world-size formula; poco keeps the raw `getCityLimitBase()` =
`happiness.empire.cityLimit.flat`; all ~11 call sites — CvPlayer/CvPlayerAI/CvCity/CvDLLWidgetData — rewired).

**✅ The C++ reverse-index / bespoke-parse batch is DONE** (all in `doPostLoadCaching`, correctly ordered — mapFrom
populates the pocos at lines 777-857, BEFORE `doPostLoadCaching` at 965; `cascadeLoadJson` at 984/1079 is only the
FK reverse-index tail): `CvTechInfo::getLeadsToTechs` trio (invert the prereq graph) · `CvImprovementInfo::getBuildTypes`
(build→improvement inverse) · `CvBonusInfo::getTradeProvidingImprovements` (build×bonus loop, adder on `CvBonusInfo`) ·
`CvTraitInfo::getSpecialistYield/CommerceChangeArray` (recompute-into-mutable-buffer, reusing the per-index getters).
**⚠ NOTE (verified 2026-07-11):** an initial hypothesis that `doPostLoadCaching` runs BEFORE the JSON populates the
pocos (which would have been the "no promotions" root cause) was WRONG — mapFrom runs first (lines 777-857). The
promotion bug is NOT this ordering; likely `unitCombats` under `skills` vs `identity` (confirm at `getQualifiedUnitCombatType`).

**Remaining RECOVERABLE work by fix-type:**

- **`getPropertyManipulators` family** (7 pocos: Property/Specialist/Civic/Heritage/Trait/Feature/Improvement) —
  ⚖ **PERMANENT carve-out (#429 property spatial-distribution): this is NOT a getter-wire.** Building a
  `CvPropertySource` needs an **`IntExpr` expression tree** for the amount (`CvPropertySourceConstant(PropertyTypes,
  const IntExpr*)`), no poco constructs one today, and the spatial intent it carries (`on`/`relation`/`distance`, the
  `grants.repeatable` pollution pulse) is the **pending #429 property-distribution** work (json.md §5, "curator-to-spec
  pending"). Wiring it piecemeal ahead of that model is premature — it lands with the property-engine cut, as the audit
  itself flagged.
- **Curator + regen** (commit derived data): ✅ **`CvUnitInfo::getReligionSpreads`/`getCorporationSpreads` DONE** —
  landed as the new **`spread.religion` / `spread.corporation`** block (owner ruling 2026-07-11: spread strength is a
  standing capability, NOT a timed `grants` handout; `identity` is strictly self-description, never a catch-all). Full
  two-sided landing: `CJK_INTRINSIC_KEYS += "spread"` (classifier) + `curate_unit.py` (`_keyed_int` emit, out of
  `GRANT_LIST`) + `CvUnitInfo` `readKeyedIntSimple` read + regen (2073 units, 53 with spread) + json.md §7/§9 spec.
  ✅ **`isNoRevealMap` DONE** (`CAP_BOOL` + `skill("noRevealMap")`; base corpus 0 — only a zWIP module authors it —
  so faithfully inert). ⚖ **Animal-ignores — a game-option-driven BITMASK (short) (owner 2026-07-11):**
  `CvUnit::getAnimalIgnoresBordersCount()` returns a **short bitmask** of the boundaries ignored under the active
  options — independent bits `ANIMAL_IGNORE_BORDERS|_IMPROVEMENTS|_CITIES`, NOT a cumulative tier: `STAY_OUT`→0,
  `RECKLESS`→borders, `DANGEROUS`→borders|improvements (OR'd). **The consumer side understands the mask** — the three
  `can*` getters bit-test their own bit (decoupled). Purely game-option-derived; NOT curated unit data. Consequently
  the curated `CAP_COUNT` emission is REMOVED (regen), the poco `canAnimalIgnoresBorders`/`getAnimalIgnoresBorders`
  are DEAD, and the **dead producer side was fully cleaned up (2026-07-11):** `m_iAnimalIgnoresBordersCount` + its
  changer + the two promotion/unitcombat apply sites removed, and the serialized field retired via
  `WRAPPER_SKIP_ELEMENT` + `savemigration.txt` (two-stage). ✅ **The remaining curator items are DONE:**
  **flanking** (`getFlankingStrengthbyUnitCombatType` — fixed `_pairs` to accept `Combat`-ending keys; `getFlankingStrikeUnits`
  — added `FlankingStrikes`→`flankingUnit` to VS_KEYED + an IDValueMap read; 14 by-UC + 114 by-unit emit) · **`groupSpawn`**
  (its OWN block of struct rows `{unitCombat, chance, title}` — owner-principle home, NOT `grants`; classifier reserved-key
  added; 136 units) · **`CvTechInfo::getDomainExtraMoves`** (added `targetType: "domains"` to the `mapping/TechInfo.json`
  channel → `domainMoves.empire.domains.{DOMAIN}.flat`; poco bespoke read; 12 techs).
- ✅ **`isMilitarySupport`/`isMilitaryProduction`/`isMilitaryHappiness` — DONE:** unified onto the `military` tag /
  `IS_MILITARY` (owner-confirmed 2026-07-11, per skills.md §3 — a deliberate behaviour change; the differing legacy
  corpora collapse to one verdict). The audit's "re-emit distinct bools" was overruled by the owner.
- **`getGroupSize`/mesh-group getters — PERMANENT carve-out (mesh-groups)** (UnitMeshGroups, graphics/animation only). Stubbed by design.

**⇒ Every RECOVERABLE getter that is a getter-wire is DONE. The only untouched RECOVERABLE items are the two
owner-ruled PERMANENT carve-outs (#429 property spatial; mesh-groups).**

## Verified working (do not re-chase)

- The cascade FRONTIER reads tech/bonus/building HAVE correctly — proven live: tech-gated buildings
  (`PLASTIC_SURGERY_CLINIC`/`TECH_MODERN_HEALTH_CARE`, `AMUSEMENT_PARK`/`TECH_PSYCHOLOGY`, …) sit in the buildable
  set, and the city screen is 100% on the cascade branch. Small buildable counts in mature cities are correct
  (`hasBuilding` excludes already-built).
- Manufactured resources / dormancy / cultures are fixed by the `getFreeBonuses` bridge once the free-bonus has-list
  is fresh (confirmed: a `recalculateModifiers` makes them all appear) — the recompute-on-load audit removes the
  need for the manual recalc.

## Fix with the getter sweep (owner ruling: getters first)

- **`CvJsonModifiers` string lookup on the HOT PATH.** The runtime modifier calc rides the compiled int-interned
  `DepositIndex` (fine), but some POCO GETTERS do the string-keyed `getModifiers()->all()` dotted-address match
  DIRECTLY at read — confirmed: `CvBuildingInfo::getExtraHealth()` and the `CvCivicInfo` `civSum*`/`civCollectKeyed*`
  helpers. So the `map<string,family>` matching is paid per-read by AI/eval callers, not just at load. Sequenced
  AFTER the poco-getter sweep: once every getter returns a real (or death-confirmed) value, re-home these hot reads
  onto a compiled/precomputed representation. Perf pass, not correctness — but NOT load-time-only.

## Pending

- **Pseudo-building chains break at renaissance** — the property-band (crime/education/disease/pollution) building
  chain; not yet traced.

## See also

- [enabler.md](../../specs/enabler.md) §6 (HIDDEN/GREYED/LISTED) · [cutover.md](cutover.md) (the machine cuts) ·
  [engine.md](../../reference/engine.md) §Save/load (the two-stage serialization retirement) ·
  [DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted).
