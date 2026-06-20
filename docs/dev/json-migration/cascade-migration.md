# Cascade migration — roadmap, status, and demolition map

> **Status:** plan   ·   **Verified against:** the old `docs/dev/plans/` spec set as of 2026-06-19 (the
> consolidated sources are cited per-section), NOT re-verified against live `Sources/Cascade/` for this rebuild.
> **Grounding:** `cascade-engine-430.md` (the implementation status table + build order),
> `migration-entity-ranking.md` (the entity order + per-entity state), `building-cascade-conversion.md`
> (building-pass status), `cascade-mapping-inventory.md` §14/§B (the maintainer demolition list),
> `cascade-known-discrepancies.md` + `modifier-cascade-known-discrepancies.md` (the open divergences).
> One-paragraph orientation: this is the **roadmap/status** for the #428/#430 cascade rework — *where the
> rebuild stands* (built vs pending), *what data has migrated*, *what legacy machinery is queued for deletion*,
> and *what divergences are still being chased*. It is the "what we INTEND / where we ARE" companion to the
> DESIGN, which lives in [`../explanation/cascade-architecture.md`](../explanation/cascade-architecture.md) — the
> mechanism is NOT re-explained here; this tracks its construction.
>
> **⚠ Path caveat.** The `Sources/` tree was re-bucketed since the source docs were written (`Cv*` engine →
> `Sources/Engine/`, AI → `Sources/AI/`, Infos → `Sources/Infos/`, the HTTP server → `Sources/Tools/`; the
> cascade itself is `Sources/Cascade/`). Path prefixes below are re-grounded to that layout. **Line numbers
> DRIFT** — every citation means "the function named here, around this line"; confirm the function, not the integer.

---

## BLUF — where the rework is, in one screen

