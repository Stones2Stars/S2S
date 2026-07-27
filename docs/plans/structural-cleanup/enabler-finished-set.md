# AI production decision on the enabler frontier — the finished set + the unified scorer

> **Status:** plan (owner-directed 2026-07). The realization of [roadmap §F2b](roadmap.md) — the consumer-iteration
> sweep + the `AI_chooseProduction` focus-ladder collapse. Builds on the enabler being the maintained, event-built
> frontier ([enabler.md §6/§7/§8](../../specs/enabler.md)) and answers the decide-side production-strategy carve-out
> in [ai-architecture-north-star.md](../parked/ai-architecture-north-star.md).

The AI decides what a city builds by working from the enabler's **finished set** — the maintained LISTED frontier —
instead of probing the whole entity database per id. Two pieces: (1) the set is **returned** by named frontier
getters; (2) the AI scores that set in **ONE unified pass** so every candidate competes on one comparable scale.

## 1. The frontier is RETURNED, not re-derived — ✅ REALIZED

The consumer took the whole building/unit database and matched each id against the enabler
(`canConstruct(id)`/`canTrain(id)` in a loop) to rebuild a list the enabler **already holds whole and current**.
That is replaced by first-class set getters on `CvCity` over the maintained LISTED tri-state:

- **`CvCity::getConstructibleFrontier(std::vector<int>&)`** → `m_enabler.buildings.listedIds` (the LISTED buildings).
- **`CvCity::getTrainableFrontier(std::vector<int>&)`** → `m_enabler.units.listedIds` (the LISTED units).

`out` is caller-owned (a hot loop reuses one buffer). The 12 F2b call sites read these instead of reaching into
`m_enabler` — ONE clean read surface. **`canConstruct` / `canTrain` are UNTOUCHED** — they stay the single-entity
`requires` answer (and the what-if callers: `bContinue`/`bIgnore*`/`bTestVisible`/probability). The set-getter and
the per-id gate are complementary; the gate is never the inner step of a "check everything" loop again.

An input **set is safe** ([ai-architecture-north-star.md §2.4](../parked/ai-architecture-north-star.md)); this is the
low-risk half.

## 2. NO value cache — the cache is over the top (owner ruling 2026-07)

