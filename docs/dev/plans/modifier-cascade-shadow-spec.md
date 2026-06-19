# Modifier cascade — SHADOW-TEST + build plan (#430)

> **STATUS: proposed 2026-06-19, pending owner inspection.** Companion to
> [`modifier-cascade-spec.md`](modifier-cascade-spec.md) (the data structure — owner-LOCKED), the enabler shadow
> harnesses in [`cascade-mapping-inventory.md`](cascade-mapping-inventory.md) §B (the pattern this mirrors),
> [`event-spine-spec.md`](event-spine-spec.md) (the live stream), and [`cascade-engine-430.md`](cascade-engine-430.md).
> This doc answers the owner's 2026-06-19 directive: *"we have not done any testing at all on the cascading
> modifier — it needs to be planned and specced; we don't unit-test here, we shadow-test the way the enabler was
> done; we need to know WHAT to shadowtest and have the SETUP to do it."*

---

## 0. Why this doc exists — the modifier cascade is UNBUILT, hence untested

Verified against the code 2026-06-19 (trust-but-verify, not the spec's word):

- **`CvCascadeModifier` is ONLY the combine primitive.** `CvModifierSlot::{deposit,effective}` implements
  `(base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)` (the §2 arithmetic) and nothing else.
- **`CvCascadeReadJson` parses ZERO modifier families.** It reads only the enabler/identity sections
  (`requires`/`enables`/`obsoletes`/`replaces`/`identity.autoBuild`). No `food`/`production`/`commerce`/
  `happiness`/… deposit is read at all.
- **There is no deposit-flow, no per-target accumulator wiring, and no game consumer** reading an effective value.

So "no testing at all" is literally true: **there is nothing running to test.** Unlike the enabler shadows (which
rode JSON that `readJson` *already* parsed), the modifier shadow has a **build prerequisite** (§3). This is a
build + shadow plan, not just a test plan.

## 0a. Owner rulings 2026-06-19 (durable — this section is their home)

- **R-M1 — PARITY-FIRST SCAFFOLD.** Build the modifier engine in a **parity mode** (multiplier composition OFF,
  additive-only like legacy) and drive the shadow to **ZERO divergence** to prove the deposit-flow + accumulators
  are wired correctly. Only then flip the new capabilities on. Parity is a *verification scaffold*, not the end goal.
- **R-M2 — EXPECT MANY FINAL DISCREPANCIES, and that's OK.** *"How modifiers have been calculated in the past has
  been fragmented at best."* Once capabilities are on, far more divergence than the enabler ever showed is **expected**
  — modifier-spec §2's "parity is NOT a goal" holds for the END STATE. The shadow's job there is to **catalogue +
  cause-tag**, not to drive to zero.
- **R-M3 — TRY TO KEEP ALIGNED; when you can't, ASK → owner verdicts a CARE LEVEL → record.** Default posture is to
  keep cascade and legacy aligned. When parity is genuinely unreachable, surface the discrepancy to the owner, who
  assigns a **care level** (§4) — the adjudication that decides whether it blocks cutover or is an accepted win.
- **R-M4 — PILOT CHANNEL = CITY YIELDS** (food/production/commerce — the split-family core).
- **R-M5 — NAMES + DEFAULTS LOCKED (the §6 gate).** All shadow names/defaults confirmed (full list + the two real
  sub-decisions in §6): `/diagnostic/modifierSweep`, `[MODSHADOW]`, `cascadeModifierEffective`, `cascadeModifierParityMode`
  (build-time const), `modifier-cascade-known-discrepancies.md`; rung-1 tolerance = exact-zero in parity / `|delta| ≤ 1` in
  capability mode. **The modifier build is unblocked.** (Sibling §0 decision, in `tally-cascade-spec.md`: the tally is
  PLAYER-LEAF — accepted, not city-leaf.)
- **The deliverable = (A) know WHAT to shadowtest (§2, the comparison surface) + (B) have the SETUP (§3, the harness).**

---

## 1. The two modes — how parity-first (R-M1) reconciles with expect-discrepancies (R-M2)

Each channel passes through both modes, in order:

- **Mode A — PARITY (plumbing proof).** New capabilities OFF; cascade computes additive-only, exactly like legacy.
  **Target: zero divergence.** Any nonzero diff in this mode is a **wiring bug** (a deposit missing, doubled,
  mis-scoped, or read at the wrong base) — there is no "legitimately different" excuse, so it is an unambiguous
  signal. This gates "the deposits land where they should." Maps to care levels 4–5 if it fails.
- **Mode B — CAPABILITIES ON (the real end state).** Multiplier composition + the deliberate corrections are live.
  Discrepancies are **expected** (R-M2). The shadow **catalogues + cause-tags** each one; the owner assigns a care
  level (§4). **Cutover is gated on "every remaining discrepancy has an owner care-level verdict" — NOT on zero.**

A toggle switches modes: proposed `cascadeModifierParityMode` (build-time const initially; promote to a BUG option
if live toggling is wanted). Parity mode also lets us regression-guard the plumbing after the capabilities land.

---

## 2. (A) WHAT to shadowtest — the comparison surface

**The rule:** every legacy modifier read-point (the `modifier-cascade-spec.md` §9 demolition list is the seed) maps
to a cascade `(family, member, scope, target, unit)`. The shadow diffs **per (scope-instance × family-channel)**:
`cascade effective` vs `legacy realized`, with a **decomposed** diff (flat vs percent vs multiplier) to *localize*
which deposit is wrong when the aggregate diverges.

### 2.1 Pilot table — CITY YIELDS (R-M4), grounded in the actual legacy reads (verified 2026-06-19)

| Cascade address | Legacy read-point (`CvCity.cpp`) | Role in the diff |
|---|---|---|
| `food`/`production`/`commerce` `.city` (aggregate effective) | `getYieldRate100(eYield)` — `:11246` | **PRIMARY diff** — the realized per-turn number |
| (base, pre-modifier) | `getBaseYieldRate(eIndex)` — `:22905` | the `base` the cascade's `effective(base)` starts from |
| `…city.flat` accumulator (YieldChanges) | folds into base; `getBaseYieldRateFromBuilding100` — `:10883` | DECOMPOSE: localize a wrong flat deposit (per-building) |
| `…city.percent` accumulator (YieldModifiers) | `getBaseYieldRateModifier(eIndex,iExtra)` — `:11217`, `getYieldRateModifier(eIndex)` — `:11410` | DECOMPOSE: localize a wrong percent deposit |
| (plot-yield contribution) | `getPlotYield`/`getPlotYieldChange` — `:11257`/`:10793` | plot-substrate deposits (§6.1 deliveryguy) — later sub-pass |

**Primary comparison:** cascade `effective(getBaseYieldRate)` for `(yield, city)` vs legacy `getYieldRate100`.
**On divergence:** decompose into flat (Σ YieldChanges) and percent (Σ YieldModifiers) to point at the offending
deposit class before guessing. (Multiplier is identity in parity mode, so a parity-mode divergence is flat or percent.)

### 2.2 The full channel inventory + build order (later channels — line refs from §9, verify each at its build)

| # | Channel | Cascade families | Legacy reads (verify at build) | Notes |
|---|---|---|---|---|
| 1 | **City yields** | `food`/`production`/`commerce` | `getYieldRate100` & co. (§2.1, **verified**) | the pilot; cleanest accumulators |
| 2 | **Commerce split** | `gold`/`research`/`culture`/`espionage` | `getCommerceRate*`/`getBaseCommerceRate*` (§9) | the second split-family axis |
| 3 | **Health/happiness** | `health`/`happiness` (good/bad split) | `goodHealth` `:5831`, `badHealth` `:5858`, `happyLevel` `:5689`, `unhappyLevel` `:5606` | polarity signed-split (§9); feeds dormancy preconditions |
| 4 | **Defense** | `defense` | `getDefenseModifier`/`getTotalDefense`/`getNaturalDefense` (§9) | clamp-in-family (`min` member) |
| 5 | **Maintenance/upkeep** | `maintenance`/`upkeep` | CvCity/CvPlayer maintenance reads (§9) | cost-style combine (non-default arithmetic) |
| 6 | **Unit-plane stats** | `strength`/`withdrawal`/… (`*.unit.*`) | `CvUnit` `getExtra*`/combat-stat reads (§9) | SELF-accumulator (§5), not a downward flow — largest surface, last |

---

## 3. (B) The SETUP — the harness (mirrors the enabler B-i/B-ii shadows)

### 3.1 Build prerequisite — the modifier engine's data-driven layer (minimum for the pilot)

This must exist before any modifier shadow can run. Built **for the pilot channel only first**, then extended:

1. **`readJson` parses the modifier families** into the `{ <payload>, scope?, per?, enabled?, disabled? }` deposit
   shape (§1.3 of the modifier spec). Start: `food`/`production`/`commerce`.
2. **Deposit-flow + per-target accumulators.** Deposits flow DOWN the scope spine and fold into a `CvModifierSlot`
   per `(family, member, target, unit)`. Reuse `CvScopedAccumulator` where the scope roll-up applies (same module
   the tally uses for cross-city counts).
3. **Effective-read API** — `cascadeModifierEffective(family, member, scope, target)` returning `slot.effective(base)`.
4. **The `cascadeModifierParityMode` toggle** (R-M1) — when on, `CvModifierSlot` ignores multipliers (treat as
   identity) so Mode A is additive-only.
5. The per-deposit `enabled`/`disabled` (§3 of the modifier spec) reuse the **existing enabler condition evaluator
   verbatim** (modifier-spec §0.2) — no new condition machinery.

#### 3.1a — VERIFIED BUILD MAP (2026-06-19, 5-agent ground-truth sweep) — the load-bearing facts the build rests on

- **JSON shape (ground truth from `Assets/Data/buildings/<era>/<type>.json`):** yields live at the building ROOT, **no
  `modifiers` wrapper** — `"<yield>": { "<scope>": { "flat": N, "percent": M } }`. `<yield>` ∈ `food|production|commerce|
  gold|research|culture|espionage|health|happiness|PROPERTY_*|…`; `<scope>` ∈ `city|empire|specialist|self` (+ conditional
  sub-scopes `improvements|buildings|specialists` keyed by entity type). `flat`/`percent` are **peer keys** (no `unit`
  field). Values are scalar, an array, or `{value, enabled}` (conditional). `perPopulation` is a separate sub-key.
  **Pilot scope:** `food|production|commerce` × `city`+`plot` × `{flat,percent}`, scalar first; conditional `{value,enabled}`/
  arrays reuse the enabler condition evaluator; other scopes/sub-scopes/`perPopulation` are later sub-passes (tagged as gaps).
- **SCOPE-MODEL BOUNDARIES (owner 2026-06-19):**
  - **PLOT is a first-class modifier scope.** Plots feed the city's base yield AND are modifiable by outside sources
    (buildings/civics/terrain-feature-improvement yield changes), so `MODSCOPE_PLOT` exists alongside city.
  - **`ModifierScope` encodes the COMPLETE containment spine** (`world/team/empire/area/city/plot/self/specialist/unit`),
    not piecemeal — the spine is a fixed, known model so encoding it whole is proper-once, not speculation (after team/plot
    were each found missing). **TEAM is a real modifier scope** (research/tech is team-shared; a wonder can boost the whole
    team — owner 2026-06-19). The parser maps every spine key; the deposit-flow consumes each as families need it (pilot =
    city+plot). The building-yield data sweep showed only `city|empire|specialist|self` authored so far, but the model is whole.
  - **Plot ENCAPSULATION — the plot reports a rolled-up total to the city; the city never sees per-plot detail.** A plot
    self-contains its modifier detail (its terrain/feature/improvement/route + outside buffs resolve INSIDE the plot) and
    reports ONE yield-per-turn to the city — *"yep, this is what you get from me this turn."* The city sums plot reports;
    it never tracks which plot got buffed by which improvement (same report-isolation as the tally's HAS-builder, §4).
  - **Vicinity/spatial scanning is REQUIREMENTS, not modifier.** "Does the city have improvement/buff X in vicinity (its
    workable radius)" is an enabler PREDICATE (`PRED_HAS_IMPROVEMENT` vicinity, already built) used in `requires`/`enabled`
    — NOT a modifier operation. The modifier machine does **no spatial scanning**: it deposits magnitudes and reads
    `enabled` conditions via the existing enabler evaluator. Keeps the modifier machine free of spatial queries.
- **The combine core already exists:** `CvModifierSlot` (`Sources/Cascade/CvCascadeModifier.h`) implements the §2 formula
  exactly — `deposit(MODUNIT_FLAT|PERCENT|MULTIPLIER, v)`, `effective(base)`, `isIdentity()`, identity multiplier = 100.
  Parity mode = multipliers stay identity (yields author none anyway). `CvScopedAccumulator` (sparse `map<int,int>`,
  `deposit`/`get`/iterate) is the roll-up substrate for cross-scope.
- **`readJson` has NO modifier parsing yet** (only `requires`/`allowed`/`identity`). Hook: in `cascadeReadJsonAvailability`
  after the `allowed` block, parse the root yield keys into a new per-entity modifier-deposit list (parallel to the
  count-atom parse). picojson; reuse `rjParseConditionObject` for `enabled`.
- **Legacy parity target (`CvCity.cpp`, verified):** PRIMARY = `getYieldRate100(eYield)` =
  `min(MAX, max(100, (getBaseYieldRate + m_aiSpecialistYieldTotal[y]) × getBaseYieldRateModifier(y) + 100×getExtraYield(y)))`.
  DECOMPOSE: **base** = `getBaseYieldRate` (plot+trade+free+golden); **Σflat building** = `m_buildingExtraYield100[y]`
  (+ `m_aiBaseYieldPerPopRate[y]×pop`); **Σpercent building** = `m_buildingYieldMod[y]`; full percent =
  `getBaseYieldRateModifier` (building+event+bonus+power+area+capital+player). `processBuilding` is the legacy apply-loop
  (writes `m_buildingExtraYield100`/`m_buildingYieldMod`/…). **Parity is driven to zero by ADDING deposit sources until
  cascade `effective(getBaseYieldRate)` matches `getYieldRate100`** — the first run WILL diverge on sources not yet
  deposited (bonus/civic/event/area/capital/player), each cause-tagged; that is the parity work, not a day-one expectation.
- **CALCULATION-FLOW DECISION (owner 2026-06-19) — the deepest finding of the pilot so far.** The `/diagnostic/modifier`
  endpoint (increment 2) immediately surfaced that **legacy's actual value arithmetic was never deliberately DESIGNED** —
  it accreted, with *"modifiers flow top down"* as the only intentional principle. Measuring it revealed a structural
  divergence: **legacy adds building FLAT yields OUTSIDE the percent modifier** (`getYieldRate100 = (base+specialist)×
  yieldModifier + 100×extraYield`, building flats in `extraYield`), whereas the spec's unified `CvModifierSlot` model folds
  flat INSIDE (`(base+Σflat)×(100+Σpercent)`). Rulings:
  - **The cascade is the FIRST deliberate design of the calc formula.** This reframes the modifier rework: we are now
    *choosing* the arithmetic, not inheriting an accident.
  - **CURRENT flow = `CALCFLOW_LEGACY_FLAT_OUTSIDE`** (flat added after the percent, matching legacy). Chosen because the
    unified flat-inside model would multiply every building flat by the city's yield % → a **gigantic data rebalance**;
    legacy-placement lets **parity reach zero** and keeps the existing data values valid. Picking it now does NOT foreclose
    the unified model later (it'd just need the rebalance + a flow switch at that time).
  - **The calc flow is a SINGLE SWAPPABLE dispatch point** (owner: "easily modify the calculation flow later"):
    `ModifierCalcFlow` enum + `cascadeModifierApply` (the C++03 poor-man's-strategy if/switch). Add/change a flow = add an
    enum value + a case; nothing else in the engine changes. `cascadeModifierCalcFlow` (build-time const) selects the active
    one; both the shadow and the endpoint compute via `cascadeModifierApply`.
  - **GOAL = "PARITY-ADJACENT", not parity-exact (owner 2026-06-19).** Since the formula is being deliberately redesigned,
    the END-STATE target is new values CLOSE to old (same ballpark) so the played game stays recognizable (the
    "preserve how the game works" guardrail) — NOT byte-identical. Parity-ZERO stays the *wiring* proof (parity mode,
    R-M1); parity-ADJACENT is the *capability* end-state. (The magnitude gap the pilot already shows — e.g. food
    `flat≈5250` over-shooting legacy — is the tuning work toward adjacency: reconcile which sources/conditionals each side
    counts, per-source, via increment 3's decomposed shadow.)
  - **TOOL TO BUILD — an external old-vs-new formula CALCULATOR (owner 2026-06-19).** A standalone (offline, non-DLL) tool
    that implements BOTH the legacy formula (`(base+specialist)×yieldModifier + extraYield`) and the cascade calc-flows,
    and sweeps a LARGE combination space (base × Σflat × Σpercent × source mix) computing both + their delta — so we can
    explore where/how-much they diverge and tune toward parity-adjacency FAST, and prototype new calc-flows before wiring
    them into the DLL. Complements the in-game shadow (which measures real game state); this is the formula SANDBOX.
    Likely home: `Tools/` (mirrors the `Tools/ReadJson` offline-harness pattern). **Build item — not started.**
    - **SAME INPUT to both formulas → the delta is PURELY formula-attributable (owner 2026-06-19).** Input parity is
      straightforward: the inputs are just the raw contribution lists (base + the flat/percent sources), identical for
      both sides. So the calculator is a pure FORMULA COMPARATOR (same input vector → legacy combine vs cascade combine →
      delta), NOT a game-state simulator — it isolates the combination logic, which is the only thing that differs.
- **Harness pattern (copy verbatim, `Sources/Tools/CvHttpServer.cpp`):** the `/diagnostic/<action>` route → the
  **mailbox snapshot-isolation** (`evalRequestBlocking` on the server thread enqueues; the game thread's
  `serviceEvalMailbox`→`evaluateGate` renders the answer — server thread NEVER touches game objects). Add `modifierSweep`
  to the `bNoTypeAction` set + an `evaluateGate` branch. Per-turn `[MODSHADOW]` line via `rjLogLine` from `CvGame::doTurn`
  (gated `gPlayerLogLevel` ≥1 headline / ≥2 per-divergence), beside `cascadePlacementShadow`/`cascadeDormancyShadow`.
  JSON triage = cap-250 divergence sample + UNCAPPED reason histogram (the `placementSweep` helper shape).
- **Build increments (compile-clean each):** (1) parse → per-entity modifier-deposit store + `rjParseModifiers` ✅ DONE +
  VERIFIED on real data (`[MODPARSE] BUILDING_ANCIENT_CUSTOMS 9 deposits, 0 pending` = exactly its food/production/commerce
  flat entries incl. 4 conditional BONUS-vicinity `enabled` atoms, 0 skipped); (2) deposit-flow +
  `cascadeModifierEffective(family,scope,ctx)` + `cascadeModifierParityMode`; (3) shadow + endpoints — `[MODSHADOW]` doTurn
  line, the all-cities `/diagnostic/modifierSweep`, AND a **per-entity `/diagnostic/modifier?type=X&player=N&city=M` query**
  mirroring the enabler's `canConstruct`-style gate endpoints (owner 2026-06-19): returns one entity's parsed deposits +
  computed effective on demand via the mailbox snapshot — the on-demand verification surface (no per-turn-tee timing games),
  decomposed-diff vs `getYieldRate100`.
- **Verification reminder (owner 2026-06-19):** the game holds the `.log` FILES open → reading new file entries is
  unreliable; use the **`/events` HTTP stream** (the per-turn tee) or the `/diagnostic/*` query endpoints. NB the per-turn
  slice lines (`[READJSON]`/`[MODPARSE]`) burst at the TOP of `doTurn`, so a reader must be CONNECTED to `/events` BEFORE the
  turn ticks (connect-then-end-turn) — which is the very friction the per-entity query endpoint removes.

### 3.2 Endpoint — `GET /diagnostic/modifierSweep?player=N`

The snapshot surface, gated `Autolog__HttpServer` / `gPlayerLogLevel`, the magnitude analogue of `placementSweep`:

- Per `(city × family-channel)`: `{ cascade, legacy, delta, base, flat, percent, mult, reason, care, kind }`.
- **Default** = divergence triage list (`delta != 0`, cap 250) + a summary (counts per cause-tag + per care level).
- **`?type=full`** = the complete per-cell array → the **total-observability** view (reconstruct the whole modifier
  state from the API alone, the §A bar).
- `?channel=production` (etc.) to scope to one family while iterating.
- **Snapshot-isolation HARD CONSTRAINT** (http-server): publish from the game thread; the server thread never reads
  cascade/game state.

### 3.3 Per-turn line — `[MODSHADOW]`

The runtime twin of `[PLACEMENT]`/`[DORMANCY]`, hooked in `CvGame::doTurn` (`cascadeModifierShadow`), to
`Cascade.log` + `/events`:

- **Headline** at `gPlayerLogLevel ≥ 1`: per-channel divergence counts + worst care level seen this turn.
- **Per-divergence** at `≥ 2`: `[MODSHADOW] p=N city=… ch=production cascade=… legacy=… delta=… cause=… care=…`.
- Raw-field via the spine where it lands (event-spine §3) so formatting defers to the logging consumer.

### 3.4 Cause-tags (the reason-reporter) — auto-suggest the provisional care level

The shadow tags each divergence with a machine-determinable CAUSE; the cause suggests a **provisional** care level
(§4), which the owner confirms or overrides (R-M3):

| Cause-tag | Typical meaning | Provisional care |
|---|---|---|
| `match` | delta == 0 | 0 |
| `rounding` | abs(delta) within int-rounding tolerance / sum-order noise | 1 |
| `multiplierComposition` | diff explained by ×product vs legacy additive (Mode B only) | 2 |
| `knownLegacyBug` | matches a catalogued fragmented-legacy quirk | 2 |
| `missingDeposit` / `extraDeposit` / `wrongScope` / `baseMismatch` | a deposit landed wrong (Mode A ⇒ wiring bug) | 4 |
| `overflow` / `channelGarbage` / `nan` | systemic | 5 |
| `unexplained` | cause not identified | 3 → **ask owner** |

### 3.5 Per-player — human AND AI (the cleaner test)

Sweep AI players, not just `player=0` (cascade-mapping-inventory §A): the AI has **no BUG/UI display layer**, so an
AI-player modifier sweep is a *purer* cascade-vs-engine comparison, free of human-UI artifacts.

---

## 4. The CARE SCALE — Fine → Meltdown

The seriousness axis the owner asked for (2026-06-19): *"in the spirit of this codebase we need a care-level ranking,
from 'meh it's fiiine' to 'IT'S ALL GOING TO HELL' — a useful grounding so we align on the seriousness of any
discrepancy."* In the lineage of the DESPAIR/REALISM indexes and the Oblivious→Meta surveillance scale: **one named
scale, accurate under the humor** — a six-rung **composure-collapse** arc, deadpan at the bottom, full panic at the
top.

**NAMING CONSTRAINT (owner 2026-06-19) — one word, understood COLD by an agent.** Each rung name is (a) **one word**
so we can reference a rung directly in conversation ("that's a *Weird*", "that one's a *Bug*"), and (b) chosen so the
**name alone conveys both the severity and the implied action** to an agent reading it cold — no table lookup needed
(the same read-cold principle as modifier-spec §0.7 for modders): `Fine`→ignore · `Rounding`→accept · `Better`→accept
as a win · `Weird`→investigate/ask · `Bug`→fix · `Meltdown`→stop everything. Any future rename must preserve that
cold-readability.

The shadow auto-suggests a provisional rung from the cause-tag (§3.4); the **owner's verdict sets the final
one** (R-M3). It is the modifier shadow's disposition axis — the magnitude analogue of the enabler's "16 UI-acceptable"
classification. Six rungs (0–5) parallel the surveillance scale; **extensible if a case ever needs finer grading**
(owner blessed adding levels).

| Lvl | Name | Real meaning | Action / gate |
|---|---|---|---|
| **0** | **Fine** | exact parity, or a diff blessed as identical-enough | none |
| **1** | **Rounding** | cosmetic: int-rounding / off-by-one / sum-order noise within tolerance | accept, note |
| **2** | **Better** | deliberate correction — cascade fixes fragmented-legacy math (multiplier composition etc.); the **expected** end-state divergence (R-M2) | accept as a *win*, document it |
| **3** | **Weird** | unexplained divergence, cause not yet found — **the "ask the owner" bucket** | investigate → owner verdict |
| **4** | **Bug** | confirmed cascade wiring bug (deposit missing / doubled / mis-scoped) | **must-fix before that channel's cutover** |
| **5** | **Meltdown** | systemic — whole channel garbage, overflow, plumbing broken | **stop-the-line; block cutover; → DESPAIR_INDEX candidate** |

**The top rung is an index candidate (owner 2026-06-19).** A `Meltdown` is, by nature, an audible-exhale absurdity —
so reaching rung 5 nominates the discrepancy for [`../indexes/DESPAIR_INDEX.md`](../indexes/DESPAIR_INDEX.md) (the
existing optional, owner-sanctioned catalogue of exceptional bugs), never as a substitute for the real fix. The
fragmented-legacy modifier math is a rich seam for it.

**Cutover rule:** a channel's legacy §9 reads may be deleted only when every remaining Mode-B discrepancy on it sits
at an owner-verdicted care rung **≤ 2** (or is an explicitly-accepted higher rung). Rungs 3–5 block.

## 4.1 The discrepancies catalogue

A sibling to [`cascade-known-discrepancies.md`](cascade-known-discrepancies.md), proposed
`modifier-cascade-known-discrepancies.md`, populated as the shadow runs: per row `{ channel, scope, cause-tag,
care level, owner note }`. The durable record of every blessed (≤2) divergence — what the cascade deliberately
corrected and why — and every open (3–5) one.

---

## 5. Build order + cutover (per channel, map-before-delete)

For each channel in the §2.2 order:

1. Build the data-driven layer for that channel's families (§3.1).
2. **Mode A** — shadow to **zero** divergence (plumbing proof, R-M1). Any diff = care 4–5, fix.
3. **Mode B** — flip capabilities on; shadow catalogues divergences; owner assigns care levels (R-M3/§4).
4. When all remaining diffs are owner-verdicted ≤ 2 → **delete that channel's legacy §9 reads** (the demolition).
5. Parity-mode regression-guard stays available to re-prove plumbing after the cut.

Legacy stays live and authoritative until its channel is cleared — the same map-before-delete the enabler followed.

---

## 6. Names + defaults — LOCKED 2026-06-19 (owner)

**All confirmed — the modifier build is unblocked.**

- **Names (LOCKED):** endpoint `GET /diagnostic/modifierSweep?player=N`, tag `[MODSHADOW]`, read API
  `cascadeModifierEffective(...)`, toggle `cascadeModifierParityMode`, discrepancies doc
  `modifier-cascade-known-discrepancies.md`.
- **Parity toggle (LOCKED):** a **build-time const** initially; promote to a BUG option only if live toggling is wanted.
- **Rounding tolerance, care level 1 (LOCKED):** **exact zero in parity mode** (Mode A — any diff is a wiring bug);
  in capability mode (Mode B), rung-1 "Fine" = off-by-1 in the last integer place (`int×100` rounding, `|delta| ≤ 1`).

**Still open (NOT blocking the build — defaults stand until revisited):**

- **Decomposed-diff granularity** — per-deposit-class (flat/percent/mult) is specced; whether to also break flat
  down per *source entity* (per-building) by default or only under `?type=full`. (Default: only under `?type=full`.)
- **Care-level home** — provisional level computed in-engine (the cause→care table §3.4) vs assigned purely in the
  catalogue doc. Lean (default): engine emits the provisional, the doc records the owner's final.
