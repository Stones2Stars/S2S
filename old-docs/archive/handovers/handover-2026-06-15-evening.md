# Session handover — 2026-06-15 EVENING (TIER A COMPLETE — the serial re-curation is rolling)

**Read this first to resume.** Self-contained. This session **executed the top-down re-curation through ALL of
Tier A (12/12 config/global entities)**, one info at a time, each owner-reviewed and committed — and locked a
large amount of durable MODEL LAW along the way (below). The toolkit is v3-shaped. **Next is Tier B (the
top-of-cascade sources), starting with Tech.**

**Resume reading order:** (1) this handoff; (2) the two LOCKED specs — `modifier-cascade-spec.md` (v3) +
`enabler-cascade-spec.md` (v0.3, now with **§6.1** + the **Uniformity Law**); (3) `migration-entity-ranking.md`
(the curation order you execute from); (4) `migration-renames.md` (the manual-rename registry — the C++
readers-pass mapping). Then start Tier B.

---

## ⛔ THE PROCESS — non-negotiable (owner rulings, this session)

- **STRICTLY SERIAL, one info at a time, NEVER in parallel.** The catastrophe of the abandoned mass-migration
  was loss of VERIFIABILITY, not the conversions. (memory: `serial-info-conversion-only`)
- **The per-info loop:** curate → `--write` the JSON under `Assets/Data/<info>/` → **the OWNER reviews & PASSES
  it** → only then commit that ONE info → next. The owner is the verification gate; do NOT commit/advance unprompted.
- **What "verify" means now:** we CANNOT runtime-test (atomic cutover; DLL still loads XML). Verification =
  **structural adherence to the locked shapes + ZERO invention** (every family/scope/token/edge traces to the
  spec or real XML) + cross-checking the static C++/Python consumer. **Drop re-check is MANDATORY per field**
  against `Assets/Python` AND intent (C++-only "dead" calls are a trap — caught real ones: Era `bNoAnimals` has
  a Python pedia consumer; Victory `isPermanent` is `DllExport`).
- **EXE-link check per entity:** grep `DllExport` on the `Cv*Info.h`. EXE-bound classes are hard-constrained
  (the JSON is still `readJson`-mapped, so data is free, but flag it). Civilization=7, Victory=1(isPermanent),
  Process/Hurry/CultureLevel/BonusClass=0.

## ⛔ THE MODEL LAW locked/extended this session (all in the specs + memory)

- **Data-authoring (memory `migration-data-authoring-principles`):** (1) author the unit/shape for what the
  DATUM IS, NOT how the engine combines it (a percentage is `percent` even if the engine multiplies — combination
  is §7 engine metadata; this fixed a GameSpeed `multiplier` error); (2) **the C++ data-fetching is reworked to
  fit the JSON, never the reverse**; (3) the file must read COLD to a modder; (4) **THE UNIFORMITY LAW** — *"the
  moment we do special things with enabling/granting/requirement for any particular info, the clowns with
  rollerskates ruin the day."* `enables`/`grants`/`requires` use the SAME structure for EVERY info; no per-info
  shapes, no parser branch that knows "this is the crime property." (enabler-spec §6.1)
- **`store.py` — the four `enables`-family objects** (forward-read-from-HAS, drive generation): `enables` (add) ·
  `obsoletes`/`replaces`/`disables` (subtract). `replaces` is NEW, inverted from `BuildingInfo.ReplacementBuildings`
  (B replaces the A that lists it — forward-from-HAS, no upward query). `disables` = reversible ban (scope just a
  parameter: empire-law vs the per-civ Neanderthal-barb tech ban). Enabling flows RIGHT-then-DOWN, forward only.
- **`ai` audience qualifier** (entry shape, Handicap): bare unit = all players, `ai` block = AI-only. (modifier-spec §1.3)
- **`loadPrune`** for load-stable game-option gates (CultureLevel `PrereqGameOption`), not identity.
- **§6.1 — ENABLE → check-for-immediate-GRANT → AUTO-BUILD → `requires`-active** (Property effect-buildings): a
  grant can't fire until enabled; enabling triggers a grant-check; granted thing is auto-built; then the
  BUILDING's own `requires` value-band decides active. `grants` (a plain list) and `requires` (on the building)
  are SEPARATE reserved sections. **Every `requires` atom is FULL/EXPLICIT/self-describing** (`{type, scope,
  min?, max?}`) so the parser never infers from context.
- **Module inclusion CASE BY CASE** (memory `module-inclusion-case-by-case`): `store.EXCLUDED_MODULE_SUBPATHS`
  drops `/modules/zwip/` (WIP; smuggled in PROPERTY_FRUIT/HUNT/MATERIAL/LORE + BONUSCLASS_EXTINCTION). Authority =
  `Assets/Modules/MLF_CIV4ModularLoadingControls.xml`. **⚠ The OTHER modules (Bad_Karma, NotSoGood,
  P2K_Multimaps_Test, Alt_Timelines, Cultures, Pepper2000, Thunderbrd) STILL NEED per-module confirmation BEFORE
  the heavy entities** (Building ~1567 module-added, Unit ~1031) — do this early in Tier B/before the monsters.
