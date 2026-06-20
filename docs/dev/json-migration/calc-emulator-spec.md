# Calc emulator — spec (external old-vs-new value-calculation harness, #430)

> **STATUS: spec / plan (owner 2026-06-19).** Graduated from
> [`modifier-cascade-shadow-spec.md`](../reference/cascade/shadow.md) §3.1a (the "TOOL TO BUILD" / "★ SCOPE
> EXPANDED" rulings), promoted to its own doc per that section's directive and the owner's read that **the more of
> the cascade we build, the more we will need this** — it is the long-lived integration/validation backbone, not a
> one-shot calculator. The city-yields **SEED exists**: `Tools/ModifierCalc/modcalc.py`. This doc is the scope +
> build order for growing it into the full emulator.
>
> **It emulates BOTH calcs (owner 2026-06-19): the legacy one (which exists) AND the new cascade one (which does
> NOT fully exist yet).** That second half is the point of building it as a spec, not just a comparator — see §0/§1.
>
> Companions: [`modifier-cascade-spec.md`](../reference/cascade/modifier.md) (§2 arithmetic, §9 demolition list — the calcs
> this maps), [`modifier-cascade-shadow-spec.md`](../reference/cascade/shadow.md) (the IN-GAME shadow — the live
> twin; §4 care scale), [`cascade-engine-430.md`](cascade-engine-430.md) (§4 demolition map, the DESTROY pass),
> [`../reference/http-server.md`](../reference/observability/http-server.md) (the `/diagnostic/*` mailbox pattern the new endpoint copies).

---

## 0. Why this exists — four masters, load-bearing (owner 2026-06-19)

Legacy's value arithmetic was **never deliberately designed** (it accreted, with *"modifiers flow top down"* the
only intentional principle); the `/diagnostic/modifier` pilot surfaced this. The cascade is to be the **first
deliberate design of the formula** — but **we do not have that new full calc yet.** Hence the emulator serves four
masters at once, and it is worth building once, properly:

1. **The DESTROY-pass MAP — map-before-delete, applied to CALCULATIONS (the Orwell bar for value calcs).** You
   cannot safely delete a legacy calc you have not fully mapped. The §9 demolition list (modifier-spec) names the
   legacy read-points the cascade deletes; an external emulation that reproduces each of those calcs **is** that map.
2. **The BIRTHPLACE of the new calc — design it HERE FIRST, then port (owner 2026-06-19).** The new full cascade
   calc (multiplier composition, the deliberate corrections, parity-adjacent values) **does not exist in the DLL
   yet.** We **nail it in the emulator** — offline, in fast Python iteration, against real data — and **only then
   port the validated formula into C++03.** Porting a formula that is already proven offline is **mechanical, not
   exploratory**: the hard part (does this arithmetic produce sane, parity-adjacent numbers across real loadouts?)
   is answered before a line of DLL code changes. This inverts the usual direction for the *new* side — for the
   legacy side the engine leads and the emulator maps it; for the new side **the emulator leads and the DLL
   follows** (§1).
3. **The de-facto TEST HARNESS.** A closed-EXE legacy C++ game cannot carry real unit/integration tests; a
   real-data old-vs-new emulator is the closest equivalent we will ever get — the validation backbone, independent
   of the cascade (worth having regardless).
4. **The TUNING sandbox.** Explore where/how-much legacy and the new calc diverge and tune toward **parity-ADJACENT**
   (close enough that the played game stays recognizable — modifier-shadow-spec §3.1a), across real loadouts.

**Future upside (owner 2026-06-19):** the same engine can later power a website *"simulate my city"* premium
feature. Keep the formula core a clean, reusable library so that path stays open; do not build it now.

---

## 1. The core idea — a pure FORMULA COMPARATOR over TWO calcs

