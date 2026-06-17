# Session handover — 2026-06-16 PM (Feature #21 DONE; ART ui/world/sound restructure STARTED — finish it FIRST)

**Read this first to resume.** Self-contained. This session finished **Feature #21** and then began a big
**art restructure** (flat `art` → top-level `ui`/`world`/`sound` blocks) that is **only half-applied** — that is the
**immediate next task** (below). A large amount of MODEL LAW about **modular systems / isolated surfaces** locked
this session.

## ⛔ READ-FIRST GATES (unchanged — honor every resume)
1. **READ ALL THE DOCS before touching anything** — both LOCKED specs (`modifier-cascade-spec.md` v3 +
   `enabler-cascade-spec.md` v0.3), `building-cascade-conversion.md`, `migration-entity-ranking.md`,
   **`migration-renames.md`**, the entity's `classifications/*.json`, and the prior handovers. Look concepts UP; never
   reconstruct from memory or from the C++ (it is reworked to fit the data, NOT ground truth). A spec line that
   conflicts with a later owner ruling is stale — the ruling wins.
2. **OWNER VISUALLY INSPECTS the written JSON + EXPLICITLY APPROVES before EVERY commit.** Strictly serial, one info
   at a time. PRESENT before committing; ASK before introducing any mechanic/token/shape. VERIFY facts in the code,
   never assume (this session that caught: `iEffectProbability` is cosmetic art, NOT the suspected gameplay; empty
   audios default in the EXE so they're droppable).

## STATE — Tier C through Feature #21, all committed on `json-data-migration`
Bonus #18 · Route #19 · Terrain #20 (+ river/hills-peak follow-up) · **Feature #21** ✅. Latest commit `8f0f3af6`.
Working tree clean except this handover. **3429 `Assets/Data/**/*.json` parse.**

## ⛔⛔ IMMEDIATE NEXT TASK — FINISH THE ART RESTRUCTURE (owner: do this before "whatever is next")
The art model was redesigned this session (owner 2026-06-16). **New shape — top-level `ui`/`world`/`sound` blocks,
with `art` a SUB-block of ui/world** (so non-art members sit beside it):
- `ui.art.icon` (UI icon ← `Button`/`Texture`), `ui.art.movie.{file,sound}` (video+sound TOGETHER, UI-only),
  `ui.art.{techButton,genericTechButton,advisor,fontButton,tgaIndex}`, `ui.{hotkey,altDown,ctrlDown,shiftDown,hotKeyPriority}` (non-art key triggers)
- `world.art.icon` (on-map graphic ← `ArtDefineTag`), `world.art.effect.{type,probability}` (cosmetic effect+chance),
  `world.art.{style,unitStyle}`
- `sound.{footsteps,soundscape,growth,construct,sound,soundMP,action,selection,unitVictory,unitDefeat,soundtracks,citySoundscapes,soundtrackSpace,diplo*}` — **FLAT** (audio is itself the asset)
- **Empty/`NONE` audio is DROPPED** (`curate_common.drop_empty_audio` — empties default to -1 in the EXE, verified
  `CvXMLLoadUtilitySet.cpp:2434`). `UniqueNames`→identity, `fVisibilityPriority`→leave-as-is (Building, unmigrated).

**The canonical mapping is `curate_common.ART_BLOCK` (tag → dotted path).** `cc.curate` routes art through it via
`_set_path(art_blocks, ART_BLOCK.get(tag, "ui.art."+de_i(tag)), av)` and emits top-level `ui`/`world`/`sound`.

**⚠ WHAT'S DONE vs NOT:** the `cc.curate`-based entities are CONVERTED (tech, bonus, route, terrain, feature,
process, bonusclass, civicoption, + any thin `EntityConfig` curator). **The BESPOKE curators build `art` themselves
and STILL emit flat `art`** — they need re-aligning to `ART_BLOCK`. Bespoke list to fix (each has its own art
handling, e.g. `curate_religion.py:110` `art[ART[tag]] = v`): **religion, civic, trait, specialist, civilization,
era, gamespeed, handicap, victory, vote, culturelevel, hurry, corporation, project, heritage, property, pocos.**
- **HOW:** add a shared helper to `curate_common` (e.g. `put_art(art_blocks, tag, val)` wrapping
  `_set_path(...ART_BLOCK...)` + `drop_empty_audio`), and in each bespoke curator replace its local `art`-dict +
  `out["art"]=art` with the `ui`/`world`/`sound` emit (mirror `cc.curate` lines ~278 + ~338). Re-render each, owner
  inspects, commit. Then `git grep '"art"'` the curators to confirm none remain.
- Watch: some bespoke curators have a local `ART` rename map (e.g. religion) — fold those tags into `ART_BLOCK` (add
  any missing, e.g. religion `Adjective`→text already, `iTGAIndex` present). Era soundtracks/handicap/victory etc.
  art fields are already in `ART_BLOCK`.

