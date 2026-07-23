# HTTP endpoints — the observability surface

The local server (`127.0.0.1:7227`) publishes game state for reading. It is a **GET-only** dev server,
gated by the BUG option `Autolog__HttpServer` (off by default), bound to loopback only.

> **⚑ THE ROUTE SURFACE IS CURRENTLY EMPTY — this doc is the SPEC it is rebuilt to, not a description of what
> answers today.** The transport survives and works (`Sources/Tools/CvHttpServer.cpp`: sockets, the game-thread
> single-slot mailbox, the `/events` SSE consumer, `/` liveness); the endpoint BODIES were purged wholesale
> because they had become a chief source of rollerskating — hundreds of per-feature accumulators and oracle twins
> grown behind one route each, which is the exact anti-pattern §"the server SERVES, it does not ACCUMULATE"
> already forbids ([observability.md](../reference/observability.md)). Rebuilding them is part of defining the
> access surface, not a separate errand: an endpoint reads the same uniform channel-indexed getters every other
> consumer reads ([DEC-new-getter-surface](../architecture/decisions.md#dec-new-getter-surface)), so the route
> table cannot be honestly restored before that surface exists. ⛔ Do NOT re-add routes that reach around it into
> legacy accumulators — that is how the previous surface accreted.
>
> The `*Legacy` / `*Recomputed` / `*Leg` comparison fields catalogued nowhere below are **not** to come back:
> they are tautological ([DEC-oracle-tautology](../architecture/decisions.md#dec-oracle-tautology)).

> **Why this surface exists.** The endpoints + the external drycalc together validate that the C2C→S2S port
> **lost no mechanic** — that nothing was forgotten when game logic moved to the JSON/cascade model. The method is
> per-mechanic parity: reproduce **each individual mechanic** from the raw inputs + the `Assets/Data` JSONs and
> check it against the engine to the cent. That is why `/state` must be **complete** (a missing input hides a
> mechanic) and why `/computed` keeps the **full per-source decomposition** (so a divergence localises to the one
> mechanic that drifted, never a guessed aggregate).

The surface is organized around **one axis: verification**. There are exactly two data buckets:

- **`/state/*`** — what drycalc **does NOT compute**: the raw substrate (techs known, buildings present, plots and
  their contents/state, specialists assigned) **plus** out-of-scope engine-derived values it consumes as fixed
  *inputs* (trade-route partners + the yield they give, route capacity it can't re-derive, connectivity, etc.).
- **`/computed/*`** — what drycalc **DOES compute**: its **targets** — modifier-driven yield rates (with their
  per-source decomposition), buildability/availability verdicts, counts, victory state. This is what a calculation
  is checked **against**.

> **The verification flow.** An external calculator (the validator, and the website it seeds) fetches the
> **`/state` inputs**, loads the **pre-built `Assets/Data` JSONs**, and computes its targets itself —
> **without the engine**. It then checks its result against the engine's **`/computed`** answer. Inputs and
> outputs on **separate** surfaces is what keeps the check honest.

## drycalc scope — the line between a TARGET and an INPUT

The split is **not** "raw vs. computed" — it is **"does drycalc compute this?"**:

- **In scope → `/computed` (a TARGET).** drycalc's job is **local, isolated computation — above all the impact of
  modifiers** on a city's values (the primary concern), plus the per-city verdicts/capacities derivable from local
  state + the JSONs (yield rates, buildability/trainability/availability, `maxTradeRoutes`).
- **Out of scope → `/state` (an INPUT).** **Anything that needs the whole game in one go is out of scope** — global
  cross-city/world aggregates, and selections drycalc never re-derives (e.g. trade-route *assignment*, and the
  *yield* those routes give: drycalc folds that yield in, it does not compute it). These engine results are served
  as inputs.

> ⛔ **The hard rule: `/state` must never contain a drycalc TARGET, and the calc must never CONSUME live yield/commerce
> except trade-route yield.** That — not "any engine-derived value" — is where the
> calculator (or an agent) "cheats" by reading the answer instead of deriving it. `/computed` may freely carry raw
> state too; the asymmetry is one-directional.
>
> **The ONE live-yield calc input is trade-route yield** — a *clean addition at the very end* of the base, a component
> the cascade never claims to compute (it can't re-derive the trade network), so folding it in compromises nothing.
> **Every OTHER engine yield/commerce value is forbidden from the calc**, and the distinction is whether folding it
> would land inside a drycalc TARGET:
> - **Derivable values are TARGETS — compute them, never read them.** `freeCityYield` (= Σ trait `getYieldChange`) is
>   derivable from trait JSON; consuming the live value means the trait→yield derivation is *not validated*. Compute it.
> - **CLEAN persisted event/vote stores RIDE IN.** A store that is event/vote-granted state **by construction** —
>   `m_aBuildingCommerceChangeEvents`, the per-building `m_aBuildingYieldChange` — is RAW SAVED STATE (the
>   occupation-timer class, not a computed ride-in), and the in-DLL cascade **folds it**
>   (`BuildingPackage::buildingFlat`, active-gated, ×100 at the legacy tiers); a store the cascade skips would be
>   silently LOST at the cut. The honest-divergence stance holds for the MIXED/dead stores (`m_aiExtraYield` —
>   dead-on-read legacy-side too, folded into no target; its `/state` value is read ONLY at the audit/comparison
>   boundary) and for the offline drycalc leg (StoneBase keeps the comparison-boundary rule; the ride-in is about
>   the in-DLL composition's COMPLETENESS).
> - **⚠ `getBuildingCommerceChange` / `m_aiBuildingCommerceChange` is MIXED, not wholesale-underivable.** It
>   conflates a **DERIVABLE bulk**
>   (`GlobalBuildingExtraCommerces`: a building granting commerce to OTHER building types empire-wide, static JSON —
>   engine `CvPlayer::recomputeBuildingCommerceChange`, `CvPlayer.cpp:27191`) with an **un-derivable remainder**
>   (game-event grants in `CvCity::applyEvent`, `CvCity.cpp:18207` + vote-source resolution grants in
>   `CvCity::processVoteSourceBonus`, `CvCity.cpp:14357`). **Reproduce the derivable `GlobalBuildingExtraCommerces` from the curated
>   `{c}.empire.buildings.{B}.flat`** (`BuildingKeyedEmpireCommerce100`), never read the live value; only the
>   event/vote remainder diverges honestly.
> - **If we pull live yield into the cascade calc, we are not validating the cascade at all.**

> **`/state` is held to API standards.** It is a real, stable, legible API with two consumers — the **validator**
> (the calculator's input) and the **frontend/website** (the thing that *displays* state). It is targeted and
> isolated by design: fetch *just* the tech lists, *just* the buildings, *just* the plots. It is not throwaway
> diagnostic output.

---

## The buckets

| bucket | answers | lifetime |
|---|---|---|
| `/` | alive? (`hello world`, the 11-byte smoke check) | — |
| `/events` | what just changed — the gated `[TAG]` SSE stream | — |
| `/state/*` | **what IS the state** — raw inputs, no computed values | — |
| `/computed/*` | **what the engine computes from it** — the verification ground-truth | — |

Anything that is not GET gets `405 Allow: GET`. Every response carries `X-S2S-Turn`. The `/state` and `/computed`
endpoints read live game objects, so they are served on the **game thread** via a single-slot mailbox (≤5 s
stale, consistent read); a second concurrent data request gets `503` — retry once.

---

## City identity — ⛔ city ids are NOT unique across empires

A `CvCity` id is unique only **within one player**. Two empires can (and do) hold cities with the same id, and
city **names** can collide too (renames; same name across civs). So:

- The canonical, globally-unique handle is the tuple **`(owner, id)`** (what trade-route partners already use), or
  equivalently the city's **plot `(x, y)`** (one city per plot).
- **Every** city object in `/state` and `/computed` carries `{ "owner", "id", "globalId", "name", "x", "y" }` so a
  consumer can build whatever lookup it wants (by name, by tuple, by plot).
- **`globalId`** is the derived snowflake **`"<PP>-<id>"`** (owner zero-padded to 2 digits, e.g. `05-8192`) — a
  single **stable, globally-unique** reference for the API *and* the in-game hover tooltip (both derive it from the
  same `owner`+`id`). It is purely derived — no new engine state, no save change; split on `-` to recover the
  `(owner,id)` tuple. (The engine's raw `id` is an `FFreeListTrashArray` value, unique only within a player; the
  cross-player engine handle is `IDInfo{owner,id}`, which `globalId` simply stringifies.)
- Single-city fetches key on the unambiguous tuple **`?player=N&city=M`**, or — equivalently and more conveniently
  — the snowflake **`?globalId=<PP>-<id>`** (decoded server-side back to that tuple). `?name=NAME` is a convenience
  lookup that returns a **disambiguation list** when the name resolves to more than one city. **Never** address a
  city by bare `id`.

---

## `/` · `/events`

- **`/`** — liveness (`hello world`).
- **`/events`** — the gated `[TAG]` SSE stream, live. The per-turn lines burst at the **top of `doTurn`**,
  so **connect before the turn ticks** (connect-then-end-turn). See [logging](logging.md) §5.

## `/state/*` — raw inputs (no yields, no oracles)

Shape: **`/state/<slice>`** for game-wide lists, **`/state/<entity>/...`** for entity-scoped reads.

- **`/state`** — index: lists the available slices (auto-generated from the route table).
- **player / world scope**
  - `/state/techs` — every player's **completed** techs (per-player lists)
  - `/state/civics` — each player's current civics · `/state/religions`
  - `/state/players` · `/state/players?player=N` — raw player facts: era, traits, heritages, state religion,
    civics, commerce sliders, anarchy/rebel flags, city/building/unit counts — **no computed rates**. (The
    *computed* economy — gold/science/upkeep/inflation — is verified separately on `/computed/players`; the raw
    state lives here so the calculator has its inputs without reading the engine's answer.)
  - world facts (active game options, era ordering, per-team diplomacy, raw game-define scalars) live in the
    `world` section of **`/state/all`** — ⚠ there is NO standalone `/state/world` route (a dedicated slice can be
    added if a consumer wants it without the full dump)
- **city scope** (`{owner,id,name,x,y}` on every city; see *City identity* above)
  - `/state/cities` · `/state/cities?player=N` — every city's raw substrate + out-of-scope inputs:
    - **buildings** present (+ the dormant subset), production queue, building ages/time-built
    - **specialists** — current assigned counts (normal + free) **and capacity** (max it can have, normal + bonus)
    - **plots** — terrain, feature, improvement, route, bonus, vicinity-bonus, river, irrigation, hills, peak,
      water, coast, city-center, worked flag, stored extra-yield state
    - bonuses (hasBonus / vicinity / counts), corporations (present + active), religions (+ holy-city),
      properties (current values), culture level, connectivity (connected-to-capital, distance-from-capital)
    - **wellbeingInputs** — the health/happiness RAW-STATE input vector ([modifier.md §2b](modifier.md)): the
      named anger percents, espionage/event/tax/foreign/landmark/over-limit/vassal terms, the gate flags, and
      fresh-water access — the runtime state the wellbeing calc FOLDS but never derives (the tradeYield
      precedent). ⛔ no deposit-computed wellbeing target rides here (those are `/computed/cities/wellbeing`)
    - **free XP** the city confers on a newly-built unit
    - **trade routes** — the engine-picked partner cities `{owner,id}` **and the yield those routes give**
      (out-of-scope for drycalc to derive; an input)
    - ⛔ **no drycalc TARGET** — no `getYieldRate100`, no per-channel modified yield, no buildability verdict
  - `/state/cities?player=N&city=M` (or `?name=NAME`) — one city
  - isolated sub-slices: `/state/cities/plots` · `/state/cities/buildings` · `/state/cities/specialists`
    (same selectors) — *just* that list
- **map scope**
  - `/state/plots` — **every map plot by global index** (`idx` = `CvMap::plotNum(x,y)` — the World plot id a
    consumer keys on): terrain, feature, improvement, route, bonus (+ `bonusConnected`), the predicate facts
    (water / hills / peak / coast / river / freshwater / irrig), `relief`, `isCity`, `ownerTeam`, the **`workingCity {owner,id}`**
    link + `worked` flag, the **`radiusCities [{owner,id}…]`** membership, `area`, and the stored `extraYield`. Same
    all-plots walk as `PlotSnapshot`. ⛔ raw only — no yields.
    - **`relief`** (`"PEAK"`/`"HILLS"`, absent = flat/ocean) = the REAL plot-type from `getPlotType()` — **distinct from
      the `peak`/`hills` predicate flags**, which are `isHills()` / `isPeak()||isAsPeak()` and therefore count a
      feature-induced *as-peak* (e.g. a Kilimanjaro on flat land). The plot-type **base yield** (`getPeakChange`/
      `getHillsChange`, `CvPlot::recalculateBaseYield`) keys on `getPlotType()`, so a consumer computing nature yield
      must read `relief`, not `peak`/`hills` — an as-peak gets `relief` absent and so no relief base yield. The `peak`/
      `hills` flags stay for the `HAS_PEAK`/`HAS_HILLS` predicates (which *should* count as-peaks).
    - **`workingCity`** = the single ASSIGNED city (`getWorkingCity`); **`radiusCities`** = EVERY city whose workable
      radius (`getCityIndexPlot`) includes this plot — a many-to-many superset (overlapping fat crosses), the engine's
      getCityIndexPlot **inverse**. A city's full workable-plot set (its VICINITY substrate) is "all plots whose
      `radiusCities` contains it", so the per-city plot lists become fully derivable from this map. ⚠ `bonus` is revealed
      to the **working** team, but a city's own plot list wants it revealed to **that city's** team — so a late-era
      resource on a *foreign-owned* radius plot can read differently per asking city (it never counts for that city's
      vicinity anyway: `hasVicinityBonus` needs owned + connected).
    - The per-city plot lists (`/state/cities`) are being migrated to **reference these by `idx` id** instead of
      duplicating the plot facts (in flight).
- **unit scope**
  - `/state/units` · `/state/units?player=N` — raw unit facts: type, unitAI, position, group, mission/activity,
    damage, level, domain
- **info scope**
  - `/state/info?type=ANY_INFOTYPE` — the loaded INFO OBJECT's edge unit: the authored families
    (`enables`/`obsoletes`/`obsoletedBy`/…) AND the load-derived reverse families (`related`/`requiredBy`,
    [DEC-one-reverse-view](../architecture/decisions.md#dec-one-reverse-view)), ids rendered to type names.
    **The standing readJson-correctness verification (owner ruling): what this returns must MATCH the entity's
    authored `Assets/Data` JSON** — a divergence is a load/parse defect (e.g. the aliased cross-category edge
    drop, found by exactly this read), never data to accept.

## `/computed/*` — the engine's answers (verification ground-truth)

The engine's computed outputs, **engine-only** (no cascade comparison — the cascade is validated later by
the external dry-calc + logging, not here).

- **`/computed`** — index.
- **yields** — `/computed/cities/yields?player=N&city=M` (or `?name=`): the city's `getYieldRate100` per channel
  **with its full per-source decomposition** (base/specialist/plot/trade/building/bonus/civic/… flat + percent
  terms, and the commerce split) — the modifier-impact ground-truth the calculator reconciles against. Also
  carries **`maxTradeRoutes`** (drycalc computes the route capacity, so it is a target verified here).
- **buildable / trainable oracles** — `/computed/cities/buildable?player=N&city=M`: the engine's `canConstruct`
  TRUE-set for the city (the buildable oracle) · `/computed/cities/trainable?player=N&city=M`: the engine's
  `canTrain` TRUE-set for the city (the trainable oracle).
- **wellbeing** — `/computed/cities/wellbeing?player=N&city=M`: the city's health + happiness ORACLE — the
  realized levels (`happyLevel`/`unhappyLevel`/`angryPopulation`/`goodHealth`/`badHealth`/`healthRate`) **with
  the FULL per-source decomposition**: the named anger percents (overcrowding/noMilitary/culture/religion/
  hurry/conscript/defyResolution/warWeariness/revRequest/revIndex/Σcivic), every shared signed happiness source
  (emitted RAW once — the happy side takes `max(0,·)`, the unhappy side `−min(0,·)`), every health source (the
  `…100` fields are ÷100 at use), and the gate flags (`isNoUnhappiness`/`isNoUnhealthyPopulation`/
  `isBuildingOnlyHealthy`/…) — the health/happiness channel's parity ground-truth.
- **economy** — `/computed/players[?player=N]`: the empire-scope engine calc whole — gold/science per turn,
  upkeep/inflation/maintenance decomposition, demographics, wellbeing (kept intact with its named inputs; the raw
  player facts are independently on `/state/players`).
- **gate verdicts** — `/computed/canConstruct` · `canTrain` · `canResearch` · `canDoCivics` · `canCreate` ·
  `canMaintain` `?type=PREFIX_NAME&player=N[&city=M]`: the engine verdict (+ the first failing legacy gate for
  construct/train).
- **availability oracles** — `/computed/availableTechs` · `availableCivics` · `availableBuilds` `?player=N`: the
  engine's per-player "what could be researched / adopted / built" sets.
- **enabler domains** — `/computed/enabler/buildings?player=N&city=M`: the standardized per-city building domain
  (listed/tree counts + fresh-seed oracle diff) · `/computed/enabler/units?player=N&city=M[&type=UNIT_X]`: the
  per-city unit domain (listed/tree counts + per-unit verdict decomposition) ·
  `/computed/enabler/promotions?player=N&type=PROMOTION_X&unit=U`: the promotions composite decomposition (domain
  planes + per-leg verdicts).
- **counts** — `/computed/tally?type=BUILDING_X|UNIT_X&player=N`: the engine count at world/team/empire scope.
- **diagnostics** — `/computed/whyNot?type=UNIT_X&player=N[&city=M]` (the canTrain decision inputs) ·
  `/computed/game` (turn / game-over / winner / victory countdowns — the autoplay terminal signal) ·
  `/computed/perf` (the (scope,channel) calc-count histogram + total this turn — the 50k gate,
  DEC-calc-count-gate) · `/computed/barProbe` (every city's EXE billboard bar floats, NaN/INF/out-of-range
  flagged — the value-poison probe) · `/computed/sceneReset` (re-run the LOAD-path city-scene build for the
  active player's cities — the drop-conviction probe) ·
  `/computed/units/combat?player=N[&unit=M]` — per-unit combat-strength ATTRIBUTION: the persisted per-unit base
  (`combatRaw` = the save-carried `m_iBaseCombat`, `canFight`'s gate), the loaded info's authored base
  (`combatInfo` = `identity.base.combat` — a `combatRaw`≠`combatInfo` split names save-carried vs load-time as a
  zero-strength origin), the effective read (`combatEff` = `baseCombatStr()`, SM-aware), `smStrength`, and the
  `canFight`/`canAttack`/`onlyDefensive` verdicts. Read-only; NOT the unit-plane channel decomposition (below).
- **classification-parity oracles** —
  `/computed/teamFlags?player=N`: the engine team/player capability flags by CANONICAL name (+ the
  `canTrade`/`canTradeOn`/`canWorkOn` blocks) — the capabilities-parity oracle; ·
  `/computed/unitSkills?player=N`: per-unit EFFECTIVE skill booleans (the composite getters, unit+promotion+
  unitcombat folded) — the skills-parity oracle. The cascade side of both is derived OFFLINE from `/state` + the
  `Assets/Data` JSONs (the validation.md external-dry-calc leg); the composition rules a deriver must fold are in
  [skills.md](skills.md) §3b and [capabilities.md](capabilities.md).
- **unit heal** — `/computed/units/heal?player=N&unit=M`: a PINPOINTED unit's per-turn `healRate` at its current
  plot **with the FULL per-source decomposition the engine folds** — so healing can be verified to the point
  instead of by watching random in-game heals. The `?player=N&unit=M` selector keys on the (player, unit) tuple
  (a unit id is unique only WITHIN a player, exactly like the city `(owner,id)` tuple). Fields: `healRate` (the
  authoritative engine total = what `doHeal` would apply), `healTurns`, `damage`/`maxHitPoints`/`currHitPoints`/
  `isHurt`, plot `(x,y)`, an `eligibility` block (friendly/enemy territory, isCityPlot, animal/NPC,
  hasNoSelfHeal, battlefield-medicine — the flags that can gate `healRate` to 0), a `sources` block of the NAMED
  additive terms of the non-heal-as path (`selfHealModifier`, territory base `cityHealRate`/`friendlyHealRate`/
  `neutralHealRate`/`enemyHealRate` + the matching `extra*Heal`, the city `cityContribution` = `pCity->getHealRate()`,
  the best same-tile/adjacent `supportTileHeal` + `supportHealerUnitId`), and — for heal-as-combat units
  (`numHealAsTypes`>0, where the total comes from the SLOWEST-healing type) — a `healAsTypes` array of per-unit-combat
  `healRateAsType`/`healAsDamage`/`cityUnitCombatHeal` (`getHealUnitCombatTypeTotal`). ⛔ **READ-ONLY**: computes what
  `doHeal` WOULD heal via the const `CvUnit::healRate`/`getHealRateAsType`/`healTurns` (all called `bHealCheck=false`,
  so even the support-heal scan performs no `changeHealSupportUsed`/`changeExperience100`); `doHeal`/`changeDamage`/
  `setDamage` are never called — the unit's HP is untouched. Mirrors `/computed/cities/yields` so a heal divergence
  localises to one named term. Source of truth: `CvUnit::healRate` (`CvUnit.cpp`:6065), `getHealRateAsType` (:6256),
  `doHeal` (:6511).

> Unit combat/movement CHANNEL decomposition (beyond heal and the combat-attribution diagnostic, above) is
> **deliberately not exposed yet** — those channels aren't in drycalc's current focus (yields/modifiers +
> buildability). Raw unit membership lives on `/state/units`; the computed combat/movement surfaces are added when
> those channels are worked (don't ship a lone half-channel).

---

## What was dropped (and why)

This surface is a deliberate **clean rebuild**; the following predecessors were dropped, not migrated:

- **The flat `/units` · `/players` · `/cities` endpoints** — a quick cobble-together that served
  computed values from a per-frame snapshot. To be re-created (if needed) as views over this coherent surface;
  the snapshot machinery they required is gone.
- **The `/diagnostic/*` grab-bag and `/extractor/*`** — split cleanly into `/state` (raw) and `/computed`
  (engine answers). The raw extractor dump used to smuggle computed oracles (`canConstruct`, `availableTechs`)
  into the "raw" document; those now live on `/computed`.
- **The `/shadow/*` cascade-vs-legacy sweeps** — the cascade is validated by the external dry-calc and by
  logging (same-calc-same-output), so the in-DLL sweep endpoints were retired rather than carried forward.

---

## See also
- [logging.md](logging.md) — the SSE `[TAG]` stream, the read rules, the `data-reader` minion.
- [validation.md](validation.md) — the dry-calc / parity verification this surface feeds.
- [modifier.md](modifier.md) — the magnitudes `/computed/cities/yields` decomposes.
