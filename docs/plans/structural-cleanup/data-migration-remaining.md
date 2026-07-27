# Data-migration remaining — the curator/JSON worklist

> **The #1-priority tier ([DEC-data-first](../../architecture/decisions.md#dec-data-first)).** Every item here is
> curator/JSON data work that must be finished **before** downstream cascade/shadow/observability work — a deferred
> data item forces every consumer to ASSUME its eventual shape. This doc is the consolidated output of the
> 2026-07-01 audit (the full spec surface read + the authoritative curator-code audit + two adversarial completeness
> sweeps). It is **migration-transient** — deleted when the migration completes.

## Method / completeness attestation

Audited: the whole `docs/specs/` surface (read in full) + the curator code (`Tools/Migration/curate_*.py`) + two
completeness sweeps for hidden/lost data:

- **Curator inventory** — 34 curators, one per `✅` naming.md infotype, all output folders populated. No orphan gaps.
- **Silent identity-sink sweep** (the 8 thin `cc.main` curators — `curate_common.py:413-416` routes unrecognized
  tags into `identity` with no report): **CLEAN — 0 parked gameplay data.** Every sink field is a documented,
  deliberate `identity`/`mapGeneration`/deferred-subsystem disposition per json.md §7.
- **Whitelist silent-drop sweep** (the 9 no-report, no-catch-all curators, where an unread field vanishes without
  trace): **2 real losses found** (culturelevel, property — below); the other 7 drop nothing.

So the "hidden data" question is closed **for the XML/curator surface**: the only un-migrated *XML* data is the
enumerated list below.

> **⚠ SCOPE LIMIT of this attestation (found 2026-07-06) — it covers XML-sourced data ONLY.** All three sweeps
> above run over the `Assets/XML` → curator → `Assets/Data` JSON pipeline. They cannot see gameplay that lives in
> **Python event-handler logic and reads no XML field** — a hardcoded per-turn effect keyed only on live
> game-object state (wonder presence, revolution state) is invisible to a field-census. The `-Trading`/`canTradeOn`
> class of miss (Tier 0) was still an XML field the sweep *should* have caught; the Python-hardcoded class below is
> a different, out-of-scope quadrant. See **Tier 0b**.
>
> **STATUS — the DECISION-NEEDED (🔴) tier is CLEARED (owner rulings, 2026-07-01).** Every parked/dropped field across
> every entity has been ruled and migrated (buildings, leaderhead, corp/route/tech, property-pulses, improvement,
> project, era, handicap, promotion/celebrity, unit-cargo). What remains is only the **BLOCKED/deferred** tier below
> (prerequisite-gated: state/paralyze, unitcombat→tags, NPC civs, corp-system rework, ranked-target; and the unit
> **`missions`**/`CvOutcome` migration — a GROUND-UP REWORK kept out of #430) + the **post-migration engine follow-ups**
> (e.g. the celebrity-skill CvCity scan, the `enables.traits`→HAVE self-containment step — the `IS_HOLY_CITY` eval is
> already wired). Per [DEC-data-first] the data foundation is complete — the machine backlog proceeds on solid data.
> **What's left before the cascade replaces legacy** — live verification of the 3 machines + the
> **classification-consumption rewiring** (the long pole) + the grants apply-loop — is tracked in
> [roadmap.md](roadmap.md). **Leaderhead trait remap is a PERMANENT carve-out (owner-ruled), landing after `main`**
> (leaders work without traits; another modder does it on the merged cascade).

---

## ✅ Tier 0 — NEW curator gaps (audit 2026-07-02) — EXECUTED 2026-07-02

The cut-map audit falsified the "data foundation is complete" attestation on three counts — each a confirmed
curator emission gap. **All items
below EXECUTED + regenerated 2026-07-02** (curators + `TechInfo.json` + `apply_channel` block/refList/flexArray
support + `CJK_INTRINSIC_KEYS` recognition; regen delta: 25 techs, 2 promotions, 0 civics/buildings — the civic
triple and building sliders are zero-data today, mapping-migrated). Kept for the audit trail:

- **`curate_civic.py` policies triple** — emit `policies.allReligionsActive` / `bansNonStateReligions` /
  `freedomFighter` (the trait curator emits all three, `curate_trait.py:245-246`; the civic grantor emits none —
  the civic-side counter contributions vanish at the `processCivics` cut).
- **`terrainTrades` → the root `canTradeOn` block** (owner ruling 2026-07-02, capabilities.md — named `canTradeOn`
  to not confuse it with trading resources; NOT capability booleans, which would be per-key hardcoded gates with 0
  modularity): the tech carries `canTradeOn: { terrains: [TERRAIN_…] }` with real FK-resolved terrain refs; the
  trade-route system queries the derived union over live sources. The per-terrain list currently falls through
  `engine.py`'s scalar-only enabler path and emits NOTHING (4 techs of live data:
  raft-building/sailing/seafaring/navigation). The COMMON baseline terrains (always tradable from game start) are
  homed on `TECH_GAME_START`'s `canTradeOn` block (owner 2026-07-02) — same union mechanic, no engine special-case;
  ground the baseline membership against the legacy trade-network behaviour at curator time. `riverTrade` is
  semantically distinct and RULED a capability (owner 2026-07-02: river as a trade ROAD — a conduit, not a tradable
  tile): it stays the bare bool it already is, outside `canTradeOn` (capabilities.md).
