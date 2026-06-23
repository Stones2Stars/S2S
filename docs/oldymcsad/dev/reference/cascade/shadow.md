# The modifier shadow — verifying the cascade against the live engine

> **Status:** reference · **Verified against:** old `docs/dev/plans/modifier-cascade-shadow-spec.md` +
> `modifier-cascade-known-discrepancies.md` reconciled with the landed `Sources/Cascade/CvCascadeModifier.{h,cpp}`,
> `Sources/Tools/CvHttpServer.cpp`, and `Tools/ModifierCalc/cascade_sim.py` — 2026-06-20.
> **Grounding:** the city-yields pilot (food/production/commerce, BUILDING + CIVIC deposits, parity mode) is built and
> Assert-clean; the wider channels/scopes are design-landed but unbuilt. Each claim below marks landed-vs-built. The
> `Sources/` tree was reorganized (`Cv*` → `Sources/Engine/`, cascade → `Sources/Cascade/`, `CvHttpServer` →
> `Sources/Tools/`); citations use the new prefixes. Line numbers **drift** — confirm the named function, not the integer.
>
> **BLUF.** This is the **verification methodology** for the modifier cascade: how the cascade's computed "how much?"
> value is diffed against the live legacy engine, turn over turn, until a channel is clean enough to cut its legacy
> reads. It owns the **acceptance bar** ([DEC-parity-not-goal](../../architecture/decisions.md#dec-parity-not-goal)):
> matching legacy is **not** the goal, parity-ZERO is only the *plumbing* proof, the end-state target is
> **parity-ADJACENT**, and **±10% is explicitly NOT parity-adjacent — the bar is much sharper**. It also owns the
> six-rung **care scale** (Fine → Meltdown) that dispositions each surviving divergence. The map-before-delete *why*
> ([cascade-architecture §7](../../explanation/cascade-architecture.md)), the modifier mechanism itself
> ([modifier.md](modifier.md)), and the live observability surface ([observability](../observability/README.md)) are
> linked, not re-explained.

---

## 1. Why a shadow exists — map-before-delete

The cascade replaces ~7–8k lines of legacy "maintainer" machinery. You **cannot safely delete a maintainer you
cannot fully observe**, so each modifier behaviour gets a **shadow** that diffs the cascade's effective value against
the live legacy engine, turn over turn, until clean — *then* the legacy read is cut. This is the modifier-channel
application of the map-before-delete discipline; the *why* and the total-observability ("Orwell") bar it rests on are
the [cascade overview §7](../../explanation/cascade-architecture.md) ·
[DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete). Legacy stays live and authoritative
until its channel is cleared.

Unlike the **enabler** shadows (which rode JSON that `readJson` already parsed), the modifier shadow had a build
prerequisite — the deposit-flow and accumulators (the [modifier machine](modifier.md)) had to exist before there was
anything to diff. That prerequisite is built for the city-yields pilot and pending for the wider channels.

## 2. The two modes — parity-ZERO proves the plumbing, parity-ADJACENT is the goal

Each channel passes through two modes, in order. They reconcile the two owner postures — "drive to zero first" and
"expect many final discrepancies":

| Mode | Capabilities | Target | A nonzero diff means |
|---|---|---|---|
| **A — PARITY** | multiplier composition OFF; cascade computes additive-only, exactly like legacy | **zero divergence** | a **wiring bug** — a deposit missing, doubled, mis-scoped, or read at the wrong base. There is no "legitimately different" excuse, so it is an unambiguous signal. |
| **B — CAPABILITIES ON** | the deliberate corrections live | **NOT zero** | a divergence to **catalogue + cause-tag**; the owner assigns a care level (§4). Cutover is gated on "every remaining divergence has a verdict", not on zero. |

Mode A is a **verification scaffold**, not the end goal: it gates "the deposits land where they should", and it stays
available afterward to regression-guard the plumbing once the capabilities land. A build-time const
`cascadeModifierParityMode` (in `Sources/Cascade/CvCascadeModifier.*`) selects the mode; when on, `CvModifierSlot`
treats multipliers as identity so the arithmetic is additive-only. (Promote to a BUG option only if live toggling is
ever wanted.)

**Why this is not contradictory:** parity-zero is the *plumbing* claim ("the right magnitudes reach the right
accumulators"); parity-adjacent is the *end-state* claim ("the new deliberately-redesigned formula produces values in
the same ballpark as legacy"). The cascade is the **first deliberate design of the value-calc formula** — legacy's
arithmetic was never designed, it accreted — so the end state is *chosen*, not inherited, and is allowed to diverge.
See [modifier.md](modifier.md) for the combine arithmetic and the swappable calc-flow dispatch.

## 3. The acceptance bar — parity-adjacent, and why ±10% is NOT it

> **This doc is the canonical home of [DEC-parity-not-goal](../../architecture/decisions.md#dec-parity-not-goal).**

**Matching legacy is not the goal.** Because the formula is being deliberately redesigned, the end-state target is new
values **close** to old — same ballpark, so the played game stays recognizable (the "preserve how the game works"
guardrail) — **not** byte-identical. Two distinct bars, do not conflate them:

- **Parity-ZERO** — the *wiring* proof, Mode A only. Tolerance: exact zero in parity mode; in capability mode, rung-1
  "Fine" is off-by-1 in the last integer place (`int×100` rounding, `|delta| ≤ 1`).
- **Parity-ADJACENT** — the *capability* end-state, Mode B. New values close to legacy.

**⛔ ±10% is explicitly NOT parity-adjacent — the bar is MUCH sharper (owner ruling 2026-06-19).** A channel that sits
inside ±10% is *not* done. The pilot's 6-city offline sweep landed commerce at a mean |gap| of 8.8% (5/6 within ±10%)
and that was recorded as **necessary but not sufficient** — the residual must still be driven down. The bar being
sharper than 10% is the reason the per-source attribution work (§5) exists: you cannot close a sub-10% residual by
guessing, only by attributing every remaining point to a named source.

**Parity-adjacent ≠ parity-chasing.** Where the cascade deliberately corrects a fragmented-legacy quirk (e.g. legacy
calculates pseudobuildings differently per property; the cascade uses ONE unified band calc and re-authors the *data*
to fit it), the residual is a **corrected `Better` (care 2)**, not a divergence to chase to zero. The cascade is
*sharper than legacy* on those — which is the point, not a failure. The rule: **unify the calculation, fix the data;
do not replicate legacy's per-case calc quirks.**

## 4. The CARE SCALE — Fine → Meltdown (the disposition axis)

> **⛔ OWNER RULING (2026-06-20) — the care scale is the OWNER'S acceptance vocabulary for the validation stage, NOT
> the agent's "this is OK / this is a Bug" auto-classifier.** It was written so the owner has a common vocabulary to
> tell the agent whether a divergence is acceptable — it is read by the owner, applied by the owner. The shadow's job
> is to SURFACE each divergence with its factual attribution (the cause-tag + the cascade/legacy/delta numbers), not to
> pre-judge acceptability. So `cascadeModifierClassify` auto-assigns ONLY the two safe-factual low rungs (exact
> `match`→Fine, within-tolerance→Rounding) plus the documented capability win (`multiplierComposition`→Better), and
> routes **every** real divergence to **`Weird` — the "surface it, the owner verdicts" bucket**. It deliberately
> **never self-asserts `Bug` or `Meltdown`** (those are verdicts), and never flags a legitimate value (e.g. a net-
> negative gold/health) as garbage. The scale had drifted into an agent auto-verdict (auto-`Bug` on the expected
> un-wired-source parity work, auto-`Meltdown` on valid negatives); this realigns it. *(This ruling was meant to be
> captured at the prior handover and was not — recorded here now; nothing durable may hinge on a handover,*
> [DEC-WF-rulings-to-repo](../../architecture/decisions.md#dec-wf-rulings-to-repo).*)*

Every surviving Mode-B divergence is dispositioned on a six-rung **composure-collapse** scale — the seriousness axis
that decides whether a divergence blocks cutover or is an accepted win. It is the magnitude analogue of the enabler's
"UI-acceptable" classification. **One word per rung**, chosen so the name alone conveys both severity and the implied
action to an agent reading it cold (no table lookup): `Fine`→ignore · `Rounding`→accept · `Better`→accept-as-win ·
`Weird`→investigate/ask · `Bug`→fix · `Meltdown`→stop everything.

| Lvl | Name | Real meaning | Action / gate |
|---|---|---|---|
| **0** | **Fine** | exact parity, or a diff blessed as identical-enough | none |
| **1** | **Rounding** | cosmetic: int-rounding / off-by-one / sum-order noise within tolerance | accept, note |
| **2** | **Better** | deliberate correction — the cascade fixes fragmented-legacy math (multiplier composition, the unified band calc, …); the **expected** end-state divergence | accept as a *win*, document it |
| **3** | **Weird** | unexplained divergence, cause not yet found — **the "ask the owner" bucket** | investigate → owner verdict |
| **4** | **Bug** | confirmed cascade wiring bug (deposit missing / doubled / mis-scoped) | **must-fix before that channel's cutover** |
| **5** | **Meltdown** | systemic — whole channel garbage, overflow, plumbing broken | **stop-the-line; block cutover** |

**Cutover rule:** a channel's legacy reads may be deleted only when every remaining Mode-B divergence on it sits at an
owner-verdicted rung **≤ 2** (or is an explicitly-accepted higher rung). Rungs 3–5 block. **Posture:** the shadow tries
to keep cascade and legacy aligned by default; when parity is genuinely unreachable it surfaces the divergence and the
owner assigns the final care level — the shadow's `cascadeModifierClassify` only *suggests* a provisional rung from the
cause-tag (§5). A rung-5 `Meltdown` additionally nominates the case for the
[DESPAIR_INDEX](../../../../docs/indexes/DESPAIR_INDEX.md) (optional, never a substitute for the fix). The scale is
**extensible** if a case ever needs finer grading.

## 5. Divergence attribution — map it, never guess

> **Discipline: [DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing).** When a value diverges, do NOT
> hypothesize a cause and try a fix. EMIT the full legacy decomposition (every source/component of that calc) via the
> dump, and map the cascade's value by the **same** components, so the divergence is attributed to a **named source
> with numbers**. If the data to attribute it isn't being emitted, the FIRST step is to emit it.

The shadow diffs **per `(scope-instance × family-channel)`** — for the pilot, per `(city × {food|production|commerce})`
— and the diff is **decomposed** (flat vs percent vs multiplier) so a divergence localizes to a deposit class before
anyone guesses. Each divergence carries a machine-determinable **cause-tag**, which suggests the provisional care rung:

| Cause-tag | Typical meaning | Provisional care |
|---|---|---|
| `match` | delta == 0 | 0 — Fine |
| `rounding` | within int-rounding tolerance / sum-order noise | 1 — Rounding |
| `multiplierComposition` | diff explained by ×product vs legacy additive (Mode B only) | 2 — Better |
| `knownLegacyBug` | matches a catalogued fragmented-legacy quirk | 2 — Better |
| `missingDeposit` (cascade < legacy) / `extraDeposit` (cascade > legacy) | a real divergence — typically un-wired sources (cascade undershoots) or dormant/over-counted deposits (overshoots) | **3 — Weird → ask owner** (NOT auto-Bug: the agent surfaces, the owner verdicts §4) |
| `unexplained` | cause not identified | 3 — Weird → **ask owner** |

> **`Bug` (4) and `Meltdown` (5) are OWNER VERDICTS, not auto-tags (§4).** The classifier never emits them — a confirmed
> wiring bug or a systemic meltdown is a conclusion the owner draws from the surfaced divergence + numbers, then states
> back to the agent. (Legitimate net-negative values are NOT garbage; the old auto-`channelGarbage`→Meltdown rule was the
> drift this removes.)

The cause-tagger and the rung-name lookup live ONCE in `Sources/Cascade/CvCascadeModifier.*`
(`cascadeModifierClassify` / `cascadeModifierCareName`), so the per-turn line, the endpoints, and the offline tester
share one definition.

**Worked example of attribution-not-guessing (pilot, commerce).** The pilot commerce channel over-counted by ~33%.
Rather than guess, the legacy decomposition was emitted (`/diagnostic/cityInput` per-building `buildingYields` +
`mod{Building,Player,Capital,Bonus,Power,Area}` / `extraYield`) and the cascade mapped by the same components. The gap
attributed cleanly: the EDUCATION property-band ladder authored each band at its **full** value while legacy counts only
the highest band, so the cumulative-active cascade summed +140% where legacy applied +35% — a NAMED source with
numbers, fixed by re-authoring the bands as INCREMENTAL deltas (a data fix, not a calc branch). The residual flat gap
attributed largely to **corporations** (excluded from the modifier cascade by design — a `Better`, not a bug). No step
was a hypothesis; each was a mapped source.

### 5a. Represent the mechanic — never skip one to fit the data ([DEC-represent-dont-fit](../../architecture/decisions.md#dec-represent-dont-fit))

**This doc is the canonical home of [DEC-represent-dont-fit](../../architecture/decisions.md#dec-represent-dont-fit) (owner ruling 2026-06-21).**
The default reading of a divergence is **the emulator is missing a mechanic** — trace it to its named engine source(s)
and *represent* it. The cardinal sin (the recurring bait failure) is **skipping, dropping, or inventing a mechanic to
make the number fit** — "the dormant set still yields," "exclude it wholesale," folding a value into the wrong term
because the total lands closer. A gap is a lead to a real game mechanic not yet represented, **until proven otherwise**:

- **Trace to the leaf, exhaustively.** `getExtraYield100 = m_aiExtraYield×100 + m_buildingExtraYield100`; the non-building
  part (`m_aiExtraYield`) is fed *only* by its `changeExtraYield` callers — **corporation yield** (`setCorporationYield`/
  `updateCorporationYield`) and **`BuildingYieldChange`** (`setBuildingYieldChange`, gated on `hasFullyActiveBuilding`).
  Enumerate every caller; do not stop at the first plausible source (the "specialists bake into `m_aiExtraYield`" line is
  an *old-save* comment — current code routes corp + building-yield-changes, not specialists).
- **Changing the math = changing a game mechanic.** That is permitted ONLY for a bug **verified beyond reasonable doubt**,
  and even then as a **deliberate, surfaced** decision (the owner's `Bug` verdict, §4) — never a quiet gap-closer. This
  mod's 25-year damage came from unprincipled "balance"/"fun" edits with no grasp of consequences; the rework's whole
  point is a system understood and reliable end-to-end (also the precondition for trustworthy AI work). A divergence you
  cannot yet attribute is surfaced (`Weird → ask owner`), not patched.

### 5b. Verify MECHANIC-BY-MECHANIC; never trust an averaged or aggregate output ([DEC-per-mechanic-parity](../../architecture/decisions.md#dec-per-mechanic-parity))

**This doc is the canonical home of [DEC-per-mechanic-parity](../../architecture/decisions.md#dec-per-mechanic-parity) (owner ruling 2026-06-22).**
Parity is established by checking that **each individual mechanic** computes the engine's value — the per-mechanic
quantities the dump already emits (`basePlotYield`, `baseTradeYield`, `specialist`, `modBuilding`/`modBonus`/
`modPlayer`/`modPower`/`modArea`/`modCapital`/`modEvent`, `extraBuildingYield100`, `buildingYieldChange`,
`corporationYield`, …). dry's per-mechanic computation must equal the engine's, **per scope-instance, exactly.**

**⛔ NEVER compare or AVERAGE aggregate/realized outputs** — a realized-yield gap, and *especially* an empire-
or world-AVERAGED gap, **hides offsetting per-mechanic errors**: a `modifier −47pp` cancels a compensating term
and the average reads "≈0" while the calc is wrong everywhere. That cancellation is the kraken's trap (it is how
a wrong calc reads "so close"). **Strip every empire/world average from the metrics.** The only sound metric is
per-mechanic, per-instance: *which mechanics match the engine, and for the ones that don't, which instances
diverge and by how much* — never a mean. A channel is parity **only** when every feeding mechanic matches
exactly across all instances; an aggregate that nets to zero proves nothing.

## 6. The shadow surfaces (where the diff is read)

Three live surfaces, all built for the pilot. The mechanics and gating of these hook *shapes* are the
[observability reference](../observability/README.md) ·
[DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes) — only their modifier-specific content is
described here.

- **`GET /diagnostic/modifierSweep?player=N`** — the all-cities snapshot, the magnitude analogue of `placementSweep`.
  Per `(city × family-channel)`: `{ cascade, legacy, delta, base, flat, percent, mult, reason, care, kind }`. Default
  = a divergence-triage list (`delta != 0`, cap 250) + an **UNCAPPED** cause-histogram and care-histogram. `?type=full`
  = the complete per-cell array (the total-observability view). `?channel=food|production|commerce` (or `?type=…`)
  scopes to one family.
- **`GET /diagnostic/modifier?player=N&city=M`** — the per-city on-demand decomposed spot-check (diff vs
  `getYieldRate100` with no per-turn-tee timing).
- **`[MODSHADOW]` per-turn line** (`cascadeModifierShadow` in `CvGame::doTurn`, every alive player incl. AI) →
  `Cascade.log` + `/events`. Headline at `gPlayerLogLevel ≥ 1` (per-channel divergence counts + worst care this turn);
  per-divergence at `≥ 2` (`p=N city=… ch=production cascade=… legacy=… delta=… cause=… care=…`).

All three publish from the **game thread** under the mailbox snapshot-isolation contract (the server thread never
reads cascade/game state). **Sweep AI players, not just `player=0`** — an AI player has no BUG/UI display layer, so an
AI-player sweep is a *purer* cascade-vs-engine comparison, free of human-UI artifacts.

> **Reading the surface (owner ruling).** The running game holds its `.log` files OPEN, so live-tailing `Cascade.log`
> is unreliable. Read the live diff via the **`/diagnostic/*` endpoints** (on-demand snapshot, no log/gate dependency)
> or the **`/events` stream** (connect BEFORE the turn ticks — the per-turn lines burst at the top of `doTurn`).
> Delegate the bulk read to the cheap `data-reader` sub-agent; never pull raw endpoint/log dumps into an expensive
> context. Full surface: [observability](../observability/README.md).

## 7. The offline calculators — two legs (`dry_calc.py` + `cascade_sim.py`)

The in-game shadow measures real game state; offline there are **two complementary Python calculators** in
`Tools/ModifierCalc/`, validating different things — keep them distinct:

- **`dry_calc.py` — the PRIMARY zero-ride-in full calc (per scope).** Ports every economic per-turn channel
  (city yields, commerce split, health/happiness, defense, maintenance/upkeep, growth, greatPeopleRate,
  tradeRoutes, inflation, property ±rates, empire roll-ups) and computes **every value from the JSON deposits +
  fictitious game STATE — ZERO legacy ride-in** ([DEC-calc-zero-ride-in](../../architecture/decisions.md#dec-calc-zero-ride-in)).
  It mirrors the in-game modifier engine: a `ModifierSlot` (== `CvModifierSlot`) combine with the swappable
  calc-flow, and a **scope walker** that accumulates deposits DOWN the spine into one slot. It validates each
  channel **in ISOLATION** on hand-built fabricated input (exact arithmetic; `python dry_calc.py` runs the
  self-tests) — it does **not** read live data / dumps / `samples/*.json`. (The unit-plane combat stats are
  out — last/largest surface.) This is the offline birthplace of the calc that ports to the DLL.
- **`cascade_sim.py` — the same-input-vector COMBINE comparator.** It implements BOTH the legacy combine and
  the cascade calc-flows and, fed the **same input vector** (base + the flat/percent source lists, identical on
  both sides, read from the migrated `Assets/Data` JSON), computes both + their delta — so the delta is **purely
  formula-attributable** (it is not a game-state simulator; it isolates the combination logic, the only thing
  that differs). Unlike `dry_calc.py` it **rides in** the base + engine-state values from the dump fixtures (its
  job is the formula, not the data→value derivation). It exists to:

1. **tune toward parity-adjacency fast** — sweep a large source-mix space and see where/how-much the two diverge;
2. **prototype new calc-flows** before wiring them into the DLL;
3. **serve as the map-before-delete map** — extracting + emulating every legacy calc is required anyway to safely
   delete it, so the same artifact is also the demolition map. It is the de-facto **test harness** for a closed-EXE
   game that cannot carry real unit tests.

**What FULL value-parity testing actually requires — three SEPARABLE pieces (owner clarification 2026-06-21).**
The thing that was unclear (and cost real churn) is that parity cannot be reached by *riding in* legacy
numbers; it needs the calc's input to **provably match what the game sees**, which factors into three clean
pieces, each buildable on its own:
1. **the pure CALC** — `dry_calc.py`, a function of `(raw-state input model + Assets/Data JSON)` only, zero
   ride-in ([DEC-calc-zero-ride-in](../../architecture/decisions.md#dec-calc-zero-ride-in));
2. **the full game-state JSON SPEC** — the raw state of the game as ONE document along the scope spine
   (`world→teams→empires→areas→cities→plots`), **raw observed facts only** (no calculated value; the lone
   map-number is distance-from-capital). Spec + worked example: `Tools/ModifierCalc/README.md` +
   `raw_state.example.json` (this spec did not exist before 2026-06-21 — it was the missing piece);
3. **the separate EXTRACTOR** — an app that produces that game-state JSON from the **live** game, which is
   exactly what the Orwellian observability bar ([DEC-obs-scale](../../architecture/decisions.md#dec-obs-scale))
   is for (every spec field must be readable from the wire, never the screen).

Parity then = run the calc on the live-derived document, read the game's realized values alongside, diff
per channel. Because the inputs are provably identical (same raw facts), every divergence is attributable to a
named source — not a ghost hunt. `dry_calc.py` (piece 1) + the spec (piece 2) are built; the extractor (piece
3) is the next, separate app.

**Verified pilot results (6-city sweep, 2026-06-19):** after the x100 de-scale fix and the incremental-band curation,
production reached near-parity, commerce landed at mean |gap| 8.8% (5/6 within ±10%) — which, per §3, is
**necessary but not sufficient** (±10% is not the bar). The known residuals are TECH-DOWNWARD deposits and keyed
sub-scopes (`<yield>.<scope>.{buildings|improvements|specialists}.{TARGET}`) not yet loaded by the sim, plus
corporations (excluded by design). The graduated, full-scope emulator workstream (all channels, real loadouts) is
tracked in the calc-emulator plan in the old `docs/dev/plans/` set (pending rebuild).

## 8. Scope — what is built vs designed

| Channel | Families | State |
|---|---|---|
| **City yields** (pilot) | `food` / `production` / `commerce` | **built**, Assert-clean: BUILDING + CIVIC(empire→city) deposits, parity mode, the three shadow surfaces, `cascade_sim` near-parity. |
| Commerce split | `gold` / `research` / `culture` / `espionage` | design-landed, unbuilt |
| Health / happiness | `health` / `happiness` (good/bad split) | design-landed, unbuilt |
| Defense | `defense` | design-landed, unbuilt |
| Maintenance / upkeep | `maintenance` / `upkeep` | design-landed, unbuilt (cost-style combine) |
| Unit-plane stats | `strength` / `withdrawal` / … (`*.unit.*`) | design-landed, unbuilt — SELF-accumulator, largest surface, last |

**Per-channel build + cutover order** (map-before-delete, per channel): build the data-driven layer → **Mode A**
shadow to zero (plumbing proof) → **Mode B** flip capabilities, shadow catalogues, owner assigns care → when all
remaining diffs are owner-verdicted ≤ 2, **delete that channel's legacy reads** → keep parity-mode as a
regression-guard. Legacy stays authoritative until its channel is cleared.

> **Stale-flag (2026-06-20).** The "blessed (≤2) divergence" catalogue is, as of this writing, still **empty of
> verdicted rows** — the pilot histograms are dominated by `missingDeposit` (sources not yet deposited: TRAITS, TECH
> downward, BONUS-intrinsic, AREA/POWER/CAPITAL, building empire-scope, civic sub-scopes), which is the **expected
> parity work**, not a roster of real bugs. The catalogue populates as each source is wired and the owner adjudicates
> the residue.

## See also
- [cascade-architecture.md](../../explanation/cascade-architecture.md) §7 — the map-before-delete *why* and the
  Orwellian total-observability bar this verification rests on (the design rationale; not re-explained here).
- [modifier.md](modifier.md) — the modifier machine this shadow verifies: the families × scopes × channels structure,
  the combine arithmetic, and the swappable calc-flow dispatch whose output is diffed.
- [observability/README.md](../observability/README.md) — the live surface (snapshot field / gated `[TAG]` log /
  mailbox `/diagnostic`) the three shadow hooks (§6) are instances of, plus the read-discipline.
- [decisions.md](../../architecture/decisions.md) — `[DEC-parity-not-goal]` (owned here), `[DEC-map-before-delete]`,
  `[DEC-no-guessing]`, `[DEC-obs-hook-shapes]`.
- [`../../README.md`](../../README.md) — the docs2 comprehension map.
