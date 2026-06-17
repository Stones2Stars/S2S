# Calendar & game speed (era-driven model)

How turn counts and calendar dates derive from era data. Replaced the legacy 9×14
`GameTurnInfos` tables in 2026-06 (#248, part of #196); see
`plans/gamespeed-simplification.md` for the history and removed structures.

## The model

Two info classes hold all pacing data:

- **`CvEraInfo`** (per era, declarative fields): `iHistoricalStartYear` /
  `iHistoricalEndYear` — the era's real-history span (must be contiguous with its
  neighbours; asserted at runtime in `CvDate`) — and `iNormalSpeedTurns` — how many
  turns the era lasts at Normal (100%) speed.
- **`CvGameSpeedInfo`** (per speed, fully declarative — 2 XML scalars):
  `iSpeedPercent` scales costs *and* turn counts; `iUnitYieldScalePercent` is the
  `<AdaptUnitYield>` channel (see below).

Derived accessors on `CvGameSpeedInfo` (not XML-backed):
`getTurnsInEra(iEra)` = `round(normalSpeedTurns × speedPercent / 100)` (min 1),
`getEraStartTurn`, `getTotalTurns` (= the estimate-end-turn, 2000 at Normal),
`getTicksPerTurnInEra` = era year-span ticks ÷ turns (30 ticks/month, 360/year).

**`CvDate`** converts turn ↔ date by linear interpolation of the era's year span over
the era's turn count (`CvDate::getDate`), extrapolating past the last era at its rate.
There is no stored turn→date table and no runtime state in the info classes.
`CvGameTextMgr::setDateStr` picks display granularity (years/seasons/months/exact)
from `CvDate::getTicksPerTurn` thresholds.

Consequences by construction: the game-start turn for a later-era start is exactly
that era's first turn (`CvGame` uses `getEraStartTurn`; the old `iStartPercent` era
field is gone), and the displayed date at an era boundary is exactly the era's
historical start year.

The **historical-accurate calendar** modder option (`calculateCurrentTick` in
`CvGameCoreUtils.cpp`) dates the world from tech progress instead of turn count;
it reads the *same* `CvEraInfo` year fields (the `HISTORICAL_ACCURATE_ERA_RANGE_*`
GlobalDefines and `EraRanges_GlobalDefines.xml` are gone), so the two calendars
can no longer disagree about era years.

## Adapt expression channels

XML integer expressions scale values to the running game via tag-dispatched nodes
(`IntExprAdapt`; the legacy `<Adapt><ID>ADAPT_X</ID>…` form with dynamic string IDs
is gone — the tag picks the `AdaptTypes` channel):

| Tag | Scales by | Used for |
|---|---|---|
| `<Adapt>` | `getSpeedPercent()` | mission costs (e.g. captive upgrades) |
| `<AdaptHammerCost>` | `getHammerCostPercent()` (respects `GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS`) | caravan hurry-production yields |
| `<AdaptUnitYield>` | `getUnitYieldScalePercent()` | subdued-animal yields |

`CvGameObject::adaptValueToGame` is the single evaluator (one base implementation;
the former per-handicap leg was deleted as dead data — no handicap XML ever defined
`Percents`).
