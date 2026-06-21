# Unified effect-scope platform (driver: team buildings) — design sketch

> **♻ RECOVERED + FACT-CHECKED 2026-06-17** (lost in the `Sources/docs → docs/dev` shuffle; no delete commit).
> **The CONCEPT here is WANTED and current (owner 2026-06-17): empire/team-scope "buildings" as the replacement
> for per-city autobuilds (`FreeBuilding` / `bAutoBuild`), with a shared hammer pool — #421.** This is its design
> home. What's **superseded is the implementation vehicle**: the "unified effect-scope platform" = the #421/#423
> **`CvCascadingModifierBundle`** + the prototype + the staged plan + version-pull caching — that bundle/
> repository approach was dropped for the #428/#430 top-down model (the three machines: tally / modifier /
> enabler). In the current model the tier needs **no new machinery** — a "building" simply exists at empire/team
> scope (the scope spine already has those tiers), authoring `enables` / `disables` / `requires` / modifier
> families like any entity; see `enabler-cascade-spec.md` §5 (the EMPIRE/TEAM-WIDE PROJECTS/BUILDINGS tier).
> **Read this for the concept, the autobuild-replacement motivation, the shared-hammer-pool idea, and the
> composition / acquisition-policy thoughts; treat the `CvCascadingModifierBundle` plumbing as historical.**

