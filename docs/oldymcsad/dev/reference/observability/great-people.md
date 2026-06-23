# Observability — great-people generation

> **Status:** reference (per-system observability map) · **Verified against:** live source, 2026-06-20
> **Grounding:** `Sources/Engine/CvCity.cpp`, `Sources/Engine/CvPlayer.cpp`, `Sources/Engine/CvOutcome.cpp`,
> `Sources/Tools/CvHttpServer.cpp`. Line numbers drift — confirm the named function, not the integer.
> One-paragraph orientation: this maps how Great-Person-Point (GPP) accumulation, the per-type spawn
> lottery, and the player threshold ramp are observed today — and why the whole pipeline is invisible from
> outside the running game. Read the scaffold [`README.md`](README.md) (the 0–5 scale, the Orwell bar, the
> three hook shapes) and [`http-server.md`](http-server.md) (the live surface + live-read rules) first; this
> doc does not restate them.

**Tier today: 1 (Telescreen).** A spawned GP unit shows up as a new `UNIT_GREAT_*` row in `/units` *after
the fact*; the accumulation state (GPP bank), the rate breakdown, the per-type weights, the threshold ramp,
and the spawn event itself are all invisible. You can tell "a GP appeared this turn" only by snapshot-diffing
`/units` — never how close any city is, or why.

---

## 1. How it works

### 1a. Per-turn GPP accumulation (`CvCity::doGreatPeople`)

`CvCity::doGreatPeople()` (`Sources/Engine/CvCity.cpp:16959`) runs once per city per turn from
`CvCity::doTurn` inside `PERF_SCOPE("city.doGreatPeople", getOwner())` (`CvCity.cpp:1361`).

1. **Disorder guard** (`CvCity.cpp:16962-16965`): if `isDisorder()`, returns immediately — no GPP change
   at all this turn.
2. **Accumulate flat GPP** (`CvCity.cpp:16966`): `changeGreatPeopleProgress(getGreatPeopleRate())` adds the
   per-turn rate to `m_iGreatPeopleProgress`.
3. **Accumulate per-type weights** (`CvCity.cpp:16968-16971`): for every unit type, add
   `getGreatPeopleUnitRate(UnitTypes)` into `m_paiGreatPeopleUnitProgress[iI]` — the per-type probability
   weight used at spawn.
4. **Threshold check & spawn** (`CvCity.cpp:16973`): if `getGreatPeopleProgress() >=
   GET_PLAYER(getOwner()).greatPeopleThresholdNonMilitary()`:
   - Sum all per-type progress into `iTotalGreatPeopleUnitProgress` (`CvCity.cpp:16975-16979`).
   - Draw `getSorenRandNum(iTotalGreatPeopleUnitProgress, "Great Person")` (`CvCity.cpp:16980`).
   - Walk unit types; first whose partial sum covers the rand = `eGreatPeopleUnit` — a weighted random pick
     proportional to each type's accumulated weight (`CvCity.cpp:16983-16994`).
   - If a type was selected (`CvCity.cpp:16996`): deduct the full threshold from `m_iGreatPeopleProgress`
     (`CvCity.cpp:16998`), zero all per-type progress (`CvCity.cpp:17000-17003`), call
     `createGreatPeople(eGreatPeopleUnit, true, false)` (`CvCity.cpp:17004`).

### 1b. The per-turn GPP rate

- `CvCity::getGreatPeopleRate()` (`CvCity.cpp:7153`): `isDisorder() ? 0 : getBaseGreatPeopleRate() *
  getTotalGreatPeopleRateModifier() / 100`.
- `getBaseGreatPeopleRate()` (`CvCity.cpp`, verify): `max(0, m_iBaseGreatPeopleRate) +
  GET_PLAYER(getOwner()).getNationalGreatPeopleRate()`. Sources of `m_iBaseGreatPeopleRate`: buildings with
  `getGreatPeopleRateChange() != 0` and active specialists with `getGreatPeopleRateChange() != 0` call
  `changeBaseGreatPeopleRate()` on build/demolish / assign/remove (`CvCity.cpp`, verify).
