# Golden Age — how it influences everything

A **golden age** is a temporary, player-wide boost period. Everything hangs off one counter:
`CvPlayer::getGoldenAgeTurns()` (turns remaining); **`isGoldenAge()` is just `goldenAgeTurns > 0`**
(`CvPlayer.cpp:9390`). Every effect below is gated on `isGoldenAge()` and stops the turn the counter hits 0.
It decrements 1/turn at end of the player's turn (`CvPlayer.cpp:3852`).

> **⚖ A GOLDEN AGE INSTANTLY ENDS ANARCHY — it is not merely mutually exclusive with it.** On the
> 0 → non-zero crossing `changeGoldenAgeTurns` runs `changeAnarchyTurns(-getAnarchyTurns())`, so an anarchic
> empire that triggers one is out of anarchy that instant, with the anarchy crossing's own fact and every
> consequence riding it. ⚑ **It sits in the PUBLIC setter's EFFECT half, never in
> `changeGoldenAgeTurnsInternal`** — which is exactly the [the load reseed](../spine/05-the-load-reseed.md#5-the-load-reseed)
> split doing its job: a genuine golden-age START cancels anarchy, while a save read (which reaches the internal
> setter directly, commit + maintain + announce and nothing else) restores a loaded golden age without
> retroactively clearing the anarchy the save recorded beside it.
> ⛔ So do not "simplify" the cancel into the internal setter to put it beside the counter — that would make a
> LOAD mutate base state the stream is authoritative for.

This doc exists so we don't re-derive golden-age behaviour from the engine each time. Locations are `file:line`
into `Sources/`.

---

## How a golden age STARTS (triggers)

| Trigger | Where | Mechanic |
|---|---|---|
| **Great-people units** | `CvUnit::goldenAge()` `9897`; `CvPlayer.cpp:9042/9089` | Units flagged `UnitInfo.isGoldenAge()` are consumed. Cost **RISES each time**: `unitsRequiredForGoldenAge() = BASE_GOLDEN_AGE_UNITS + numUnitGoldenAges × GOLDEN_AGE_UNITS_MULTIPLIER`. Activating kills the required units, then `changeGoldenAgeTurns(getGoldenAgeLength())` + `changeNumUnitGoldenAges(1)`. |
| **A building** | `CvCity.cpp:14589` | A building flagged `BuildingInfo.isGoldenAge()` fires `changeGoldenAgeTurns(1 + getGoldenAgeLength())` on completion. |
| **An event** | `CvPlayer.cpp:21714` | An event flagged `EventInfo.isGoldenAge()` triggers one length. |
| **Great-person birth** | `CvPlayer.cpp:20467` | A trait's `getGoldenAgeOnBirthofGreatPeopleType()` auto-triggers a golden age when that GP type is born. |

## How LONG it lasts (duration)

`getGoldenAgeLength() = max(1, getModifiedIntValue(game.goldenAgeLength100, getGoldenAgeModifier()) / 100)`
(`CvPlayer.cpp:9460`).

- **Base × gamespeed:** `goldenAgeLength100 = GOLDEN_AGE_LENGTH × gamespeed.speedPercent` (`CvGame.cpp:3257`).
- **Modifier:** `getGoldenAgeModifier()` accumulates trait `getGoldenAgeDurationModifier()` + building `getGoldenAgeModifier()`.

---

## What it DOES — the effects

### 1. Yields & commerce — THREE additions, all in `base` (the cascade-relevant part)

All three are gated on `isGoldenAge()` and land in the city's **`base`**, so they ride the `× modifier`
(per [modifier.md](../cascade.md) §2, golden age is a **base supplier**, not a flat add-on). All three now
live in the cascade, not raw engine reads — see "Cascade representation" below for where each applies.

1. **Per-plot yield bonus.** **It is NOT a flat "+1 on every worked plot."** It is **threshold-gated**: a worked
   plot gets `+CvYieldInfo.getGoldenAgeYield(y)` **only if** its yield clears
   `CvYieldInfo.getGoldenAgeYieldThreshold(y)`; a plot below the threshold gets nothing. This addend is part of
   `basePlotYield` — the per-plot isolated base package ([modifier.md §2a](../cascade.md)).
   - **⚠ The threshold IGNORES the plot's improvement & route (counter-intuitive, and load-bearing for faithful reproduction):**
     the test runs on the **PRE-improvement, PRE-route** running yield — `nature + extra + [centre] + playerTerrain +
     seaPlot + getYieldChangeAt + landmark + extra/less-threshold`. The improvement and route are added **AFTER**
     the golden-age check, so a rich improvement/route on a tile does **not** help it qualify — only the tile's
     intrinsic + city/player yields decide. (Thresholds are low for some yields — e.g. commerce — so the bonus
     fires almost everywhere and *looks* like "+1 on everything", but it is genuinely gated, and a
     low-pre-improvement tile can miss it.)
   - **⚖ THE PRE-IMPROVEMENT, PRE-ROUTE OPERAND IS REPRODUCED FAITHFULLY, and it is what forced
     the ROUTE into a plot segment of its own.** The plot package stores nature / improvement / **route** / rest
     precisely so this operand — `nature + the city block + the owner's plot flats` — is expressible at all;
     while route and the owner's flats shared one sum it was not ([modifier.md §2](../cascade.md)).
     ⚠ **The distinction decides the bonus on nearly every tile, so it is not a rounding question:** both
     authored thresholds are **1**, so a tile whose only production or commerce comes from its improvement or
     its route qualifies on the full total and did not in the engine. **FOOD authors neither value**, so food
     correctly gains nothing from this leg.
     ⚠ The extra-yield SCALING step is deliberately outside the operand: the engine's own operand was
     pre-improvement, while the cascade's scaling is ruled to test the FULL total
     ([modifier.md §2](../cascade.md)), so folding it in would drag improvement and route back into the
     very test this excludes them from.
2. **Player base golden-age yield** — a flat player-wide per-yield bonus, fed by traits' `getGoldenAgeYieldChanges[]`.
   Part of TIER-1 `base` ([modifier.md §2a](../cascade.md)) — distinct from the per-plot bonus above.
3. **Golden-age commerce** — base commerce adds `100 × player.getGoldenAgeCommerce(c)`, fed by traits'
   `getGoldenAgeCommerceChanges[]`.

> PURE_TRAITS option filters the trait-fed golden-age yield/commerce (a positive bonus on a negative trait drops).

### 2. Growth — UNAFFECTED

⛔ **A golden age does NOT lower the food a city needs to grow.** `CvPlayer::getGrowthThreshold` finishes
at the AI-handicap step and returns; there is no golden-age term, and one must not be added — **never re-add a
golden-age term to `getGrowthThreshold`.**

> **⚖ THE MECHANIC IS DEAD BECAUSE IT NEVER LIVED, AND THAT IS WHAT DECIDES IT:** *"if growth reduction
> for golden age has never worked, we won't introduce it now — the game has been balanced around not having
> it."* The legacy define it depended on was misspelled and defined nowhere, so the discount was inert in every
> game ever played while reading as implemented at the call site — every balance judgement the mod has ever made
> was made against a threshold a golden age does not move, so correcting the spelling would have been a BALANCE
> CHANGE, not a bug fix. The branch and the define are deleted along with the story of the typo:
> [superseded-ideas #31](../architecture/superseded-ideas.md).

### 3. Great people — faster

`+GOLDEN_AGE_GREAT_PEOPLE_MODIFIER%` to a city's great-people rate (`CvCity.cpp:7177`).

### 4. Governance — NO anarchy

Civic changes (`getCivicAnarchyLength:8940`) and religion changes (`getReligionAnarchyLength:9001`) cost **0
anarchy** while in a golden age — the canonical "switch civics for free" window. The AI deliberately exploits it
(`CvPlayerAI.cpp:6401`, `17314`).

---

## How to OBSERVE it (no fishing)

⛔ **The routes this section used to name are gone with the route-table purge**
([../specs/http-endpoints.md](../specs/http-endpoints.md)), and an endpoint is not the way back — a route reading a
legacy member keeps that member alive past the compiler census. Observe golden age the way everything else is
observed now: **the spine log** (`Cascade.log`, readable while the game runs) and `/events`
([spine.md](../spine.md)). What to look for: the per-player `isGoldenAge` fact, and — for the per-plot
addend — the pre-improvement base the threshold actually tests, which is the value a cascade must reproduce and
which nothing currently emits. **Emitting it is step one of verifying this.**

## Where the NUMBERS live (data fields)

| Source | Fields |
|---|---|
| `CvYieldInfo` | `getGoldenAgeYield`, `getGoldenAgeYieldThreshold` (the per-plot bonus + threshold) |
| `CvTraitInfo` | `getGoldenAgeYieldChanges[]`, `getGoldenAgeCommerceChanges[]`, `getGoldenAgeDurationModifier`, `getGoldenAgeOnBirthofGreatPeopleType` |
| `CvBuildingInfo` | `isGoldenAge` (trigger on completion), `getGoldenAgeModifier` (duration) |
| `CvUnitInfo` | `isGoldenAge` (consumable trigger unit) |
| `CvEventInfo` | `isGoldenAge` (event trigger) |
| Defines | `BASE_GOLDEN_AGE_UNITS`, `GOLDEN_AGE_UNITS_MULTIPLIER`, `GOLDEN_AGE_LENGTH`, `GOLDEN_AGE_GREAT_PEOPLE_MODIFIER` |

---

## TL;DR for the cascade

Golden age touches the yield path in **three** places, all inside `base` (so all `× modifier`): the
**per-plot** threshold bonus (in `basePlotYield`), the **player** golden-age yield, and the **golden-age
commerce** — plus faster great people and zero-anarchy civic swaps elsewhere. The one reproduction
gotcha is the per-plot bonus's **pre-improvement/pre-route** threshold test (`CvPlot.cpp:8403`).
⛔ It does **not** touch city GROWTH: the food-for-growth discount is a dead mechanic that never once ran (§2).

> **⚖ WHERE EACH OF THE THREE ACTUALLY APPLIES (the cascade's say in a golden age is its LENGTH and its
> grant — the yield EFFECT is the engine's).** The two player-wide legs (2 and 3) apply at the CITY combine, as
> a plain package read of the `{ch}.empire.goldenAge` mirror. The PER-PLOT leg (1) applies in the PLOT RESOLVE,
> read live off the plot's owner — the same engine-core shape the city-centre block already uses
> (`cascadePlotCityAdd100`'s twin), added AFTER the scaling and BEFORE the MinCity floor, which is the engine's
> own position for it. ⛔ None of the three is an authored deposit and none becomes one; ⛔ and the per-plot leg
> is never folded into the `empire.goldenAge` mirror, which is the trait-fed player-wide source at a different
> scope.

> **Cascade representation — PERMANENT engine member-mirror, effect-only.**
> [conditions are predicates, never bespoke members](../specs/json/03-the-shared-vocabulary/05-predicates-a-systems-runtime-state.md#35-predicates--a-systems-runtime-state-query) retires condition-as-member
> shapes (`empire.capital` → `enabled:IS_CAPITAL`). **The golden-age YIELD EFFECT is the standing PERMANENT exception:**
> the per-plot threshold bonus, the player golden-age yield, and the golden-age commerce are applied by the **core
> engine** and are **not defined as data anywhere** — the per-plot bonus is a base-yield threshold test ("does the plot
> already have enough of this base yield? +1"), improvement-independent, which the XML/JSON never modeled. Modelling it
> through the `IS_GOLDEN_AGE` predicate would mean authoring it virtually everywhere it fires, so the cascade mirrors it
> as the `empire.goldenAge` member permanently. This is **NARROW — only the yield/commerce EFFECT is carved out:**
> golden-age **LENGTH** (trait `iGoldenAgeDurationModifier`, building `iGoldenAgeModifier` → `goldenAge.empire.percent`)
> and the golden-age **GRANT** (`grants.goldenAge`, `grants.goldenAgeOnBirthOfGreatPerson`) ARE curated JSON. The
> `IS_GOLDEN_AGE` predicate ([json](../specs/json.md) §3.5) exists and is reserved for any future engine-core rework,
> not a migration item.