> **SCOPE IN ONE LINE (owner 2026-06-19, the real takeaway): we are modelling ALL the modifying calculations that
> run during `doTurn()`.** That is the whole boundary — the per-turn value computations the engine performs each
> turn (yields, commerce, maintenance, growth, health, happiness, great-people, property ±rates, the player-scope
> per-turn rates, …). Anything that is NOT a `doTurn()` modifying calc is out: cross-turn/time-evolving meta
> (win-for-losing, tech-diffusion — final-value modifiers), stateful accumulators + spatial propagation (the #429
> solver), stochastic events. If it modifies a value during `doTurn()`, the emulator models it; if it doesn't, it's
> someone else's job.

> **MODELLING CONVENTION — start from a ZERO accumulator (owner 2026-06-19).** When all is said and done, every
> `doTurn()` modifying calc ultimately ADDS its result to some accumulated value (a yield total, a property level, a
> treasury, …). For our modelling we treat the **accumulated value already there as 0** and compute purely the
> **per-turn CONTRIBUTION** — what this turn adds. This unifies the channels: a flow (yields) is recomputed fresh
> each turn anyway, and an accumulator (a property) we model as its **±rate from a 0 base** (matching the property
> rate-contribution scope, §3). The parity bar compares the *contribution*, never the standing total.

**Same input vector → legacy combine vs new (cascade) combine → delta. The delta is PURELY formula-attributable.**
Input parity is trivial: the inputs are just the raw contribution lists (base + the flat/percent/multiplier
sources), **identical for both sides** — so any output difference isolates the *combination logic*, the only thing
that differs. The tool does NOT simulate game state; it does arithmetic on input vectors. (modifier-shadow-spec
§3.1a: *"SAME INPUT to both formulas → the delta is PURELY formula-attributable."*)

The two calcs it emulates have **opposite authorities**:

- **The LEGACY calc — the engine LEADS, the emulator MAPS it.** `getYieldRate100`-shape:
  `(base + specialist) × (100 + Σpercent)/100 + Σflat` — specialist INSIDE the percent; building flats added OUTSIDE
  (the `extraYield` term). The accidental-but-actual old formula; the emulator's job is to reproduce it faithfully
  (the fidelity credential, §3a).
- **The NEW (cascade) calc — the emulator LEADS, the DLL FOLLOWS.** It does not fully exist yet. The DLL has only
  the swappable dispatch seed (`cascadeModifierApply`, `Sources/Cascade/CvCascadeModifier.cpp`) with two flows —
  `CALCFLOW_LEGACY_FLAT_OUTSIDE` (current: `base × (100 + Σpercent)/100 + Σflat`, matches legacy flat-placement so
  parity can reach zero) and `CALCFLOW_UNIFIED_FLAT_INSIDE` (deferred: `(base + Σflat) × (100 + Σpercent)/100 ×
  Π(mult/100)`, needs a data rebalance). The **full** new calc (which flow, what multiplier composition, what
  deliberate corrections, what values land parity-adjacent) is **designed and nailed in the emulator first**, then
  ported into that dispatch point. Adding/prototyping a flow = a function + a registry entry, mirroring the DLL's
  enum + `case` — but done HERE before there.

Integer math throughout (matches the engine; no floats — OOS determinism, engine-430 §5), so a formula nailed in the
emulator ports without float-vs-int surprises.

---

## 2. Two input sources — synthetic grid AND live game-dump

| source | what | use |
|---|---|---|
| **Synthetic grid** (`modcalc sweep`, **exists**) | a coarse grid over base × Σflat × Σpercent × source-mix | explore divergence across the whole input space; prototype a new flow fast; find worst-case adjacency gaps |
| **LIVE game-dump** (the locked loadout, owner 2026-06-19; **next build**) | a REAL city's full input vector pulled from a new endpoint, + that same city's live legacy & cascade outputs | validate the emulator against ground truth; tune against REAL loadouts ("all the fun things" — building/tech/civic/bonus sets), not just synthetic combos |

The live-dump is what makes this more than a calculator: it feeds the emulator the actual contribution lists a
running game produces, so the comparison is grounded in real data, and the emulator's legacy side can be checked
against the engine's realized number (§3a). The synthetic grid is where the **new** calc is stress-tested fast (§0.2).

