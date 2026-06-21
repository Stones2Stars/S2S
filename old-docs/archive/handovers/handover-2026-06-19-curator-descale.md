# Handover 2026-06-19 — curator ×100 de-scale (#432) + the read-gate context-loop

> Transient task-list relay (per the handover ruling). Durable facts already live in the specs;
> this only carries *what's done / what's next*. Delete-able once consumed.

---

## ⛔ #0 — THE READ-GATE IS EATING THE SESSION (owner: resolve before resuming)

The cascade read-gate (`.claude/read-gates/cascade.json`) uses `docsDirs:[docs/dev/plans,
docs/dev/reference]` = the **entire ~109-doc set**, and the session-start directive + the PreToolUse
**deny** force reading them ALL **before any edit / subagent / write-Bash**. Reading all 109 **in full**
(owner: "reading means reading") costs **~500k tokens / ~90% of the 1M window** — so the session hits
autocompact *before* the actual curator work runs. This session did exactly that: full manifest read,
then context full, zero edits landed. **This is a process deadlock, not a task failure.**

Options for the owner (pick one before the next session does real work):
1. **Lift the gate** — declare the codebase "under control" (only the owner can), or temporarily set
   `sessionStart:false` / narrow `docsDirs` for the curator pass.
2. **Accept read-once-then-work** — but autocompact discards the read, re-triggering the loop. Not viable
   at 109 docs unless the work is small enough to finish in the post-read budget (it isn't here).
3. **Minion exception** — the gate exempts spawned sub-agents; the orchestrator (having read) briefs a
   minion with just the curator slice. But the orchestrator itself must still pass the gate to *launch*
   the minion (subagent launch is gated), so this only helps if the gate is first satisfied or relaxed.

**Recommendation:** for the curator de-scale pass specifically, relax the gate to the handful of docs that
actually govern it (below), do the work, then restore. The de-scale is a contained value-correctness task;
it does not need the full 109-doc context — only: `cascade-fixed-point.md`, `legacy-value-calc-map.md`,
`migration-renames.md` (§Tech/§Building), `building-cascade-conversion.md` §3+§7, and
`migration-entity-ranking.md`.

---

## #1 — THE TASK: finish the curator ×100→human de-scale (#432), regenerate, verify. DO NOT COMMIT.

Owner directive (this thread): "it is the curators' job to ensure that ×100 values are mapped to real
human-readable JSON values… redo it so all of them parse out to the correct values, all verified by the
standards set, not defer something later." Migration is **strictly serial**; **owner visually inspects the
written JSON before any commit**. So: edit curators → `--write` → owner inspects → (owner) commits.

### Verified CURRENT STATE of each curator (trust-but-verified against on-disk this session)

| Curator | de-scale state | PENDING |
|---|---|---|
| `curate_common.py` | `descale100()` helper EXISTS (lines 24-37). `accumulate_boosts` does **NOT** de-scale. | **ADD** `BOOST_PER100` + de-scale the tech-inverted building boosts (see #2). |
| `curate_building.py` | `_descale100()` + `PER100_TAGS = frozenset(("TechYieldChanges","TechCommerceChanges"))` EXIST (293-303), applied at 378-379 in the COND_KEYED pass-2. So building-side Tech{Yield,Commerce}Changes ARE de-scaled. | `BonusCommercePercentChanges` percent→flat + add to PER100 **(verify ×100 first — #3)**; `perPopulation` de-scale **(⛔ likely WRONG — #3)**. |
| `curate_tech.py` | Comment line 17 still says Tech{Yield,Commerce}Changes "carried FAITHFULLY (#432)". | **UPDATE** that comment once BOOST_PER100 lands (they're now de-scaled, not faithful). De-scale itself is in curate_common, not here. |
| `curate_era.py` | `iInitialCityMaintenancePercent` → `maintenance.city.initial.flat` (line 65), ×100, **0 in all eras → never emitted** (the `int(t)!=0` guard). | De-scale is **defensive-only / moot today**. Low value; do it for completeness only if the owner wants zero deferral (needs a special-case in `curate()` FAMILIES path, which does `int(t)` raw). |
| `curate_unitcombat.py` | imports `descale100`; de-scales `tag.endswith("100")` (101-102) → `iExtraUpkeep100` handled. | `iCelebrityHappy` coverage — see #4. |

### #2 — curate_common BOOST_PER100 (the one confidently-correct edit)
`Building.TechYieldChanges` / `Building.TechCommerceChanges` are inverted **onto the tech** via
`TECH_BOOSTS` (curate_tech.py:18-30) through `accumulate_boosts`. They are ×100 (`getTechYieldChanges100`,
700 = +7.00) — confirmed by name AND the math (legacy-value-calc-map; cascade-fixed-point §2). The
building-side path already de-scales them (PER100_TAGS); the **tech-side inversion does not** → tech JSON
currently holds 700, the building JSON holds 7 → inconsistent. Fix in `accumulate_boosts`:
- Before the loop: `BOOST_PER100 = frozenset((("BuildingInfo","TechYieldChanges"),("BuildingInfo","TechCommerceChanges")))`
- In `for cfg…`: `per100 = (src_ent, fld) in BOOST_PER100`
- First line of `for ref, u, val in _boost_entries(...)`: `if per100: val = descale100(val)`
**Verify after:** `Assets/Data/techs/<era>/tech_*.json` with a `yield`/`commerce` building-keyed deposit
shows human values (e.g. `tech_laser` research building flat = **7**, not 700). NB the building-side and
tech-side must now AGREE.

### #3 — ⛔ THE UNVERIFIED VALUE-CORRECTNESS DE-SCALES (NO-GUESSING — do NOT blind-apply)
The earlier plan listed "de-scale `perPopulation` + `BonusCommercePercentChanges`". **I was mid-verifying
these against the live C++ when context ran out.** Applying a ×100 de-scale to a field that is actually ×1
turns `+1` into `+0.01` = silent data corruption — exactly the error the owner's NO-GUESSING rule exists to
prevent. **Verify each against `Sources/Engine/CvCity.cpp`** (⚠ the file is at `Sources/Engine/CvCity.cpp`,
NOT `Sources/CvCity.cpp`) before touching:
- **`BonusCommercePercentChanges`**: building-cascade-conversion §7 "#432 trap" (line ~708) states it is
  ×100-space "added inside `getBuildingCommerce100`, no `*100` getter." → **almost certainly genuinely
  ×100** + the tag is mislabeled `percent` (it's a flat change). Confirm the accumulation, then: in
  curate_building COND_KEYED (line 255) `percent`→`flat`, AND add to PER100_TAGS so 378-379 de-scales it.
- **`YieldPerPopChanges` / `CommercePerPopChanges` (perPopulation)**: legacy-value-calc-map §1 +
  observability/food-yields-wastage §1.2 show `getBaseYieldPerPopRate * getPopulation()` added at **×1**
  into `getExtraYield` — i.e. **per-pop looks like ×1, NOT ×100**. If so, the planned perPopulation
  de-scale is **WRONG and must NOT be applied.** Confirm the exact accumulation in CvCity.cpp first; only
  de-scale the ones that genuinely flow through an ×100 bucket.
- General rule (cascade-fixed-point §2): the `…100()`-getter set is the de-scale list, **plus** the few
  ×100-space addends that lack a `…100()` getter (the #432-incomplete-heuristic note) — `BonusCommercePercent`
  is the confirmed one of those. Map each, don't pattern-match the name.

### #4 — curate_unitcombat iCelebrityHappy
legacy-value-calc-map §10.4 correction: **celebrity happiness DOES exist** (`CvCity::getCelebrityHappiness`,
unit-derived, feeds `happyLevel`). `iCelebrityHappy` on UnitCombat/Promotion should be in the curator's
coverage set (handled or explicitly DROP-ped), not silently leftover. Verify it's accounted for; the
unitcombat curator has a coverage check that should flag it.

---

## #2-tier — STILL-OPEN owner tasks (after the de-scale lands)
1. **Walk ALL docs to fix inconsistencies/wrong statuses** (owner-ordered after regen). Partly done last
   session (cascade-fixed-point §2 marked DONE, legacy-value-calc-map §10.4/§12 corrected,
   modifier-cascade-known-discrepancies §A.1/PART-2 recorded). Re-sweep once the de-scale set is final —
   especially any doc that still says a field is "carried faithfully ×100" / "#432 deferred."
2. **Map happiness/anger NOW** (owner: "I want all those things mapped now so I don't come back to this").
   The map already exists — legacy-value-calc-map §3 (happyLevel/unhappyLevel/goodHealth/badHealth full
   source lists) + observability/health-happiness.md. The actionable gap is the **dump add-list**:
   legacy-value-calc-map §12 lists the singular happiness/anger inputs the `/diagnostic/cityInput` dump does
   NOT yet emit (Σ civic %anger, defy/revRequest %anger, espionage happy counter, eventAnger, taxRate
   unhappy, foreign-unhappy, landmark, celebrity, vassal, the noUnhappiness/noUnhealthyPop gate flags). That
   is the work — extend `cityInput` so the offline emulator can shadow happiness.
3. **Drive parity tolerance MUCH sharper** (owner: "±10% is NOT parity-adjacent"). Per
   modifier-cascade-known-discrepancies §A.1 PART-2: the residual after the band fix is (a) TECH-DOWNWARD
   deposits — `cascade_sim.py` doesn't load tech JSONs nor handle keyed sub-scopes
   (`<y>.<scope>.{buildings|improvements|specialists}.{TARGET}`); (b) corporation flat (by-design EXCLUDED
   per calc-emulator §2a — a `Better`, not a bug). Wire tech-downward into `Tools/ModifierCalc/cascade_sim.py`,
   then re-sweep.

## Validation surface
- Offline: `Tools/ModifierCalc/cascade_sim.py --glob` (6-city sweep vs legacy `getYieldRate100`).
- Live (game running): `/events` SSE + `/diagnostic/cityInput|modifierSweep|modifier` (per the
  data-reader minion — never pull raw dumps into an expensive context). Logs are held open by the running
  game → don't live-read `.log` files.

## Working tree (uncommitted — owner to inspect before any commit)
`git status` at session start showed many `Assets/Data/buildings/*.json` modified = the prior session's
band work (education incremental bands + the `requires.operate` PROPERTY-in-band atoms on all 188
pseudobuildings, via `curate_building.apply_property_bands`/`property_band_atoms`). The curator edits
listed above (descale100, PER100_TAGS, band atoms) are already on disk. Re-run the affected curators with
`--write` after the de-scale edits, owner inspects, then commit. **Branch: `json-data-migration`. Do NOT
commit; do NOT switch branches (owner builds from the working tree).**