An earlier draft cached the AI VALUES per frontier item. **Dropped.** The measurements say the cost was never the
valuation — the CABV **PreLoop** (building the candidate SET by sweeping all ~5,202 buildings) was **~94% of
`CalculateAllBuildingValues`**, while the per-building valuation dimensions summed to **~1.7%**
([turn-time-optimization.md](../parked/turn-time-optimization.md)). Iterating only the frontier (dozens of items,
not thousands) removes exactly that cost, so re-valuing the frontier per pass is cheap and **nothing needs
retaining**. The value cache is also the single piece the record shows **hung the game** (the building-value cache
looping `AI_chooseProduction`, [ai-architecture-north-star.md §2.4](../parked/ai-architecture-north-star.md) / the
`CvCity.cpp:1256` retention experiment). Dropping it deletes the one loop-prone, hazardous plane outright — a
simplification, not a loss. (Reopen only if a measurement on the frontier-only path ever proves a cache is needed —
and even then it is the AI advisory-heuristic plane [superseded-ideas #1], never a cascade `CvDerivedCache`, and its
only save-safe retained form is the [build-queue-parity](../parked/ai-build-queue-parity.md) snapshot-then-recalc.)

## 3. `AI_chooseProduction` — ONE unified scoring pass (the active work)

**The problem (measured + structural).** `AI_chooseProduction` is an **~84-stage sequential priority ladder**: each
stage calls a *typed* chooser with a narrow focus + threshold, and **the first stage whose chooser clears its
threshold commits and returns** — the rest never runs. The four candidate types (buildings / units / projects /
processes) sit on **four incompatible value scales**, bridged only by stage order + `m_iTempBuildPriority`. Two
mechanisms force each building stage to be single-dimensional: the **per-focus threshold gate**
(`AI_buildingValueThreshold` zeroes a building that fails the *focused* dimension even if its general value is high)
and **`bMaximizeFlaggedValue`** (×20 the focused dimension). Consequence, the owner's own diagnosis: **the AI cannot
reliably score a production-based building over a commerce-based building** — production and commerce buildings only
compete inside the combined-focus `iEconomyFlags` stages (#48/#53/#72/#73), and only if no earlier narrow stage
already grabbed the build.

**The enabling insight.** `CalculateAllBuildingValues` **already computes every dimension** per building into
per-dimension buckets (`FocusValueSet::m_focusValues[]`, the `BUILDINGFOCUSINDEX_*` slots — food / production /
gold / research / culture / happy / health / defense / experience / specialist / maintenance / …). The ladder just
reads *narrow slices* of a table that is already fully populated. So unifying is a **SELECTION change, not a
valuation rewrite** — the `iEconomyFlags` combined-focus path is literally the prototype of the unified scorer
already in the code.

**The design — Level A (buildings unified; owner-chosen 2026-07).** The ladder conflates two different jobs, and
only one is a value ranking:

1. **Urgency / safety** — defenseless → defender NOW, emergency happiness (`<-7`), emergency health/property,
   minimum workers/defenders, keep-current-if-nearly-done. **Not** a value comparison (a defenseless city must build
   a defender even if a bank scores higher). These stay as a **thin priority PRE-PASS**, ahead of the value pass.
2. **The economic building choice** — the ~70 focus/economy building stages **collapse into ONE unified scoring
   pass**: each frontier building scored by the **weighted sum of ALL its dimension buckets** (emphasis/needs supply
   the per-dimension weights), with the **per-focus threshold and the ×20 maximize retired**. Highest score wins →
   production and commerce buildings compete directly on one comparable scale. Emphasis becomes dimension *weights*,
   not separate stages.

**Level A scope (the first slice):** buildings only. Units keep the contract-broker tender, projects/processes keep
their own gates and their relative ladder priority; only the building economic choice unifies. **Level A needs NO
new metric** — the building dimension buckets are ALREADY one currency (the AI value points
`CalculateAllBuildingValues` produces; the `iEconomyFlags` path already sums across them), so unifying buildings is
just reading the whole common-currency row instead of a narrow slice.

**⛔ Level B is PARKED behind a real common-metric design (owner ruling 2026-07) — NOT a scope-creep target.**
Bringing units / projects / processes onto one scale would require a **cross-type common metric** (value-per-hammer,
so a settler vs a granary vs a bank vs a wealth-process can be ranked together). That metric **does not exist and is
undesigned** — and *"just whacking one in is the precise issue we already have"*: a combat/role unit value, an
economic building value, a project value and a process value are genuinely different currencies, so fabricating a
conversion by fiat manufactures a fake common scale and reproduces the exact incomparable-ad-hoc-scales disease this
whole rework is curing, one level up. Level B also reaches deeper into AI behaviour than the owner wants touched.
So Level B is a follow-on gated on a **deliberate common-metric design first**, never an implementation shortcut,
and is out of this work's scope.

**⚖ Behaviour change is EXPECTED and does NOT gate on a playtest (owner ruling 2026-07).** Removing the per-focus
threshold + ×20 changes which building the AI picks — deliberately (better cross-focus decisions). "The AI is in
general bad; this kind of testing is for users to report on after release." So Level A ships on a clean build +
endpoint-sane checks; it is **not** held for a pre-release behaviour playtest, and it is a deliberate improvement,
not a mirror ([DEC-playability-not-a-gate](../../architecture/decisions.md#dec-playability-not-a-gate) applied to
the decide-side AI rework).

**✅ Realized (`CvCityAI`).** No new scorer was needed — the unified pass is the existing `AI_chooseBuilding` called
with a full-dimension mask (`AI_bestBuildingsThreshold` already iterates `getConstructibleFrontier` and scores each
building via the per-dimension `m_focusValues` buckets; a multi-flag mask sums them). Concretely:
- **`BUILDINGFOCUS_ECONOMY`** (`CvCityAI.h`) = the OR of every ECONOMIC value dimension: food / production / gold /
  research / culture / happy / health / maintenance / specialist / espionage. Deliberately EXCLUDED (each keeps its
  own dedicated stage, not economic value): the situational focuses (WONDER / DEFENSE / DOMAINSEA / PROPERTY /
  CAPITAL) and **EXPERIENCE** — the latter is the **military domain-XP focus** (build a domain's XP buildings), a
  military axis decided by its own stage (#52, kept, land-XP-gated; the per-domain mechanic is ambiguous and out of
  scope).
- **Early economic tier (#25/#26):** the single-focus FOOD-then-PRODUCTION passes collapse into ONE
  `AI_chooseBuilding(BUILDINGFOCUS_ECONOMY, …)` before the main military stack; #26's worker sub-stage is kept.
- **Late economic tier:** `iEconomyFlags` is redefined to `BUILDINGFOCUS_ECONOMY` (the opaque emphasize-gated
  construction deleted), so the #48/#53/#72/#73 economic stages auto-unify; the redundant narrow FOOD (#36/#44) and
  PRODUCTION (#73 sub-block) stages are deleted.
- **Unchanged (the priority skeleton):** the two-tier economic-vs-military ordering, the emergency/wellbeing stages,
  wonders, property control, culture-victory, naval, units, projects, processes, and the process fallback.
Assert-green. Behaviour change is expected and un-gated (owner ruling above); sanity is a loaded-save check of the
picks, not a pre-release playtest.

**The second half of "simplify how value is calculated"** — the ~1,500 lines of per-dimension arithmetic inside
`CalculateAllBuildingValues` — is its own follow-on pass; this unification simplifies the *selection*, not the
dimension math.

## 4. The upgrade cache — retire `allUpgradesAvailable` onto `ud_reachable` (a separate clean slice)

`CvCity::allUpgradesAvailable` + its hand-invalidated `m_eCachedAllUpgradesResults` cache + the five
`clearUpgradeCache` sites retire onto the enabler's `ud_reachable` (`CvUnitEnabler.cpp`), which already computes the
upgrade-tree reachability event-maintained. AI sites read the maintained verdict (`!canTrain`, the simplified
predicate — owner-chosen); the lone production-queue-migration site that needs the resolved upgrade *unit id* gets a
thin on-demand resolver over `ud_reachable`. Dead consumer `canTrainInternal` drops with it. Independent of §3.

## Sequencing
Frontier getters (§1) ✅ · cache dropped (§2) ✅ · the `AI_chooseProduction` Level-A unification (§3, active) · the
upgrade-cache retirement (§4) · Level B and the dimension-math simplification are parked follow-ons. Verified live
per item via `/computed/can*` + the production picks on a loaded save ([validation.md](../../specs/validation.md));
turn time stays the standing perf tripwire
([DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)).