- `getNationalGreatPeopleRate()` (`Sources/Engine/CvPlayer.cpp:29863`): `max(0, m_iNationalGreatPeopleRate)`
  — a player-wide flat applying to every city, driven by national wonders / global-effect buildings via
  `changeNationalGreatPeopleRate()`.
- `getTotalGreatPeopleRateModifier()` (`CvCity.cpp`, verify) — multiplicative: base 100%; city-local
  `getGreatPeopleRateModifier()` (buildings); player-wide `owner.getGreatPeopleRateModifier()` (civics,
  global buildings via `getGlobalGreatPeopleRateModifier()`, traits via `getGreatPeopleRateModifier()`);
  if state religion matches the city, `owner.getStateReligionGreatPeopleRateModifier()` (civics, traits);
  if golden age, `GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER()`. Result `max(0, iModifier)`.
- `getGreatPeopleUnitRate(eIndex)` (`CvCity.cpp`, verify): `max(0, m_paiGreatPeopleUnitRate[eIndex] +
  owner.getNationalGreatPeopleUnitRate(eIndex))` — fed by GP-typed buildings/specialists and the player
  national rate.

### 1c. Threshold scaling

`CvPlayer::greatPeopleThresholdNonMilitary()` (`Sources/Engine/CvPlayer.cpp:9174`):
```
iThreshold  = GREAT_PEOPLE_THRESHOLD × era.getGreatPeoplePercent()
iThreshold  = getModifiedIntValue64(iThreshold, getGreatPeopleThresholdModifier())
iThreshold *= gameSpeed.getSpeedPercent() / 10000
return max(1, (int)iThreshold)
```
Inputs:
- `GREAT_PEOPLE_THRESHOLD` — global define.
- `era.getGreatPeoplePercent()` — the game's **start era** (`GC.getGame().getStartEra()`), NOT the current era.
- `m_iGreatPeopleThresholdModifier` — bumped each spawn by
  `GREAT_PEOPLE_THRESHOLD_INCREASE × (getGreatPeopleCreated()/5 + 2)` — non-linear growth via the `/5 + 2`
  step (`CvPlayer.cpp`, verify, in `createGreatPeople`).
- `m_iGreatPeopleCreated` — lifetime GP count, incremented at GP spawn when `bIncrementThreshold` is true;
  governs the step above.
- `gameSpeed.getSpeedPercent()`.

`greatPeopleThresholdModifier` is the only surviving accumulator; the team-level equivalent was disabled by
a prior change (commented out in `CvPlayer.cpp`, verify).

### 1d. GP creation and threshold bump

`CvPlayer::createGreatPeople()` (`Sources/Engine/CvPlayer.cpp:20457`): spawns via `initUnit`; if
`getGoldenAgeOnBirthOfGreatPersonCount(...) > 0` triggers a golden age; if `bIncrementThreshold` (true for
`doGreatPeople` spawns, false for battle-XP spawns) increments `m_iGreatPeopleCreated` then bumps
`m_iGreatPeopleThresholdModifier`; if `bIncrementExperience` (Great-General path) increments
`m_iGreatGeneralsCreated` and bumps the generals threshold. **No logging at this call site.**

### 1e. GPP from `CvOutcome` (secondary path)

`CvOutcome` can inject GPP directly via `pCity->changeGreatPeopleProgress(m_iGPP)` and
`changeGreatPeopleUnitProgress(m_eGPUnitType, m_iGPP)` (`Sources/Engine/CvOutcome.cpp`, verify) — outcome of
a unit action (e.g. a missionary special action). It bypasses `doGreatPeople()` but feeds the same
`m_iGreatPeopleProgress`. **No logging at this site.**

---

## 2. What's on the wire today

**Tier 1 (Telescreen).** Observable:

