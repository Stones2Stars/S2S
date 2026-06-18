# Session handover — 2026-06-14 (PM marathon)

**Read this first to resume.** Self-contained on purpose (context compaction has burned us before).
Branch: **`json-data-migration`** (working tree clean unless noted). All commits below are on that
branch EXCEPT the index pages, which were also published to **`main`** (hosted webpages).

## What landed this session (committed)

- **Trait (390) MIGRATED — "Mount Doom" done** (`b6eafc64`). One `CvTraitInfo` class serves both
  trait systems; the vanilla↔complex link is the `ReplacementID`/`CvInfoReplacements` conditional
  whole-Info swap (the workflow missed it; `store.py` was Frankenstein-merging it → now
  ReplacementID-aware: split into base + `TRAIT_COMPLEX_*` + a `replacedBy` edge). Full rulings in
  the §7 "Trait" note of `building-cascade-conversion.md`; analysis in
  `Tools/Migration/classifications/trait-classification.json`. Riders: store trait/tech prereq
  edges, `curate_bonus.py` BonusHappiness fold, tech `workerSpeed`→`workRate`.
- **Lighter-four CLASSIFIED + saved** (`a1c7ab7c`, `classifications/light-four-classification.json`)
  — **NOT curated yet.** All four are SOURCES (not POCOs), each with a nuance: SpecialUnit
  (combat/withdrawal land on the *loaded cargo unit* — a unit-as-target gap), CultureLevel (config
  entity; threshold-collapse assumes a not-yet-existing GameSpeed field), Vote (new policy-family
  vocab + 2 enabler edges), Civilization (grants at 3 scopes + hand-written read). Curate from the
  saved classification.
- **Combat-class system AUDITED** (`docs/dev/reference/unitcombat.md`, commits `ff2046e5`,
  `321b131c`) + issues **#434** (swapped vs-class/vs-type UI labels) and **#435** (the rework).
  Key finding: the class-vs-class matrix is vestigial (18 edges / 14 of 418 classes; 96% inert
  tags); the real strength logic is 966 per-unit edges (54×) summed additively into one
  indistinguishable number — nobody, not even the engine, can trace a combat number's origin.
- **Equipment-tier unit-combat purge DONE** (`02d0312c`): removed 179 dead
  `UNITCOMBAT_ATTACK_FORM_*`/`ARMOR_*`/`WEAPON_*`/`SHIELD_*` from base (636→457). Gated:
  0 engine attributes, 0 C++ name refs, 0 dangling. **This was the careful redo after a blunt
  purge over-reached twice and was fully reverted** (`dccf8e5b`, `ccf0b4db`).