- **`waterWork` → the `canWorkOn` block** (owner rulings 2026-07-02, capabilities.md): coarse plot classes —
  `water`/`ocean`/`peaks`/`space`, no terrain lists (if explicit terrains are ever needed, rework THEN).
  Curator-time traces required, do not assume the single-flag model: the ocean/deepOcean requirement the owner
  half-remembers was not found in `canWork` this pass; peaks ride TECH_MOUNTAINEERING (indirect, via
  impassability); space is semi-modelled/future. `bWaterWork` (TECH_TRAP_FISHING) is the one direct work gate.
- **The `-Trading` capability family → the root `canTrade` block** (owner ruling 2026-07-02, capabilities.md —
  "what may appear on your trade table"): re-home the emitted flat keys (`techTrading`→`canTrade.techs`,
  `goldTrading`→`gold`, `mapTrading`→`maps`, `embassyTrading`→`embassy`, `defensivePactTrading`→`defensivePact`,
  `vassalTrading`→`vassals`, `permanentAllianceTrading`→`permanentAlliance`) and emit BOTH
  `canTrade.openBorders` + `canTrade.rightOfPassage` from the single legacy `isOpenBordersTrading` flag. The
  block is open-ended (owner: `bonuses`, `freeTradeAgreement`, "and so on"); the deal system queries it
  generically. Flat capabilities keep only the non-trading abilities.
- **The capability CANONICAL-NAME pass** (owner ruling 2026-07-02, capabilities.md — clear semantics,
  `can<Verb><Object>`/`has<Thing>`): rename the emitted keys per the canonical table (`moveFastPeaks`→
  `canMoveFastOnPeaks`, `desertFarming`→`canFarmDesert`, `irrigation`→`canSpreadIrrigation`, `ignoreIrrigation`→
  `canIgnoreIrrigation`, `bridgeBuilding`→`canBuildBridges`, `riverTrade`→`hasRiverTrade`, `rebaseAnywhere`→
  `canRebaseAnywhere`, `extraWaterSeeFrom`→`canSeeFurtherFromWater`, `mapCentering`→`hasCenteredMap`,
  `mapVisible`→`hasWholeMapRevealed`, `language`→`hasLanguage`, `setScienceRate`/`setCultureRate`/
  `setEspionageRate`→`canSet<X>Rate`). Touch points: `TechInfo.json` channel names, `curate_building.py`
  COMMERCE_SLIDER_CAP, `tech_game_start.json` (hand-carries `setScienceRate`), and the C++ query strings
  (`en_empireHasCapability` callers — `canFoundOnPeaks`/`canPassPeaks` are unchanged, the shadow survives).
  `dcmAirBomb1/2` are NOT renamed — DCM air bombing is slated for whole-system removal (owner 2026-07-02,
  structural-cleanup.md Tier 2); drop the two channels from `TechInfo.json:42-43` in that pass.
- **`canMovePeaks` skill → rename `canPassPeaks`** (owner ruling 2026-07-02, capabilities.md/skills.md — dual-plane
  same-name: promotion-granted unit skill ∪ TECH_MOUNTAINEERING empire capability): rename the emit in
  `curate_promotion.py` (+ the unit-combat sibling flag if emitted) and regen. `canLeadThroughPeaks` stays distinct.
- **`commerceFlexible` tech-side** — same scalar-only fall-through; expand the per-commerce array to the ruled
  discrete keys (`setCultureRate`/…, owner 2026-07-01). One live case: `TECH_DRAMA` → culture slider.
  (`TechInfo.json`'s bare `commerceFlexible` mapping is stale vs the ruling.) Building-side keys are emitted but
  unparsed/unqueried — that half is the `CvJsonBuildingInfo` parse + `en_empireHasCapability` HAVE-widening
  (code work, capabilities.md ruling 2026-07-02), not a curator item.

## Tier 0b — SCOPE BOUNDARY: purely-Python, never-XML effects are OUT OF #430 SCOPE (owner ruling 2026-07-06)

> **⚖ RULING (owner 2026-07-06):** *the hardcoded wonder effects in Python are outside of scope, because they are
> not dealt with in XML — they are purely Python.* **#430 migrates the XML-dealt-with surface** (XML data + the DLL
> machinery that reads it → the cascade). Gameplay that lives **only** in Python and reads **no** XML field is a
> separate surface the cascade does not touch, so it is **out of migration scope** — not a data gap, not a curator
> item, not a legacy-removal exposure.

An apply-site sweep for **repeatable grants** (per-turn spawn / heal / promotion / property) confirmed the *core*
building-repeatable machinery is DLL, in `CvCity::doTurn`, all XML-fed and in scope (`doHeal` :21999/:1355 ← building
`iNumUnitFullHeal`; `doPromotion`→`assignPromotionsFromBuildingChecked` :20915/:1345 ← `getFreePromoTypes`;
`doPropertyUnitSpawn` :24077/:1400 ← building `PropertySpawn*` + property state). It also surfaced a **purely-Python
wonder island** (grep-confirmed absent from `docs/`), which the ruling above places **out of scope**:

- **Per-turn wonder SPAWNS, hardcoded in `CvEventManager.onCityDoTurn`** — `UNIT_CRUSADER` from CRUSADE
  (`CvEventManager.py:2734-2740`), a random `UNIT_SUBDUED*` animal from BIODOME (`:2754-2763`). Wonder-presence-keyed
  (`aWonderTuple` :673), hardcoded unit literal / name-substring scan — **no XML grant field**.
- **Per-turn wonder GOLD/XP grants, hardcoded in Python** — WORLD_BANK/CYRUS_CYLINDER/TOPKAPI (`onBeginPlayerTurn`
  :805/:817/:824), WEMBLEY (`onEndPlayerTurn` :842). Same purely-Python, no-XML class.
- **Structurally INEXPRESSIBLE Python effects (owner example 2026-07-06: ALAMO → culture burst on dead units).** A
  reactive, event-triggered effect (culture pulse *when a unit dies*) has **no home in the `grants`/modifier/enabler
  vocabulary** — the new model declares provisions on standard triggers (build/create/per-turn/tech/religion/civic),
  not arbitrary event reactions. These are **doubly** out of scope: Python-only AND not representable. They stay
  Python by necessity, and are never a migration or legacy-removal concern.

**Why they never appeared in the cut plan — and why that is CORRECT, not a miss:** the completeness bar is an XML-field
census + a DLL apply-site map; a purely-Python effect that reads no XML field is, by the ruling, **not in the surface
the migration owns**, so its absence from the inventory is the scope boundary working as intended, not a gap.

**Consequence — no exposure, by construction:** the cascade grants machine only ever applies what is in the
XML-derived JSON; these effects were never in XML, so they never enter the JSON, so the cascade never applies them →
**no double-up, no silent loss.** They stay Python, untouched, across the XML drop (they read live game-object state,
not XML) and across the grants machine (which replaces only the in-scope DLL appliers). Any future rework of them is its
own separate initiative, never a #430 concern.

*(Contrast: the Revolution/RevDCM rebel spawns ARE in the cut plan — but only because the
civic `revolution` grant touches an XML **data** field (`getRevIdxSwitchTo`), which put the civic-side hook in scope;
the broader revolution mechanic is itself a deferred ground-up rework, not migrated. These wonder effects have no such
XML data hook, so they are wholly out.)*

## ⚖ Tier 0c — the HEAL channel is KEEP-LEGACY through #430; the cascade heal family is a PERMANENT carve-out (owner-ruled ground-up rework)

> **RULING (owner 2026-07-06, refined):** *healing is done by units (healers) or buildings (hospitals, healers'
> hut, …). I do not want to touch it much right now — as long as units and buildings still heal units, I'm happy;
> it needs a broader rework to make sense anyway.* So the cascade **does NOT build a `heal` modifier family in
> #430**; the legacy heal machinery **stays intact** and keeps applying, and the proper heal model is a
> **PERMANENT carve-out (owner-ruled) — a broader rework** (siblings: `isPower` KEEP, the missions/CvOutcome ground-up rework).
>
> **⚖ ACCEPTANCE BAR — heal is BEHAVIORAL-LOOSE, NOT parity (owner 2026-07-06):** *"how healing has worked has
> always been slightly diffuse; as long as healing isn't lost, or isn't wildly overpowered, I am happy… I care
> far less about the diff and more that it works properly."* So heal is EXEMPT from the usual exact-parity /
> `diverging=0` discipline. The test is the two failure modes only — **(1) heal LOST** (a unit's heal rate
> collapses toward 0) and **(2) heal WILDLY OVERPOWERED** (heal rate far too high, e.g. a ×100 or double-count
> bug). Both are directly visible per-unit on the **`/computed/units/heal`** endpoint (the real `healRate()` +
> decomposition). The `[READJSON/healdiff]` shadow is downgraded to a **coarse net** (a huge `diverging` count or
> a heal collapse is the signal), NOT a 0-target gate — do not chase heal parity.
>
> **⚖ THE HEAL REDESIGN (PERMANENT carve-out) IS A COUPLED CLUSTER (owner-ruled):** *"healing overall needs to be
> thought out and redesigned so it makes more sense, but that ties into the dismantling of the unitcombat fiesta
> that currently exists, and we need to flesh out tags properly."* The heal redesign is **NOT standalone** — it
> depends on (1) **dismantling the current unit-combat complexity** (the heavy per-UnitCombat heal-as-type
> dimension — `getHealRateAsType` / `HealUnitCombat` lists — is a symptom of that "fiesta") and (2) the **tags**
> system being fleshed out properly (the unitcombat→`tags` pass — a PERMANENT carve-out, [tags.md](../../specs/tags.md) §Tech/
> equipment class + Tier 3 below). Do the heal redesign only AS PART OF that cluster, post-migration — designing it
> before the unitcombat/tags cleanup would just re-encode the current mess. This is the WHY behind KEEP-legacy-now:
> a dependency order, not a punt.