## 2a. THE LOADOUT-FED CASCADE SIMULATOR — the full shape (owner 2026-06-19)

The emulator's endgame is bigger than a formula comparator: **feed the calculator a LOADOUT — a list of techs,
civics, buildings, and plots — and have it COMPUTE THE NEW CASCADE MODEL ITSELF** (reading the migrated `Assets/Data`
JSON deposits + applying the deposit-flow/combine), since we already know how the model is supposed to behave.
Then compare to the LEGACY model on the same loadout. *"We simulate the simulation"* — this Python model is the
**prototype of the in-game cascade engine** we port back later (§0.2), so building it is required anyway.

- **Loadout source — REAL or SYNTHETIC.** Real: the `cityInput` LOADOUT dump (techs/civics/buildings/plots +
  per-plot terrain/feature/improvement/route/bonus + yields). Synthetic: **fabricate a techlist by "simulating
  that techs were researched"** (and likewise civics/buildings/plotmaps) — arbitrary hypothetical states, not just
  the live game. The endpoint precomputes initial real loadouts for a few test cities to iterate on offline.
- **Input SHAPE = presence DICTIONARIES toggled yes/no (owner 2026-06-19).** The loadout is per-class dicts
  `{TYPE: bool}` — `techs` / `civics` / `traits` / `buildings` / `resources`(bonuses) / … — each entity
  enabled/disabled with a simple yes/no, plus the `plots` list. **Toggling a flag IS a synthetic state** (enable a
  tech ⇒ "simulate it researched"). **BOTH sides of the aisle consume the SAME dict and must each compute a valid
  output:** the NEW (cascade) model reads each PRESENT entity's `Assets/Data` JSON deposits; the OLD (legacy) model
  its contributions. For REAL states the legacy side is the live dump (already have it); building the legacy side
  OFFLINE (from the pre-migration XML) is the bigger lift that unlocks **fully-synthetic** test cases + a **direct
  XML-vs-JSON migration check** (same dict → both data sources → divergence = a parse/curation blindspot).
- **LIVE DATA IS NOT REQUIRED (owner 2026-06-19).** Because we model the per-turn CONTRIBUTION from a ZERO
  accumulator (§1), the calculator is **self-contained**: a loadout (presence dicts, SYNTHETIC or real) + the JSON
  deposits is all it needs. Live dumps are for *validating* the legacy reproduction and for *convenient* real
  loadouts — **not a dependency.** The new calc + the parity sweeps run **fully offline on fabricated loadouts**.
  **Still worth keeping real data as a SAMPLE SOURCE (owner 2026-06-19):** the saved fixtures (`samples/*.json`)
  give realistic loadouts + base values to model synthetic states *from* (more representative than hand-rolled).
- **PLOT data = the BASE YIELD (owner 2026-06-19).** The per-plot yields in the loadout are the **base yield at that
  point in time** — the base-value input the modifiers apply to (the "100's" of the acceptance bar, §3). **Plots
  supply the BASE; buildings/civics/techs supply the MODIFIERS**, and the calc applies the latter to the former.
  Real plot data therefore lets us model **realistic base inputs** (actual base-yield distributions) rather than the
  toy "100 of each" — better-grounded parity sweeps.
