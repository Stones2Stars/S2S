# CvGameSpeedInfo simplification (Percents → named fields, GameTurnInfos collapse)

Part of the #196 declarative-loading migration (sub-issue #248). Follows the `CvWorldInfo`
precedent (#310): restructure odd data into descriptive named fields instead of
special-casing the loader.

Status: **IMPLEMENTED 2026-06** (awaiting in-game playtest). The resulting model is
documented in `../reference/calendar-and-gamespeed.md`; this doc remains as the findings
record and decision log. Deviations from the plan below, per owner direction:
- Phases 2+3 were merged — the intermediate "GameTurnInfos as declarative struct-vector"
  state was skipped (no doing the work twice); the table went straight to derivation.
- The `<Adapt>` grammar became tag-dispatched (`<Adapt>`/`<AdaptHammerCost>`/`<AdaptUnitYield>`,
  fixed `AdaptTypes` enum) instead of pattern-matching an `<ID>` child string.
- `CvEraInfo.iStartPercent` was also deleted (start turns now derive exactly from per-era
  turn counts). Bonus finding: the old `iStartPercent` data proved the GameTurnInfo rows were
  **off by one vs eras** (prehistoric had two rows, Future none), so `CvGame.cpp`'s
  `getGameTurnInfo(getStartEra())` doSpawns site was reading the previous era's pacing.
- Per-era Normal-speed turn counts were taken from the `iStartPercent` deltas (round
  designed numbers summing to 2000), not the generated row turn counts.

## Findings (verified 2026-06)

### The `Percents` map (`IDValueMapPercent m_Percent`)

- Across all 9 game speeds the map holds exactly **3 IDs**:
  - `ADAPT_DEFAULT` — **identical to `iSpeedPercent` in every speed** (redundant).
  - `ADAPT_BUILDING_AND_UNIT_COSTS` — **also identical to `iSpeedPercent` in every speed**.
  - `ADAPT_UNIT_YIELD` — the only independent data (subdued-animal yield scaling;
    ≈ `sqrt(speed/100) * TECH_COST_MODIFIER` per the XML comment).
- Sole consumer is `adaptValueToGame` (`CvGameObject.cpp`), reached only from `IntExprAdapt`
  (`<Adapt><ID>…</ID><expr/></Adapt>` in XML int-expressions). Live XML users:
  caravan hurry-production yields (`ADAPT_BUILDING_AND_UNIT_COSTS`), captive-upgrade mission
  costs (`ADAPT_DEFAULT`), subdued-animal yields (`ADAPT_UNIT_YIELD`).
- **The handicap leg of `adaptValueToGame` is dead data**: no handicap XML anywhere defines
  `Percents`, so `CvHandicapInfo::getPercent` always returns the map default (100). Same
  dead-structure pattern as the WorldInfo leg removed in #310.
- The Adapt IDs are *dynamic* `getOrCreateInfoTypeForString` ints, not an enum — this is what
  makes the map non-declarable (`InfoClassTraits<int>` doesn't exist). The ID-less `<Adapt>`
  form silently uses global info-type index 0 (essentially arbitrary); **zero live XML usages**.
- Latent inconsistency: `getHammerCostPercent()` applies the
  `GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS` modifier to `iSpeedPercent`, but the
  `ADAPT_BUILDING_AND_UNIT_COSTS` percent (caravan hurry production) does **not** get the
  modifier — with the option on, costs scale up but caravan hurry amounts don't.
- `CvGameSpeedInfo` + `CvHandicapInfo` are the **last two users of `IDValueMapPercent`**;
  removing both eliminates blocker (2) "Non-FK IDValueMap" from
  `reference/declarative-info-loading.md` outright.

### `GameTurnInfos` (calendar pacing)

- **Three** overlapping stores of the same knowledge:
  1. `GameTurnInfo* m_pGameTurnInfo` — raw `new[]` array holding month + turns-per-increment
     (the XML's `iDayIncrement` is **dropped** from this struct);
  2. `std::vector<CvDateIncrement> m_aIncrements` — month + day + cumulative end-turn +
     runtime-computed end-date + the `m_bEndDatesCalculated` mutable flag;
  3. the `HISTORICAL_ACCURATE_ERA_RANGE_*_START/_END` GlobalDefines (era → real-year ranges)
     used by the historical-accurate calendar (`calculateCurrentTick`).
- The only live-consumed field of `GameTurnInfo` is `iNumGameTurnsPerIncrement`
  (start-turn / estimate-end-turn sums in `CvGame.cpp:243-269`, the doSpawns delay at
  `CvGame.cpp:5856`, Python `TimeKeeper.py` + `WBGameDataScreen.py`). Its `iMonthIncrement`
  is consumed only by a commented-out block in `getTurnMonthForGame`.
- The 14 entries per speed implicitly correspond 1:1 to the 14 eras; `CvGame.cpp:5856`
  (`getGameTurnInfo(getStartEra())`) depends on that **unenforced positional** mapping.
- **Out-of-bounds write bug** in `copyNonDefaults`: when a modular gamespeed inherits
  `GameTurnInfos`, `m_aIncrements[j] = …` indexes an *empty* vector (never resized).
- Why "not working as designed": the table assumes era progression follows a fixed turn
  schedule, but actual progression is play-dependent, so the displayed date drifts from the
  played era. `MODDERGAMEOPTION_USE_HISTORICAL_ACCURATE_CALENDAR` was added to fix this by
  computing the tick from tech influence at runtime — bypassing the table (forward-only
  clamp in `CvDate::getDate`).
- The table is generated data (~600 lines of magic numbers): `iTurnsPerIncrement` scales
  ≈ Normal × `iSpeedPercent`/100 (with rounding), and the month/day increments are derived
  from the era year spans.
- Misc rot: dead `m_szGameSpeedName` member; dead `dateModifier` local in
  `CvDate::increment`; debug `sprintf` in `read()`; lazy end-date computation mutates the
  info object via non-const `getIncrements()`.

## Plan

### Phase 1 — Percents → named fields (structure only; one flagged behaviour fix)

1. `CvGameSpeedInfo`: delete `m_Percent`. Map the three legacy IDs:
   - `ADAPT_DEFAULT` → `getSpeedPercent()` (data identical everywhere).
   - `ADAPT_BUILDING_AND_UNIT_COSTS` → `getHammerCostPercent()` — **behaviour fix**: caravan
     hurry production now respects the upscaled-costs game option (needs owner sign-off;
     alternative is a plain second accessor returning `m_iSpeedPercent`).
   - `ADAPT_UNIT_YIELD` → new named scalar `m_iUnitYieldScalePercent` + XML tag
     `iUnitYieldScalePercent` (the only new XML data; default 100).
2. `IntExprAdapt`: parse the `<ID>` string at XML-read time into a small fixed `AdaptTypes`
   enum (`ADAPT_DEFAULT`, `ADAPT_BUILDING_AND_UNIT_COSTS`, `ADAPT_UNIT_YIELD`;
   `FErrorMsg` on unknown ID and on the ID-less form). `evaluate` switches to the named
   accessor. No more dynamic info-type IDs.
3. Delete the dead handicap leg: `CvHandicapInfo::m_Percent`/`getPercent`, and collapse the
   `CvGameObjectPlayer/City/Unit::adaptValueToGame` overrides into the base (game-speed-only).
4. XML/schema: drop `Percents` from the gamespeed + handicap `ElementType`s in
   `C2C_CIV4GameInfoSchema.xml`, delete all `<Percents>` blocks from `CIV4GameSpeedInfo.xml`,
   add the new tag. Run `XmlValidator -a`.
5. Migrate `CvGameSpeedInfo` (and unblock `CvHandicapInfo`) to declarative
   `getDataMembers` for its scalars. Delete dead `m_szGameSpeedName`.

### Phase 2 — collapse GameTurnInfos to one struct vector

1. Single struct (month, day, turns-per-increment) loaded via the declarative `addStruct`
   wrapper; delete the raw `GameTurnInfo` array, `allocateGameTurnInfos`,
   `m_iNumTurnIncrements` (use vector size), the debug `sprintf`. This also kills the
   `copyNonDefaults` OOB bug.
2. Move derived state (cumulative end turn, end date) **out of the info class** — compute
   once post-load instead of the lazy `setEndDatesCalculated` mutation through non-const
   `getIncrements()`.
3. Make the era↔increment 1:1 explicit: load-time `FAssert(size == GC.getNumEraInfos())`
   and a named per-era accessor for the `CvGame.cpp:5856` doSpawns site.
4. Update consumers: `CvGame.cpp` sums, `CvDate`, `CvGameTextMgr`, Python exposure
   (`CyInfoInterface2`/`CyStructsInterface1`, `TimeKeeper.py`, `WBGameDataScreen.py`).
5. Checksum changes are acceptable per the owner ruling in
   `reference/declarative-info-loading.md` (one recalc prompt per old save).

### Phase 3 (optional, behaviour-affecting — owner decision) — derive the table

Replace the 9×14 generated table with derivation from single sources of truth:
- Move the era→year ranges from the `HISTORICAL_ACCURATE_ERA_RANGE_*` GlobalDefines onto
  `CvEraInfo` (e.g. `iHistoricalStartYear`/`iHistoricalEndYear`); `initEraYearRanges` dies.
- Store one base turns-per-era list (Normal speed, on `CvEraInfo`); scale by
  `iSpeedPercent`/100 per speed; derive month/day increments from era year span ÷ turns.
- `CvGameSpeedInfo`'s XML shrinks to ~3 scalar tags; ~600 lines of generated XML deleted;
  the static calendar and the historical-accurate calendar share one era→year dataset.
- **Cost**: exact turn totals shift slightly (rounding differences vs the generated table),
  i.e. real gameplay pacing impact → needs a playtest and explicit owner go/no-go.

Phases 1 and 2 are independent of Phase 3 and load-verifiable
(`XmlLoad.log` counts, no `Xml_MissingTypes.log`, no new asserts).
