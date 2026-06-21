# Handover 2026-06-19 — modifier cascade: in-game shadow + the fixed-point/scale rework

> A handover is a TRANSIENT relay (work done + next), NOT a source of truth. Every durable fact/ruling below already
> lives in a committed doc (cited). Deletable-without-loss. **Read the durable docs first.**

## ⛔ FIRST, READ (the read-gate + the durable homes for THIS work)
- Re-read `AGENTS.md` + `Sources/AGENTS.md` + the #428/#430 read-gate docs (data-model / enabler / modifier / tally /
  event-spine / cascade-engine-430 / migration-renames / migration-entity-ranking).
- **NEW this session (the durable homes — read these for the modifier work):**
  - **`docs/dev/reference/cascade-fixed-point.md`** — the LOCKED design: the 3-layer chain (human JSON → readJson
    converts → pure integer cascade), the ×100 ("2 decimals") model, the translation table, the per-100 de-scale map.
  - **`docs/dev/plans/modifier-cascade-known-discrepancies.md`** — the diagnosis, the 6-city sweep findings, and the
    governing rulings: **UNIFY-the-calc / FIX-the-data**, bands cumulative+incremental, verify-via-emulator.
  - `docs/dev/plans/modifier-cascade-shadow-spec.md` (increments 1–3) · `cascade-engine-430.md` §Impl Status ·
    `calc-emulator-spec.md` (the offline tester) · `http-server.md` (the `/diagnostic/*` surface).
- **⛔ The running game holds `.log` files OPEN — never live-read them; use `/events` or `/diagnostic/*` endpoints**
  (AGENTS.md cascade-observability §, added this session). Delegate dumps to the `data-reader` sub-agent.

## What was done this session
1. **Modifier in-game shadow — increment 3 (committed-ready, Assert-clean, on working tree):** `/diagnostic/modifierSweep`
   (all-cities triage + cause/care histograms), per-turn `[MODSHADOW]` (`cascadeModifierShadow` in `CvGame::doTurn`),
   the `cascadeModifierClassify` cause-tagger + Fine→Meltdown care scale (`CvCascadeModifier.{h,cpp}`). NB the prior
   "modifier ABSENT" status docs were stale — increments 1+2 were already committed; status docs corrected.
2. **Increment 4:** specialist-in-base (`cascadeModifierCityBase`) + civic empire-scope deposits folded into the city slot.
3. **THE SCALE REWORK (the session's core):** discovered the cascade massively over-counted (≈10×) because the migrated
   JSON mixed scales — base `YieldChange` ×1 but `TechYieldChanges100` ×100, both under the unified `flat` key. Locked
   the design with the owner (see cascade-fixed-point.md): human JSON; conversion ONLY in readJson; curator owns the
   one-time per-100→human de-scale; cascade is pure ×100 integer math; one unified calc, fix the data not the calc.
   - **Curator de-scale DONE:** `Tools/Migration/curate_building.py` `PER100_TAGS` ÷100 → **555 building JSONs
     regenerated human-readable** (clean 1:1 value swap, on working tree, uncommitted).
   - **Offline emulator proven:** `Tools/ModifierCalc/cascade_sim.py` reworked (human→×100 import, buildings+civics,
     all-present, multi-city `--glob` sweep) + 6 city fixtures (`samples/m_p{0,1,2,4,5,6}.json`).
   - **`cityInput` endpoint** now emits the active set (`hasFullyActiveBuilding`) + `dormantBuildings` (CvHttpServer.cpp).
4. **Diagnosis CONFIRMED (6-city sweep):** scale bug fixed; **production 5/6 + food 3/6 parity-adjacent**; commerce
   isolated to **property-effect bands counted at FULL value** (education ×7 = +140% vs the +35% it should be). Active-set
   exclusion was the WRONG fix (broke production −25%); the right fix is **all-present + incremental band values**.

## ⭐ RESUME HERE — the next task
**Task #10 — incremental band-value curation (fully specified + unblocked).** Band structure FOUND:
`CIV4PropertyInfos.xml` `PropertyBuildings` (per property: building + `iMinValue`/`iMaxValue`; cumulative, max≈∞;
rank by `iMinValue`; increment = `value[i] − value[i−1]` by rank). **Wrinkle:** parallel ladders (e.g.
`ARGUMENTATIVE_AWARENESS_1/2/3` overlap mins) + a separate NEGATIVE ladder (`UNAWARE…`, ranked by `max`) → must GROUP
into ladders, not naive min-sort. Build: (1) curator groups bands per property into ladders + computes per-band deltas
+ authors them in the band JSONs; (2) regenerate; (3) `cascade_sim --glob` sweep → confirm commerce parity-adjacent
across cities AND all properties (crime/disease/tourism/pollution — each UNIFIED to the one incremental model, NOT
matched to legacy's per-property quirks). **Then** the DLL port: tasks #7 (`CvCascadeModifier` integer helpers) + #8
(`readJson` human→×100 import + the deposit gating) — the final game rebuild.

**The offline loop needs NO rebuild** (curator + `cascade_sim` read JSON files). Only the final readJson port (task
#7/#8) needs a rebuild. Re-fetch fixtures (`curl /diagnostic/cityInput?player=N -o samples/m_pN.json`) if game state moved.

## Working tree (ALL uncommitted — owner inspects; nothing committed this session)
`CvCascadeModifier.{h,cpp}`, `CvCascadeReadJson.{h,cpp}`, `CvGame.cpp`, `Sources/Tools/CvHttpServer.cpp` (shadow +
endpoints + cityInput split); `Tools/Migration/curate_building.py` (de-scale); 555 `Assets/Data/buildings/**/*.json`;
`Tools/ModifierCalc/cascade_sim.py` + `samples/m_p*.json`; the docs above. Branch `json-data-migration`. Assert builds GREEN.

## Tasks: #1–6,9 done · #7 (fixed-point helpers) + #8 (readJson port) pending (DLL, after offline model nailed) ·
**#10 (band curation) = NEXT** · increments 3+4 shadow built but the ×1-scale comparison in `[MODSHADOW]`/`modifierSweep`
will be reworked to ×100 when #8 lands.
