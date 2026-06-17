# Session handover — 2026-06-15 PM (BOTH cascade structures LOCKED + the top-down ranking)

**Read this first to resume.** Self-contained. This session **nailed the modifier-cascade object structure
& vocabulary** (the thing that kept getting re-flopped) and produced the **top-down entity ranking**. Both
cascade structures are now LOCKED; the next phase is toolkit-update → top-down re-curation.

**Resume reading order (this sequence):** (1) **this handoff** — orientation + state; (2)
**`modifier-cascade-spec.md` (v3)** + **`enabler-cascade-spec.md` (v0.3)** — the LOCKED rulesets; (3)
**`migration-entity-ranking.md` — THE authoritative curation-order artifact you EXECUTE FROM** (full
per-entity context + the ordering reasons). Then begin the NEXT PHASE below.

## TL;DR
- **The MODIFIER structure is DESIGN-COMPLETE and owner-LOCKED** — read **`modifier-cascade-spec.md` (v3)**.
- **The ENABLER model stays v0.3** — `enabler-cascade-spec.md`; its **instance-fate table is owner-accepted**.
- **The whole point achieved:** one standardized object structure + one shared vocabulary, reused across
  modifiers / grants / conditions / count-scaling — so the JSON target is fixed *before* the next migration
  attempt (the prior flopping was from locking a structure before knowing the full field population).
- **Top-down ranking produced + owner-verified** (below) — the curation order.

## The locked MODIFIER structure (the one-page version — full detail in `modifier-cascade-spec.md` §1)
- **Top-level object = RESERVED section keywords** (`enables`/`obsoletes`/`replaces`/`requires`/`grants`/
  `text`/`cost`/`art`/`identity`/`ai`, non-exhaustive) **+ flat modifier families** (everything else:
  `food`/`production`/`commerce`/`gold`/`happiness`/`health`/`buildRate`/`maintenance`/… + one per
  `PROPERTY_*`). No `modifier:` wrapper.
- **A family addresses as** `<family>.<scope>.<targetType>.{TARGET}.<member>.<unit>` = value.
- **One entry shape everywhere** (modifier deposit AND grant): `{ <payload>, scope?, per?, enabled?, disabled? }`
  — optionals default to no-ops; a leaf is one entry or a cumulative LIST.
- **Vocabulary:** Types = data Types + catch-all tokens (`TURN`/`POPULATION`/`MILITARY`/`SELF`); Scopes =
  `world|team|empire|area|city|plot{improvement|feature|terrain|route}|building|specialist|unit`; Conditions
  (`enabled`/`disabled`/`per`) = the enabler `requires` object **verbatim, object form**; Units = `flat` |
  `percent` (additive delta) | `multiplier` (full-scale, `×2`=`200`, product).
- **Accumulation:** `effective = (base+Σflat) × (100+Σpercent)/100 × Π(multiplier/100)`. Exact arithmetic
  pins at #430.
- **Conditioning** = per-deposit `enabled`/`disabled` (no group wrapper). **Count-scaling** = `per:{type,scope}`
  (subsumes perPopulation/perMilitaryUnit/perTurn). **Unit plane** = self-accumulator (promotion stack).
  **CREST resolved** = keep-on-source via `enabled`/`per` (nothing inverts; `buildRate` un-folded).
- **§0.6 hard boundary:** Info DATA = values/payloads/relationships only; engine MACHINERY (create-unit
  subroutine, combat/visibility resolvers, event-hook dispatch, tally) is never authored in the data.

