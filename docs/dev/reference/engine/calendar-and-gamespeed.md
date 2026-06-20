# Calendar & game speed — the era-driven pacing model

> **Status:** reference   ·   **Verified against:** `Sources/Infos/CvEraInfo.{h,cpp}`, `Sources/Infos/CvGameSpeedInfo.{h,cpp}`, `Sources/Engine/CvDate.{h,cpp}`, `Sources/Engine/CvGameObject.cpp`, `Sources/Defines/CvEnums.h`, `Sources/Infrastructure/IntExpr.cpp` (paths + symbols confirmed 2026-06-20)
> **Grounding:** the accessor signatures and the `AdaptTypes` channel table were re-confirmed present at the cited files; line numbers below are "the function named here, around this line" — confirm the **function/symbol**, not the integer (line numbers drift).
> How turn counts and calendar dates derive from era data. This replaced the legacy 9×14
> `GameTurnInfos` tables in 2026-06 (#248, part of #196). After reading this you'll know which two info
> classes hold all pacing data, how a turn maps to a date, and how the `<Adapt*>` XML expression channels
> scale values to the running game.

## The model — two info classes, fully declarative

All pacing data lives in two info classes; there is **no stored turn→date table and no runtime state** in
either.

- **`CvEraInfo`** (per era — `Sources/Infos/CvEraInfo.h:31-33`, members `:81-83`):
  - `getHistoricalStartYear()` / `getHistoricalEndYear()` — the era's real-history span (must be contiguous
    with its neighbours; asserted at runtime in `CvDate`).
  - `getNormalSpeedTurns()` — how many turns the era lasts at Normal (100%) speed.
- **`CvGameSpeedInfo`** (per speed — two XML scalars, `Sources/Infos/CvGameSpeedInfo.h`):
  - `getSpeedPercent()` (`:28`, member `m_iSpeedPercent` `:46`) — scales costs **and** turn counts.
  - `getUnitYieldScalePercent()` (`:30`, member `m_iUnitYieldScalePercent` `:50`) — the `<AdaptUnitYield>`
    channel (below); per the inline note it grows slower than `iSpeedPercent`.

**Derived accessors on `CvGameSpeedInfo`** (computed, not XML-backed — `Sources/Infos/CvGameSpeedInfo.h:33-36`):

- `getTurnsInEra(iEra)` = `round(normalSpeedTurns × speedPercent / 100)` (min 1).
- `getEraStartTurn(iEra)`.
- `getTotalTurns()` = the estimate-end-turn (2000 at Normal).
- `getTicksPerTurnInEra(iEra)` = era year-span ticks ÷ turns (30 ticks/month, 360/year).

## Turn ↔ date conversion

**`CvDate::getDate(iTurn, eGameSpeed)`** (`Sources/Engine/CvDate.h:39`) converts a turn to a date by **linear
interpolation** of the era's year span over the era's turn count, extrapolating past the last era at its
rate. `CvDate::getTicksPerTurn(eGameSpeed)` (`Sources/Engine/CvDate.h:35`) reports the per-turn tick
granularity, and `CvGameTextMgr::setDateStr` (`Sources/UI/CvGameTextMgr.cpp:193`) picks the display
granularity (years / seasons / months / exact) from `getTicksPerTurn` thresholds.

**Consequences by construction:**
- The game-start turn for a later-era start is exactly that era's first turn (`CvGame` uses
  `getEraStartTurn`; the old `iStartPercent` era field is gone).
- The displayed date at an era boundary is exactly the era's historical start year.

## The historical-accurate calendar option

The **historical-accurate calendar** modder option dates the world from **tech progress** instead of turn
count, via `calculateCurrentTick()` (`Sources/Engine/CvGameCoreUtils.cpp:3187`). It reads the **same**
`CvEraInfo` year fields — the old `HISTORICAL_ACCURATE_ERA_RANGE_*` GlobalDefines and
`EraRanges_GlobalDefines.xml` are gone — so the two calendars can no longer disagree about era years.

## Adapt expression channels

XML integer expressions scale values to the running game via **tag-dispatched** nodes (`IntExprAdapt`). The
legacy `<Adapt><ID>ADAPT_X</ID>…` form with dynamic string IDs is gone; the **tag itself** picks the
`AdaptTypes` channel. The tag→channel dispatch is in `Sources/Infrastructure/IntExpr.cpp:64-74`; the channel
enum is `AdaptTypes` in `Sources/Defines/CvEnums.h:1091-1100`.

| XML tag | `AdaptTypes` | Scales by (`CvGameSpeedInfo` accessor) | Used for |
|---|---|---|---|
| `<Adapt>` | `ADAPT_DEFAULT` | `getSpeedPercent()` | mission costs (e.g. captive upgrades) |
| `<AdaptHammerCost>` | `ADAPT_BUILDING_AND_UNIT_COSTS` | `getHammerCostPercent()` (respects `GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS`, `CvGameSpeedInfo.cpp:42`) | caravan hurry-production yields |
| `<AdaptUnitYield>` | `ADAPT_UNIT_YIELD` | `getUnitYieldScalePercent()` | subdued-animal yields |

`CvGameObject::adaptValueToGame(eAdapt, iValue)` (`Sources/Engine/CvGameObject.cpp:1591`) is the single
evaluator — one base implementation. The former per-handicap leg was deleted as dead data (no handicap XML
ever defined `Percents`; see the inline note at `CvGameObject.cpp:1589`).

## See also

- [`reference/engine/handicaps.md`](handicaps.md) — the other game-difficulty scaler; the per-handicap
  `Percents` leg that `adaptValueToGame` once carried lived in handicap data (now dead).
- [`plans/parked/gamespeed-simplification.md`](../../plans/parked/gamespeed-simplification.md) — the findings
  record / decision log for removing the legacy `GameTurnInfos` 9×14 tables and the structures this model
  replaced (#248 / #196); this reference doc is the *implemented* model it points back to.
- [`README.md` §0](../../README.md) — the non-negotiables; the frozen toolchain those `Cv*` classes compile
  under.