Status: **DESIGN SKETCH — evolving.** **The platform is now its own issue: #423** (unified effect-scope
cascade — the FOUNDATION). **#421** (team projects/buildings) is the *consumer* that sits on top of it.
This doc is the shared design home for both. Prototype is in the working tree (Assert build green +
deployed), nothing committed/branched. **Save-break posture (refined 2026-06-13):** owner originally
approved a save-break, but the save/load investigation (`../reference/save-load-format.md`) shows it's mostly
unnecessary — the cascade is **derived / save-neutral** (serializes nothing; see "SAVE-NEUTRAL" below), and
**removing the old accumulators is a SOFT change, not a save-break** (name-keyed format drains orphan tags on
load). `@SAVEBREAK` is reserved for the four genuinely-HARD cases and for *tenants* that hold authoritative
runtime state (e.g. #421's shared hammer pool) — not for the cascade or its accumulator cleanup.

> Sequencing decision (owner, 2026-06-13): **build the unified bonus/effect platform FIRST**, so team
> buildings, world projects, #411 power, and everything else are *tenants* of one mechanism rather than
> yet another bespoke accumulator. Team projects are the first new tenant that validates the platform.

## The problem (inventory-confirmed, not speculation)

Two read-only inventories of `CvGame`/`CvTeam`/`CvPlayer`/`CvCity` and the four big effect sources
confirm the owner's hypothesis ("widely separated systems shoehorned together"):

- **There is NO shared effect abstraction.** Civics, Traits, Buildings, and Projects are **four
  independent hand-rolled effect bundles** — each a giant Info struct (~100+ effect channels each:
  `CvCivicInfo` ~105, `CvTraitInfo` ~115, `CvBuildingInfo` ~125, `CvProjectInfo` ~20) with its **own
  hand-written apply function** (`CvPlayer::processCivics` ~`CvPlayer.cpp:17981`,
  `CvPlayer::processTrait` ~`:28407`, `CvCity::processBuilding` ~`CvCity.cpp:4499`,
  `CvTeam::processProjectChange` ~`CvTeam.cpp:4508`). No common base, no shared apply
  helper/visitor, no shared effect representation. `CvInfoUtil`/`getDataMembers` is a generic
  *XML-field-wrapping* harness, not an effect abstraction.
- **They all converge on the SAME ~30–40 player-level accumulators** (`m_iExtraHappiness`,
  `m_iYieldRateModifier[]`, `m_iCommerceRateModifier[]`, `m_iFreeSpecialist`, the maintenance
  modifiers, GP-rate modifiers, …). This shared ledger is the *de-facto* convergence point — the
  unification target already exists; it's just fed by four parallel open-coded loops.
- **Aggregation is ad-hoc per field.** Every city yield/commerce/happiness function independently
  reaches up to the player (`CvCity.cpp:5643/5703/5842/11998`, `getTradeRoutes` even reaches to
  `CvGame`). There is **no** helper that walks city→player→team→game; the cascade exists only as
  copy-pasted per-field reads, and it stops at the player.
- **`CvProperties` is NOT a usable substrate.** It exists on all four scopes
  (`CvGame`/`CvTeam`/`CvCity` hold `m_Properties`; `CvPlayer` via `getGameObject()`), but its
  `PropertyTypes` enum is **empty** (`CvEnums.h`, only `NO_PROPERTY`) and it's used for
  building/event flags, not bonuses. Could be extended, but it carries no scoped-bonus semantics today.
- **Good/bad channels are split inconsistently.** Buildings split happiness/health into
  good/bad accumulators at the city (`changeBuildingGoodHappiness` vs `…Bad…`,
  `CvCity::processBuilding`); civics/traits/projects use signed net scalars at the player. A unified
  layer must standardize this (keep good/bad separate — the UI needs the breakdown).
- **Reversibility is uneven.** Civics/Traits/Buildings apply with `iChange = ±1` (reversible);
  `processProjectChange` wraps most effects in `if (iChange > 0)` so a removed project wouldn't undo —
  a latent bug to fix when projects become a platform tenant.

## The platform — one effect model, four scopes

Ownership/containment chain (top → bottom), each one pointer hop from the next:

| Scope | Object | Applies to | Tenants that deposit here |
|-------|--------|-----------|---------------------------|
| Game | `CvGame` | all players | world projects; future co-owned global builds (#421 stretch) |
| Team | `CvTeam` | all players on team (== one player in standard games) | **team buildings / team projects** |
| Empire | `CvPlayer` | all cities of one player | **civics, traits (both kinds), player-wide project effects** |
| City | `CvCity` | one city | **normal buildings** |

Three pieces:

1. **A unified effect channel set** — the union of the bonus vocabulary the owner listed and the
   inventory found: per-`YieldTypes` flat + modifier (food/hammer%/…), per-`CommerceTypes` flat +
   modifier (science/gold/culture/espionage), happiness **good/bad** + anger, health **good/bad** +
   unhealth, free specialists, maintenance modifiers, GP/GG rate, trade routes, production modifiers,
   experience, … Plain C++03 arrays / `std::vector` / the existing `IDValueMap` sparse type (already
   used by traits/buildings for per-building/bonus channels). No big-bang: start with the channels
   `processProjectChange` already moves, widen per tenant.
2. **A scoped holder** — each of Game/Team/Player/City owns one effect bundle, with a uniform
   reversible `applyEffects(bundle, int iChange)` and typed getters. This *replaces the four bespoke
   apply functions over time*: `processCivics`/`processTrait`/`processBuilding`/`processProjectChange`
   each become "look up my declared bundle, call `applyEffects` at my scope." The shared player ledger
   stays the storage; the four open-coded loops collapse to one.
3. **A cascade reader** — `effective(channel, city) = city + player + team + game`, a single helper
   instead of per-field copy-paste. Cache rolled-up per-(scope,channel) totals in the **derived-data
   repository** (`derived-data-repository.md`, bounded staleness, invalidated on bundle change) so the
   walk-up isn't repeated every query — this is the perf payoff that lets #421 delete N per-city
   building rows and their cache flushes.

### Composition: mutually-exclusive policies as modules, shared effects as the tenant

The two leader-trait systems (original vs **developing leaders**, `GAMEOPTION_LEADER_DEVELOPING`;
plus `GAMEOPTION_LEADER_COMPLEX_TRAITS`) are **mutually exclusive** and selected by game option. They
already share the **same effect chokepoint**: every path funnels through
`setHasTrait → processTrait(±1)` (`CvPlayer.cpp:29289`); the option only changes *acquisition/leveling*
(developing-line traits are `getLinePriority() != 0`). This is the model the owner wants generalized
(2026-06-13, "modularize with composition"):

- **Effect application = the shared platform tenant** (one path, already converged). Developing
  leaders is far more expansive but needs **no special effect plumbing** — unifying the effect layer
  makes it compatible for free.
- **Acquisition policy = a composed, swappable module** behind an interface (original / developing /
  complex), chosen by game option — not branching `isOption(...)` scattered through `CvPlayer`. This
  fits the standing "shrink AI inherited classes / interface-bounded composition" goal.

## First real tenant: handicaps — and the FLAT vs MODIFIER channel-kind requirement (owner, 2026-06-13)

The cleanest *real* first tenant (better than a synthetic self-test) is **difficulty handicaps**. They are
already a per-player difficulty lever that behaves like a scope-cascade (game handicap → applies to all of
a player's cities), today read ad-hoc — e.g. `happyLevel` reads
`GC.getHandicapInfo(getHandicapType()).getHappyBonus()` (`CvCity.cpp:5698`). Two reasons this is the right
tenant:

1. **It collapses the AI/human duplication into one lever.** `CvHandicapInfo` carries *parallel* human and
   AI field families: `getHappyBonus`/`getHealthBonus` and `getResearchPercent`/`getUnitUpkeepPercent`/
   `getInflationPercent`/… **plus** a whole `getAITrainPercent`/`getAIConstructPercent`/`getAIResearchPercent`/
   `getAICreatePercent`/… set (`CvHandicapInfo.h:29-79`). Routing handicaps through the bundle means a
   handicap deposits ONE value into the player-scope bundle (chosen by whether the player is AI or human),
   and every consumer reads the cascade — one uniform AI/human handicap lever instead of two scattered field
   families. This also serves the AI-vs-human benchmarking goal: dial the whole handicap bundle to zero to
   test pure competence, with a single observable place for the numeric advantage.

2. **It FORCES the bundle to model channel KIND, which is the concrete answer to the design review's
   "heterogeneous effect semantics" flaw.** Handicaps are not all additive:
   - **FLAT channels** (sum additively down the chain): happiness, health, flat yields — what the prototype
     wires today.
   - **MODIFIER channels** (percent): research cost %, training/build cost %, upkeep %, inflation % — these
     are *percentages*. They still "sum down the chain" (Civ4 sums percent modifiers), but are interpreted
     and applied differently by the consumer (sum-of-percents applied multiplicatively to a base, not a flat
     add). So `CvCascadingModifierBundle` needs a per-channel **kind** tag (FLAT vs MODIFIER) and the cascade reader must
     return the kind-correct aggregate; a single additive int-sum is wrong for cost/research.

So the channel set is not just "more flat fields" — it is two interpretation classes. Get this split into
the bundle early (handicaps exercise both), or the modifier channels will be silently mis-applied.

### Prototype landed (working tree, 2026-06-13, NOT committed)
A minimal cascade slice is in the tree to validate mechanics: header-only `Sources/CvCascadingModifierBundle.h`
(FLAT-only starter: happiness, health), a `m_cascade` member + `getCascadeBundle()` on
`CvGame`/`CvTeam`/`CvPlayer`/`CvCity`, the cascade reader `CvCity::getCascadeHappiness()` (= city +
player + team + game) wired **additively** into `happyLevel` (behaviour-neutral while empty), and a gated
self-test in `CvGame::doTurn` (GlobalDefine `CASCADE_BONUS_SELFTEST`, default 0) that deposits 5/3/2/1 at
the four scopes and logs the resolution to `Cascade.log`. No persistence yet (self-test repopulates
each turn). The MODIFIER channel kind (FLAT/MODIFIER changesets) and the version-pull caching design followed.

**First real tenant LANDED (2026-06-13, working tree, Assert-green + deployed):** the difficulty-handicap
**happy bonus** is migrated into the cascade, parity-exact. `CvPlayer::updateCascade()` resets the
player bundle and deposits `GC.getHandicapInfo(getHandicapType()).getHappyBonus()` into CASCADEFLAT_HAPPINESS
at PLAYER scope; it is called at turn-start (`CvPlayer::doTurn`) and on load (`CvPlayer::read`). The two
direct handicap reads in `CvCity::happyLevel` (+max) and `unhappyLevel` (−min) were removed and replaced by
`getCascadeHappiness()` (good/bad split on the net), so every city of the player reads the same value
via the cascade — parity holds because `CvCity::getHandicapType() == CvPlayer::getHandicapType()`. The
synthetic self-test is gone; observability now uses the standard `logCascade(1, …)` (added to
`BetterBTSAI.{h,cpp}`, gated by `gPlayerLogLevel`, teed to `/events`), and the `CASCADE_BONUS_SELFTEST`
GlobalDefine was removed (gating is the uniform log level). HANDICAP_EMPEROR happy=2, so Emperor cities stay
+2 (unchanged); `/events` `[CASCADE]` shows `flatHappy=2`.

**Deferred (next focused step): the cost MODIFIER tenant.** The handicap has no symmetric *human*
research-cost field (only `getAIResearchPercent`), and the cost path is parity-sensitive — so it gets its
own change. `EMPEROR iInflationPercent=130` is a real human-facing modifier candidate. Also TODO: a
new-game-init recompute hook (today new games get the bundle from the first `doTurn`; loaded saves are
covered by the `read` hook).

## What belongs in the bundle (and what does NOT) + the balance philosophy (owner, 2026-06-13)

Three owner rulings that fix the *boundary* and the *method* of this work:

1. **The bundle is for UNIFORM, scope-wide effects — not every building field.** Buildings today carry
   many *indexed/targeted* percent modifiers (per-specialist yield %, per-plot yield %, per-bonus,
   per-terrain) that apply to a specific sub-thing, not uniformly to the city. Those are a different
   SHAPE from a scope-cascade channel and must NOT be forced into a flat bundle (doing so rebuilds the
   per-field complexity the review flagged). So: a source deposits its *uniform* slice (flat happy/health,
   broad yield/commerce percents) into a bundle; its *indexed/conditional* effects stay as their own
   structures. `CvCascadingModifierBundle` is the uniform-effects substrate, NOT a total replacement for
   `CvBuildingInfo`'s effect fields. "Every building defines a bundle" is feasible only for the uniform
   portion.

2. **Structure first, numbers second — uniformity makes balance a separate, safe pass.** Because the
   cascade is additive and behaviour-neutral at zero, every effect can migrate into the uniform structure
   *carrying its existing numbers* (parity-preserving, zero balance change). Balancing is then a deliberate
   later pass on the now-uniform numbers. The structural migration and the re-balance never share a risky
   step. "If the structure is uniform, we can balance the numbers afterwards."

3. **Replace enable/disable-as-balance-lever with tunable numbers — that hack is the spaghetti's origin.**
   Much of the conditional/disable complexity came from toggling bonuses on/off to balance. The migration
   splits two things the code conflates:
   - **Balance-hack disables** (a bonus switched off purely to nerf) → collapse into a *tunable number* in
     the uniform structure (channel = 0 or a balanced value). The spaghetti dissolves.
   - **Genuine gameplay conditions** (has-bonus, has-tech, coastal, state-religion — the review's real
     conditionals) → STAY as conditions; they are not balance hacks.
   Net cleanup: the uniform structure absorbs the balance-hack disables as numbers, leaving only the
   legitimately conditional effects.

## Deposit model: RECOMPUTE at turn-start, not event-driven change() (owner, 2026-06-13)

Bonus values drift over time (handicaps scale with era; dynamic conditions flip), so the bundle's
contributors are **recomputed from scratch at the start of every turn, before any other calculation** —
not deposited once via `change(±N)` on gain/loss.

- **Why this is correct, not just convenient:** recompute-from-scratch is *current by construction* and
  **self-healing**. The event-driven `change(±N)` model (what `processBuilding`/`processProjectChange` do)
  requires a matching delta at every value-change site — miss one and the accumulator is stale forever
  (this is exactly the `processProjectChange` `if(iChange>0)` reversibility bug, and a major source of the
  enable/disable spaghetti). Recompute has no "did we hook every change site?" failure mode; a bad turn
  cannot permanently corrupt state.
- **Mechanics:** per scope, each turn — **clear → sum all current contributors → set**. This accumulates
  multiple sources correctly (handicap +2 happy AND a team-building +1 happy → player-scope flat[happy]=3)
  while staying idempotent across turns.
- **Placement:** a dedicated `updateCascadees()` pass at **turn-start**, before yields/happiness/
  research/cost are computed, so all on-demand reads see fresh values. (The prototype's self-test fires at
  the END of `CvGame::doTurn` — a test artifact; the real pass moves to turn-start.)
- **The boundary that keeps it cheap (critical):** this is viable ONLY because the bundle holds **few,
  uniform contributors** (handicap per player, a handful of team/game deposits) — O(small) per turn. It
  must NOT be extended to re-sum every per-building/civic/trait effect each turn; those are stored deltas
  applied once (the engine deliberately avoids per-turn re-summation — see the perf-premise correction).
  The "uniform-effects-only" boundary is what makes turn-start recompute affordable.
- **Mid-turn discrete events** (a building finishing this turn): accept a one-turn lag, or trigger a
  recompute on the event. Turn-start recompute is the default.
- **Nothing remembers the old bonus → the bundle is DERIVED, not authoritative → the foundation is
  SAVE-NEUTRAL.** Because every value is recomputed from contributors each turn, the bundle holds no
  state worth persisting: on load it is simply empty and the first turn-start recompute rebuilds it from
  the (already-serialized) contributors. So the bundle members are **NOT serialized** — adding them is not
  a save-format change, and the "add runtime persistence / `@SAVEBREAK` / transitional dual-read" concern
  for the cascade disappears. (Save-breaks attach only to specific *tenants* holding authoritative runtime
  state — e.g. #421's shared hammer pool — never to the cascade itself.) This also places the bundle in
  the **derived-data repository** domain: a value derived from contributors, refreshed each turn (bounded
  one-turn staleness). Edge: between load and the first turn-start recompute the bundle reads 0 — recompute
  on load too, or accept the one-frame lag.

This supersedes the earlier "deposit once via change() on gain/loss" note for the bundle's time-varying
contents — `change()` is the brittle path when values drift. (`change()` remains available and is how a
recompute pass sums contributors from a cleared bundle.)

### Optional optimization: version-pull caching (only if measured)

If the unconditional recompute ever proves to cost something, cache it via **per-scope version counters +
pull-on-read** — the derived-data repository's `TDependency` mechanism, so no new machinery:
- Each scope (game/team/player/city) keeps one `bonusVersion` int, bumped when its own bundle changes.
- Each city caches its effective total **plus the `(gameVer, teamVer, playerVer, cityVer)` tuple it was
  computed from**. On read, compare current versions to the cached tuple; recompute only on a mismatch.
- This yields the natural hierarchical blast-radius: a **Game (top) change** bumps `gameVer` → every city's
  tuple mismatches → all recompute; a **single City (bottom) change** bumps only that `cityVer` → only that
  city recomputes; player/team changes scope to that player's/team's cities.
- Crucially it is **PULL, not PUSH**: a top-scope change bumps ONE int (O(1)) — you do NOT walk down and
  mark every descendant dirty; each city lazily detects the mismatch (a 4-int compare) on its next read. No
  fan-out, no "did I invalidate every child?" bookkeeping. A **bounded-staleness backstop** (force-refresh
  every N turns) means even a missed version-bump self-corrects — so this keeps the self-healing property
  that naive dirty-flag caching throws away.
- **Checksum vs version counter — this decides the mechanism.** A *checksum* (hash the contributors)
  reads the SAME inputs the recompute reads, so it costs ~the same as just re-summing them — a checksum can
  never beat the recompute it is trying to avoid. The only detector genuinely cheaper than recompute is a
  **version counter**: one int per scope, bumped on change, checked in O(1) *without reading the
  contributors*. So if caching is ever added, it must be version-pull, NOT checksum.
- **Where it does / doesn't pay (cost analysis):**
  - *Empire-wide (game/team/player)* — a handful of contributors → recompute is trivially cheap →
    **don't cache** (checksum can't beat it; a version check isn't worth the bookkeeping). Recompute every turn.
  - *All cities (city scope × N cities)* — the only place re-deriving everything could add up. But the cache
    pays ONLY if a single city's recompute exceeds a version check, and under the uniform-effects boundary a
    city's bundle also has few contributors → the sum stays cheap. So **measure the per-city recompute**;
    cache cities only if it bites, and then with version counters.
- **Default:** build unconditional recompute first (simple, self-healing); add version-pull caching only
  when measurement justifies it (and never checksums — they cost as much as the recompute).

## Staged plan (incremental, parity-checked — NOT a big-bang rewrite)

This touches four 100+-channel, save-serialized apply paths; per AGENTS.md "nothing is a one-liner."
Stage it so each step is independently diff-validated against the old numbers (gated `[PERF]`-style
parity logging, the proven #195 `reqmodel` approach). No save-break gate is needed: the cascade is
save-neutral and retiring the migrated accumulators is a soft change (see the save-break posture above).

- **Stage 0 — cascade skeleton + first tenant (team projects).** Introduce the effect-bundle type and
  the Team + Game scope holders. Redirect `processProjectChange` to deposit into the **Team** bundle
  (reversible — fix the `iChange>0` guard). Make the existing city/player read sites also consult
  `GET_TEAM`/`GC.getGame()`. Validate in a **standard game** (team == player → numbers must be
  byte-identical), now sourced from the team object. Funding stays per-city; the shared hammer pool is
  **out of scope** (#421 core).
- **Stage 1 — converge the apply paths.** Migrate `processCivics`, `processTrait`, `processBuilding`
  one at a time onto `applyEffects(bundle, scope, iChange)`, each behind a parity check. Standardize
  good/bad channels. Modularize the trait-acquisition policies (composition) as a parallel cleanup.
- **Stage 2 — cache + retire duplication (the #421 payload).** Cache rolled-up totals in the
  derived-data repo. Convert uniform `FreeBuilding`/`bAutoBuild` effects (e.g. Worm Gatherer) to
  Team-scope tenants; route prereq-marker autobuilds to first-class predicates (#195); add the shared
  hammer pool so every city funds one team build.

## Out of scope here (tracked elsewhere)
- Shared hammer-pool funding ledger → #421 core.
- Prereq-marker → first-class predicate → #195 (`unified-prerequisites-195`).
- Cislunar-as-stage-projects / multimap gating → #421 + `multimap-switch-viability`.

## Open decisions
1. Stage-0 channel set: recommend exactly what `processProjectChange` moves today, widen per tenant.
2. Team-game sharing: a team build benefits all allied players (recommended yes — it's a team build).
3. Cache home: derived-data repo rolled-up totals vs eager recompute (recommend repo, bounded staleness).
4. Good/bad: standardize split-channel everywhere (recommend yes — UI needs the breakdown).
5. Substrate: new typed effect-bundle vs extending `CvProperties` (recommend new typed bundle — Properties
   carries no bonus semantics and an empty enum; revisit only if a dynamic/data-driven channel set is wanted).
6. Trait-acquisition modularization: do it inside this work (composition) vs defer (recommend: design the
   interface now, migrate opportunistically in Stage 1).
