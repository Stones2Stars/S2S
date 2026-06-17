# Conscription (the draft)

Conscription (drafting) converts a city's **population into a finished military
unit immediately**, trading long-term growth and short-term happiness for a unit
on the spot. It is the same mechanic Civ IV calls "draft," extended by C2C/S2S
global and civic settings.

## What you get

When a city is conscripted it spawns the **best conscriptable unit it can
currently train** — the trainable unit with the highest `iConscriptionValue`
(set per unit in XML). The unit appears in the city with **0 moves for that
turn**, and receives the city's normal production experience and free
promotions.

The number of units produced is `CONSCRIPT_POPULATION` (a GlobalDefine,
default **1**, floored at 0).

## Requirements

A city can be conscripted only if **all** of the following hold (`CvCity::canConscript`):

| Requirement | Detail |
|---|---|
| Not in disorder, not already drafted | Skipped when the draft happens on city capture. A city can be drafted once before its `drafted` flag clears (normally next turn). |
| Enough population | City population must be **strictly greater than** `CONSCRIPT_POPULATION`. |
| Under the player's draft cap | The player's running conscript count must be below `getMaxConscript()` (driven by the player's **civics**). |
| Enough cultural control | The owning team's culture on the city plot must be ≥ `CONSCRIPT_MIN_CULTURE_PERCENT` (skipped on capture). |
| A draftable unit exists | At least one trainable unit must have a non-zero `iConscriptionValue`. |

## Costs

Conscripting a city (`CvCity::conscript`) applies:

- **−1 population** to the city.
- **Draft anger** for `flatConscriptAngerLength()` turns, where
  `length = max(1, CONSCRIPT_ANGER_DIVISOR × gameSpeed% ÷ 100)`. Anger scales
  with game speed, and a player who drafts repeatedly accrues more unhappiness
  (each non-capture draft increments the player's conscript count).
- The city is flagged **drafted**, blocking another draft until the flag clears.
- On a normal (non-capture) draft, the player's **conscript count +1**, counting
  against `getMaxConscript()`.

## Governing settings

- **GlobalDefines (XML):** `CONSCRIPT_POPULATION`, `CONSCRIPT_ANGER_DIVISOR`,
  `CONSCRIPT_MIN_CULTURE_PERCENT`.
- **Per-unit (XML):** `iConscriptionValue` — which unit a draft produces.
- **Civics:** the per-player maximum number of conscripts (`getMaxConscript`).

## Source

`CvCity::conscript`, `canConscript`, `getConscriptUnit`, `getConscriptPopulation`,
`flatConscriptAngerLength`, `initConscriptedUnit` — all in `Sources/CvCity.cpp`
(around lines 4145–4253).

> Note: a bug where the draft consumed population without producing a unit was
> fixed in issue #57 — make sure you are on a build that includes that fix.
