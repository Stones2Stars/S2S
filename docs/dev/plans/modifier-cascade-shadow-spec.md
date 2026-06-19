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

## 6. Open / to confirm with the owner

- **Names** (propose, confirm): endpoint `/diagnostic/modifierSweep`, tag `[MODSHADOW]`, toggle
  `cascadeModifierParityMode`, doc `modifier-cascade-known-discrepancies.md`.
- **Parity toggle** — build-time const vs a real BUG option (lean: const first, promote if live toggling is wanted).
- **Rounding tolerance** for care level 1 (`rounding` cause) — absolute, relative, or per-channel.
- **Decomposed-diff granularity** — per-deposit-class (flat/percent/mult) is specced; whether to also break flat
  down per *source entity* (per-building) by default or only under `?type=full`.
- **Care-level home** — provisional level computed in-engine (the cause→care table §3.4) vs assigned purely in the
  catalogue doc. Lean: engine emits the provisional, the doc records the owner's final.
