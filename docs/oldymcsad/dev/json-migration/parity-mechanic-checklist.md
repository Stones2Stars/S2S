# Calc parity — the per-mechanic checklist (tick at 30/30, never on an average)

> **The rule ([DEC-per-mechanic-parity](../architecture/decisions.md#dec-per-mechanic-parity)):** a mechanic is
> ticked ONLY when dry's per-mechanic value equals the engine's emitted per-mechanic value **exactly, for every
> city** (no rounding slack beyond ±1 x1-unit, no realized check, **no empire/world average — averages hide
> offsetting errors, the kraken's cancellation trap**). The engine emits each quantity on `/diagnostic/cityInput`;
> dry computes each; we compare leaf-by-leaf, instance-by-instance.
>
> **Status:** ✅ verified 30/30 · 🔴 broken (count shown) · ⬜ not yet per-mechanic-checked · ⚪ n/a this save
> **Date:** 2026-06-22 · re-run the harness after any change; a green tick is only as good as its last run.

## How each is verified

`getYieldRate100 = (getBaseYieldRate + getSpecialistYieldTotal) × getBaseYieldRateModifier + 100 × getExtraYield`
(CvCity.cpp:11246). Every term below is a leaf of that. dry function → emitted engine field → exact compare.

## Channel: YIELDS (food, production)  — `city_yield100`

### Base — `getBaseYieldRate` (22906)
| # | mechanic | engine field | dry source | status |
|---|---|---|---|---|
| 1 | **plot yield** (worked tiles: terrain/feature/improvement/bonus/route + per-plot `m_aExtraYield` events) | `basePlotYield` | `plot_yield` Σ worked | 🔴 **1/30** (dry under ~50/city) |
| 2 | trade yield | `baseTradeYield` | `trade_yield100` / `ctx.tradeYield` | ⬜ |
| 3 | free-city yield | `baseFreeCityYield` | (not computed — `ctx.freeCityYield`) | ⬜ (dry emits 0) |
| 4 | golden-age yield | `baseGoldenAgeYield` | golden-age deposit | ⬜ |

### Specialist — `getSpecialistYieldTotal`
| 5 | **specialist yield** | `specialist` | `specialist_yield` | 🔴 **0/30** (engine 0 → flat in `m_aiExtraYield` this save; dry placed it in the MODIFIED base, overshoots. Fix placement to flat for parity; "modified" is future #444) |

### Modifier (7-way) — `getBaseYieldRateModifier` (the `× modifier`)
| 6 | building modifier | `modBuilding` | `gather_slot.percent` (bldg part) | 🔴 **0/30** (total dry under ~30pp; split into the 7 below) |
| 7 | bonus modifier | `modBonus` | gather_slot (bonus) | ⬜ |
| 8 | player modifier | `modPlayer` | player % deposit | ⬜ |
| 9 | power modifier | `modPower` | power-gated % deposit | ⬜ |
| 10 | area modifier | `modArea` | area % | ⬜ |
| 11 | capital modifier | `modCapital` | capital % | ⬜ |
| 12 | event modifier | `modEvent` | (city yield-rate modifier) | ⬜ |

### Extra (UNMODIFIED) — `getExtraYield` (`+ 100 × …`, outside the multiply)
| 13 | building flat extra | `extraBuildingYield100` | `gather_slot.flat` (bldg) | 🔴 **0/30** (dry over ~9) |
| 14 | per-pop extra | `extraPerPopRate` (× pop) | per-pop deposit | ⬜ |
| 15 | **corp yield** | `corporationYield` | `_corp_yield100` | ✅ **30/30** |
| 16 | **buildingYieldChange** (BonusYieldChanges / VicinityBonusYieldChanges / vote-source religion / events) | `buildingYieldChange` (per-building) | (not represented — dry 0) | 🔴 **0/30** (dry missing ~49; STORAGE_EMPIRE part is being poofed, FORGE etc. is real BonusYieldChanges) |

## Channel: COMMERCE (gold, research, culture, espionage)  — `commerce_split100`
⚠ Owner-flagged "on drugs / where the bait ran free" — **distrust entirely, re-derive.** Enumerate from
`getCommerceRateAtSliderPercent` / `getBaseCommerceRate100`: YIELD_COMMERCE×slider, building commerce (base +
shrine + corp-HQ + GlobalBuildingExtra), specialist commerce, the commerce % stack (player + building), free
commerce. — ⬜ not yet enumerated leaf-by-leaf.

## Other channels — scope to enumerate (each its own per-mechanic table)
⬜ maintenance (+ inflation) · ⬜ health · ⬜ happiness · ⬜ great-people-rate · ⬜ culture (building/specialist) ·
⬜ espionage · ⬜ food-growth/threshold · ⬜ production-cost/hurry · ⬜ trade-routes · ⬜ defense · ⬜ properties

## Harness
The per-mechanic comparison (dry value vs emitted engine field, per city, exact) — NOT the averaged sweep, which
is retired per [DEC-per-mechanic-parity]. Each row's "status" is the last harness run; a change re-runs it.
