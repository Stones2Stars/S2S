# Observability map — Great people generation

> DRAFT observability map (2026-06-18, parent: cascade-mapping-inventory.md) — all claims cited from
> live code; verify before relying. Line numbers are anchors at time of writing; they drift.

---

## 1. How it actually works

### 1a. Per-turn GPP accumulation (city scope)

`CvCity::doGreatPeople()` is called once per city per turn from `CvCity::doTurn()` inside a
`PERF_SCOPE("city.doGreatPeople", getOwner())` wrapper (`CvCity.cpp:1351`).

**Step 1 — disorder guard.** If the city is in disorder (`isDisorder()`), the function
returns immediately with no GPP change (`CvCity.cpp:16870-16872`).

**Step 2 — accumulate flat GPP.** `changeGreatPeopleProgress(getGreatPeopleRate())` adds the
city's per-turn rate to `m_iGreatPeopleProgress` (`CvCity.cpp:16874`).

**Step 3 — accumulate per-type unit weights.** For every unit type, the city's
`getGreatPeopleUnitRate(UnitTypes)` is added to `m_paiGreatPeopleUnitProgress[iI]`
(`CvCity.cpp:16876-16879`). This maintains the per-type probability weight used at spawn.

**Step 4 — threshold check.** If `getGreatPeopleProgress() >= greatPeopleThresholdNonMilitary()`
(`CvCity.cpp:16881`), a spawn fires:

- Sum all per-type unit progress values (`iTotalGreatPeopleUnitProgress`).
- Draw a synced RNG number in `[0, iTotalGreatPeopleUnitProgress)` via
  `getSorenRandNum(iTotalGreatPeopleUnitProgress, "Great Person")` (`CvCity.cpp:16888`).
- Walk unit types in order; first type whose partial sum covers the rand = `eGreatPeopleUnit`
  (`CvCity.cpp:16891-16901`). This is a weighted random pick proportional to each type's
  accumulated unit progress.
- If a unit type was selected: deduct the full threshold from `m_iGreatPeopleProgress`
  (`CvCity.cpp:16906`), zero all per-type unit progress (`CvCity.cpp:16908-16910`), call
  `createGreatPeople(eGreatPeopleUnit, true, false)` (`CvCity.cpp:16912`).

### 1b. The per-turn GPP rate

`CvCity::getGreatPeopleRate()` (`CvCity.cpp:7143-7150`):
```
if (isDisorder()) return 0;
return getBaseGreatPeopleRate() * getTotalGreatPeopleRateModifier() / 100;
```

`getBaseGreatPeopleRate()` (`CvCity.cpp:7137-7140`):
```
return std::max(0, m_iBaseGreatPeopleRate) + GET_PLAYER(getOwner()).getNationalGreatPeopleRate();
```

Sources of `m_iBaseGreatPeopleRate` (city-local flat GPP):
- Each building with `kBuilding.getGreatPeopleRateChange() != 0` calls
  `changeBaseGreatPeopleRate()` on build/demolish (`CvCity.cpp:5076`).
- Each active specialist with `getGreatPeopleRateChange() != 0` calls
  `changeBaseGreatPeopleRate()` on assign/remove (`CvCity.cpp:5139`).

`getNationalGreatPeopleRate()` — player-wide flat that applies to every city (`CvPlayer.cpp:29863`):
```
return std::max(0, m_iNationalGreatPeopleRate);
```
This is driven by national wonders or global-effect buildings via `changeNationalGreatPeopleRate()`.

`getTotalGreatPeopleRateModifier()` (`CvCity.cpp:7153-7170`) — multiplicative factors:
- Base 100%.
- `getGreatPeopleRateModifier()` — city-local modifier from buildings
  (`CvCity.cpp:4618` on building process, driven by `kBuilding.getGreatPeopleRateModifier()`).
