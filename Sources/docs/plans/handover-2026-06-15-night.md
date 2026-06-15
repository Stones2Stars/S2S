# Session handover — 2026-06-15 NIGHT (TIER B COMPLETE + module inclusion confirmed)

**Read this first to resume.** Self-contained. This session **executed all of Tier B** (the top-of-cascade
sources, infos #13–#17), one info at a time, each owner-reviewed and committed, and **resolved the per-module
inclusion question** (the gate before the monsters). A large amount of MODEL LAW was locked along the way (below).
**Next is Tier C (resources & map substrate), starting with Bonus (#18).**

**Resume reading order:** (1) this handoff; (2) the two LOCKED specs — `modifier-cascade-spec.md` (v3, now with the
`each`/`anyOf` `per` forms + the SELF token) + `enabler-cascade-spec.md` (v0.3, now with §3 **object-evaluated
predicates** + the `noneOf`/build-negative + the grants-pulse note); (3) `migration-entity-ranking.md` (the curation
order); (4) `migration-renames.md` (the per-entity old→new registry — has Tech/Civic/Religion/Corporation/Trait now).

---

## ⛔ THE PROCESS — non-negotiable (owner rulings, reinforced this session)
- **STRICTLY SERIAL, one info at a time.** curate → `--write` → **OWNER reviews & PASSES** → only then commit → next.
- **PRESENT before COMMITTING, ASK before introducing a mechanic/token/shape** — every change incl. corrections, not
  just the first cut. "It's in the vocab already" / "it follows from a ruling" ≠ pre-approved. (The SELF-in-`requires`
  incident: I added it + committed without asking; owner pushback. memory: `ask-before-new-mechanic`.)
- **The OLD generated JSON is NOT a correctness baseline** — only the OLD XML (`Assets/XML`) is, with the live
  C++/Python consumer to learn meaning. "byte-identical to committed" only proves a shared-code edit didn't touch
  OTHER entities; it says nothing about correctness. (memory: `old-json-not-a-baseline`.)
- **Verify** = structural adherence to the locked v3/v0.3 shapes + ZERO invention + reading the live consumer.
  EXE-link check per entity (grep `DllExport` on the `Cv*Info.h`). Drop re-check vs `Assets/Python` AND intent.

## STATE — Tier A (12) + Tier B (5) DONE, all committed on `json-data-migration`
**Tier A:** GameSpeed · Handicap · Era · Process · Victory · Vote · CultureLevel · Hurry · BonusClass · CivicOption ·
Property · Civilization. **Tier B:** Tech (#13) · Civic (#14) · Religion (#15) · Corporation (#16) · Trait (#17).
Each JSON under `Assets/Data/<plural>/`. **Module inclusion CONFIRMED** (store committed).

## ⛔ MODEL LAW locked/extended this session (all in the specs + memory)
- **OBJECT-EVALUATED PREDICATES (enabler-spec §3).** Conditions needing runtime object state the static Info can't
  hold are a single conditional `{PREDICATE: param}` (or `{PREDICATE: true}`); the **object (city/player) self-reports**;
  the engine's predicate owns any compound logic. Vocab: `HAS_RELIGION:R`, `STATE_RELIGION:R`, `HOLY_CITY:R`,
  `IS_CAPITAL:true`, `HAS_CORPORATION:C`. Used inside `requires`/`enabled`/`disabled`. (Civic `capital`/`stateReligion`
  retrofit to these LATER; many capital bonuses in Trait+Civic.)
- **`SELF` token (modifier-spec §1.4).** The entity-relative catch-all = "the owning entity's own type." In `per` =
  count of own type; in a `requires` atom = this same entity. Canonical use: global-uniqueness
  `requires.build.noneOf:[{type:SELF,scope:world}]` — tech `bGlobal` (religion-founding-once) + world wonders.
  It was already vocab; reusing it in `requires` is what I should have ASKED about first.
- **`requires` NEGATIVE = `noneOf`** (enabler-spec §3; ≡ the older `disableIfAny`). Can sit in `requires.build` for
  one-time RACE/uniqueness gates, not just operate/dormancy.
- **`per` gains `each` (the quantum: "per how many of type"; effect = value×(count/each)) and `anyOf`** (sum over a
  SET of types) — modifier-spec §4. Property-source attribute scaling is always `each:1`.
- **`grants` carries one-time NUMERIC PULSES, not just entity lists** (enabler-spec §6) — Civic `iRevIdxSwitchTo` →
  `grants.revolution` (a signed burst on civic-switch); the shape the OUTCOMES system will use for one-time yields.
- **Shared `engine.property_source_v3`** (THE property-source standard) — every entity's `PropertyManipulators` →
  `<PROPERTY>.<scope>.<unit>` (CONSTANT→flat, DECAY→percent, attribute-scaled→`flat:{value,per:{type,each,scope}}`,
  scope from GameObjectType, RELATION_ASSOCIATED dropped). Replaced the bespoke `perTurn/mult` shape in
  Property/Civic/Religion/Trait. Fixed a committed Property #11 bug (ATTRIBUTE_CONSTANT pop-scaling was dropped).
- **Corporation = the Religion model** (FIRST PASS — needs a rework pass): founding makes the HQ building, corp
  SPREADS like a religion; per-city effects gated by `enabled:{HAS_CORPORATION:SELF}`, per-bonus output
  `per:{anyOf:[prereqBonuses]}`. PrereqBonuses = scaling basis + HQ-building `requires.build` (Building pass), NOT a
  corp `requires` and NOT an enabler edge (store rows dropped). HeadquarterCommerces/spread deferred.
- **Trait simple/complex SPLIT into two folders** `Assets/Data/traits/{simple,complex}/` (88/302). They are "2
  completely different traits hacked on top of each other" (TB) — `replacedBy` hack-link DROPPED; complex set is
  ENTIRELY the Thunderbrd module; complex traits become their OWN Info type behind a shared interface in the CODING
  pass (#430). Classifier: complex iff a ReplacementID variant OR `GAMEOPTION_LEADER_COMPLEX_TRAITS`-gated.

## MODULE INCLUSION — CONFIRMED (gate before Bonus/monsters; memory `module-inclusion-case-by-case`)
`store.EXCLUDED_MODULE_SUBPATHS = ["/modules/zwip/", "/modules/bad_karma/", "/modules/p2k_multimaps_test/"]`.
- **INCLUDE (bLoad=1):** Cultures (1618), Pepper2000 (1136), Thunderbrd (473), Alt_Timelines (217), NotSoGood (131).
- **EXCLUDE:** zWIP; Bad_Karma (all content subs bLoad=0); P2K_Multimaps_Test (bLoad=0; its 92 "space" units are a
  100% DUPLICATE of loaded Pepper2000 units — space content stays via Pepper2000, net roster change zero).
- Authority = `Assets/Modules/MLF_CIV4ModularLoadingControls.xml` (config `Modules_Main_1` + nested MLFs).

## NEXT — Tier C (resources & map substrate)
Per the ranking: **Bonus (#18)** · Route (#19) · Terrain (#20 ☐) · Feature (#21 ☐) · Improvement (#22 ☐) ·
Build (#23). Bonus/Route were curated pre-reset (RE-VERIFY vs v3 + drop re-check + EXE-link, exactly as Tier B).
- **Bonus (#18):** ~45% are CULTURE intermediary bonuses (all the Cultures module's 399) → `Assets/Data/bonuses/
  cultures/`; migrate the building→bonus→building chain faithfully, collapse in the post-migration purge.
  `TechReveal`/`TechCityTrade` (tech→reveal/trade); `TechObsolete`. **DROP the `buildRate` fold from `curate_bonus`**
  (the un-folded `BonusProductionModifiers` is authored on building/unit/project per §6 keep-on-source). Switch its
  `_properties` (if any) to the shared `property_source_v3`. A resource is never a target — only `enables`/amplifies.
- Tier D (Specialist/Heritage/Project/PromotionLine/Promotion/UnitCombat/LeaderHead) · E (the monsters:
  SpecialBuilding→Building, SpecialUnit→Unit, LAST).

## DEFERRED NEIGHBORS (don't lose these)
- **Building pass owes:** corp HQ `FoundsCorporation` `requires.build` (the corp's PrereqBonuses found-req) +
  GlobalCorporationCommerce HQ amplifier; Religion **shrine** (`GlobalReligionCommerce × countReligionLevels`,
  world-scaled, routed through a shrine building — religion files carry a parked `shrine` section of raw values).
- **Coding pass (#430):** complex-trait NEW Info type behind the shared trait interface; the object-predicate
  engine evaluation; the `each`/`anyOf`/`noneOf` branches; corp rework pass (HeadquarterCommerces perCorporationLevel
  + spread); civic `capital`→`IS_CAPITAL` + `stateReligion` predicate retrofit; property-system second pass.
- **P2K units:** if any truly break the Unit pass, that's where to look (excluded now; Pepper2000 is canonical).

## Pointers
- Specs: `modifier-cascade-spec.md` (v3) · `enabler-cascade-spec.md` (v0.3). Order: `migration-entity-ranking.md`.
  Renames: `migration-renames.md`. Prior handoff: `handover-2026-06-15-evening.md` (Tier A complete).
- Toolkit: `Tools/Migration/` — `store.py` (module merge + enabler indexes + EXCLUDED_MODULE_SUBPATHS),
  `engine.py` (`property_source_v3` + vocab), `curate_common.py` (shared core + `requires_fn`/keyed-container),
  per-entity `curate_*.py`.

## Git state
Branch `json-data-migration`; Tier A + Tier B (#13–17) + the module-confirmation all committed. Working tree clean
except this handoff (commit it docs-only with the migration on the branch).