## ⛔ MODEL LAW locked this session (all in the specs)
- **DELIVERYGUY / SEMANTIC-SENSE ownership (modifier-spec §6.1):** "who BRINGS the modifier to the table?" owns it;
  the root test is *what makes semantic sense*. A thing ON A PLOT owns its own modifiers. TWO first-class tools —
  keep-on-source (conditioned via `enabled`/`per`) vs fold-onto-the-owner; semantic sense picks.
- **DEDICATED BLOCKS / MODULAR SYSTEMS (modifier-spec §0.8):** system-specific data lives in ITS OWN block (vision,
  global-warming-object, each `PROPERTY_*`, the `ui`/`world`/`sound` art blocks). **Goal: modular systems with
  isolated surfaces** (add/swap/remove a system as a unit). **CAVEAT:** *produced* data (tile yields) stays where
  it's produced, tagged by a system PREDICATE (river yield on the terrain, `enabled:HAS_RIVER`). **Universal** — any
  concept defined as a system gets this.
- **PREDICATES (enabler-spec §3):** organized BY SYSTEM (each a system's isolated query-surface); each SYSTEM
  documents its OWN predicates; **a missing predicate is IGNORED (dropped from eval), NOT `false`** (`inactive ==
  ignored` — else `enabled:MISSING`→false would spuriously disable a deposit). **Bare-string shorthand:** a
  parameter-free predicate is a bare string desugaring to `{PREDICATE:true}` (`HAS_RIVER`, `IS_CAPITAL`);
  parameterized ones stay objects (`{HOLY_CITY:RELIGION_X}`); negation via the `disabled` twin. **New `HAS_RIVER`
  plot-state predicate** (first used by Terrain river bonus + Feature `RiverYieldChange`).
- **VICINITY precise (enabler-spec §8):** the plots a city CURRENTLY reaches (1→2→3 rings, grows with culture);
  per-city + overlapping (1 plot in 2 cities). Accepted consequence: slightly buffs natural wonders / plot-bonus
  builds (2 overlapping cities both qualify); clean barrier now, iterate later via a `workedBy:SELF` predicate
  (also needed at Buildings — buildings scaling by worked tiles).
- **PLANNED ALIGNMENT — Phase F (ranking, HARD GATE):** after ALL infos migrated and BEFORE #430 parsing, sweep back
  to align every info to latest conventions (predicate modularity for religion/corp/traits + any system; the art
  blocks; family names). The art restructure above is this gate brought forward.

## Terrain #20 river model (committed; recap for context)
River = a plot edge-attribute, NOT an entity. The +1 commerce base river bonus lives ON THE TERRAIN as
`commerce.plot.flat[].{value:1, enabled:"HAS_RIVER"}` on the 19 river-capable LAND terrains (EARTH ∧ ¬AQUATIC +
HILL/PEAK). Hills/peak plot-yields moved off `YieldInfo` onto `TERRAIN_HILL`/`PEAK`. Feature `RiverYieldChange`
compounds on top. `YieldInfo` (its other fields) is a small later curation.

## GLOBAL WARMING — scrapped mod, issue #436
`GLOBAL_WARMING` is `// #define`d out (compiled out). `iWarmingDefense` DROPPED from Feature (a future GW mechanic
gets its own base object). Vestige-removal scope + concept captured in `global-warming-mod.md`; issue
**Stones2Stars/S2S#436**. (`global-warming-mod.md` is committed on this branch — reaches `main` when #428 merges.)

## NEXT — after the art alignment is finished
Per the ranking: **Improvement #22** (heavy plot-leaf; ≡ its Build; run a `classify-improvement` workflow like
terrain/feature) · **Build #23** · then Tier D (Specialist done; Heritage done; Project done; PromotionLine →
Promotion · UnitCombat · LeaderHead) · Tier E (SpecialBuilding→Building, SpecialUnit→Unit, LAST). Then Phase F.

## Pointers
- Specs: `modifier-cascade-spec.md` (v3, now §0.8/§6.1) · `enabler-cascade-spec.md` (v0.3, now §3/§8). Order +
  Phase F: `migration-entity-ranking.md`. Renames (has §Terrain/§Feature): `migration-renames.md`. Art deferral +
  the icon-headache resolution: `building-cascade-conversion.md` "DEFERRED — ART SUB-BLOCKS".
- Toolkit: `Tools/Migration/` — `store.py` (now registers Terrain/Feature/YieldInfo), `curate_common.py`
  (`ART_BLOCK`, `drop_empty_audio`, `post_process` hook, `_set_path`), per-entity `curate_*.py`,
  `classifications/{terrain,feature}-classification.json`.
- Prior handover: `handover-2026-06-16.md` (Bonus #18 / start-of-day).

## Git
Branch `json-data-migration`; everything committed (`8f0f3af6`). Commit this handover docs-only. **Resume: finish
the art alignment (bespoke curators) FIRST, owner-inspect+commit, then Improvement #22.**