| Observable | How | Notes |
|---|---|---|
| GP unit appeared (post-hoc) | `/units?playerNumber=N` — new `UNIT_GREAT_*` row | snapshot-diff only; no turn/city of birth, no cause |
| GP unit type | `type` field in the `/units` row | only after it's on the map |
| GP count (player total) | count `UNIT_GREAT_*` in `/units?playerNumber=N` | no per-city breakdown |
| Player era (threshold context) | `era` in `/players` | **current** era only — start era (the threshold input) is not exposed |

Everything in the accumulation pipeline is **not** observable: the per-city GPP bank
(`m_iGreatPeopleProgress`), the effective threshold (`greatPeopleThresholdNonMilitary()`), per-city rate
(`getGreatPeopleRate()` / `getBaseGreatPeopleRate()`), the per-type weights and rates
(`m_paiGreatPeopleUnitProgress[i]` / `m_paiGreatPeopleUnitRate[i]`), lifetime count
(`m_iGreatPeopleCreated`), accumulated threshold modifier (`m_iGreatPeopleThresholdModifier`), the spawn
event (no log line fires from `doGreatPeople`), `CvOutcome` GPP injection, disorder suppression, and the
golden-age / state-religion rate bonuses.

---

## 3. The gap

The entire accumulation pipeline is invisible. An observer can only infer "a GP spawned sometime this turn"
from the next `/units` diff. It cannot know: how close any city is (GPP bank vs threshold); how much GPP per
city per turn (rate breakdown); which GP type is most likely (per-type weights); when a threshold bump makes
the next GP harder; react to the spawn in the turn it happens (no `[CIT/gpp]` line reaches `/events`); or
reconstruct **any** of it for AI players — a human can watch their own GPP bar, but AI players have no GPP on
`/cities`, `/players`, or any log. This is an opaque-system case for the cascade hard-switch (§5).

---

## 4. Proposed hooks

All hooks are one of the three canonical shapes — see
[DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes).

### Hook A — `[CIT/gpp]` per-turn rate snapshot (level 1)
In `doGreatPeople`, after the `changeGreatPeopleProgress` accumulation (`CvCity.cpp:16966`), gated by
`gCityLogLevel`/`gPlayerLogLevel` ≥ 1:
```
[CIT/gpp] turn=N city=<name> owner=N progress=N rate=N threshold=N pct=NN
```
`progress = getGreatPeopleProgress()` (post-accumulation); `rate = getGreatPeopleRate()`;
`threshold = GET_PLAYER(getOwner()).greatPeopleThresholdNonMilitary()`; `pct = progress*100/threshold`
(clipped). Makes per-city GPP state reconstructible from `/events` every turn — the bare minimum for the bar.

### Hook B — `[CIT/gpp/spawn]` spawn event (level 1)
In `doGreatPeople`, inside the `if (eGreatPeopleUnit != NO_UNIT)` block (`CvCity.cpp:16996`), just before
`createGreatPeople`:
```
[CIT/gpp/spawn] turn=N city=<name> owner=N unit=<UNIT_GREAT_xxx> totalWeight=N thresholdNew=N
```
`totalWeight = iTotalGreatPeopleUnitProgress` (the distribution at selection, without enumerating types);
`thresholdNew` = the threshold after the bump (compute post-deduct, or log inside `createGreatPeople`). The
primary event hook — count spawns by turn, by city/player/type, track the ramp.

### Hook C — `[PLR/gpp]` player-level threshold snapshot (level 1, once per player turn)
In `CvPlayer::doTurn` or the player-turn boundary, gated `gPlayerLogLevel` ≥ 1:
```
[PLR/gpp] turn=N player=N created=N threshMod=N threshold=N
```
`created = getGreatPeopleCreated()`; `threshMod = getGreatPeopleThresholdModifier()`;
`threshold = greatPeopleThresholdNonMilitary()`. Reconstructs the threshold ramp without watching every spawn.