- `owner.getGreatPeopleRateModifier()` — player-wide modifier from civics
  (`CvPlayer.cpp:18024`), global buildings (`CvPlayer.cpp:7384`, via
  `kBuilding.getGlobalGreatPeopleRateModifier()`), and traits
  (`CvPlayer.cpp:28440`, via `GC.getTraitInfo(eTrait).getGreatPeopleRateModifier()`).
- If the player has a state religion AND the city has that religion:
  `owner.getStateReligionGreatPeopleRateModifier()` — driven by civics
  (`CvPlayer.cpp:18027`), traits (`CvPlayer.cpp:28505`).
- If player is in a golden age: `GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER()` (global define).
- Result is `std::max(0, iModifier)`.

Per-type unit rate (`getGreatPeopleUnitRate`, `CvCity.cpp:13817-13823`):
```
return std::max(0, m_paiGreatPeopleUnitRate[eIndex] + owner.getNationalGreatPeopleUnitRate(eIndex));
```
Sources:
- Building with `kBuilding.getGreatPeopleUnitType() != NO_UNIT`:
  `changeGreatPeopleUnitRate(eGreatPeopleUnit, kBuilding.getGreatPeopleRateChange())` (`CvCity.cpp:5082`).
- Specialist with `getGreatPeopleUnitType() != NO_UNIT`:
  `changeGreatPeopleUnitRate(eGreatPeopleUnit, getGreatPeopleRateChange())` (`CvCity.cpp:5136`).
- National rate from player (`CvPlayer.cpp:29856`).

### 1c. Threshold scaling

`CvPlayer::greatPeopleThresholdNonMilitary()` (`CvPlayer.cpp:9174-9188`):
```
iThreshold = GREAT_PEOPLE_THRESHOLD × era.getGreatPeoplePercent()
iThreshold = getModifiedIntValue64(iThreshold, getGreatPeopleThresholdModifier())
iThreshold *= gameSpeed.getSpeedPercent() / 10000
return std::max(1, (int)iThreshold)
```

Inputs:
- `GREAT_PEOPLE_THRESHOLD` — global define (XML `GlobalDefines`).
- `era.getGreatPeoplePercent()` — the game's **start era**, NOT the current era
  (`GC.getGame().getStartEra()`).
- `m_iGreatPeopleThresholdModifier` — modified upward each time a GP spawns:
  `changeGreatPeopleThresholdModifier(GREAT_PEOPLE_THRESHOLD_INCREASE × (getGreatPeopleCreated()/5 + 2))`
  (`CvPlayer.cpp:20476`). So threshold grows non-linearly with every 5th GP spawned (the
  `/5 + 2` step function).
- `m_iGreatPeopleCreated` — lifetime GP count for this player, incremented at every
  production-queue or `doGreatPeople()` GP spawn when `bIncrementThreshold=true`
  (`CvPlayer.cpp:20474`). Governs the step function above.
- `gameSpeed.getSpeedPercent()` — game speed modifier.

`greatPeopleThresholdModifier` is the only surviving accumulator; the team-level equivalent
(commented out at `CvPlayer.cpp:20483`) was disabled by a prior change.

### 1d. GP creation and threshold bump

`CvPlayer::createGreatPeople()` (`CvPlayer.cpp:20457-20507`):
1. Spawns the unit via `initUnit`.
2. If `getGoldenAgeOnBirthOfGreatPersonCount(eGreatPersonUnit) > 0` — triggers a golden age.
3. If `bIncrementThreshold` (true for `doGreatPeople` spawns, false for battle-XP spawns):
   increments `m_iGreatPeopleCreated`, then bumps `m_iGreatPeopleThresholdModifier` by
   `GREAT_PEOPLE_THRESHOLD_INCREASE × (getGreatPeopleCreated()/5 + 2)`.
4. If `bIncrementExperience` (false for GPP spawns, true for Great General path):
   increments `m_iGreatGeneralsCreated` and bumps the generals threshold.

No logging at this call site.

### 1e. GPP from CvOutcome (non-main path)