**Verified state (2026-07-06, heal-inventory sweep) — data-CARRIED, logic-ABSENT.** The heal DATA is largely in the
JSON, but **nothing in the cascade consumes it** — no `DepositIndex::lookupSegment("heal")` or `("healing")` exists
anywhere in `Sources/`, and the unit cascade reads only the enabler fields (no `deposits`). So every heal field is
either PARSED-but-unconsumed or RESOLVED-but-not-applied. The data homes are **fragmented across three shapes** for
one mechanic:

- **promotion + unit-combat heal** → a proper **`heal` modifier family** (`heal.enemy/neutral/friendly/sameTile/`
  `adjacentTile/selfModifier/support/victory/victoryAdjacent/victoryStack`, `heal.unit.unitCombat.{UC}`) — parsed
  into `CvJsonInfo::deposits` (generic family walker), **zero readers**. `alwaysHeal`/`noSelfHeal`/`healsAs` → skills.
- **building `iHealRateChange`** → a **`healing`** family (`healing.city.flat`) — also parsed-but-unconsumed.
- **building `iNumUnitFullHeal` + `HealUnitCombatTypes`** → **`grants.repeatable`** — resolved to a `[GRANTS]`
  diagnostic COUNT only, never applied.
- **dropped (unit-level):** `iSelfHealModifier`, `bNoSelfHeal`, `HealAsTypes` are absent from `curate_unit`'s tables —
  but verified UNPOPULATED on today's unit records (`U_Land`), so no live loss; ⚠ not exhaustively checked across
  `U_Sea`/`U_Neanderthals` — a coverage-verify if any unit ever sets them.

("health" in `Sources/Cascade/` is the separate WELLBEING sickness channel — built + consumed by `CvCascadeWellbeing`
— NOT unit healing.) The whole thing matches the plan's "unit plane lands last" ([modifier.md](../../specs/modifier.md)
§6) but is **under-tracked**. Legacy apply (KEEP): `CvUnit::doHeal` (CvUnit.cpp:6467) → `changeDamage(-healRate())` / per-UnitCombat
`changeHealAsDamage`; building full-heal via `CvCity::doHeal` (:21999); city heal-pool `getHealRate()` fed by building
`iHealRateChange` via `processBuilding`.

