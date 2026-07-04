# Scope packages — the substrate rebuilt to the founding design (#430)

> **Status:** DESIGN FOR OWNER REVIEW — no code until blessed. · **Authority:** [modifier.md](../../specs/modifier.md)
> §1 (deposit DOWN, accumulate, read O(1) — **the founding design of this migration**) +
> [state-repositories.md](../../architecture/state-repositories.md) (the `CvDerivedCache` component, the
> event→cache routing, the load-only-full-rebuild capstone).
>
> **Why this document exists (2026-07-04, the honest record):** the implemented substrate (increment C→G,
> `CascadeRateSlots` + epochs + rollups) drifted from the founding design — it stores UPPER-scope sums per city
> (store-at-target), which forced version-polling (epochs/stamps), fan-out invalidation, and the read-side ensure
> protocol whose cost collapsed unit automation. The owner's 2026-07-04 statements were NOT new rulings but the
> core spec restated against that drift. This document is the design stated once, whole, so the rebuild happens
> once, properly ([DEC-proper-once]).

---

## 1. The design (the spec, in implementation terms)

**One uniform PACKAGE format.** A package is the §2 slot shape per channel: `Σflat` (×100 where the channel is
fixed-point) and `Σpercent` — the compiled result of every deposit AT ONE SCOPE for one channel-component. No
other shape exists at any scope.

**One package set per SCOPE OBJECT, holding ONLY its own scope's deposits.**

| scope object | member | holds (accumulated AT this scope) |
|---|---|---|
| `CvPlot` | `m_yieldCache` (exists, conforming) | the plot's own base package |
| `CvCity` | `m_cascadeCityPackages` | this city's `*.city.*` deposits, **as ISOLATED per-component packages** (below) |
| `CvArea`* | (via the player package's per-area maps, v1) | `*.area.*` sums grouped by area — *promoted to a real `CvArea` member when a second channel needs it* |
| `CvPlayer` | `m_cascadePlayerScope` | `*.empire.*` deposits from the player's active buildings anywhere + adopted civics + held traits (PURE-filtered) + team techs; SR-gated / coastal / connected sums kept as **separate fields** (their gates are per-city, applied at read) |
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
| building percents | the percent stack (one additive stack with the player-scope percent packages) |

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
**⏳ OPEN QUESTION for the owner (asked, not assumed):** §2a lists the free-city + golden-age trait flats
as BASE-tier city-wide terms with no physical provider among the four — their provider-home under this law
(a city-level base package? re-homed onto a provider? a deliberate fifth city-base slot?) needs the
owner's classification before the rate-channel migration step.

**Every package on the ONE component** (`CvDerivedCacheSet<TOwner>`), living ON the object, never serialized,
all-dirty from birth/reset, rebuilt at load (the eager warm-up), refreshed via the owner's thin delegate to the
module-side math.

**ONE freshness philosophy: events mark, the cache knows.** No epochs, no stamps, no version polling, no
turn-roll blanket. The event marks the package(s) **at the scopes its deposits actually touch**, with the mask
**derived from the compiled deposit index** (percent-vs-flat and channel split — "it's the percentage recalcs
that hurt"; a flat-only source never rebuilds a percent stack):

| event | marks |
|---|---|
| building completed/lost (`processBuilding`) | the CITY package (derived mask) + the OWNER package iff the building carries empire-scope deposits + the WORLD package iff world-scope deposits + facts (always — operate conditions) |
| religion/corp spread | the city package (SR/corp-conditioned components) + facts |
| specialist count change | the city package's specialist components only |
| civic swap / GA flip / tech researched | the player package + the player's cities' packages (**legitimate fan-out: conditions on city-scope deposits reference these** — `enabled:{TECH_X}`) + facts |
| unit movement | **nothing, ever** ([DEC-unit-modifiers-on-top]; units may never author yield percentages) |
| slice start (`CvPlayer::doTurn` top) | the self-heal re-check marks for the not-yet-proven hook classes + eager `ensure` of this player's packages ("Cascade.RebuildCache(myPlayerId)") |

**Reads are BARE FETCHES composed by the channel's COMBINE FORMULA over the packages.** A realized getter
fetches the scope packages (plot pull + city + [area] + player + [team] + world as the channel needs) and
combines them **per the channel's spec'd positions** — for yields the §2a shape:
`(Σ BASE-tier packages) × (100 + Σ percent packages)/100 + (EXTRA-tier packages, truncated)` — with the
**per-city gates live**: SR-in-city, coastal, connected-to-capital, area membership, golden age, disorder,
the slider, population, the live military count. No ensure on any read path — freshness is entirely
write-side (event marks + boundary rebuilds). The only live calculation is this trivial position-aware
arithmetic; nothing ever re-walks a source.

**Full rebuild of every package happens at LOAD only** (the capstone). Post-load, every recompute is a marked
package at a boundary.

**The calculators stay, as ORACLES only.** The existing from-scratch calculators (`YieldRate`, `CommerceCalc`,
`CascadeWellbeing`, `CascadeScalarChannels`) never feed the packages' read path — they are what the per-turn nets
diff the package sums against. An oracle that read a package could not catch a package bug.

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
compiled `DepositIndex`; `m_cascadeFacts` (city-scope facts, event-marked — loses only its stamp fields); the
calculators as oracles; the derived building mask concept; the bare-fetch read + slice-start boundary +
`gameId`/perf census pieces of increments F/G.

## 3. Migration order (each step compiles + nets before the next)

1. **The package structs + scope members** (`CvCity`/`CvPlayer`/`CvGame` + delegates + load warm-up + reset
   marks), epochs/stamps deleted, `markPlayerScopeAndCities` as the player-event mark. The SCALAR channels move
   first (smallest surface, hooks proven): city halves per city, player/world parts per their packages, getters
   = the package adds. The `[MODIFIER/scalar]` net diffs the composition vs the oracles.
2. **The rate channels** (`aPct`/`aEmpFlat`/`aCBase100` split per the gap map) — the `[SLOT]`/`[GETTER]` nets
   re-verify each split; the flipped yield/commerce getters become package adds (their read cost DROPS —
   today's `acc_ensure` per read disappears).
3. **The wellbeing channel** (the verdict realization moves to read; the parts live per scope) — the
   `[MODIFIER/wellbeing]` net re-verifies; the ruled end-turn cadence is preserved by the slice-start marks.
4. **The per-channel mask granularity** (AccDirty bits split per channel — "we know exactly which package, for
   what yield") + the tech/civic/trait derived masks (the building mask's successors).
5. The area/team scope members when a channel first genuinely needs them (maintenance area is the candidate).

Verification per step: the standing nets (slot-vs-oracle, casc-vs-legacy) + the census counters + the frozen-save
protocol (end turn → automates → end turn), with the perf rows landing in the StoneBase store.

## 4. What this deletes at the end

The epochs, the stamps, the rollup structs, `acc_ensure`'s read protocol, the turn-roll blanket, and every
store-at-target field — leaving: packages on objects, events marking through derived masks, boundary rebuilds,
bare-fetch summed reads. One component, one philosophy, one package format — the cascade as designed.
