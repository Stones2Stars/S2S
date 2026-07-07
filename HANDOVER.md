# HANDOVER — 2026-07-08 (branch `json-data-migration`)

> **TRANSIENT one-time relay** (AGENTS.md handover rules): work done + work upcoming, nothing load-bearing
> lives only here. Every ruling below is captured durably in the named docs — read THOSE, not this summary.

## ⛔ Before you do ANYTHING

1. **Session-start protocol** (AGENTS.md banner): read every file in `docs/specs/` + `docs/architecture/` IN FULL.
2. **The tree deliberately does NOT compile** ([DEC-red-ratchet], AGENTS.md Build And Test ⛔): the 23 XML
   `CvXInfo` classes are archived in `SourceArchive/Infos/` as a fallback-proof ratchet. **NEVER restore them,
   never re-add a `CvXInfo`, never treat the RED build as a defect.** Green = the getter wiring below, finished.
3. **No insta-commits of code** (AGENTS.md Conventions, owner 2026-07-08): code lands in the WORKING TREE, the
   owner reviews the diff, THEN it commits. Curator + regenerated `Assets/Data` commit together freely.
4. Do not invent. It is all specced. A question posed to the owner is a hard stop.

## State: what this session landed (all committed)

- **The composable-unit foundation** (`1102a373a`): one unit class per json.md section in `Sources/JsonInfo/`
  (`CvJsonRequires`/`Edges`/`Allowed`/`Grants`/`Provides`/`Gate`/`BoolBlock`/`Modifiers`), write-once at load,
  const-only readers, composed BY VALUE **on the derived infos only** (the base `CvJsonInfo` carries ZERO section
  data — data-free virtual accessors, one dispatch, unconsumed-section census via `jsonNoteUnconsumed`). All 24
  subclasses recomposed per the data-grounded table + 12 new subclasses; parse primitives live in
  `JsonInfo/CvJsonParse.{h,cpp}`.
- **The not-to-spec cleanup** (same commit): the retired deposits model (`CvJsonInfo::deposits` /
  `CvCascadeDeposit`-on-the-info / `whenObsoleteDeposits`) is PURGED from all calc files; the compiled
  `DepositIndex` keeps its runtime shape and now compiles from `getModifiers()`/`getWhenObsolete()` (the
  cascade-side `CascadeDeposit` record). Phantom `shadowEngine`×4, the dead healdiff shadow, loadPrune members,
  stale epoch/stamp comments: gone. `cascadeStartNode` re-homed to `CvJsonTechInfo` (reset-recreate).
- **loadPrune is dead end-to-end** (`36a14af98` data + `e1fff9685` docs): the entity-level `enabled`/`disabled`
  gate ([DEC-entity-gate], json.md §2 Applicability) replaced it — curators emit it via `curate_common.gate_entity`,
  readJson parses it (`CvJsonGate`), `cascadeGateOk` evaluates it in the unit/building cascades.
- **perMilitaryUnit is dead** (`a357b8d67` + `4147ecfa0`): the ledger's named banned member now authors as the
  §3.7 `unit: IS_MILITARY`-qualified `cities` entry; `CvJsonModEntry` gained `unitQual` (entry-form AND node-form —
  the node form also un-drops cargo's live `{unit: IS_LAND}` data) and the `per` count-scaler + §6 COUNT leaves
  (`CASC_UNIT_COUNT`) the first cut missed.
- Docs commit `e1fff9685` also swept in the July-6 uncommitted docs work (disclosed to the owner; stands).

## Upcoming: the road to green (the standing task list)

1. **GC.get<X>Info accessor surface + every engine/AI/UI consumer** of the 23 archived classes onto
   `CvJson<X>Info` (live callers only — dead getters die with their callers). Solve the `DllExport` EXE-bound
   proxy signatures first (owner says solved — find the intended mechanism; do NOT guess).
2. **`enPromotionValid`** (`CvCascadeAccumulator.cpp:604`, includes `:38/:39/:43`) still reads archived Infos +
   `isPromotionValidLegacy` — rework onto CvJson surfaces; wire `cascadeGateOk(j->getGate())` into the promotion
   path (units/buildings already gated). Same class: `CvCascadeProperty.cpp:24` `GC.getPropertyInfo` read.
3. **RJ_REPO_TYPES dispatch gaps**: civilizations/eras/handicaps/gamespeeds/specialbuildings/leaderheads/
   specialunits/victories/votes/hurries/bonusclasses folders are NOT dispatched — the grants machine reads
   `InfoRepo<CvCivilizationInfo>` etc., which never populate. Add the dispatch rows.
4. **Verify live once loadable**: freeSpecialists count-leaves now parse — `sc_fsFold`/`fillFreeSpecialistsCity`
   must re-find their amounts; the `perScaled` census goes nonzero; the `[READJSON]` unconsumed-sections census
   should be EMPTY (anything in it is a representation gap — surface it, never work around it).
5. **Gate consumers** for culturelevels/unitcombats/promotionlines land with their systems' wiring;
   `gr_resolveBuilding` should consume the structured `repeatables()`/`foundBuildings()`.
6. Ranked-`cities` qualifiers (`max`/`orderedByDescending`, authored beside the flat) are still walk-ignored —
   inert by design until `plans/parked/ranked-target-selection.md` unparks; do not "fix" ad hoc.

## Where the truth lives

- The unit/representation model: `Sources/JsonInfo/CvJsonInfo.h` header comment + json.md.
- The rulings of 2026-07-08: [DEC-entity-gate], [DEC-red-ratchet] (decisions.md) + AGENTS.md Conventions.
- The cut/wiring worklists: `docs/plans/structural-cleanup/` (cutover.md, code-cut-map.md).
- Verification greps that must stay clean: `cascadeJson|CvCascadeJsonParse` (0), `loadPrune` outside
  tombstones (0), `perMilitaryUnit` in data (0), `->deposits|whenObsoleteDeposits` (0).