**⛔ THE CUT-SAFETY GATE (owner's primary concern, 2026-07-06) — legacy heal reads XML-backed Info getters, so an
XML cut BREAKS it unless the JSON serves those getters.** Verified: `CvUnit::doHeal`/`healRate` and `CvCity::doHeal`
get their values *through* the engine Info getters — `CvBuildingInfo::getHealRateChange` returns `m_iHealRateChange`,
populated ONLY by the XML read (`.add(m_iHealRateChange,…)` CvBuildingInfo.cpp:1807; `getNumUnitFullHeal` :1848);
`CvBuildingInfo.cpp` has ZERO `InfoRepo`/`CvJsonInfo`/cascade references. `CvCity`'s heal pool + `CvUnit`'s heal
accumulators are fed by `processBuilding`/`processPromotion`/`processUnitCombat` reading those getters. So **cutting
the XML now → the heal Info members reset to 0 → `doHeal` loses every contribution → healing stops.** The JSON heal
data sits in `CvJsonInfo` (parsed, unconsumed) and **never reaches `CvUnit`/`CvCity`**.

- **THE REQUIRED BRIDGE (owner's bar: "if the json data gets properly read into CvUnit and CvCity, I'm happy for
  now"):** serve the heal Info getters from the JSON — `getHealRateChange` / `getNumUnitFullHeal` /
  `getSelfHealModifier` / the promotion + unit-combat heal getters return the `CvJsonInfo` value instead of the XML
  member. Then the existing `processBuilding`/`processPromotion`/`processUnitCombat` apply path feeds `CvCity`/`CvUnit`
  unchanged and `doHeal` runs off cascade-sourced data — no heal family, no logic rework, identical behaviour. This is
  the readjson.md §3 "fresh structures serve the EXE accessor surface" step, extended to the type-specific heal
  getters (it currently only covers the `CvInfoBase` base getters). **This bridge is the concrete gate that makes
  KEEP-legacy heal survive the XML cut — it is NOT built.** ⚠ Requires the building `iNumUnitFullHeal`/
  `HealUnitCombatTypes` heal data to be reachable as its Info-getter shape, i.e. re-home it OUT of `grants.repeatable`
  (the getter can't read a repeatable entry) OR emit it in a getter-serveable form — so the double-up cleanup and the
  cut-safety bridge are the SAME fix.
- **KEEP all heal XML fields + the legacy heal machinery** — mark them KEEP-rows; they are **NOT retired** at the
  #430 legacy removal, so units + buildings keep healing units off the surviving legacy path. (Confirmed via the heal-inventory
  sweep: no heal field is on an active cut list; the risk is only the wholesale XML-read deletion, gated by the bridge above.)

> **⚖ GENERALIZES — the bridge is the same for EVERY KEEP-legacy DLL per-turn apply that reads XML-backed Info data
> (owner ruling 2026-07-06: "build json-based heal + repeatable-grant read-in now; the cascade apply-loop is
> post-cut").** The verified DLL per-turn surfaces that read XML Info members and would break at the XML cut are
> **FOUR**, not the heal pair: **(1) heal** (`doHeal` ← building `getHealRateChange`/`getNumUnitFullHeal`/
> `HealUnitCombatTypes` + unit/promotion/unitcombat heal getters); **(2) building free-promotions**
> (`doPromotion`→`assignPromotionsFromBuildingChecked` ← `getFreePromoTypes`/`isApplyFreePromotionOnMove`,
> CvBuildingInfo.cpp:2461/:1747); **(3) property/criminal spawn** (`doPropertyUnitSpawn` ← `getPropertySpawnUnit`/
> `Property` :3056 + property state); **(4) property pulses** (the property solver ← `m_PropertyManipulators` :1680 —
> ⚠ solver read per-turn assumed from the XML-backed data, trace to confirm). The **Python** per-turn spawns
> (CRUSADE/BIODOME/Revolution, Tier 0b) are the "rest is Python-driven" and stay out of scope.
>
> **THE PLAN (owner 2026-07-06):** build the JSON→Info-getter read-in for these four NOW so the existing legacy DLL
> apply keeps working after the XML cut; **DEFER the cascade grants apply-loop** ([grants-machine.md](grants-machine.md)
> increment 5) to POST-cut. This is *simpler* than increment 5 (no new per-turn apply machine + no atomic
> legacy-retirement) and it satisfies the cut-safety gate. Once the read-in works, nothing DLL-side is left needing
> the apply-loop pre-cut. **Recommended mechanism:** `readJson` populates the KEEP-legacy engine `CvXInfo` members
> from the parsed JSON (a bounded, deliberate exception to "CvJsonInfo never duplicates engine fields", sanctioned as
> the readjson.md §3 "fresh structures serve the accessor surface" step for these specific getters) — getters +
> `processBuilding`/`processUnit`/solver stay unchanged. Consequence: the building heal/spawn/pulse data must be
> reachable in getter-shape — so re-homing it OUT of `grants.repeatable` (§ double-up guardrail) and the cut-safety
> read-in are the SAME fix.

- **⛔ THE ONE GUARDRAIL — the grants apply-loop (increment 5) must NOT apply the heal repeatables.** The building
  heal is currently **mis-homed** to `grants.repeatable` (`curate_building.py:598-600` `iNumUnitFullHeal` →
  `{heal:"full",count,interval}`, the `heal:"full"` string an unflagged agent invention; `:601-612`
  `HealUnitCombatTypes`). It is inert today (nothing consumes `grantRepeatables`), but if the apply-loop ever applies
  it **while legacy `doHeal` also applies it → double-heal.** Since heal is KEEP-legacy, the apply-loop excludes the
  heal repeatables; cleanest is to **stop emitting them** (revert the curator emit — the building heal simply isn't in
  the cascade JSON, legacy applies it from XML). Low-touch; do it at/with increment 5, not urgently (inert until then).
- The proper heal model (ONE `heal` modifier family: unit/promotion/unitcombat/building sources deposit into a
  cascade `healRate()`, applied via the surviving `changeDamage`; the per-UnitCombat heal-as-type dimension;
  unifying the three fragmented homes above into one channel) is a **PERMANENT carve-out (owner-ruled) broader rework**, not #430.
  ⚠ The `mapping/*.json` files are **dead/unconsumed metadata** — the heal curators use hardcoded tables, NOT the
  mapping (`curate_building.py:47`: "the mapping's were often wrong"), so their `unitFullHeal`/`healRate`/`identity`
  heal listings mislead; ignore them and reconcile channel naming from the curator code when the rework runs.

## Tier 1 — DONE (committed)

- **Trap family** (11 tags) → `DROP` in `curate_promotion.py` — dead mechanic (traps removed). Zero data delta (no
  live promotion XML carries trap values). Commit `b9c2804c0`.
- **`military{Happiness,Production,Support}`** dropped from `curate_unit.py` `CAP_BOOL` — reclassified to the
  `IS_MILITARY` predicate/tag (json §3.5); `militaryTrade` kept; `bMilitarySupport`'s tag-signal read (`:655`)
  preserved. 1352 units regenerated. Commit `b9c2804c0`.
- **`BUILDING_PALACE`** dropped from ~48 civilizations' `grants.buildings` — redundant with the settler's
  `foundBuildings` (json §5). Commit `b9c2804c0`.
- **`iOperationalRange{Min,Max}`** (property) → the **`ai` block** (`ai.operationalRange:{min,max}`) — AI-only
  decision-scoring band (`CvCityAI`), NOT the #429 propagation drop (owner 2026-07-01). 7 property JSONs regenerated.
- **`iMaxNationalWondersOCC`** (culturelevel) → justified **DROP** — One City Challenge is not feasible in this mod
  (owner 2026-07-01); the mislabeled curator rationale ("OCC forces limits off") corrected (comment-only, no data change).
- **Already-verified DONE** (curator code confirms): `bSpy`→`spy` tag · `freeSpecialistPer*Wonder`→`freeSpecialists`
  · `EraCommerceChanges`→ERA-threshold flats · corp `iMaintenance` de-scale · `GlobalBuildingExtraCommerces`→
  `empire.buildings.{B}.flat` · trait `nonStateReligionCommerce` stays a policy.
- **`grants` classification pass (2026-07-01)** — the survey found `grants` was a **34-key grab-bag** (~half off-grammar).
  Re-homed the mis-classified keys to their real blocks: promotion `unitCombats`/`removesUnitCombats`→**skills**; project
  `grantsSpecialBuilding`→**`enables.specialBuildings`** (flips SpecialBuildingValid — unlocks, hands out nothing); corp
  `bonusProduced`→**`provides.bonuses`** (continuous supply §5a); unit `builds`→a dedicated **`builds`** block (readJson
  `CJK_INTRINSIC_KEYS` + `CvJsonUnitInfo` parse); building `holyCity`→**`requires.build`** (`IS_HOLY_CITY` predicate, a
  build gate not a setter — verified `CvCity.cpp:2728`) + `traits`→**`enables.traits`** (held-trait — `owner.setHasTrait`
  while active); `freePromotions`→**`repeatable`**. All 0-stale, new-home counts == originals; json.md §5/§8 updated;
  Assert green. **Consumer-wiring:** the enabler `IS_HOLY_CITY` eval is ALREADY wired (verified — parser
  `CASC_PRED_IS_HOLY_CITY` + evaluator `ev_evalPredicate` `CvCascadeConditionEval.cpp:246`), so the `holyCity` gate
  works out of the box. The one remaining follow-up is the self-contained **`enables.traits`→empire-active-trait HAVE**
  computation (today the modifier reads engine `hasTrait`, which already includes the building's `setHasTrait`, so the
  effect flows; the cascade-computed active-trait set is a self-containment step, not a data gap).
- **PERMANENT carve-out (owner-ruled) — unit `missions` + the `CvOutcome` system.** The grants pass found the unit
  *activated-mission* keys — `buildings` (MISSION_CONSTRUCT), `greatPersonAction`, `goldenAge` — are **missions**, NOT
  grants (a `skill` is a permanent property; a **mission** is an action producing an OUTCOME, often consuming the unit),
  and the engine's **`CvOutcome`** system (`CvUnitInfo` `KillOutcomes` + `m_aOutcomeMissions` — *"outcome system (no
  wrapper)"*) is **entirely un-migrated**. Owner ruling: a **`missions`** block (json §8) unifies the hardcoded mission-
  abilities AND CvOutcome. **Owner ruling (updated 2026-07-01): the mission concept is a GROUND-UP REWORK, not ported.**
  The entire mission concept is being **redesigned from scratch**, so porting the old CvOutcome model would be
  **throwaway work** — hence kept entirely OUT of #430. The outcomes stay in the OLD XML and legacy keeps applying them
  meanwhile; the subsystem, **like the random-events system** (also a ground-up rework, also kept out), is isolated
  enough to defer cleanly. When it runs, the `missions`
  block just **LISTS which missions a unit can use** — BOTH the data-driven outcome-missions (`<Actions>`) and the
  hardcoded mission-abilities (`greatPeople` included); the `greatPersonAction` `base`/`multiplier` magnitudes are
  **left as-is** (used-or-not TBD). Until the pass runs, the four deferred keys stay in `grants` untouched (the grants
  machine ignores them). Behaviour reference: [`../../reference/mission-outcome-system.md`](../../reference/mission-outcome-system.md)
  — the earlier "7-question full-port" design there is **SCRAPPED**.

---

## Tier 2 — DECISION-NEEDED (owner ruling: is-it-live? / where-does-it-go?)

These are real gameplay values the curators **parked in `identity` or dropped** on a re-verify claim. Each needs an
owner call before a curator edit — per [DEC-no-guessing] the agent must not decide "dead" or invent a home.

### Confirmed real losses — RESOLVED (owner 2026-07-01, see Tier 1)

The whitelist completeness sweep found exactly two, both now ruled and landed:

- **`iMaxNationalWondersOCC`** → DROP (OCC not feasible in this mod).
- **`iOperationalRange{Min,Max}`** → `ai.operationalRange:{min,max}` (AI-only).

### Real data parked in `identity` / dropped (from the curator-code audit — need is-it-live + home)

- **building — DONE (owner rulings 2026-07-01, executed):**
  - NEW **`attributes`** block (json §8) — building-**HELD** city-scope capability bools (16): `nukeImmune`,
    `borderObstacle`, `protectedCulture`, `noUnhappiness`, `noUnhealthyPopulation`, `buildingOnlyHealthy`,
    `forceAllTradeRoutes`, `quarantine`, `mapCentering`, `teamShare`, `orbital`, `orbitalInfrastructure`,
    `governmentCenter`, `capital`, `zoneOfControl`, `providesFreshWater` (fresh water is NOT a `BONUS_`, so NOT `provides`).
  - `noHolyCity` → `requires.build.disabled: IS_HOLY_CITY` (a placement predicate, not an attribute).
  - `applyFreePromotionOnMove` → `grants` pulse (folds with the building's `FreePromotions`: a unit that stays in the
    city gets the promotion).
  - `commerceFlexible` → **`capabilities` PROVIDED to the empire**, as discrete booleans `setCultureRate`/
    `setEspionageRate` (and `setScienceRate`, which rides `TECH_GAME_START` — every civ has it). **Capabilities are
    empire-HELD, grantor-PROVIDED** (tech/civic/**building**); a building **provides**, never **holds** — the opposite of
    `attributes` (which the building holds). json §8 to state this distinction.
  - `shrine` (GlobalReligionCommerce) → promote to the top-level **`shrine`** bespoke section (json §9 reserves it).
  - `corporationHQ` (GlobalCorporationCommerce) → NEW **`headquarters`** bespoke section (mirror of `shrine`).
  - counter-damage (`bDamageAllAttackers` + `damageAttackingUnitCombats`) → fold into the **`defense`** family with the
    already-migrated `iDamageToAttacker`/`iDamageAttackerChance` (one mechanic, one home).
  - KEEP as cascade markers in identity: `stateReligionCommerce`, `commerceDoubleTime` (a flat family provably can't
    model the pool×count / whole-commerce doubling — documented + cascade-read).
  - STAY in identity (buildability/placement, json §7): `autoBuild`, `noInstanceLimit`, `forceNoPrereqScaling`, `centerInCity`.
  - DROP (confirmed DEAD): the aid mechanic (`BonusAidModifiers`/`AidRateChanges` — city arrays saved but ZERO
    write-from-building + ZERO read-for-effect; only AI-valuation/pedia read the raw Info) + the `DROP_DEAD`/`DROP_MODULE` set.
  - `EnabledCivilizationTypes` → `requires.build` (folded when NPC civs are wired; authored under `identity` until then); `bAllowsNukes` → `requires.build.disabled` (done).
- **leaderhead — DONE (owner rulings 2026-07-01 / 2026-07-21): ALL traits stripped; leaders ship TRAITLESS.** Every
  leader trait assignment (`Traits`, `DefaultTraits`, `DefaultComplexTraits` — simple AND complex) is dropped from the
  JSON; **no leader carries traits**. The engine `CvLeaderHeadInfo` is now JSON-fed (its trait members stay empty), so
  with the leaderhead XML cut the leaders run **traitless** — this is intended: **re-adding traits is a COMMUNITY task
  post-merge** (owner 2026-07-21: "it is supposed to be traitless, this is something for community to add after
  merge"), NOT a carve-out we build. 118 leaderheads regenerated.
- **era — DONE (owner ruling 2026-07-01):** `bNoGoodies`/`bNoBarbUnits`/`bNoBarbCities` → a bespoke **`worldGen`** block
  (LIVE C++ world-RULE gates: goody/barb placement — "bespoke worldgen works better", not identity/modifiers). All-false
  in every era today → mapping migrated, 0 output change (zero-drop).
- **handicap — DONE (owner ruling 2026-07-01):** `advancedStart` (`iAdvancedStartPointsMod`, `iAIAdvancedStartPercent`)
  **stays `identity`** (owner: "can stay where it is") — pre-game points config, no consumer wired, handicap-intrinsic.
- **tech**: `TechMovementChanges`/`TechSpecialistChanges` inverted onto tech where no consumer reads (non-functional).
- **corporation** (owner rulings 2026-07-01): `iSpread`→`identity.spreadFactor` (concept-parallel to religion's
  spread), `iSpreadFactor`→`identity.competingSpreadCostPercent` (fix the misnomer — it's a competing-corp spread-cost
  inflation, `CvUnit.cpp:8687`), `iSpreadCost`→`cost.spread`, `CompetingCorporations`→**`excludes`** (json §9 same-tier
  mutual exclusion; empty in base XML, so the MAPPING migrates but shipped output is unchanged) — all executing.
  **NB: the corporation SYSTEM deserves a principle-level rework later** (owner 2026-07-01: "don't like how corporations
  work in principle") — that is a PERMANENT owner-ruled carve-out — a
  principle-level rework of the corp MODEL, held as its own deliberate piece of work rather than folded into the
  data migration; the corp-HQ revenue (`HeadquarterCommerces`) rides that rework.
- **improvement — DONE (owner rulings 2026-07-01):** `iAirBombDefense` → **`defense.plot.air.flat`** (101 improvements;
  the air-bomb defense magnitude, `CvUnit.cpp:7127`). `iFeatureGrowth` / `iCultureRange` / per-bonus `depletionRand` →
  **STAY in `identity`** (owner: "leave them in identity") — improvement-INTRINSIC mechanics read by their own `CvPlot`
  systems (feature-regrowth / culture-seed / depletion), NOT cascade modifiers, so identity is the correct home.
  (`cultureRange` verified: the real culture-spread was nuked ~4yr ago — `pushCultureFromImprovement` is now only a
  cosmetic 1-culture placement seed, `CvPlot.cpp:4062`; the field is a live-but-vestigial remnant.)
- **project — DONE (owner ruling 2026-07-01):** `AnyonePrereqProject` → **`requires.build: {type, scope:"world"}`** —
  a single project that ANY player must have built (world-scope presence, `CvPlayer.cpp:6868` blocks when world
  `getProjectCreatedCount==0`; NOT the `.any` combinator — it's one project). Empty in base XML → mapping migrated,
  0 output change. `PrereqProjects` (the ALL case) already modeled via store→`enables` inversion; per-edge `iNeeded`
  is all-`1` today (a `count>1` would need a count-bearing edge — flagged, not lost).
- **unit**: `iCargo` → **`cargo.space.flat`** (+ `DomainCargo`→`cargo.space.{unit:IS_<domain>}`, modifier §6) —
  owner-RULED 2026-07-01, **DO-NOW** (currently UNHANDLED on 90 units). `EnabledCivilizationTypes` is **NOT** the
  unique-unit system (that's UnitClass/CivilizationInfo) — the train gate fires ONLY under
  `isNPC() && isStronglyRestricted()` (`CvCity.cpp:2231`), inert for real civs — so it folds with `stronglyRestricted`
  (Tier 3, → `requires.build` when NPC civs are wired; authored under `identity` until then). cargo restriction → `identity`.
- **promotion — DONE (owner ruling 2026-07-01):** `iCelebrityHappy` (the numeric per-unit celebrity-happiness stat) →
  a boolean **`skills.celebrity`** (3 promotions: INSPIRE3/6/9; unit-combats carry the same field → `skills.celebrity`
  too, 0 today). The AMOUNT is dropped ("not a random field on a unit"); **failure-to-close (Gate-3 skills work):** `CvCity`
  scans for celebrity-skilled units and owns the happiness (replacing the `CvCity.cpp:5718` per-unit-stat sum).
  `iPoisonProbabilityModifierChange` inert (kept).
- **handicap**: `advancedStart` (`iAdvancedStartPointsMod`, `iAIAdvancedStartPercent`) → `identity`, no consumer wired.

---

## Tier 3 — BLOCKED (needs a prerequisite / decision first)

- **`paralyze`** → `state` block — a failure-to-close: it blocks on the greenfield `state` model (state.md), which is
  in-scope migration work to BUILD (not a park). No data is lost or moved meanwhile; it closes when the `state` model is built.
- **`mechanized`/`gunpowder`/`mounted` tags** — derived from unitcombats in the **unitcombat→tag pass**; the
  obvious-identity first pass is LANDED (`curate_unit.py` folds a unit's combat classes to tags — mechanized/
  gunpowder/mounted + ~30 others emitted). The flagged-remainder taxonomy folds remain.
- **`stronglyRestricted`** (NPC build-lockdown) → a `requires.build` civ-membership gate (paired with
  `EnabledCivilization`) — a PERMANENT carve-out (owner-ruled), pending **NPC civilizations being wired**. **⚖ NOT a flip/legacy-removal constraint
  (owner ruling 2026-07-02):** losing the NPC lockdown during the enabler flip is **accepted** — *"I truly don't
  care about NPC barbarians or neanderthals being locked down or not… it's something to solve post migration, we
  may after all want to do it in a better manner anyway."* The enabler gates may flip without preserving the
  `isNPC() && isStronglyRestricted()` clause; the NPC build restriction gets re-solved (possibly redesigned)
  post-migration.
- **Property pulses = repeatable grants (owner ruling 2026-07-01) — DATA is DO-NOW, NOT #429-blocked.** A per-turn
  `PROPERTY_*` pulse an entity emits → a `grants.repeatable` entry carrying its spatial intent
  (`{ PROPERTY_X: N, interval, on, relation, distance }`, json §5). Capturing the pulse + spatial fields is pure DATA
  and is unblocked. Work: a shared **property-source cleaner** — fix `engine.property_source_v3` to EMIT spatial
  sources as `grants.repeatable` instead of *raising* on `RELATION_NEAR`, and route the improvement/feature/building
  property arrays through it (replacing the verbatim `properties` parking). **#429 is ONLY the ENGINE
  spatial-distribution** that later reads `on`/`relation`/`distance` — a separate consumer, not a data blocker.
- **Corp HQ revenue** (`HeadquarterCommerces`) → the corp-rework pass.
- **`ranked-target-selection`** (`max:`/`orderedBy`) — a failure-to-close: design locked; the implementation is pending
  migration work to finish. Until it lands it blocks retiring `largestCity` (so `curate_civic.py`/`curate_trait.py` still
  emit `iLargestCityHappiness`). See `../parked/ranked-target-selection.md`.

---

## Tier 4 — VERIFY (promise-based cross-curator holes)

- `curate_bonus` actually inverts civic `BonusCommerceModifiers` (the civic curator drops it on that promise).
- The yield resolver reads `identity.movementCost` (feature/terrain `iMovement` → `identity.movementCost`).
- `PropertyPropagator`/`ChangePropagators` re-home at the unit/building passes actually happens (else lost with #429).
- PropertyBuilding `iMinValue`/`iMaxValue` are consumed by the Building pass (the building-side `requires` value-band).
- Every bespoke PASS2 tag has live emit code (not just set-membership) — esp. the building PASS2 audit.
- Stale docs: `Tools/Migration/README.md:86-88` references a non-existent `curate_pocos.py`; `curate_building.py:22-24`
  docstring says PASS2 tags "show as UNHANDLED" but they're mostly implemented.

---

## Downstream (explicitly BENEATH this tier, per DEC-data-first)

The machine backlog does not start until the data tier is closed: substrate rebuild → full modifier port
(AFTER/BASE/assembler/commerce) → **A5 wire `skills`/`tags`/`capabilities`/`policies` onto the game object** ("SUPER
IMPORTANT, easily LOST") → enabler verification pass → grants machine → trait simple/complex engine fix →
§4 deletions. Plus the observability dump gaps. Tracked against [modifier.md](../../specs/modifier.md).