### Hook D — `/cities` snapshot additions (the highest-value hook)
Add to `CitySnap` / the `/cities` builder (`Sources/Tools/CvHttpServer.cpp`):
```json
"gppProgress":  <int>,   // m_iGreatPeopleProgress
"gppRate":      <int>,   // getGreatPeopleRate()
"gppThreshold": <int>    // GET_PLAYER(owner).greatPeopleThresholdNonMilitary()
```
`gppThreshold` is per-player but emit per-city anyway (small; avoids a `/cities`↔`/players` join). Full GPP
state from a single GET for any player including AI — the snapshot twin of Hook A.

### Hook E — `/players` snapshot additions (GP lifetime counts)
Add to `PlayerSnap` / `renderPlayers` (`Sources/Tools/CvHttpServer.cpp`):
```json
"greatPeopleCreated":   <int>,   // getGreatPeopleCreated()
"greatPeopleThreshMod": <int>    // getGreatPeopleThresholdModifier()
```
With Hook D's `gppThreshold`, lets an agent reconstruct the full threshold formula without the logs.

### Hook F — `[CIT/gpp/rate]` rate breakdown (level 2, forensic)
```
[CIT/gpp/rate] turn=N city=<name> owner=N base=N natRate=N cityMod=N playerMod=N relMod=N gaMod=N effective=N
```
`base = m_iBaseGreatPeopleRate`; `natRate = getNationalGreatPeopleRate()`;
`cityMod = getGreatPeopleRateModifier()`; `playerMod = owner.getGreatPeopleRateModifier()`;
`relMod = owner.getStateReligionGreatPeopleRateModifier()` (0 if N/A); `gaMod =
GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER()` if golden age else 0; `effective = getGreatPeopleRate()`. For
debugging unexpected rates, not routine monitoring.

### Hook G — `[CIT/gpp/weights]` per-type weight dump (level 3, deep forensic)
One key=value per unit type with `getGreatPeopleUnitProgress(iI) > 0`:
```
[CIT/gpp/weights] turn=N city=<name> owner=N UNIT_GREAT_XXX=NNN ...
```
Only when diagnosing GP type-selection bias.

**Priority:** Hook **D** (snapshot — full reconstruction from one GET, incl. AI; unblocks the bar) → Hook
**B** (spawn event on the wire) → Hook **A** (per-turn heartbeat) → Hook **E** (completes threshold
reconstruction) → Hook **C** (player-level ramp, low volume) → Hook **F** → Hook **G**.

---

## 5. Cascade relevance (#428/#430)

GPP is an opaque system that needs both understanding and observability before the cascade hard-switch
([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)):
- GP buildings contribute `getGreatPeopleRateChange()` as both a flat-rate source and a per-type weight
  source — both become `enables`/`modifiers` in the cascade model.
- `getGreatPeopleRateModifier()` (city) and `getGlobalGreatPeopleRateModifier()` (player) are
  `modifier-cascade` targets.
- The `greatPeopleThresholdModifier` ramp is player-owned state the cascade's tally must track (it gates
  city spawns) — currently outside the cascade's awareness.
- The per-type unit weights are a **stochastic gate** tied to the active GP-contributing building/specialist
  set; the cascade must understand this distribution to validate the type mix its placement yields.
- Hooks D + B are the minimum substrate for a future **GPP shadow** (analogous to `placementSweep` /
  `[PLACEMENT]`): comparing the cascade's expected GPP yield against the live engine's `getGreatPeopleRate()`.

---

## See also
- [`README.md`](README.md) — the observability scale, the Orwell bar, and the three hook shapes this map applies.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/players`, `/units`, `/diagnostic/*`) these hooks extend, and the live-read rules.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — [DEC-obs-scale], [DEC-obs-hook-shapes], [DEC-map-before-delete].
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing, and the cascade model the GP sources feed.
- [`../../README.md`](../../README.md) — the comprehension map (overview-of-overviews).