- **Indices** — Despair (#6 "Strength by Committee", #9 "The Wonder That Builds Character",
  #10 "The Trait in a Trenchcoat"), Realism (#7 "You Are What You Build", #8 "Cultural Exports"),
  and a **new Complexity Index** (one entry: the whole codebase). Cross-linked. **Published live to
  `main`** (latest `ba489a5c`).
- **Issue #433** — AI never founds space cities (`AI_foundValue` scores raw pre-terraform yield).

## ⚠ Hard-won lessons (do not relearn)

- **NO ENTITY IS TRULY A POCO — it can only pretend to be (owner rule, 2026-06-14).** A 0-channel
  first-pass mapping is a FILTER, not proof of "data-holder." Verify every "POCO" against
  `getDataMembers` + the live consumer. Proven: the lighter-four are all sources; SpecialBuilding
  (curated as a POCO) is a building-group with a per-player cap. **Re-verify the 6 already-fast-pathed
  POCOs** (BonusClass/CivicOption/Hurry/Victory/PromotionLine/SpecialBuilding) before trusting them.
- **"XML-unreferenced ≠ dead"** for combat classes (and likely other entities). Two blind spots a
  by-name scan misses: (1) **module-inactive content** (a module's class looks unreferenced because
  the module's own units aren't active in the scan — punk/cultures `UNITCOMBAT_CULTURE_*`);
  (2) **engine attribute-matches** — the engine selects classes by `getEra()`/`getReligion()`/
  `getCulture()` *in code* (`doSetUnitCombats` `CvUnit.cpp:26224`; religion `:30950/:30969`), plus
  globals `getUNITCOMBAT_X()`. Any class with `<Era>`/`<Religion>`/`<Culture>` or a global is LIVE
  despite zero XML refs. Gate every future purge on these.
- **Index pages → `main` via a FULL worktree** (`git worktree add <path> main`, NOT `--no-checkout`
  — that leaves the index empty and stages a delete-everything). Always verify the staged diff is
  exactly the intended files before commit/push. Never switch the primary working tree off
  `json-data-migration` (owner builds from it).
- **No author call-outs** in the index pages (Despair/Realism/Complexity). Dev reference docs may
  cite code comments factually.
- **CRLF gotcha:** Python `str.replace` with `\n` anchors silently no-ops on these CRLF files (it
  bit the Despair #6 HTML insert). Use the Edit tool (handles EOL) or single-line anchors.
- **Module-merge into core is NOT needed** — `store.py` already globs base + `Assets/Modules` and
  merges by Type for every curator (v1 replace-by-tag; list-append/load-order are deferred). The
  migration bakes modules into the JSON at curation time. (Owner confirmed.)

## ⏭ SpecialBuilding — re-curate with the Building pass + rename audit (NOT a POCO)

`SpecialBuilding` was curated as a **POCO** (`curate_pocos`, text/identity/art) — **wrong; it has
gameplay.** `CvSpecialBuildingInfo` = a **building GROUP with a per-player build cap**:
`getMaxPlayerInstances()` + `TechPrereq`/`TechPrereqAnyone`/`ObsoleteTech` + `bValid`; the cap is
enforced via `CvPlayer::isBuildingGroupMaxedOut`/`getBuildingGroupCount`/`changeBuildingGroupCount`
(`CvPlayer.cpp:13784-13867`). So "can't hoard 8 university wonders" = the university wonders share a
SpecialBuilding with `MaxPlayerInstances=1`. **Heterogeneous uses (owner):** wonder one-per-group
exclusion; the **punk/culture building-group** case (`SPECIALBUILDING_PUNK_BIO` etc. — the
buildings/culture situation); and the **World Bank** doing something. The store already inverts its
`TechPrereq`/`TechPrereqAnyone` → `enables.specialBuildings`. **Action:** re-curate as a
building-GROUP concept **with the Building pass** (it belongs there, not as an early POCO); the
`SpecialBuilding → buildingGroups` rename **needs its own audit** of the heterogeneity first.

## Queue (fresh-context order)

1. **Combat-class rework (#435)** — unbundle the 3 conflated roles (keep ~10–20 combat arms for
   matrix + promotion gating; split orthogonal taxonomies size/species/motility/heal into
   attributes; keep engine classes; collapse 966 per-unit edges into one matrix). + fix #434 labels.
2. **SpecialBuilding audit + re-curation** (with the Building pass; → buildingGroups rename).
3. **Lighter-four curation** (from `light-four-classification.json`).
4. **Remaining heavy entities:** Improvement · Terrain · Feature · UnitCombat · Promotion · Build ·
   Property · LeaderHead → **Building, Unit LAST** (per the migration order ruling).

## Pointers

- Migration model + per-entity rulings: `docs/dev/plans/building-cascade-conversion.md`
  (read the "⇒ CHECKPOINT & RESUME" + "THE MODEL" sections).
- Combat-class reference + audit: `docs/dev/reference/unitcombat.md`.
- Classifications (resume analyses): `Tools/Migration/classifications/`.
- Toolkit: `Tools/Migration/` (`store.py`, `curate_common.py`, bespoke `curate_*.py`).
- Open issues: #433 (space founding), #434 (swapped combat labels), #435 (combat-class rework).