## THE TOP-DOWN RANKING (curation order — owner-verified 2026-06-15)
**Authoritative, full-context version: `migration-entity-ranking.md`** — per-entity edges / modifier
surface / status / decisions + the "why this order" reasons. Summary below.
Config & sources first, monster targets last. `Improvement ≡ its Build`. `Special*` ride their monster.
1. **Config / global axes:** GameSpeed · Handicap · Era · Process · Victory · Vote · CultureLevel · Hurry ·
   BonusClass · CivicOption · Property (defines `PROPERTY_*`; diffusion→#429) · Civilization
2. **Top-of-cascade sources:** Tech (spine root) · Civic · Religion · Corporation · Trait
3. **Resources & map substrate:** Bonus · Route · Terrain · Feature · Improvement(+Build) · Build
4. **City/unit producers & unit-plane sources:** Specialist · Heritage · Project · PromotionLine → Promotion ·
   UnitCombat · LeaderHead
5. **Monsters (last):** SpecialBuilding → **Building** · SpecialUnit → **Unit**

## NEXT PHASE
1. **Update the toolkit to the LOCKED structures** (`Tools/Migration/`):
   - `store.py`: emit the FOUR `enables`-family objects (`enables`/`disables`/`obsoletes`/`replaces`)
     distinctly; **retain tech `AndPreReqs`/`OrPreReqs` as `requires.build.all`/`.any`** (don't flatten to
     `enables.techs`); apply source-vs-target / inversion rules.
   - `curate_common` + curators: emit modifier FAMILIES per v3 (reserved sections + flat families); the
     entry shape `{value, scope?, per?, enabled?, disabled?}`; `enabled`/`disabled` = verbatim enabler
     object; `per:{type,scope}`; units `flat`/`percent`/`multiplier`; `requires.build` vs `requires.operate`.
   - **Un-fold `buildRate`** from `curate_bonus` (author on the building/unit/project instead — CREST §6).
   - **Re-render `modifier-cascade-mapping.json`** to the locked shapes (it still shows `when`/`perCountOf`).
2. **Re-curate top-down per the ranking** — config → monsters (Building/Unit) last, each against BOTH specs.
   Per-entity method = curation not transcription; **drop-list re-check is mandatory per entity** (see below).

## ⚠ MANDATORY per-entity drop re-check (the grounding's dead-list is C++-only — ~4 of 15 were wrong)
Every "dead" field gets sorted into FOUR categories (modifier-spec §8), re-verified against `Assets/Python`
AND intent before dropping: **(i) truly dead** → drop; **(ii) unwired modifier** → revive (e.g.
`FreeSpecialistCounts`→`freeSpecialists`, `iPillageGoldModifier`→`pillageGold.empire.percent`) or drop-if-bad
(`Project YieldModifiers` nuked); **(iii) unwired world-state feature** → separate issue (`bNoAnimals`);
**(iv) deliberate balance cut** → keep capability, cut a source (`healthPercent` modifier kept, not from
improvements). Do NOT blanket-trust the mapping.

## Banked for later phases (not blockers)
- **Enabler-spec additions** (reuse-verbatim, conscious change only): age/duration predicate (`existedFor` +
  a created-timestamp, created where absent); `firstToResearch`/`firstToDiscover`; `grants` family includes
  **bonuses** (`ExtraFreeBonuses` — wonder/culture grants a tradeable resource, fired on delivery).
- **Unit modifier vocabulary** (unit-stat families) → the Unit/Promotion/UnitCombat pass.
- **#430:** catch-all token registry; event-hook + tally infrastructure; combination semantics
  (`combineStyle`/`polarity`/`flatPlacement`/`combine`); `multiplier` arithmetic.
- **Separate future issues:** "disable animals" BUG/game option; empire-wide yield buffs; the #429
  spatial-leakage / adjacency-yield system; `PropertyEffect`/`BaseEffect` rework of threshold pseudobuildings;
  a dedicated Size-Matters pass.

## Pointers
- **Curation order (full-context + reasons): `docs/dev/plans/migration-entity-ranking.md`.**
- **Modifier structure (LOCKED): `docs/dev/plans/modifier-cascade-spec.md` (v3) — READ FIRST.**
- Enabler model: `docs/dev/plans/enabler-cascade-spec.md` (v0.3; instance-fate table accepted).
- Migration plan + per-entity rulings: `docs/dev/plans/building-cascade-conversion.md` (its §3
  modifier-structure section is **superseded** by the v3 modifier spec).
- From→to field mapping: `docs/dev/plans/modifier-cascade-mapping.json` (⚠ re-render to locked shapes).
- Toolkit: `Tools/Migration/` (`store.py`, `curate_common.py`, bespoke `curate_*.py`).
- Prior handover: `docs/dev/plans/handover-2026-06-15.md` (AM — enabler lock + branch reset).

## Git state
- Branch **`json-data-migration`**: docs + `Tools/Migration/` toolkit (no `Assets/Data`). This session added
  `modifier-cascade-spec.md` (v3) + `modifier-cascade-mapping.json` + this handover; edited
  `enabler-cascade-spec.md` (instance-fate accepted). All working-tree (not committed) unless the owner says so.
- Archive of the pre-reset 66-commit state: tag `archive/json-migration-pre-reset-2026-06-15`.