- **The data side** (XML→JSON via the offline curators) is **far along**: all of Tiers A–E are curated
  (~35 entities incl. the two monsters, Building + Unit), with a "stragglers" tail (Tier G) and two deferred
  systems (Outcomes → #430, Events → #425) outstanding.
- **The engine side** (the runtime that consumes the JSON) is **early-to-mid**: the substrate is built; the
  tally, enabler, and modifier are each **partial** (a working slice + named gaps); `grants` is **absent**.
- **The demolition** (deleting ~7–8k lines of legacy maintainers) is **mapped and shadow-gated, not started** —
  two of ~four maintainer clusters now have a shadow; nothing is cut until its shadow runs clean
  ([DEC-map-before-delete](../architecture/decisions.md#dec-map-before-delete)).
- **Open divergences** are fully cause-tagged on the buildability side (28, mostly accepted UI-layer) and
  actively shrinking on the modifier side (the city-yields pilot, residual driven down toward a sharper-than-±10%
  bar).

---

## 1. Engine build status — what's built vs pending

The runtime is one **substrate** (scope spine + accumulator + event spine) with **three machines** (tally,
modifier, enabler) + `readJson` + `grants`. Build order: **readJson + substrate → tally → modifier → enabler**.
Design: [`../explanation/cascade-architecture.md`](../explanation/cascade-architecture.md). The table below is
the status from `cascade-engine-430.md`'s adversarial docs-vs-code sweep (2026-06-19) — **UNVERIFIED against
live `Sources/Cascade/` for this doc; trust it as of that sweep's date, re-confirm before relying on a row.**

| Component | Status | What's there / what's missing |
|---|---|---|
| **Substrate** (spine + accumulator + event spine) | ✅ **BUILT** | `CvEventSpine` + `CvScopedAccumulator`; logging + tally consumers registered (`cascadeRegisterConsumers`). |
| **Tally** | ⚠ **PARTIAL** (model decided) | Player-leaf model **accepted** (owner 2026-06-19); wired + load-bearing for **buildings + units only** (2 of ~6 count domains — tech/civic/religion/bonus/project/specialist absent, so atoms over those read 0). Save handling (rebuild-on-load) done, hooked at `onFinalInitialized`. CITY/PLOT read direct; widening to city-leaf later is a contained, no-save-break change. |
| **Enabler** | ◐ **GATE built, GENERATOR inert** | `cascadeBuildable` (build ∧ operate ∧ allowed-cap) + `cascadeOperational` (dormancy) mature; allowed-cap enforced via tally. **Missing:** the `enables`-family CAN-GET generator (`cascadeTechReachable` dead, no frontier producer); `disables` unparsed; `notConstructible` parsed but unconsumed. |
| **Modifier** | ◐ **PILOT BUILT + SHADOWED** (city-yields) | The data-driven deposit flow for **food/production/commerce** is built and shadowed (`cascadeModifierCitySlot`/`cascadeModifierEffective`). Sources wired: city-scope **building** deposits, **specialist** base yield, the active **civics'** empire→city deposits. **Missing sources:** trait/tech-downward/bonus/area/power/capital, building **empire**-scope, civic sub-scopes, the clamp; then PLOT + other scopes; the other channels (commerce-split / health-happiness / defense / maintenance / unit-plane); the multiplier-capability flip (Mode B). |
| **`grants`** | ❌ **ABSENT** | No `grants` consumer registered (the spine diagram lists it; not built). |
| **`readJson`** | ◐ **gate-surface + PILOT modifier feed** | Parses `requires.build/operate`, `allowed`, `identity.{spawnOnly,notConstructible,autoBuild}`, four ad-hoc reverse-index scans, the `PROPERTY_X` band atom, AND the pilot modifier families (food/production/commerce flat/percent at city scope, with `enabled`/`disabled`). Does **not** yet parse the other modifier scopes/channels or `grants`. Header marks it TEMPORARY. |

**Big remaining engine items** (the honest roadmap, from `cascade-engine-430.md`):
1. **Modifier** beyond the city-yields pilot — PLOT + other scopes, non-building sources, the full channel
   inventory, the Mode-B capability flip.
2. **`grants`** consumer (entirely absent).
3. **Tally** domain coverage (the player-leaf-vs-city-leaf model decision is *resolved* — player-leaf accepted).
4. **Enabler** `enables`-family generator + `disables` parsing.

**Smaller gaps the sweep flagged:** `ATOMDOMAIN_HERITAGE` implemented-but-undocumented; `workedBy`/`natureYield`
predicates specced-but-unparsed; `CvCascadeReadJson.cpp` still `#include "CvInfos.h"` (contradicts the
retire-umbrella ruling — folded into the sources-structural-cleanup pass).

**The cutover is ATOMIC and LAST.** Each machine is developed in **shadow** alongside the live XML-driven
machinery behind gated logging; the JSON only becomes the source at the final flip, when `readJson` replaces
`readXml` at the load seam and the shadow accumulators become the sole source. Parity is **not** the success
metric — the cascade is *expected* to correct latent legacy bugs ([DEC-parity-not-goal](../architecture/decisions.md#dec-parity-not-goal)).

## 2. Entity migration order + status (XML → JSON)

Source: `migration-entity-ranking.md`. The order is a **topological sort**: config/sources first, the
most-targeted "monsters" last, so every edge a target consumes is already authored on its source (the
edge is authored **once**, on the source/conditioner). **Conversion is strictly serial — one entity at a time,
each verified against its live C++/Python consumer before the next** (owner ruling; the abandoned mass-migration
detour outran verification). The curators live in `Tools/Migration/`; the data lands in `Assets/Data/**`.

**Status legend:** ✅ curated · ◐ heavy/partial · ☐ not yet curated · ⏸ deferred to a later system rework.

| Tier | Entities | Status |
|---|---|---|
| **A — config / global axes** | GameSpeed, Handicap, Era, Process, Victory, Vote, CultureLevel, Hurry, BonusClass, CivicOption, Civilization | ✅ all curated · **Property** ☐ (defines the `PROPERTY_*` channels; diffusion → #429) |
| **B — top-of-cascade sources** | **Tech** (the spine root), Civic, Religion, Corporation, Trait | ✅ all curated |
| **C — resources / map substrate** | Bonus ✅, Route ✅, **Terrain** ☐ (heavy), **Feature** ☐ (heavy), **Improvement** ☐ (heavy), Build ✅ | ◐ 3 heavy plot-leaves remain |
| **D — producers / unit-plane sources** | Specialist, Heritage, Project, PromotionLine, Promotion, UnitCombat, LeaderHead | ✅ all curated |
| **E — the monsters** | SpecialBuilding ✅, **Building** ✅ (5202 records), SpecialUnit ✅, **Unit** ✅ (2073 records) | ✅ all curated |

**Tier G — the "stragglers" tail (scope verified 2026-06-16):** ~13 small/medium gameplay entities still to
migrate — `CvSpawnInfo` (barb/animal spawn), `CvGoodyInfo`, `CvEspionageMissionInfo`, `CvEmphasizeInfo`,
`CvVoteSourceInfo`, `CvCommerceInfo`, `CvUpkeepInfo`, the `CvWorldInfo` gameplay half, `CvMapCategoryInfo`,
`CvInvisibleInfo`, plus finishing `CvYieldInfo` — and a config/map-gen set (`CvClimateInfo`, `CvSeaLevelInfo`,
`CvTurnTimerInfo`, the `CvWorldInfo` grid half) migrated as config sections, not cascade.

**⏸ Deferred to their own reworks (NOT migrated faithfully in #428):**
- **`CvOutcomeInfo`** (134) → the **#430 outcome-system pass** — misnamed (it is the *gating* half, not the
  result); the result half rides raw in the unit JSON. Both halves reworked together.
- **`CvEventInfo`** (873) + **`CvEventTriggerInfo`** (509) → the **#425 event-system rework** — "OOS-prone and
  catastrophically coded", so a rework not a port.

**Phase F — final alignment** (a *light, iterative* pass, not an exhaustive hunt): after migration, sweep every
already-migrated entity into line with conventions that were locked later (art blocks ✅, `allowed` caps ✅,
membership predicates ✅; the modifier-ownership/tech-gate review + the build-vs-operate move for continuous
gates remain, handled as encountered during #430 wiring). Structure was audited 2026-06-16 and confirmed
**fundamentally sound**.

> **Old JSON is NOT a baseline** (owner ruling): the committed `Assets/Data/*.json` predate the locked v3/v0.3
> model; "matches the old file" proves nothing. Validate by structural adherence + zero invention + reading the
> live consumer, never by diffing the old JSON.

## 3. The demolition map — what the cascade deletes, and when

The cascade replaces **~7–8k lines** of imperative "maintainer" machinery. **Nothing is deleted until its
behaviour is fully observable and a shadow proves the cascade replicates it, turn over turn**
([DEC-map-before-delete](../architecture/decisions.md#dec-map-before-delete); the
[Orwellian observability bar](../reference/observability/README.md) is the verification substrate). Sources:
`cascade-engine-430.md` §4 (the line-level demolition list) + `cascade-mapping-inventory.md` §B (the per-turn
state maintainers + their shadows).

### 3a. Bulk machinery deleted at cutover (per machine)

| Machine | Legacy machinery it deletes/rewires (re-grounded to `Sources/Engine/`) |
|---|---|
| **Enabler** | `CvCity::canConstruct`/`canTrain`/`canCreate` + the `CvPlayer` twins; `canEverResearch`, `canDoCivics`, `canFoundReligion`, `canFound`; the `m_bCanConstruct*` / canTrain caches (+ `VALIDATE_*` shadow checks); `CvCityAI::CalculateAllBuildingValues` PreLoop (~1500 lines); the `setHasBuilding` extension/replace chain-walk. (The #195 enabler index is **partly KEPT** — it *is* the `enables` generation index.) |
| **Modifier** | the `CvCity` yield/commerce/health/happiness/defense/maintenance accumulators + `getBaseCommerceRateFromBuilding100`/`getBuildingYield`; `processBuilding`/`processSpecialist`/`processBonus`/`processCorporation`; `CvPlayer::setCivics`/`processTrait`; `CvTeam`/`CvPlayer::processTech`; the `CvUnit::changeExtra*` setter stack (**91 setters**, not the spec's "~200"). |
| **Tally** | the cross-city count loops inside the gate functions (the `getNum*` prereq scans) + the demographics/score scans → reads of the one tally. |

### 3b. Per-turn state maintainers (§14 H) — the shadow-before-cut queue

These act on already-built/auto-placed things, so the buildability sweep can't see them — **each needs its own
behaviour shadow before deletion** (`cascade-mapping-inventory.md` §B). Current state:

| Cluster | Legacy maintainer | Cascade replacement | Shadow / state |
|---|---|---|---|
| **B-i Auto-placement** | the per-turn autobuild loop + `checkPropertyBuildings` (`CvCity::doTurn` building block) — `changeHasBuilding(true/false)` that mutates the building set | `identity.autoBuild` placement marker + `requires` (place/drop as requires flip) | ✅ **SHADOWED** — `/diagnostic/placementSweep` + `[PLACEMENT]` per-turn. Property-band buildings still show `reason=noMarker` until curated. |
| **B-ii Dormancy** | religiously-limited (`isReligiouslyLimitedBuilding`) + resource (`isActiveBuilding`/`isDisabledBuilding`) | `requires.operate` (state-religion + resource gates) | ✅ **SHADOWED** — `/diagnostic/dormancySweep` + `[DORMANCY]` per-turn. B1 (religious) MATCH-verified; B5 (resource) shadowed, divergences drive the build→operate curation. |
| **B-iii Group gate** | `isSpecialBuildingNotRequired` (the SpecialBuilding group cap/tech/obsolete/waiver) | uniform group-gate inheritance | 🔭 **UNSHADOWED** — cap half + tech/obsolete inheritance shipped; membership shadow not built. |
| **B-iv Waiver** | `hasAllReligionsActive` (religion exemption) | a declared `requires` waiver clause | folds into B-ii; currently moot (no civic sets it) but must be a defined fact pre-switch. |

**Observability rating (self-assessed 2026-06-18):** the cascade/buildability surface is at **Tier 3 (Big
Brother)** with a Tier-4 foothold (2 of ~4 maintainer clusters shadowed). To climb: shadow B-iii, curate the
property bands + the B5 build→operate resource prereqs, then give each opaque system (food wastage, espionage,
religion spread, corporations, culture pipeline, finance, AI diplo memory) a live read surface. Scale:
[DEC-obs-scale](../architecture/decisions.md#dec-obs-scale).

**The demolition is "complete" only when every divergence row (§4) is CHANGE-accepted or ALIGN-done and every
maintainer above has a clean shadow.**

## 4. Open known discrepancies (cascade shadow vs. live game)

Two living lists track every place the cascade differs from legacy, so each is a deliberate **CHANGE** (accept
the cascade's behaviour as a correction) or **ALIGN** (fix to match legacy by the hard switch) — never a
cutover surprise.

### 4a. Availability / enabler divergences (`cascade-known-discrepancies.md`)

Buildability is **fully cause-tagged** by the on-demand reason-reporters — 28 divergences (27 over + 1 under),
**zero `other`** (this collapsed ~310 guess-based clusters to 28). The bulk is accepted UI-layer behaviour:

- **`alreadyQueued` (12)** + **`replaced` (4 remaining)** — ✅ ACCEPTABLE: the cascade frontier is *correct*;
  queue-filtering and `HIDE_REPLACED_BUILDINGS` are UI display layers, not frontier gates. CHANGE-accept.
- **`prereqOrBuildings` (2)** — ⚠ ALIGN: scope the obsolete-waiver out of OR-building groups.
- Predicate gaps (`latitude`, `location`/improvement, radius-scoped `requiresBuild`) — ✅ FIXED via new
  predicates (`PRED_LATITUDE`, `PRED_HAS_IMPROVEMENT`, `PRED_HAS_TERRAIN` now scans the workable radius per the
  spec's deliberately-more-permissive VICINITY model — expect new cascade-more-permissive over-offers = CHANGE).

Runtime maintainers (B1–B5) are tracked there too; B1 (religious dormancy) is MATCH-verified, B2
(`hasAllReligionsActive`) is a known-gap currently MOOT, B3/B4 (property-band + autobuild) are shadowed with
dispositions pending, B5 (resource dormancy) is shadowed and drives the build→operate curation.

### 4b. Modifier / magnitude divergences (`modifier-cascade-known-discrepancies.md`)

The city-yields pilot (food/production/commerce, Mode A parity) is shadowed via `/diagnostic/modifierSweep` +
`[MODSHADOW]`, dispositioned on the **care scale** (Fine · Rounding · Better · Weird · Bug · Meltdown). Current
state is dominated by **`missingDeposit` (care Bug)** — not real bugs, but **sources not yet wired** — shrinking
as each source is added. Offline emulator (`Tools/ModifierCalc/cascade_sim.py`) findings (2026-06-19, 6-city sweep):

- **Production** near-parity; **food** partial (dormancy variance + un-wired sources); **commerce** was the
  outlier (+25–38% over), root-caused to the **education property-band ladder**.
- **The band fix landed:** pseudobuildings now **stack cumulatively** (never `replace` each other) and are
  authored as **incremental deltas** so the cumulative active bands reproduce legacy's top-band total;
  `requires.operate` `PROPERTY_X`-in-band gating is authored on all 188 pseudobuildings. Commerce moved
  1/6 → 5/6 within ±10%; all residual gaps now NEGATIVE (cascade under).
- **⛔ ±10% is NOT the bar** ([DEC-parity-not-goal](../architecture/decisions.md#dec-parity-not-goal)) — the
  residual must be driven sharper. Grounded attribution: the residual is mostly **tech-downward deposits** (the
  sim doesn't load tech JSONs / keyed sub-scopes yet) + **corporations** (excluded from the modifier cascade
  **by design** — a `Better`, not a bug). Next: wire tech-downward keyed deposits, then decide corporation handling.
- **Governing principle:** ONE unified calculation; re-author the DATA to fit it — do **NOT** replicate legacy's
  per-property calc quirks. The result is parity-*adjacent*, with legacy's inconsistency a corrected `Better`.

Later channels (commerce-split / health-happiness / defense / maintenance / unit-plane) are 🔭 UNSHADOWED, built
channel-by-channel.

## See also

- [`../explanation/cascade-architecture.md`](../explanation/cascade-architecture.md) — the **DESIGN** this
  roadmap tracks the construction of (the mechanism: the three machines, the spine, the tally). Read it for
  *how it works*; read here for *where it is*.
- [`../architecture/decisions.md`](../architecture/decisions.md) — the rulings this doc links by `[DEC-id]`
  (map-before-delete, parity-not-goal, deliveryguy, fixed-point, obs-scale, …) instead of restating.
- [`../reference/cascade/fixed-point-and-scales.md`](../reference/cascade/fixed-point-and-scales.md) — the scale
  registry the modifier work grounds its ×100 math against.
- [`../reference/cascade/data-model.md`](../reference/cascade/data-model.md) — the JSON shape the curators emit
  and `readJson` consumes; the migration here produces data of that shape.
- [`../reference/observability/README.md`](../reference/observability/README.md) — the surveillance surface that
  is the verification substrate for every shadow-before-cut step in §3.
- [`../README.md`](../README.md) — the comprehension map; this doc is the "Plans / roadmap" entry for the cascade.
- the old `docs/dev/plans/` set this consolidates — `cascade-engine-430.md`, `migration-entity-ranking.md`,
  `building-cascade-conversion.md`, `cascade-mapping-inventory.md`, `cascade-known-discrepancies.md`,
  `modifier-cascade-known-discrepancies.md` (pending archival once this rebuild is trusted).