`CvOutcome` can add GPP directly via `pCity->changeGreatPeopleProgress(m_iGPP)` and
`pCity->changeGreatPeopleUnitProgress(m_eGPUnitType, m_iGPP)` (`CvOutcome.cpp:1124-1129`).
This is a secondary source (outcome of unit actions — e.g. a missionary special action)
that bypasses `doGreatPeople()` but feeds the same `m_iGreatPeopleProgress`. No logging at
this site.

---

## 2. Current observability

**Overall tier: 1 (Telescreen)** — the spawned GP units are visible as new rows in `/units`
after the fact, but the accumulation state, the rate breakdown, and the spawn event are all
invisible.

### What IS observable today

| Observable | How | Notes |
|---|---|---|
| GP unit appeared (post-hoc) | `GET /units?playerNumber=N` — new unit row with a `UNIT_GREAT_*` type string | Detected as a new entry comparing snapshot-to-snapshot; no turn of birth, no city of birth, no trigger cause |
| GP unit type | `type` field in `/units` row (e.g. `UNIT_GREAT_SCIENTIST`) | Only after the unit is on the map |
| GP unit count (player total) | Count `UNIT_GREAT_*` entries in `/units?playerNumber=N` | No per-city breakdown |
| Player era (context for threshold) | `era` field in `/players` | Start era is not exposed; current era only |

### What is NOT observable today

| Gap | What is missing |
|---|---|
| `m_iGreatPeopleProgress` | Current GPP bank in each city — the most critical gap |
| `greatPeopleThresholdNonMilitary()` | Effective threshold this player must cross |
| `getGreatPeopleRate()` per city | Per-turn GPP yield (flat × modifier) |
| `getBaseGreatPeopleRate()` per city | Flat GPP before modifiers |
| Per-type unit weights (`m_paiGreatPeopleUnitProgress[i]`) | Current probability distribution over GP types — invisible |
| Per-type unit rate (`m_paiGreatPeopleUnitRate[i]`) | Per-type base rate feeding the weights |
| `m_iGreatPeopleCreated` | Lifetime GP count (drives threshold step function) |
| `m_iGreatPeopleThresholdModifier` | Modifier already accumulated (component of threshold) |
| Spawn event | No `[CIT/produced]` or any log line fires from `doGreatPeople()` — zero signal |
| GPP from CvOutcome | GPP injected by unit-action outcomes is invisible |
| isDisorder suppression | Whether disorder is currently blocking GPP — not surfaced per city |
| Golden-age GPP bonus | Whether golden-age modifier is currently active per player — not surfaced (but `isGoldenAge()` is derivable from unit scan) |
| State-religion GPP modifier | Whether religion bonus is applying — not surfaced |

---

## 3. The gap

The entire accumulation pipeline is invisible. An agent watching the wire today can only
infer "a GP spawned sometime this turn" by noticing a new `UNIT_GREAT_*` in the next `/units`
snapshot. It cannot:
- Know how close any city is to spawning a GP (GPP bank vs threshold).
- Know how much GPP per city is being generated per turn (rate breakdown invisible).
- Know what type of GP is most likely (per-type weights invisible).
- Know when a threshold bump will make the next GP harder to generate.
- React to the spawn event in the turn it happens (no `[CIT/gpp]` log line reaches `/events`).
- Reconstruct the state for AI players at all — the human player can observe their own GPP bar,
  but AI players have no GPP on `/cities`, `/players`, or any log.

This is the §A "opaque system" category from cascade-mapping-inventory.md: **the in-flight
state-mapping sweep flags this as currently unmapped / unobservable.**

---

## 4. Proposed hooks

All hooks should follow the existing gating pattern: file log gated by `gPlayerLogLevel`,
stream tee via `streamLogTee` through `logCityAI` / `logPlayerAI` so they appear on `/events`.
"Cheap + gated" — zero cost when `gPlayerLogLevel == 0`.

### Hook A — `[CIT/gpp]` per-turn rate snapshot (level 1 headline)

