# Scope packages — the substrate rebuilt to the founding design (#430)

> **Status:** LANDED whole (one landing, owner-blessed; modifier-substrate.md increment I) — under live
> verification (the played-turn proof). · **Authority:** [modifier.md](../../specs/modifier.md)
> §1 (deposit DOWN, accumulate, read O(1) — **the founding design of this migration**) +
> [state-repositories.md](../../architecture/state-repositories.md) (the `CvDerivedCache` component, the
> event→cache routing, the load-only-full-rebuild capstone).
>
> **Why this document exists:** the implemented substrate (increment C→G, `CascadeRateSlots` + epochs +
> rollups) drifted from the founding design — it stores UPPER-scope sums per city (store-at-target), which
> forced version-polling (epochs/stamps), fan-out invalidation, and the read-side ensure protocol whose cost
> collapsed unit automation. This document is the spec's design stated once, whole, so the rebuild happens
> once, properly ([DEC-proper-once]).
>
> **The governing validation ruling — [DEC-structure-before-shadow](../../architecture/decisions.md#dec-structure-before-shadow)
> ([validation.md](../../specs/validation.md) §cadence):** an end turn does NOT confirm a STRUCTURE — a
> per-change endpoint check produces false confirmation even on a wrong structure; structure is gated by fidelity
> to the SPEC, not by a green endpoint check. The drifted substrate read back green through the endpoints for
> increments; that green never validated its structure. This rebuild stands the spec-faithful structure up FIRST;
> the endpoint nets then verify behaviour through it.

---

## 1. The design (the spec, in implementation terms)

**One uniform PACKAGE format — a package is ONE standing summed number.** Package identity is
**(scope × component × channel × UNIT-KIND)**: a **flat package** (Σflat, ×100 where fixed-point) and a
**percent package** (Σpercent) are always SEPARATE packages — never two fields of one struct (percentages and
hard yields never live in the same package; the unit is part of the slot key, [modifier.md](../../specs/modifier.md) §2). The separation is
load-bearing for invalidation: "it's the percentage recalcs that hurt," so a flat-only event must leave every
percent package structurally untouched — package-level separation makes that free, not mask bookkeeping.
(`Πmultiplier` would be a third package kind — identity throughout the migration, no yield/commerce source
authors one.) **The combine mode is FAMILY METADATA, never per-package** (§2: `polarity: signed-split` for
wellbeing, the `min` member floor for defense, `combine: max|min`) — packages store the sums; the family's
metadata wires how the read combines them. A package's INTERNAL composition resolves in isolation before it
joins the combine (the §2a specialist's own percent layer applied to its intrinsic before joining BASE; the
§2 plot package fully resolved before any city stack). No other shape exists at any scope.

**One package set per SCOPE OBJECT, holding ONLY its own scope's deposits.**

| scope object | member | holds (accumulated AT this scope) |
|---|---|---|
| `CvPlot` | `m_yieldCache` (exists, conforming) | the plot's own base package |
| `CvCity` | `m_cascadeCityPackages` | this city's `*.city.*` deposits, **as ISOLATED per-component packages** (below) |
| `CvArea`* | (via the player package's per-area maps, v1) | `*.area.*` sums grouped by area — *promoted to a real `CvArea` member when a second channel needs it* |
| `CvPlayer` | `m_cascadePlayerScope` | the CITY-AGNOSTIC sums only (the city-realization law below): per-source-city building walks (empire percents/flats, each source evaluated in its OWN city's ctx), trait/heritage flats (free-city / GA ungated / playerExtra), the SR pools, the grantor ledgers, the wb area/empire fold maps; gated sums as separate fields (pure city-boolean gates applied at read) |
| `CvTeam` | (none yet) | team-scope deposits when a channel first needs one |
| `CvGame` | `m_cascadeWorldScope` | `*.world.*` sums across all living players |

**⛔ WITHIN a scope, the packages stay ISOLATED per COMBINE POSITION — they never merge into one per-scope
number.** Package identity is **(scope × combine-position component × channel)**. The reason is
[modifier.md](../../specs/modifier.md) §2a's two-tier shape: **worked plots and specialists take the city
percent modifier** (TIER 1 BASE — "specialist yields take the city modifier exactly like worked tiles",
#317), **building flat yields do NOT** (TIER 2 EXTRA — "flat, added AFTER the percentages, NEVER
multiplied"). Summing a city's specialist flats and building flats into one number would destroy the tier
split. So the city's yield packages are, per channel:

| city package | tier / combine position |
|---|---|
| specialist flat yields (counts × specialist deposits, own sub-stack inside) | BASE — multiplied by the percent stack |
| building flat yields (+ perPopulation) | EXTRA — added after the percentages, truncated per §2a |
| **the WHOLE percent stack, CITY-REALIZED** (city+area+empire buildings + civics + traits + projects + civic-keyed, THIS city's ctx) | the percent stack |

**⚖ THE CITY-REALIZATION LAW (the measured Burdigala class):** a deposit whose CONDITION references the city
(SR-in-city, city building presence, any city predicate) is a **city-realized join** regardless of its authored
scope — evaluating it once at player scope (against any single city's ctx) mis-serves every other city, as the
live nets proved (+18..+27 persistent pct deltas on non-capital cities). Therefore ALL conditioned percent
stacks and civic/trait/tech sums realize per city, in the city package, with that city's ctx; the player scope
holds only the genuinely city-agnostic sums — the per-source-city building walks (each source evaluated in its
OWN city's ctx), the trait/heritage flats, the pools, the grantor ledgers, and the gated fields whose gates are
pure city booleans applied at read.

(the plot base is the `CvPlot` package, pulled — also BASE tier; the player scope splits the same way:
free-city/GA trait flats = BASE tier, civic/trait/empire-building percents = stack contributions. Every
channel's combine defines its own positions — wellbeing's signed-split terms, the scalar stacks — and the
packages of a scope follow that channel's positions, never a per-scope blob.)

**The PROVIDER axis (the deliveryguy model, [modifier.md](../../specs/modifier.md) §4): PLOTS, SPECIALISTS,
BUILDINGS, and TRADE ROUTES are the ONLY things that provide yields to the cascade.** They are what
physically produces yield in-game; every other source kind (trait / civic / tech / religion / corporation)
only MODIFIES or CONDITIONS a provider's output, so every yield deposit resolves onto a provider-kind
package (a trait's specialist boost onto the specialist package, a civic's building-keyed percent onto the
building percent stack, …). Trade-route yield stays the one live input (never derived).
**⚖ RULED (owner 2026-07-04): the golden-age trait flats are FLAT BONUSES that ride the flat yield
packages "outside" the normal provider chain — use the existing mechanism, keep it simple (golden age is an
extremely core engine mechanic).** No provider-home is needed: the four-provider law describes what
physically produces base yield; these are plain player-scope flat packages joining BASE at the combine
(`flatGoldenAge`, gate live at read; the trait "free-city" yield flats — `flatFreeCity`, the
`m_aiFreeCityYield` class — ride identically), exactly the landed shape. A later remodel that fully aligns
golden age (the member-mirror → predicate extraction, [modifier.md](../../specs/modifier.md) §3's
golden-age carve-out) stays a possible post-migration item. ⚠ "free-city" = the trait accumulator, NOT the
WLTKD celebration (sole effect: zero city maintenance — [economy.md](../../reference/economy.md)).

**Every package on the ONE component** (`CvDerivedCacheSet<TOwner>`), living ON the object, never serialized,
all-dirty from birth/reset, built at load by the **event reseed** (the eager load build — every present-fact
replayed), refreshed via the owner's thin delegate to the module-side math.

**ONE freshness philosophy: events mark, the cache knows.** No epochs, no stamps, no version polling, no
turn-roll blanket. The event marks the package(s) **at the scopes its deposits actually touch**, with the mask
**derived from the compiled deposit index** (percent-vs-flat and channel split — "it's the percentage recalcs
that hurt"; a flat-only source never rebuilds a percent stack):

| event | marks |
|---|---|
| building completed/lost (`processBuilding`) | the CITY package (derived mask) + the OWNER package iff the building carries empire-scope deposits + the WORLD package iff world-scope deposits + operating buildings (always — operate conditions) |
| religion/corp spread | the city package (SR/corp-conditioned components) + operating buildings |
| specialist count change | the city package's specialist components only |
| civic swap / GA flip / tech researched | the player package + the player's cities' packages (**legitimate fan-out: conditions on city-scope deposits reference these** — `enabled:{TECH_X}`) + operating buildings |
| unit movement | **nothing, ever** ([DEC-unit-modifiers-on-top]; units may never author yield percentages) |
| `doTurn` top | **no blanket** — only event-marked (dirty) packages ensure. The former mark-all "full per-player rebuild" self-heal is REMOVED ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)): complete emit coverage (every mutation — power flips, bonus-network shifts, timers — emits a DOMAIN event that marks its package) replaces it, so nothing stales and no blanket is needed. This is why the proper, COMPLETE event spine is built FIRST |
| city founded / acquired (`CvPlayer::found` + `acquireCity` setup tails) | the creation-time **EAGER ENSURE** (`CascadeAccumulator::cityCreated`: operating buildings, then packages) — the ONE ruled exception to boundary-only rebuilds (below) |

> **⚖ RULED (owner 2026-07-04, the precipice review): the freshness contract is the PER-PLAYER-SLICE
> SNAPSHOT — *"start of next turn is what is expected; getting a yield event in the middle of a turn is not
> retroactive."*** A package marked mid-slice rebuilds at its owner's NEXT slice boundary; mid-turn reads
> serve the turn-start snapshot (the event marks carry the dirt to the boundary — they trigger no mid-turn
> rebuild). Consequences, all ruled/known: (1) end-turn PROCESSING is always boundary-fresh (the slice
> rebuild runs before the cities process), so gameplay magnitudes are right; (2) the human's mid-turn
> DISPLAY (city screen during specialist juggling, after a mid-turn action) shows turn-start values until
> the next turn — watch in play; the surgical remedy if it feels wrong is a UI-only eager refresh hook,
> never a return of read-side ensures; (3) a getter-net divergence sampled mid-turn against always-fresh
> legacy is an ACCEPTED class under this ruling (attributed to the snapshot, never chased); (4) the ONE
> exception is CITY CREATION — owner, same day: *"when a new city is founded, its natural that the initial
> yields and caches are immediately set up, so that we can see time to build"* → `cityCreated` runs at the
> end of the founding/acquisition setups. This supersedes — for the shadow window too — the
> [state-repositories.md](../../architecture/state-repositories.md) "flipped getters must match legacy's
> always-fresh answers" parity-discipline line: the snapshot semantics are in force NOW, not deferred to
> the AI build-queue-parity rework.

**Reads are BARE FETCHES composed by the channel's COMBINE FORMULA over the packages.** A realized getter
fetches the scope packages (plot pull + city + [area] + player + [team] + world as the channel needs) and
combines them **per the channel's spec'd positions** — for yields the §2a shape:
`(Σ BASE-tier packages) × (100 + Σ percent packages)/100 + (EXTRA-tier packages, truncated)` — with the
**per-city gates live**: SR-in-city, coastal, connected-to-capital, area membership, golden age, disorder,
the slider, population, the live military count. No ensure on any read path — freshness is entirely
write-side (event marks + boundary rebuilds). The only live calculation is this trivial position-aware
arithmetic; nothing ever re-walks a source.

**The percent handling is §2a's "ONE additive stack", verbatim:** every percent package is its own trivial
standing number; at read they are **all summed together into the one modifier** ("purely additive — +30%
+20% −10% = +40%"), then **applied ONCE** to exactly the packages the spec puts inside the multiplication
(plots + specialists — the BASE tier), and never to the EXTRA tier. Each isolated calculation is deliberately
super simple in isolation — that simplicity IS the design; complexity anywhere in a package rebuild or a
read composition is a sign the scope/position split was gotten wrong.

### ⚖ THE READ PATH — the CASCADE PROVIDES, the GAME OBJECT SUMS (owner 2026-07-13, LOCKED)

> The load-bearing correction the read layer must conform to. It is where the misunderstanding that cost the
> repeated rebuilds lives: agents treat "the cascade" as the thing that *computes* a yield, and leave the game
> objects as passengers. It is the opposite.

- **The cascade = the invalidated PACKAGE STORE, nothing more.** Per (scope × channel × combine-position) it holds
  one standing sum — *how a yield is influenced, and by how much, from every source*. It answers "what influences
  this," and it **never computes a final number**. Its only job beyond storing is keeping each package fresh via the
  eventspine marks.
- **ONE unified reporting surface.** A single way to enumerate "the packages influencing (object, channel)", read
  **identically** by (a) the consuming game object, to SUM, and (b) the observability endpoints / StoneBase, to
  DECOMPOSE. One surface ⇒ the number a city computes and the breakdown an endpoint renders are the **same bytes**;
  the legacy-shadow decomposition (the engine's `modBuilding`/`modPlayer` accumulators masquerading as the cascade's
  answer) becomes structurally impossible, not merely discouraged.
- **The GAME OBJECT sums LIVE; the sum is NEVER cached.** The object that consumes a channel (plot → city → empire)
  fetches its packages + the lower providers' outputs and applies the channel's §2a combine **on read**. The sum is a
  handful of integer ops over ~5 cached packages — caching it would only add a second thing to invalidate for zero
  gain. Freshness is entirely write-side (the packages carry the event marks); the read never ensures, never
  re-walks a source. **The cascade does NOT own a `yieldRate100`-style "compute the whole rate" function** — that
  summing belongs to the consuming object.
- **Plot / specialist / building caches are PROVIDERS on the surface.** Each computes its own output by pulling its
  influences **FROM THE CASCADE**, never from a legacy player accumulator. ⛔ Today the plot base still reads the
  legacy accumulators (`CvPlayer::getTerrainYieldChange` / `getSeaPlotYield` / `getExtraYieldThreshold`, maintained
  by legacy `processCivics`/`processBuilding`/`processTrait`) — i.e. the plot is **not on the cascade surface at
  all**, which is the measured "plots read low" symptom. Moving the plot base onto the surface (terrain / civic /
  trait / threshold influences pulled from the cascade) is the cure and the concrete meaning of "the plot is a
  cascade provider."
- **One consuming scope per channel — CULTURE the lone exception.** Every channel is summed at exactly ONE scope
  (food/production → city; gold/research/espionage → empire), so there is no combinatorial rollup and no dual
  machinery. The single genuine dual-CONSUMER is **culture**: the city sums it for plot-culture + border expansion,
  the empire sums it for civ-culture + traits — two independent live sums over the same culture packages, each blind
  to the other.
- **A channel's READER is whatever consumes it — not always a yield getter.** A yield's reader is the city/empire
  rate getter; a **property's reader is the PROPERTY SOLVER** — it reads the cascade's summed per-turn property FEED
  (the `PROPERTY_*` packages) off the surface and runs its propagation/equilibrium, writing the plot/city property
  VALUES (its own internal state, then read by e.g. a `requires.operate` band). free-XP, free-specialist AMOUNTS,
  combat channels, buildRate, defense, maintenance — each has its own consuming reader. The surface is uniform; the
  readers are many and various.

**⚠ THE READER STRUCTURE DOES NOT PROPERLY EXIST TODAY — it must be DEFINED.** The "unified reporting surface" is not
a thing the code has; today each getter hand-reaches into specific package fields (or into legacy accumulators). The
rework must first DEFINE the reader structure: the one contract by which a consumer enumerates/fetches "the packages
influencing (object, channel)" and combines them — the same contract the observability endpoints render. This is
net-new design, not a rewire of something present.

**The current deviation (the rework target):** `getYieldRate100`/`getCommerceRate` route into
`CascadeAccumulator::yieldRate100` — the *cascade* computing the whole rate — and the plot base is legacy. The rework
(1) DEFINES the unified reader structure, (2) moves the summing INTO the game objects over it, (3) moves the plot base
ONTO the cascade, and (4) recasts the per-channel calculator modules (`YieldRate`/`CommerceCalc`/…) as the surface's
providers, not the owners of the final sum. **Prerequisite step:** exhaustively IDENTIFY every reader (all yields, all
properties → the solver, free XP, free-specialist amounts, combat/defense/maintenance/buildRate, every
modifier-influenced consumer — [DEC-all-means-all]) so the reader structure is designed against the real, complete
consumer set — then wire each reader to read packages off the surface, which exposes the real state of what is
cascade-backed vs legacy today. The inventory is [reader-inventory.md](reader-inventory.md).

**⚖ THE MIGRATION AXIS — "cascade vs legacy" is NOT "done vs todo" (owner 2026-07-13).** A reader has two independent
properties: WHO does the calc (the game object, or the cascade), and WHAT it sums (cascade packages, or legacy
accumulators). The target quadrant is **game-object calc + cascade-package inputs** — which *neither* current pattern
occupies:

- A **game-object calculator that reads legacy accumulators** (`CvCity::getProductionPerTurn` summing
  `getBaseYieldRateModifier`'s `m_ai*` members; `foodDifference`; the warehouse-style readers) is in the RIGHT place
  — the game object SHOULD compute its own number; it is wrong only in its INPUTS. Migration = swap the inputs to
  cascade packages via the surface; do NOT move the calc into the cascade.
- A **getter that delegates to the cascade** (`getYieldRate100 → CascadeAccumulator::yieldRate100`) has the cascade
  doing the calc — the WRONG place under this model. Migration = **unwind** the sum back into the game object over
  the surface. A "cascade-backed" reader is therefore sometimes the thing to *undo*, not the finish line.

**Game-object MECHANICS are not cascade jobs.** Several channels have a game-object mechanic on top of the cascade
input, and that mechanic is NOT the cascade's to compute:
- the production **warehouse** — `doProduction`/`getProductionDifference` banking, spending the queue, overflow — is
  the city's own mechanic (it READS the cascade-fed making-rate, then accumulates/spends);
- the **commerce rate** (`getCommerceRate`) — converting base commerce into gold/research/culture/espionage is the
  **user's slider ratio** (live state), and the empire rate is the **aggregation of city commerce** (`m_aiCommerceRate`
  = Σ cities). The cascade feeds the base commerce YIELD + the per-channel commerce base-terms (`cBaseOwn100`/
  `cKeyed100`); the base×slider split + the Σ-cities rollup are the game object's, NOT a cascade concern ever;
- the **culture VALUE** (`getCulture`/`getCultureLevel`) — accumulate the cascade-fed culture rate, spend it on borders
  + level-ups (the culture warehouse).

All stay as-is by design; any future interactivity is OUT of #430 scope. The cascade feeds the rate/base + the
influence terms; the warehouse/conversion/aggregation is the game object's. (Consequence for the getter flip-list: a
getter over one of these mechanics is `warehouse-mechanic` — leave it; do not chase it as a "legacy" flip.)

**ONE rebuild mechanism — events mark, dirty packages ensure; the FULL build happens ONLY on load.** During play
an incremental event marks only the package(s) its deposits touch, and only those rebuild. On LOAD the **event
reseed** ([event-spine.md](../../specs/event-spine.md) /
[DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed)) — the save read fires the events for every fact
— so every package is marked and the whole cascade builds once, the ONE full build. Same mechanism both times
(event → mark → ensure); load's events come from the read, play's from live changes. The old recompute-from-state warm-up (`playerSliceRebuild`
+ `worldRebuild`) was a drift-stabilizing STOPGAP and is REMOVED. Turn-time-is-king holds: the reseed is eager,
prepaying the build at load so turns run warm.

**⚖ THE LAW — ONE calculation path for EVERYTHING; a family is METADATA, never a code path
([json.md](../../specs/json.md) §6.3: "the combine mode is family metadata, never the per-value unit";
[modifier.md](../../specs/modifier.md) §2: "declared as FAMILY metadata, never per-deposit"; the one-substrate
principle, cascade-engine-430 §1).** ALL yields work the same way. ALL properties work the same way. Happiness = Σ providers − Σ anger
(negative ⇒ angry); health identically. Every channel — every yield, every commerce, GP-rate, defense,
maintenance, both wellbeing families, tradeRoutes, buildRate, every `PROPERTY_*` — is the identical model:
isolated packages summed per (scope × position), one trivial family-parameterized combine at read, live
inputs folded on top. **There are ZERO special channels.** Properties are the spec'd case of the same split
([json.md](../../specs/json.md) §5/§6 + modifier.md §1 rule (c), data-vs-machinery): each `PROPERTY_*` is an
ordinary family whose deposits the cascade sums — that summed per-turn feed is the CASCADE'S job — while the
**property EVALUATION (the equilibrium engine deciding which way the property moves) sits OUTSIDE the
cascade**, machinery consuming the feed. What differs per family is pure parameterization: the combine
metadata (which positions exist, `polarity: signed-split`, the `min` floor, the §2a EXTRA truncation), the
live-input folds at read (slider, disorder, trade yield, military count, the per-city gates), and — for
buildRate — target-keyed package selection. **Family metadata's home is settled by the spec itself**
([json.md](../../specs/json.md) §6.3 "you author the values; the ENGINE combines them" + modifier.md §1
rule (c) data-vs-machinery): a **static engine-side table**, one row per family (positions / polarity /
floor member / truncation), plain compiled constants keyed by family id — never authored in JSON, zero
runtime lookup cost. So the engine is **one generic package-combine** (packages in,
family metadata drives the combine, live inputs fold at read); migrating a channel = wiring its packages +
metadata onto it, never writing a channel-specific path. The four per-channel calculator modules exist only as
migration scaffolding (per-channel attribution needed independent walkers). The two spec'd exclusions:
movement/range (modifier.md §6 — a per-edge resolver, not the deposit shape) and the tally (counts roll up).

**There is NO performance case for per-channel paths — perf argues FOR the one path.** The performance lives
in the PACKAGE layer (standing pre-summed numbers, bare-fetch reads), indifferent to path count; a
metadata-driven combine is the same integer adds + one multiply as a hand-rolled function, and one hot path
beats four for the instruction cache and for having one place to optimize. The one perf-real rule the generic
engine obeys — a constraint, never a license for duplicates: **GENERIC CODE, STATIC STORAGE.** Family metadata
compiles to ints/flags at load (the deposit-index precedent); the packages stay flat named/indexed fields
(compile-time channel indices on the scope structs); runtime never touches a string, a map lookup, or a
dynamic dispatch on the read path. Per-channel code buys nothing but duplication and special-case risk.

**ONE oracle too — the law extends to the verification layer, with the [patterns.md](../../architecture/patterns.md)
single-source precision:** the `Calc/*` **position packages** (`PercentStack`, `YieldBasePackages`,
`BuildingPackage` — StoneBase-mirrored 1:1, already generic over channels) **STAY** — they are the single-source
per-position functions. What consolidates is the per-channel **ASSEMBLERS** (`YieldRate`, `CommerceCalc`,
`CascadeWellbeing`, `CascadeScalarChannels` + their bespoke parts structs) and the per-channel endpoint emits —
that is where the sprawl lives; every channel can use the exact same assembler. The end-state: **one generic
deposit-walker assembler** (walk family F's deposits through the position packages, combine by family metadata)
and **one generic family-parameterized endpoint decomposition** — the axes are identical for every family.
Migration path: the generic assembler is proven ORACLE-VS-ORACLE (diffed offline against each per-channel
assembler); each retires as the generic one reproduces it. The one deliberate end-state duplication: the ORACLE
re-walks sources; the PACKAGES stand pre-summed — independent by construction, so the nets can catch a package
bug.

## 2. The gap map — what today's code holds vs where it belongs

`CascadeRateSlots` today (the store-at-target drift), decomposed to its proper homes:

| today's per-city field | contains | proper home |
|---|---|---|
| `aPct[3]` | city+area+empire buildings, civics, traits, projects percents — ONE baked stack | city pct package (city buildings only) + player pct package (empire bldgs/civics/traits/projects) + area package; summed at read |
| `aSpec[3]`, `aCSpec100[4]` | specialist totals | city package (stays — city-scope) |
| `aExtra100[3]` | building flats + perPop | city package (stays) |
| `aEmpFlat[3]` | trait free-city + GA flats | **player package** (pure player scope; GA gate live at read) |
| `aCBase100[4]` | religion/corp/GA/building-block/playerExtra | split: city-scope terms stay; the player-extra + GA terms → player package w/ live gates |
| `aWb[4]` + `iWbMilPerUnit` | realized verdicts incl. area/empire parts | city wb package (city terms) + the player wb package (already the area/empire split maps); verdicts REALIZED at read from the parts; military stays live-on-top |
| scalar fields (F/G) | full stacks incl. player parts | city halves stay; player/world parts → their packages (the H surgery had this right in direction) |
| `iEpoch`/`iTurn` + the epoch statics + `CvCascadePlayerStamp` | version polling | **deleted** |
| module-side rollups (`s_wbRollup`, `ScPlayerRollup`) | player-scope sums off-object | the `CvPlayer` package |

**Conforming already (survives untouched):** the `CvPlot` yield cache + the city's plots-PULL at combine; the
compiled `DepositIndex`; `m_operatingBuildings` (city-scope operating buildings, event-marked — loses only its stamp fields); the
calculators as oracles; the derived building mask concept; the bare-fetch read + slice-start boundary +
`gameId`/perf census pieces of increments F/G.

## 2b. The TOUCH MAP — every site the rebuild lands on (the code-cut-map discipline)

> Mechanically swept from the live tree at `d35efd21b`+docs (grep: `CascadeAccumulator::`,
> `m_cascadeRateSlots.`, the epoch/stamp primitives, the module statics, the slot-field readers). Line numbers
> drift — re-verify the named function, not the integer. **⛔ This is a PASS-1 mechanical sweep; per
> [DEC-all-means-all] it gets an ADVERSARIAL second pass (assume incompleteness, hunt the miss) BEFORE phase 1
> starts — a self-certified map is not a map.**

| # | site | what it is | phase action |
|---|---|---|---|
| 1 | `Cascade/CvCascadeAccumulator.h` (`CascadeRateSlots`, `AccDirty`, `CvCascadePlayerStamp`) | the store-at-target struct + bits + the stamp primitive | ph1 rebuild in place: city-only package fields (flat/percent separate, per-package bits); stamp DELETES ph5 |
| 2 | `Cascade/CvCascadeAccumulator.cpp:28-60` (`s_iEpoch`/`s_aiPlayerEpoch`, `bumpEpoch`/`bumpPlayerEpoch`/`epochFor`, `freshen`) | the polling machinery | ph5 DELETE (marks replace) |
| 3 | `CvCascadeAccumulator.cpp:87-103` (`acc_ensure`) + every accessor's ensure call (`:172,181,190,198,206,213,220,234`) | the read-side ensure protocol | ph2–4 accessors recompose to package combines; ensure dies ph5 (reads = bare fetches) |
| 4 | `CvCascadeAccumulator.cpp:106-167` (`refreshComponents`) | the masked recompute dispatch | ph1+ splits: city-half walks per city package; player/world halves to their scopes' delegates |
| 5 | `CvCity.cpp:59` bind · `:482-489` reset (incl. the stamp lines + operating buildings stamps) · `CvCity.h:1845` delegate | the city cache lifecycle | ph1 extends (new package member); stamp lines DELETE ph5 |
| 6 | `CvCity.cpp` dirty hooks ×5 — `:4548` building, `:6944` pop, `:14171` specialist, `:15318`/`:15523` religion/corp | the event marks | ph1 masks re-derive per package bits (building = the derived-mask precedent); semantics keep the ruled cadences (WB end-turn) |
| 7 | `CvCity.cpp:11312`/`:12049` (`getYieldRate100`/`getCommerceRate`, flipped) | the live rate getters | ph3 recompose internals to the §2a package combine (under the standing `[GETTER]` net) |
| 8 | `CvCity.cpp:5637-5651` (wellbeing verdict getters ×4, flipped) | the live wb getters | ph4 recompose: verdicts realized at read from good/bad packages |
| 9 | `CvCity.cpp:7196+` (the four scalar getters, REVERTED to legacy; `*Legacy` siblings) | the scalar getters | ph2 recompose + flip (in-body net through real turns first) |
| 10 | `CvPlayer.cpp:9414` GA · `:14296` civics · `CvTeam.cpp:4952` tech (`bumpPlayerEpoch` ×3) | the player-event sites | ph1 `markPlayerScopeAndCities` lands beside; ph5 replaces alone; + `CvPlayer` gains member/bind/delegate ph1 |
| 11 | `CvGame.cpp:649` (the load-end block warms plots + city slots) | the load build | the RESEED builds ALL packages, all scopes, at load (the capstone's realized site — every present-fact replayed) + `CvGame` gains the world member; the recompute-from-state warm-up (`playerSliceRebuild`/`worldRebuild`) is removed |
| 12 | `Cascade/CvCascadeOperatingBuildings.h:28-31` + `EnablerKernel::operatingBuildings` (`:255-274`, `epochFor` stamp block, include `:22`) | the operating buildings' polling half | ph5 stamps DELETE (pure Set; slice-start covers the unhooked classes) |
| 13 | `Cascade/CvCascadeWellbeing.cpp:180-216` (`WbPlayerRollup`, `s_wbRollup[MAX_PLAYERS]`, the local `WbSplit`) | player wb sums off-object | ph4 → the `CvPlayer` package (fold maps); statics DELETE |
| 14 | `Cascade/CvCascadeScalarChannels.cpp:87+` (`ScPlayerRollup` on the shared stamp) | player scalar sums off-object | ph2 → the `CvPlayer` package; static DELETES |
| 15 | `Cascade/CvCascadeModifierMath.cpp` — rate/wb net reads (`:367,:499,:515,:559`) + the `[MODIFIER/scalar]` net (`:584-604`, raw `sst.iSc*` reads) + the `MDF_*` field registry | the verification harness | rate/wb nets unchanged (oracle side); the scalar net's slot side recomposes with ph2 (raw package reads — the non-tautological form) |
| 16 | `Tools/CvHttpServer.cpp:1754-57` (casc wb) + `:1840-44` (slot twins `scSt.iSc*`) | the endpoint emits | ph2/ph4 recompose to package reads; field names stay (StoneBase parser unaffected) |
| 17 | the `[MODIFIER/perf]` census + StoneBase `PerfLineParser`/`perf_fields` | the perf pipeline | UNTOUCHED (names stable); the ph2 read-cost proof consumes it |
| 18 | `CvGame`/`CvPlayer` `doTurn` tops | the rebuild boundary | ONLY event-marked (dirty) packages rebuild — NO slice-start blanket, NO full per-player rebuild ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)); the turn-end unified pass of flagged caches is the post-plan boundary move |
| 19 | `Cascade/CvCascadePerfCount.{h,cpp}` (the census counters — `scRefresh`/read counters increment inside the refresh/ensure paths) | the perf census internals | counters re-home to the package refresh/read sites as each flip lands; EMITTED names stay stable (StoneBase parser) — *adversarial-pass find* |

**Not touched by this rebuild (verified out of scope):** the enabler gates + operating buildings *content* (only the stamps
go), the tally, capabilities (already cut), the plot cache, the DepositIndex, grants (unbuilt), StoneBase's
tracker/perf surfaces, and every legacy accumulator the cutover's own map owns (`code-cut-map.md` — this map
covers the SUBSTRATE rebuild only, not the legacy cut).

## 3. The implementation plan — ONE LANDING, then incremental CHECKING

**⛔ THE BUILD LAW: no increments in the BUILD.** Incrementing on a specific scope or yield means chasing
parity without a critical element (a missing scope's term reads as a divergence → a patch → special-case
fuckery). The whole structure goes up in ONE landing — all yields, all channels, all scopes, the packages,
the engines, the marks, the deletions — and only THEN is the data **checked** in increments (nets, probes,
owner-played turns), against a structure that is complete. The oracles + *Legacy getter siblings provide the
verification safety; coexistence half-states do not exist.

**Phase 1 — the package substrate (no behaviour change).**
New `Sources/Cascade/CvCascadeScopePackages.h` (+ `.cpp` math in the accumulator module): the three scope
structs, every field ONE package (one summed number, flat/percent always separate), each struct on one
`CvDerivedCacheSet` with one dirty bit per package(-group):

- `CascadeCityPackages` on `CvCity` — per yield channel: `pctBuildings` / `flatSpecialists` (BASE) /
  `flatBuildings100` (EXTRA, incl. perPop); per commerce: `cSpec100` / `cFlatCity100` (religion/corp city
  terms) / `cPct`; the wellbeing city-scope split terms; the scalar city halves (gpBaseBld, gpBaseSpec,
  gpModCity, defense, maintModCity, tradeCity).
- `CascadePlayerScope` on `CvPlayer` — per yield channel: `pctEmpire` (empire buildings + civics + traits +
  projects) / `flatFreeCity` / `flatGoldenAge` (gate live at read); commerce twins; the wellbeing area/empire
  fold maps; the scalar player parts (gpModPlayer + SR-gated field, maintPlayerAll + area maps + conn-gated,
  tradeEmpire + coastal-gated); every gated sum its own field.
- `CascadeWorldScope` on `CvGame` — the world flats (tradeRoutes first tenant).
Wiring: ctor binds + reset marks + thin refresh delegates (math module-side); the **event reseed** builds ALL
packages at load (the eager load build — the capstone); during play ONLY event-marked (dirty) packages rebuild —
**no slice-start blanket, no full per-player rebuild on `doTurn`** ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal));
the mark sites route through the cache-invalidation consumer ([f0-eventspine-invalidation.md](f0-eventspine-invalidation.md)).
Phase 1 also stands up **the two generic engines** (the one-path law): the **package-combine engine** (packages
in, family metadata drives the combine, live inputs fold at read) and the **generic deposit-walker oracle**,
the latter proven ORACLE-VS-ORACLE offline against each existing per-channel calculator before it serves any
net. Phases 2–4 are then pure family migrations onto these engines — no new channel code paths, and each
per-channel calculator + bespoke endpoint retires as its family lands.
*Verify:* Assert build; endpoint twins emit package values beside the old slots; zero getter changes.

**Phase 2 — the FLIP LIST (not channel projects — the channels dissolved into the one engine).**
There is no per-channel build work: the generic fill walks the compiled deposit index ("sum scope S's deposits
of family F at position P"), so every term — including the free-city flats, the golden-age flats, and
freeSpecialists — arrives as an ordinary keyed/conditioned flat deposit the walker picks up (GA = a flat with
its gate live at read; freeSpecialists = the spec'd count-family; **no special provider-home exists or is
needed** — the four-provider law describes what physically produces base yield; a trait's flat is just a
deposit at its scope and §2a position). What remains per channel is exactly three small items: its **metadata
row** (positions / `signed-split` / `min`-floor / truncation), its **live-input folds** (slider, disorder,
anger raw-state, military count, the per-city gates), and its **getter flip proof**. The flip list, in
dependency/risk order, each entry = recompose the getter to the generic combine + in-body net + real played
turns clean + flip (the capabilities lesson: offline parity alone is insufficient; frozen-save protocol,
perf census rows to the StoneBase store — the 1.28M-reads class must go flat):

1. the five scalar getters (smallest surface, hooks proven; module statics delete onto the player package);
2. `getYieldRate100`/`getCommerceRate` (already flipped — internals recompose under the standing `[GETTER]`
   net; `aPct`/`aEmpFlat`/`aCBase100` content splits per the gap map);
3. the four wellbeing verdicts (§2b verbatim: Σ providers − Σ anger, negative ⇒ angry; health identically —
   two flat packages per scope, verdicts realized at read; `s_wbRollup` deletes; military live-on-top; the
   ruled end-turn cadence rides the slice-start marks);
4. buildRate — the same engine, keyed selection: its packages are PER-KEY LEDGERS (key = the compiled index's
   interned target id: unit/building/domain/unitCombat/category), one per scope, filled by the same generic
   walk; the read for the head item sums the entries matching the item's keys + its bonus-gated `self` mods.
   Its historic "deferral" was an artifact of the flat-int slot storage, not the model — the channel is
   already the best-attributed of all (the keyed walk reconciles exactly through `sumKeyed4U`). Ledger storage
   obeys generic-code-static-storage — ✅ since 2026-07-05 in the DENSE form (`CascadeBrLedger` per-kind
   vectors indexed by game enum id; reads are one array index, the tree-map form retired).

**Phase 3 — the ONE-PHILOSOPHY atomic cut.**
With every channel package-served and netted clean: delete the epochs (`bumpEpoch`/`bumpPlayerEpoch`/
`epochFor`), `CvCascadePlayerStamp`, the slots' + operating buildings' `iEpoch`/`iTurn` stamps, the turn-roll blanket, and
the read-side ensure protocol — events mark, boundaries rebuild, reads fetch. The three player-event sites run
`markPlayerScopeAndCities` alone. The derived masks refine: percent-vs-flat per source kind (building landed;
tech/civic/trait next), then per-channel bits ("which package, for what yield").

**Phase 6 — the queue on the finished substrate** (each its own work item): the tradeRoutes vote-class split,
property-engine wiring, the unitInput endpoint, grants, area/team scope members when a channel first needs
them (maintenance-area the candidate).

**The event-spine-driven single build/invalidation is the F0 foundation, built NOW**
([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal),
[f0-eventspine-invalidation.md](f0-eventspine-invalidation.md)): the epoch/stamp/self-heal blankets are all
replaced by targeted, spine-routed per-source marks — the cascade is built at load by the reseed and, during play,
ONLY dirty packages rebuild. There is NO slice-start blanket and NO full per-player rebuild. The one open item is
the rebuild BOUNDARY for the dirty packages: the written end-state is **one unified TURN-END rebuild pass of the
flagged caches in dependency order** (plot → city → player), which lands WITH the
[parked AI build-queue-parity rework](../parked/ai-build-queue-parity.md) (the snapshot IS that rework's fairness
mechanism). The mask refinement survives that transition unchanged (the marks stay; only the rebuild boundary of
the flagged caches moves).

Verification per step: the standing nets (slot-vs-oracle, casc-vs-legacy) + the census counters + the frozen-save
protocol (end turn → automates → end turn), with the perf rows landing in the StoneBase store.
**The owner's play-report semantics:** the mark-1 eyeball is never a metric EXCEPT for extreme outliers — and
**game unresponsiveness IS the owner's trigger warning**: any "it hung / went unresponsive" report is a hard
signal to instrument + attribute immediately (both such reports during this landing were true positives — real
hot-path defects). Magnitude judgments ("felt slower") route to the instruments, never to tuning by feel.

**The binding flip lesson ([capabilities.md](../../specs/capabilities.md), the reverted first capability cut):
for getters feeding DERIVED ENGINE STATE, offline parity is necessary but NOT SUFFICIENT** — the capability flip
was 0-diverging offline and still broke the trade network on the first real turn. Every getter flip in steps
1–3 therefore requires the IN-BODY instrument (the cascade-vs-legacy diff at the real call moment, through real
played turns) clean BEFORE the flip — never an offline/endpoint parity alone. And per
[DEC-structure-before-shadow]: the nets verify each step's *behaviour*; the *structure* is verified once, here,
against this document and the specs it cites.

> The full legacy↔cascade DUPLICATE-SURFACE index (every legacy↔cascade pair still standing, who serves, the
> nets, the cut homes) is its own document: [duplicate-surface.md](duplicate-surface.md).

## 3b. The caching-surface census (one surface, enforced)

Post-landing, the cascade plane has ONE caching surface — the `CvDerivedCache` component (flag form for
homogeneous leaves: the plot yields; Set form for the package structs: city/player/world/operating buildings — owner-side
storage, the component owns the dirty protocol). The residuals, each with its disposition: the **legacy
accumulators** behind the `*Legacy` oracles are the flip net-oracle residuals — a demolition-pending remnant
that dies at the atomic cut ([code-cut-map.md](code-cut-map.md)), NOT a sanctioned duplication: the shadow phase
has ended, so no duplication is sanctioned ([patterns.md](../../architecture/patterns.md) rule 7); ✅
**`CascadeCapabilities`'s hand-rolled per-team union CONVERGED onto the Set protocol
(2026-07-05)** — `CascadeTeamCaps` is an owner-side `CvTeam::m_cascadeTeamCaps` member on `CvDerivedCacheSet`
(setHasTech/reset mark, queries ensure; the module statics + `bValid` + the `invalidate` API deleted); the **load-time compiled statics**
(DepositIndex, candidate lists) are static-data artifacts with no freshness surface; the **AI heuristic
caches** are out of scope (superseded-ideas.md — the AI rework's plane).

## 4. What this deletes at the end

The epochs, the stamps, the rollup structs, `acc_ensure`'s read protocol, the turn-roll blanket, and every
store-at-target field — leaving: packages on objects, events marking through derived masks, boundary rebuilds,
bare-fetch summed reads. One component, one philosophy, one package format — the cascade as designed.