- **YIELD BASE — the source enumeration, fully TRACED & VERIFIED (owner 2026-06-19 + a full code trace).** The
  owner's recollection was *trade routes / plots / buildings / specialists* (the majors) with "unless I forgot
  something" — and the trace of `getYieldRate100` → every leaf (CvCity.cpp:11246 / `getBaseYieldRate` :22906 /
  `getExtraYield100` :11326; `CvPlot::calculateYield` :8320) confirms it's broader. "How much from each" stands as
  the model; the verified buckets are:
  `getYieldRate100 = (getBaseYieldRate + getSpecialistYieldTotal) × modifier + 100 × getExtraYield`
  - **MODIFIED (receives the %):**
    - **plots** (`getPlotYield`) — itself a deep tree: terrain base + bonus + feature/river + IMPROVEMENT
      (base / riverside / irrigated / route / tech / civic / bonus-synergy / player / team) + route + city-site +
      player-terrain + sea-plot + landmark + thresholds + plot-golden-age;
    - **trade routes** (`getTradeYield`);
    - **free city yield** + a **city-level GOLDEN-AGE yield** term (player/trait-level, GA-gated) —
      `getBaseYieldRate` = plot + trade + freeCity + `(isGoldenAge() ? getGoldenAgeYield : 0)` (CvCity.cpp:22910).
      **NB this is DISTINCT from the per-plot golden-age +1** (`CvPlot::calculateYield` :8403, already in the plot
      tree above): there are **TWO golden-age yield paths** — the per-plot bonus (the "+1 on all plots" recollection,
      correct) flowing through `getPlotYield`, AND this separate city/trait term — both fire only during a golden age.
      **NEW-MODEL DESIGN (owner 2026-06-19): collapse both legacy paths into ONE clean form — an EMPIRE-LEVEL modifier
      gated by an `isGoldenAge` BOOLEAN toggle on top.** The cascade represents golden-age yield as a single
      empire-scope deposit enabled by the golden-age state flag (just another loadout boolean, §2a), not two
      inline-gated legacy paths — a deliberate clean-up (a `Better`), and a clean fit for the presence/toggle model;
    - **specialists** (`getSpecialistYieldTotal`, modified like tiles — #317);
    - the **modifier** itself = bonus + building + event(city+player) + power + area + capital.
  - **UNMODIFIED (flat, ×100):** **building flat yields** + **tech-dependent building yields** (distinct) +
    **building per-pop yields** + **per-building yield-changes** + **corporations**.
  - **★ CORPORATIONS — EXCLUDED from the cascade model BY DESIGN (owner 2026-06-19): "a demon we do not want to
    add."** Model them as a **flat additive on top, cleanly, LATER** — never part of the modifier cascade. The trace
    confirms corp yield sits in the UNMODIFIED bucket today, which is exactly right: **modifiers must NOT buff corp
    yield** — doing so would make corporations "beyond turbobroken." So corp stays a deferred flat post-add, out of
    scope for the per-turn modifier model.
- **BOTH cascades, not just the modifier.** A loadout drives the **modifier** cascade (the per-turn VALUES, vs
  legacy `getYieldRate100` & co.) AND — crucially for synthetic techlists — the **enabler** cascade: feeding "these
  techs are researched" exercises the CAN-GET frontier (what those techs unlock), which **validates the enabler AND
  generates a clean enabler cascade** (vs legacy `canConstruct`/`canTrain`).
- **FOUR payoffs from one comparison (loadout → new model + old model):**
  1. **Test cases** — a loadout + its legacy outputs is a reusable test fixture (saved, iterated offline).
  2. **Parity-adjacency tuning + the new-calc DESIGN** (§0.2) — nail the flow/values offline before porting.
  3. **★ XML→JSON migration BLINDSPOT detection** — if the new model (reading the JSON) diverges from legacy in a
     way the formula choice doesn't explain, it points at a **deposit/edge legacy has that the migrated JSON LACKS**
     (a parse/curation gap). The comparison doubles as a migration-completeness checker. **DIVERGENCE TAXONOMY
     (owner 2026-06-19) — distinguish two kinds:** (a) **BLINDSPOT** = JSON missing a deposit legacy has → a bug to
     FIX; (b) **BY-DESIGN OMISSION** = a legacy mechanic the new cascade DELIBERATELY does not model → EXPECTED, the
     offline recalc simply won't have it. **Win-for-losing and process (production→research) conversion are
     by-design omissions** (the new model drops them); their legacy-vs-new diffs are correct, not blindspots. (Maps
     to the care scale: blindspot = `Bug`, by-design omission = `Better`/accepted — modifier-shadow-spec §4.)
     **SCOPE — the PER-TURN SNAPSHOT at various states (owner 2026-06-19).** What we model: research/gold/culture
     **per turn** + the host of **city-specific per-turn calcs & effects** — the per-turn snapshot of a given state
     (loadout). What we DON'T: cross-turn, time-evolving meta. Win-for-losing / tech-diffusion are exactly that —
     FINAL-VALUE meta-modifiers applied downstream on the finished total, only measurable by standing up a full
     multi-player playset and watching research diverge over many turns, and roughly invariant anyway. The cascade
     computes the per-turn base; those meta-adjustments ride on top unchanged, so emulating them buys nothing.
  4. **Clean enabler-cascade generation/validation** — synthetic techlists produce + check the availability frontier.
- **Offline iteration (the workflow win):** pull/fabricate a loadout ONCE (`--save`), then iterate the model with
  `--file` — no game restarts. The live dump is the ground-truth snapshot; the model-tuning loop runs offline.

**Build path:** (1) the `cityInput` LOADOUT dump — DONE (techs/civics/buildings/plots). (2) Pull a few real loadout
fixtures. (3) Build the Python cascade simulator: read loadout + per-entity `Assets/Data` JSON deposits → apply the
deposit-flow + combine → the new-model output. (4) Compare to legacy (dump) → test cases + parity + blindspots.
(5) Extend to synthetic loadouts + the enabler frontier.

### 2a.1 DEMO FINDINGS → the DLL-cascade build (owner 2026-06-19: "you can build the dll cascade")

The offline demo (`Tools/ModifierCalc/cascade_sim.py`, increments 1–2) ran against real London/player-1 loadouts and
produced the data + model to build the real DLL cascade. Carry these forward (durable — not stuck in a chat thread):

- **The verified YIELD-SOURCE MAP (§2a) is the DLL cascade's source-coverage spec** — 7 buckets, MODIFIED vs
  UNMODIFIED, corp excluded, golden-age & power as toggle-gated modifiers.
- **The condition evaluator is ported + spec-grounded** (`cascade_sim` `eval_condition`/`_eval_atom`, from the
  data-model-spec + enabler-cascade-spec §8 contract): `all`/`any`/`noneOf` + presence atoms + parameterized plot
  predicates + state booleans (`HAS_POWER`/`IS_CAPITAL`/`IS_GOLDEN_AGE`/`HAS_RIVER`). On London: 1039 conditional
  building-yield deposits, 0 unevaluable. **This is the reference for the DLL evaluator.**
- **★ KEY FINDING — the DLL pilot cascade does NOT condition-gate.** `cascadeModifierCitySlot` (CvHttpServer dump
  `cascadeFlat`) includes deposits gated by techs the team lacks (verified: future-tech `TECH_NANOMINING`/
  `TECH_MEGASTRUCTURE_ENGINEERING`-gated production on London — the entire 21,827 flat gap). The Python evaluator
  correctly excludes them. **DLL BUILD TASK #1: wire the enabler condition-eval into the modifier deposit-flow**
  (`cascadeModifierCitySlot` / `CvCascadeModifier`), so the cascade gates each deposit's `enabled`/`disabled` like
  the demo does. Until then the DLL cascade over-counts every state/tech-gated deposit.
- **Validation pivot:** the `py-vs-DLL-cascade` check is exhausted (it proved the ungating); the live acceptance bar
  is `cascade_sim`-vs-LEGACY full output (base × full modifier + flat) at parity-adjacency, across loadout states.
- **Loadout completeness so far:** techs/civics/buildings/`resources`(available bonuses)/plots(+river)/`state`
  (isPowered/isCapital/isGoldenAge). Known still-missing inputs surface as `cascade_sim` "unevaluable" atoms.

---

## 3. The validation credential — TWO comparisons, do not conflate them

- **(a) FIDELITY — emulator-legacy vs LIVE-legacy (`getYieldRate100` & co.).** Does the emulator's *legacy* side
  reproduce the engine's realized number for the same city? **This match is the DESTROY-pass CREDENTIAL** for that
  channel — the proof the emulator faithfully maps the legacy calc. Until it matches, the emulator is unverified and
  **licenses no deletion**. (The first runs WILL diverge on sources the emulator doesn't yet feed — each gap is a
  cause-tagged "add this source" task, exactly the mapping work.)
- **(b) FORMULA DELTA — emulator-legacy vs emulator-new.** Given identical inputs, how far does the new calc diverge
  from legacy? This is the **parity-adjacent design + tuning** axis (the goal is close, not byte-identical —
  modifier-shadow-spec §3.1a) and **the surface on which the new calc is actually designed** (§0.2). Divergences get
  the **care scale** (Fine → Meltdown, modifier-shadow-spec §4). Once (b) lands where we want it, that new formula is
  **ported into the DLL's `cascadeModifierApply`** — the offline-first → port step.

(a) is fidelity of the *map*; (b) is the *design* of the new calc. (a) must reach zero per channel before that
channel's legacy reads are demolished; (b) is expected to be nonzero (the deliberate redesign) and is catalogued +
tuned, not forced to zero.

**ACCEPTANCE BAR, concretely (owner 2026-06-19).** Hold the BASE values fixed — e.g. **100 food / 100 production /
100 commerce, which are BASE INPUTS, not outputs** — feed BOTH calcs the same loadout, let each apply its modifier
model, and the **MODIFIED OUTPUTS should land near-parity** (parity-ADJACENT, not byte-identical). The bar must hold
**swept across DIFFERENT SETS of techs / buildings / improvements / civics / …** (toggle the presence dicts, §2a) —
parity across loadout *states*, not just one city. So the simulator's end metric is **modified-output parity vs
legacy across loadouts**, NOT a byte-match of the intermediate cascade aggregates (that was only the JSON-reading
sanity check). This is what "the modifier calculator is relatively close in parity to the old one" means in practice.

**PROPERTIES — model the building→±RATE contribution (owner 2026-06-19).** For properties (disease / education /
crime / tourism / pollution / …) what the emulator models is **how adding or removing a building changes that
property's +/- RATE** — i.e. the sum of present-buildings' `PROPERTY_*` rate deposits (buildings carry them at JSON
top level, e.g. `building_forge` → `PROPERTY_AIR_POLLUTION` / `PROPERTY_FLAMMABILITY`). Toggling a building in the
loadout add/removes its contribution; that rate delta is the parity target. The ABSOLUTE property value (the
stateful accumulator + spatial propagation) is the #429 solver's job, **not** the emulator's — same per-turn-snapshot
scope: we model the rate contributions, not the cross-turn accumulation.

---

## 4. Build order — channel by channel, in DEMOLITION order

Each channel: (1) implement the legacy combine (map it) + the new combine (design it); (2) feed both the live-dump
inputs; (3) drive fidelity (a) to zero (the map credential); (4) design + tune the new calc on delta (b) under the
care scale; (5) port the nailed new formula to the DLL; (6) only then is that channel's legacy §9 read-points
eligible for the demolition (engine-430 §4). Mirrors modifier-shadow-spec §2.2 and the modifier-spec §9 list:

| # | Channel | Legacy read-points it maps (verify each at build) | Notes |
|---|---|---|---|
| 1 | **City yields** (food/production/commerce) | `getYieldRate100` & the per-building yield reads | **the SEED** (`modcalc.py`); cleanest accumulators |
| 2 | **Commerce split** (gold/research/culture/espionage) | `getCommerceRate*` / `getBaseCommerceRate*`, `getBaseCommerceRateFromBuilding100` | the second split-family axis |
| 3 | **Health / happiness** (good/bad split) | `goodHealth`/`badHealth`/`happyLevel`/`unhappyLevel` accumulators | polarity signed-split |
| 4 | **Defense** | `getDefenseModifier`/`getTotalDefense`/`getNaturalDefense` | clamp-in-family (a `min` member) |
| 5 | **Maintenance / upkeep** | CvCity/CvPlayer maintenance + upkeep reads | cost-style combine (non-default arithmetic) |
| 6 | **Unit-plane stats** | CvUnit `getExtra*` / combat-stat reads (the ~91-setter `changeExtra*` stack) | SELF-accumulator; largest surface, **last** |

The apply-loops behind these (`processBuilding`/`processSpecialist`/`processBonus`/`processCorporation`,
`setCivics`/`processTrait`, `processTech`) are the engine-430 §4 demolition targets the emulator is the map for.

---

## 5. The next concrete build — `GET /diagnostic/cityInput?player=N&city=M`

The endpoint that produces the live-dump (§2). Copy the **mailbox snapshot-isolation** pattern verbatim
(`Sources/Tools/CvHttpServer.cpp`: a `/diagnostic/<action>` route enqueues; the game thread's
`serviceEvalMailbox`→`evaluateGate` renders — the server thread never touches game objects; http-server reference).
Add `cityInput` to the `bNoTypeAction` set + an `evaluateGate` branch beside the existing `modifier` action.

Dumps, for the requested city (else the capital), per channel (city-yields first):

- the **input vector**: `base` (pre-modifier components — plot/trade/free/golden), present buildings / techs / civics
  / bonuses / specialists / population, and each contribution's `flat` / `percent` decomposition;
- the **live LEGACY output** (`getYieldRate100` & co. — the fidelity-(a) ground truth);
- the **live CASCADE output** (`cascadeModifierApply` — what the engine's current seed flow computes).

Then extend `Tools/ModifierCalc/` to **consume the dump**: reproduce the legacy formula offline and assert
**emulator-legacy == live-legacy** (the §3a credential), and run the new formula on the same inputs to design/tune
delta (b). The dump is the on-demand surface (no per-turn-tee timing games; the per-entity-query reasoning from
modifier-shadow-spec §3.1a).

---

## 6. Home + structure

- **`Tools/ModifierCalc/`** (mirrors the `Tools/ReadJson` offline-harness pattern). `modcalc.py` is the
  **city-yields seed**; it grows into a small emulator package as channels land.
- **A channel registry** so adding a channel is additive (a new module + a registry entry), not a rewrite — the
  doc's "build to grow" intent in code form. Keep the formula core a clean, reusable library (the website-feature
  path, §0; and the porting target — a clean offline formula is the easiest thing to port to C++03).
- Pure Python, offline, no game state, no DLL link (like `modcalc.py` today). The DLL side it mirrors / ports into is
  `Sources/Cascade/CvCascadeModifier.{h,cpp}`.

---

## 7. Relationship to the in-game shadow (complementary, not redundant)

- **In-game shadow** (`/diagnostic/modifierSweep` + `[MODSHADOW]`, modifier-shadow-spec §3) measures **real game
  state, turn over turn** — live divergence on actual cities, *after* a calc is in the DLL.
- **This emulator** is the **offline FORMULA sandbox + the validated calc MAP + the design surface for the new calc
  before it is in the DLL** — same input to both formulas, the demolition credential, parity-adjacent design/tuning,
  flow prototyping.

Two legs of the same validation: the emulator proves we *understand and faithfully map* the legacy formula AND that
the *new* formula is sound, before it ships; the shadow then proves the ported calc behaves right *in the running
game*. The `cityInput` endpoint feeds the emulator; the `modifierSweep` endpoint feeds the shadow.

---

## 8. Open / flagged

- ⚑ Exact input-vector schema per channel (which base components + source lists to dump) — pinned per channel at its
  build, starting with city-yields (§5).
- ⚑ How much of the new calc can be designed purely offline vs needs dumped engine state (some sources — bonus/
  civic/event/area/capital/player — are engine state, so they ride in the dump rather than being recomputed).
- ⚑ The port step's exact contract: the emulator's nailed formula → the DLL `cascadeModifierApply` flow (enum value
  + `case`). Keep the emulator's flow shape 1:1 with the DLL enum so the port is a transcription.
- ⚑ Whether the emulator eventually consumes the migrated `Assets/Data` JSON directly for the contribution lists vs
  taking them all from the live dump — lean: dump first (ground truth), JSON later if useful.
- ⚑ The website "simulate my city" feature — future, out of scope; keep the core reusable so it stays open.