**Where:** `CvCity::doGreatPeople()`, immediately after the `changeGreatPeopleProgress` call at
`CvCity.cpp:16874`, guarded by `gCityLogLevel >= 1` (or the existing `gPlayerLogLevel`).

**What to emit:**
```
[CIT/gpp] turn=N city=<name> owner=N progress=N rate=N threshold=N pct=NN
```
Fields:
- `progress` — `getGreatPeopleProgress()` AFTER the accumulation this turn.
- `rate` — `getGreatPeopleRate()`.
- `threshold` — `GET_PLAYER(getOwner()).greatPeopleThresholdNonMilitary()`.
- `pct` — `progress * 100 / threshold` (integer, clipped to 100).

This single level-1 line makes the per-city GPP state reconstructible from `/events` every
turn for every city of every player — the bare minimum for the "never look at the screen" bar.

### Hook B — `[CIT/gpp/spawn]` spawn event (level 1 headline)

**Where:** `CvCity::doGreatPeople()`, inside the `if (eGreatPeopleUnit != NO_UNIT)` block at
`CvCity.cpp:16904`, just before `createGreatPeople()`.

**What to emit:**
```
[CIT/gpp/spawn] turn=N city=<name> owner=N unit=<UNIT_GREAT_xxx> totalWeight=N thresholdNew=N
```
Fields:
- `unit` — `GC.getUnitInfo(eGreatPeopleUnit).getType()`.
- `totalWeight` — `iTotalGreatPeopleUnitProgress` (the sum of per-type weights — captures
  the distribution at the moment of selection without enumerating all types).
- `thresholdNew` — the new threshold AFTER the bump that `createGreatPeople` will apply
  (compute as `GET_PLAYER(getOwner()).greatPeopleThresholdNonMilitary()` called after the
  deduct but before `createGreatPeople` — OR log it inside `createGreatPeople` itself).

This is the primary event hook. An agent watching `/events` can: count spawns by turn,
know which city and player, know which GP type, and track the threshold ramp.

### Hook C — `[PLR/gpp]` player-level threshold snapshot (level 1 headline, once per player turn)

**Where:** `CvPlayer::doTurn()` or the `doGreatPeople` call chain's natural player-turn
boundary, gated by `gPlayerLogLevel >= 1`.

**What to emit:**
```
[PLR/gpp] turn=N player=N created=N threshMod=N threshold=N
```
Fields:
- `created` — `getGreatPeopleCreated()` (lifetime GP count).
- `threshMod` — `getGreatPeopleThresholdModifier()` (accumulated penalty so far).
- `threshold` — `greatPeopleThresholdNonMilitary()` (effective value this player must cross).

This is sufficient to reconstruct the threshold ramp across sessions without watching every
spawn event.

### Hook D — `/cities` endpoint additions (GPP snapshot fields)

**Where:** `CitySnap` struct (`CvHttpServer.cpp:83`) and the snapshot builder loop
(`CvHttpServer.cpp:1542-1578`).

**New fields to add to `CitySnap` and the `/cities` JSON output:**
```json
"gppProgress":  <int>,   // m_iGreatPeopleProgress (city's current GPP bank)
"gppRate":      <int>,   // getGreatPeopleRate()
"gppThreshold": <int>    // GET_PLAYER(owner).greatPeopleThresholdNonMilitary()
```
`gppThreshold` is the same for every city of the same player; emit it per-city anyway (it
is small, and it avoids a join between `/cities` and a `/players` extension).

These three fields make the current GPP state reconstructible from a single `/cities` GET
without needing the log stream — the snapshot equivalent of the per-turn `[CIT/gpp]` lines.

### Hook E — `/players` endpoint additions (GP lifetime counts)

**Where:** `PlayerSnap` struct and `renderPlayers` (`CvHttpServer.cpp:270`).

**New fields:**
```json
"greatPeopleCreated":  <int>,   // getGreatPeopleCreated()
"greatPeopleThreshMod": <int>   // getGreatPeopleThresholdModifier()
```
Together with the city-level `gppThreshold`, this lets an agent reconstruct the full
threshold formula without touching the log files.