- **Encoding = game-loadability VALIDATION** (Civilization): the JSON writer uses the DEFAULT (game-matching)
  encoding ON PURPOSE — if the toolkit can't encode a string, the game's loader can't load it either, so an
  encode error surfaces BROKEN content to DROP (a modder's Old-Norwegian special-char city names), not to mask
  with utf-8. `cityNames` stay (integral to founding); only the un-encodable names are filtered (`_encodable`).

## STATE — Tier A DONE (12/12), all committed on `json-data-migration`

GameSpeed · Handicap · Era · Process · Victory · Vote · CultureLevel · Hurry · BonusClass · CivicOption ·
Property · Civilization. Latest commit `4ca436fc`. Each its own bespoke/thin curator under `Tools/Migration/`;
each JSON under `Assets/Data/<plural>/`. **Property is a FIRST PASS** (memory `properties-first-class`): decay =
modifiers (toward `targetLevel`), effect-buildings = `grants` (pure list; the `requires` value-band is the
building's, Building pass), `targetLevel` isolated, leaking (operationalRange + diffusion + ChangePropagators) →
[#429](https://github.com/Stones2Stars/S2S/issues/429); the property SYSTEM (C++ + heavy Python) gets reworked to fit the data — a 2nd pass is expected.

## TOOLKIT state (`Tools/Migration/`)

- `store.py` — four enables-family indexes (`enabled_by`/`obsoletes_of`/`replaces_of`/`disables_of`); zWIP filter;
  PropertyInfo registered. **TODO at Tech:** retain child tech `AndPreReqs`/`OrPreReqs` as `requires.build.all`/
  `.any` (store currently flattens both into `enables.techs`, losing AND/OR — the tech-tree multi-parent fix;
  see enabler-spec §13.8). It's curator-side (`curate_tech` reads the child record), not a store reverse-index.
- `curate_common.py` — emits the four `enables`-family sections + the flat modifier families (v3). Thin curators
  ride it (`curate_process`, `curate_civicoption`); bespoke curators for the rest.
- `migration-renames.md` — manual/semantic renames logged per entity (mechanical de-Hungarian is the uniform
  `engine.de_i`/`FIELD_RENAME` maps, not re-logged). Keep adding per entity.

## NEXT — Tier B (top-of-cascade sources; HEAVY)

Per the ranking: **Tech (#13 — the SPINE ROOT, ~943 records)** · Civic · Religion · Corporation · Trait. These
were curated pre-reset (✅) — RE-VERIFY each against v3 + the drop re-check + the EXE-link check, exactly as Tier
A. Tech specifics: the `requires.build` retrofit above; tech modifiers are DOWNWARD `enabled`-deposits authored
ON the tech (§0.4/CREST), NOT inverted onto targets; `FreeSpecialistCounts` → `freeSpecialists` (Python-live, not
dead). Then Tier C (Bonus/Route/Terrain/Feature/Improvement/Build) · D (Specialist/Heritage/Project/PromotionLine/
Promotion/UnitCombat/LeaderHead) · E (the monsters: SpecialBuilding→**Building**, SpecialUnit→**Unit**, LAST).

## Pointers

- Specs: `modifier-cascade-spec.md` (v3) · `enabler-cascade-spec.md` (v0.3 + §6.1 + Uniformity Law).
- Order: `migration-entity-ranking.md`. Renames: `migration-renames.md`. Plan: `building-cascade-conversion.md`.
- Prior handoff: `handover-2026-06-15-pm.md` (the locked structures going in).
- Despair (DEFERRED — owner, needs real archaeology, "for another time"): a "**4 (5?) combat calculators**"
  entry was drafted then PULLED to get the facts straight. They were present at the START of the rework and
  discussed in one of the FIRST sessions — but **this predates the established git routines**, so the git history
  will NOT reliably capture it (the calcs couldn't be found in-tree). **The primary record is the EARLY-SESSION
  CONVERSATION TRANSCRIPTS**, not `git log` — start the archaeology there. Threads to pull:
  separate combat calcs for **unit build-worthiness**, **stack strength**, the **AI attack go/no-go heuristic**
  (`AI_attackOdds` returns a loss-ratio "goodness", not a win-%), and a **purely-visual `CvGameTextMgr`
  recompute** that re-derived odds just to draw the bar; the **OOS came from two MP clients disagreeing on what
  the AI wanted** (not async fighting); the new binomial `getCombatOdds` (`CvCombatModel`) consolidated them and
  was fixed EARLY. Get the count + specifics right before it goes in the index.

## Git state

Branch `json-data-migration`; Tier A all committed (12 infos + the foundation/toolkit + the zWIP filter). Working
tree clean except this handoff + the despair entry (commit them docs-only). Docs-only may go to `main`, but these
sit with the migration on the branch.