### Hook F — `[CIT/gpp/rate]` detailed rate breakdown (level 2)

**Where:** `CvCity::doGreatPeople()`, level-2 guard, after the rate is computed.

**What to emit:**
```
[CIT/gpp/rate] turn=N city=<name> owner=N base=N natRate=N cityMod=N playerMod=N relMod=N gaMod=N effective=N
```
Fields:
- `base` — `m_iBaseGreatPeopleRate`.
- `natRate` — `GET_PLAYER(...).getNationalGreatPeopleRate()`.
- `cityMod` — `getGreatPeopleRateModifier()`.
- `playerMod` — `owner.getGreatPeopleRateModifier()`.
- `relMod` — `owner.getStateReligionGreatPeopleRateModifier()` (0 if condition not met).
- `gaMod` — `GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER()` if golden age, else 0.
- `effective` — `getGreatPeopleRate()`.

This is level 2 (verbose — one line per city per turn). Use it to debug unexpected GPP rate
values, not for routine monitoring.

### Hook G — `[CIT/gpp/weights]` per-type weight dump (level 3)

**Where:** `CvCity::doGreatPeople()`, level-3 guard, before the threshold check.

Emit one key-value pair per unit type that has `getGreatPeopleUnitProgress(iI) > 0`:
```
[CIT/gpp/weights] turn=N city=<name> owner=N UNIT_GREAT_XXX=NNN ...
```
Level 3 by convention; used only when diagnosing GP type selection bias.

---

## 5. Priority ranking

| Priority | Hook | Why |
|---|---|---|
| **Highest** | D — `/cities` gppProgress + gppRate + gppThreshold | Snapshot-queryable; enables full reconstruction from a single GET for any player including AI; unblocks the "never look at the screen" bar for this system |
| **High** | B — `[CIT/gpp/spawn]` | Makes the spawn event visible on `/events` in the turn it happens; feeds any shadow that wants to verify cascade GPP triggers |
| **High** | A — `[CIT/gpp]` per-turn rate | Per-city per-turn heartbeat; makes the accumulation visible on the log stream without a snapshot |
| **Medium** | E — `/players` greatPeopleCreated + greatPeopleThreshMod | Completes the threshold reconstruction; small addition |
| **Medium** | C — `[PLR/gpp]` player-level snapshot | One per player per turn; low volume, high utility for tracking threshold ramp |
| **Low** | F — `[CIT/gpp/rate]` level-2 breakdown | Forensic; useful when debugging but not needed for routine coverage |
| **Low** | G — `[CIT/gpp/weights]` level-3 weight dump | Deep forensic; rarely needed |

---

## 6. Cascade relevance (#428/#430)

The GPP system is a **§A opaque system** in cascade-mapping-inventory.md — it needs both
understanding and observability before the cascade hard-switch. Specifically:

- GP buildings contribute `getGreatPeopleRateChange()` as both a flat-rate source and a
  per-type weight source — both become `enables`/`modifiers` in the cascade model.
- GP buildings also contribute `getGreatPeopleRateModifier()` (percent modifier) at city
  scope and `getGlobalGreatPeopleRateModifier()` at player scope — both are `modifier-cascade`
  targets.
- The `greatPeopleThresholdModifier` ramp is player-owned state that the cascade's tally
  must track (it gates city-level spawns). It is currently outside the cascade's awareness.
- The type-selection probability (per-type unit weights) is a **stochastic gate** tied to
  the set of GP-contributing buildings and specialists active in a city — the cascade must
  understand this distribution to validate that the building set it places yields the
  intended type mix.
- Hook D (GPP endpoint fields) and Hook B (spawn event) are the minimum substrate for a
  future **GPP shadow** analogous to `placementSweep` / `[PLACEMENT]`: comparing the
  cascade's expected GPP yield for a city against the live engine's `getGreatPeopleRate()`.
