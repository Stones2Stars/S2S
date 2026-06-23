# Decisions ledger — the canonical, ID'd home for cross-cutting rulings

> # ⛔ EVERY `DEC-*` IS A HARD RULE — BINDING, NON-NEGOTIABLE, NOT A SUGGESTION.
> A `DEC` in this ledger is a **rule you MUST obey by default** — never advice to weigh, a "convention" to bend, or
> a "decision" you can re-open on your own judgement. The words *decision / ruling / convention* in this repo all
> mean **RULE**. None of these was invented for tidiness: **each was paid for by an agent before you** charging
> ahead and having its context **eaten by the kraken** — the standardless tangle this codebase is. Reading and
> obeying them up front is *far cheaper* than re-learning them by slamming into them (**"fast is slow, slow is
> fast"** — [DEC-fast-is-slow](#dec-fast-is-slow)).
>
> Operating posture (the **standing default**, not a phase): **maximal rigor** — verify everything against ground
> truth, enumerate exhaustively, take zero shortcuts; treat any pull toward *"this is just guidance / probably fine
> / I'll infer it"* as the bait that sinks the ship ([DEC-kraken](#dec-kraken), [DEC-no-guessing](#dec-no-guessing),
> [DEC-all-means-all](#dec-all-means-all)). **The ONLY thing that relaxes any of this is the OWNER explicitly saying
> so — never an agent's read of the situation.** A question you have posed to the owner is a HARD STOP until answered.
>
> *(Owner ruling 2026-06-22: the decisions are hard rules, not suggestions — learned by multiple agents before, gobbled
> by the kraken; and **if a decision doc leaves the binding nature in any way unclear, the phrasing is wrong and gets
> redone.** This banner is that fix.)*

> **What this is.** An **index, not a re-statement**: one stable `DEC-id` per cross-cutting ruling, a
> one-line summary, and a pointer to its authoritative home. It exists to break the duplication loop —
> rulings kept getting re-stated doc-after-doc because there was no discoverable canonical home, so agents
> re-added them defensively, *ad infinitum*. **Operational rule: before adding any cross-cutting ruling
> anywhere, grep this ID table first.** A doc that needs a ruling links `[DEC-id]`; it does not
> re-articulate it. Full rationale: [`AGENTS.md`](../../../AGENTS.md) Conventions ("the discoverability half").
>
> **Transition note (2026-06-19):** the rulings are being carried from the old `docs/dev/decisions.md`
> into this docs2 set. Until each subsystem's home is rebuilt (tracked in
> [`_meta/build-plan.md`](../_meta/build-plan.md)), an entry's **full text** may still live in the old set;
> the one-line + home below is authoritative for *what the ruling is and where to read it*.

---

## Index (grep this first)

| ID | One-line | Home |
|---|---|---|
| [DEC-fixedpoint-x100](#dec-fixedpoint-x100) | All cascade value math is integer ×100; the one human→int conversion lives only in readJson | `reference/cascade/fixed-point-and-scales.md` |
| [DEC-per100-closed-set](#dec-per100-closed-set) | The legacy per-100 fields are a CLOSED set of exactly 6 `…100()` accessors | `reference/cascade/fixed-point-and-scales.md` §4b |
| [DEC-curator-owns-descale](#dec-curator-owns-descale) | The curator absorbs all per-100 ambiguity once; JSON is uniformly human; readJson re-applies ×100 | `reference/cascade/fixed-point-and-scales.md` §1 |
| [DEC-deliveryguy](#dec-deliveryguy) | A cross-entity modifier lives on whoever DELIVERS it (the deliveryguy), keyed by the target — by semantic sense, not inversion | `reference/cascade/modifier.md` §6 |
| [DEC-vicinity](#dec-vicinity) | Vicinity = a city's working-range plots (permissive radius scan); ALL special vicinity variants fold to ONE standard vicinity; duplicate same-target folds `combine:max`, never summed | `reference/cascade/enabler.md` §8 |
| [DEC-cascade-bidirectional](#dec-cascade-bidirectional) | The cascade is bidirectional (enabler `require` callback UP the chain); down-only was tried and fails AND-modeling + dumps maintenance on modders | `architecture/north-star.md` §2 |
| [DEC-no-guessing](#dec-no-guessing) | Do not guess/infer/assume ANYWHERE — an assumption IS a shortcut; map divergences to named sources, never infer a cause/answer/permission (binds agents + minions) | `AGENTS.md` |
| [DEC-all-means-all](#dec-all-means-all) | "ALL" = exhaustive/locust mode: recurse every aggregate to leaves, no judgment-filter; go exhaustive immediately (don't piecemeal); prove completeness adversarially | `AGENTS.md` |
| [DEC-maintenance-bookkeeping](#dec-maintenance-bookkeeping) | Maintenance & inflation are separate bookkeeping channels OUTSIDE the commerce chain; engine hard-deductors (distance/numCities/colony/corp) fetched from the live dump, JSON computes building-cost + modifiers | `reference/observability/gold-maintenance-inflation.md` §1-D.1 |
| [DEC-calc-zero-ride-in](#dec-calc-zero-ride-in) | The cascade calc computes EVERY value from JSON + game state; the ONLY raw dump ride-in is distance-from-capital maintenance; validate each part in ISOLATION (no aggregate-diff-chasing); build dry-first | `json-migration/calc-emulator-spec.md` §2a.2 |
| [DEC-kraken](#dec-kraken) | The OVERALL ruling: skipping/assuming/guessing/shortcuts/"perceived laziness" is the cardinal sin (despair index) — the codebase is a standardless kraken, so maximal rigor by default. The WHY behind every rigor rule | `AGENTS.md` |
| [DEC-fast-is-slow](#dec-fast-is-slow) | "Fast is slow, slow is fast" — reading the docs FULLY/completely is never the slower path; skimming to save 5% routinely costs far more downstream (re-derivation, wrong fixes, wasted context). Read gated/subsystem docs in full before acting | `AGENTS.md` |
| [DEC-map-before-delete](#dec-map-before-delete) | You cannot delete a maintainer you cannot fully observe; shadow it until clean, then cut | `AGENTS.md`; old `cascade-mapping-inventory.md` §A |
| [DEC-parity](#dec-parity) | **Parity is the ONLY goal** (reversed 2026-06-23) — exact match; no "adjacent"/tolerance/care-scale; a divergence is a data-collection gap, not a math difference | [`specs/validation.md`](../specs/validation.md) |
| [DEC-no-parity-results-in-docs](#dec-no-parity-results-in-docs) | Parity-pass results (divergence counts, checklists, pilot numbers) stay OUT of the docs — stale results poison contexts | [`specs/validation.md`](../specs/validation.md) |
| [DEC-tally-serializes-nothing](#dec-tally-serializes-nothing) | Tally + scope accumulators serialize NOTHING — rebuilt from loaded objects on load | [`reference/cascade/tally.md` §4](../reference/cascade/tally.md) |
| [DEC-save-remove-is-soft](#dec-save-remove-is-soft) | Removing a serialized field/Type is SOFT in the name-keyed format; only 4 cases are HARD | [`reference/engine/save-load-format.md`](../reference/engine/save-load-format.md) |
| [DEC-derived-never-trusted](#dec-derived-never-trusted) | Derived data is never trusted from a save; reset() marks dirty and recomputes | [`reference/engine/save-load-format.md`](../reference/engine/save-load-format.md) |
| [DEC-obs-scale](#dec-obs-scale) | The Observability Scale (0 Oblivious … 5 Meta) + the reconstruct-from-API "Orwell" bar | [`reference/observability/README.md`](../reference/observability/README.md) |
| [DEC-obs-hook-shapes](#dec-obs-hook-shapes) | The 3 canonical observability hook shapes (snapshot field / gated `[TAG]` log / mailbox `/diagnostic`) | [`reference/observability/README.md`](../reference/observability/README.md) |
| [DEC-interface-contracts](#dec-interface-contracts) | C++03 Clean-Architecture contracts: pure-virtual bases, MI = implements, poor-man's-DI at a composition root | `AGENTS.md` |
| [DEC-proper-once](#dec-proper-once) | Build the proper structure once — reject transitional shims | `AGENTS.md` |
| [DEC-keep-unkilled-ideas](#dec-keep-unkilled-ideas) | Retire only code-reconstructible-stale or explicitly-killed docs; an un-killed idea is kept (out-of-scope ≠ retire) | `_meta/CONVENTIONS.md` §7 |
| [DEC-WF-rulings-to-repo](#dec-wf-rulings-to-repo) | Every owner ruling → repo docs immediately, unprompted, same work item | `AGENTS.md` |
| [DEC-WF-no-commit-unmandated](#dec-wf-no-commit-unmandated) | Edit working tree only unless tied to an issue; never switch branches mid-build | `AGENTS.md` |
| [DEC-WF-surface-sprawl](#dec-wf-surface-sprawl) | Surface "getting out of hand"/undefined-structure to the owner instead of overcompensating with serial partial fixes; don't make the owner restate; optimise for efficiency | `AGENTS.md` |
| [DEC-ephemeral-project-folder](#dec-ephemeral-project-folder) | A massive one-time project gets a dedicated, explicitly-named, front-and-center folder, not held to the durable bar, deleted wholesale when done (durable knowledge extracted first) | `_meta/CONVENTIONS.md` §9 |
| [DEC-represent-dont-fit](#dec-represent-dont-fit) | A calc divergence means the EMULATOR is missing a mechanic — trace it to its named engine source(s) and represent it; NEVER skip/drop/invent a mechanic to fit the data. Changing the math = changing a game mechanic: allowed only for a bug proven beyond reasonable doubt, as a deliberate surfaced decision (not a quiet gap-closer) | [`reference/cascade/shadow.md` §5a](../reference/cascade/shadow.md) |
| [DEC-per-mechanic-parity](#dec-per-mechanic-parity) | Parity is verified MECHANIC-BY-MECHANIC against the engine's emitted per-mechanic value — NEVER by comparing or averaging aggregate/realized outputs. An averaged or whole-output gap hides offsetting per-mechanic errors (the kraken's cancellation trap: a wrong calc reads "≈0"). A channel is parity ONLY when every individual mechanic feeding it matches the engine exactly | [`reference/cascade/shadow.md` §5b](../reference/cascade/shadow.md) |

---

## Entries

### DEC-fixedpoint-x100
All cascade value math is integer fixed-point ×100; JSON is human-readable; the single human→int conversion
+ percent semantics lives only in readJson; the cascade never knows about ×100. **Home:**
[`reference/cascade/fixed-point-and-scales.md`](../reference/cascade/fixed-point-and-scales.md).

### DEC-per100-closed-set
The legacy per-100 fields are a CLOSED, verified set of exactly six `…100()` accessors — that set is the
curator's entire de-scale list; figure scale from the math, not the name. **Home:**
[`reference/cascade/fixed-point-and-scales.md` §4b](../reference/cascade/fixed-point-and-scales.md#4b-the-closed-per-100-set--100-to-humanize).

### DEC-curator-owns-descale
XML→JSON happens once; the curator absorbs all per-100-vs-normal mixing and emits uniform human numbers, so
readJson has zero per-field scale knowledge. **Home:**
[`reference/cascade/fixed-point-and-scales.md` §1](../reference/cascade/fixed-point-and-scales.md).

### DEC-deliveryguy
Ownership of an **entity-keyed (cross-entity) modifier** is decided by **semantic sense — "who BRINGS this
modifier to the table?"** That deliverer (the *deliveryguy*) OWNS it; the other entity is the **condition**
("what ENABLES it?"). Two equally first-class expression modes, chosen per-case by what reads sensibly:
- **keep-on-source** — the source owns it and references the other entity as an `enabled`/`per` condition
  (e.g. a civic's +happiness buff conditioned on `BONUS_X` stays on the civic).
- **fold-onto-the-deliveryguy** — the modifier lives on the delivering entity, keyed by the target (e.g. a
  route making an improvement better → the boost lives on the **route** keyed by improvement; a building
  making a terrain's tiles yield more → stays on the **building** keyed by terrain, **NOT** inverted onto
  the terrain).

Plot-substrate entities (terrain/feature/improvement/route) each own their *own* intrinsic output at plot
scope. This REFINES keep-on-source and **superseded the earlier "inversion" approach** — the discriminator
is *who delivers*, not conditioner-vs-target. **Home:** [`reference/cascade/modifier.md` §6](../reference/cascade/modifier.md#6-ownership--the-deliveryguy-rule) (owner ruling 2026-06-16).

**Refined 2026-06-20 (modifier.md §6.5/§6.6).** The explicit HOME RULE — *a modifier lives on the entity that
OWNS/GOVERNS the thing it modifies; a conditioner is REFERENCED (`enabled`/`requires`), never the home* —
producing two shapes: **own-output** (a specialist's/improvement's/unit's own output → on that entity, source as
`enabled`) and **governing-deliverer** (a route upgrading improvements, a building delivering religion influence →
on the actor, keyed by target). Conditioner AXES: a **tech** references on the **enabling** axis (`enabled`,
monotonic); a **religion/resource** on the **requiring** axis (`requires.operate`, reversible). **Corrections:**
`Building.SpecialistYieldChanges` is own-output → on the **specialist** (not keep-on-building). **DATA ≠ RUNTIME** —
JSON is organised for humans; `readJson` builds the links both ways at parse; target-landing inversion is a parse
transform, never a data shape (the reason the derived-data repository was retired). **No special cases** — the rule
decides every case (permissive but restrictive). Movement & range are two separate defined families
(`movement`+`moveCost` vs `range`). **Home:** [`reference/cascade/modifier.md` §6.5/§6.6](../reference/cascade/modifier.md#65-the-home-rule--own-output-vs-governing-deliverer-owner-refinements-2026-06-20) (owner refinements 2026-06-20).

### DEC-vicinity
**Vicinity is ONE simplified mechanic: the plots in a city's CURRENT working range** (a radius scan that grows
with culture), evaluated with **no ownership/worked filter** — deliberately more permissive than the legacy
semantics it subsumes (`isValidTerrainForBuildings`, `GOM_TERRAIN`, `hasVicinityBonus`, `hasRawVicinityBonus`).
Owner-accepted tradeoff: **two cities overlapping a bonus plot BOTH qualify** (only one *works* it).
**ALL "special" vicinity variants fold into this one standard vicinity** (`connection: vicinity`) — raw-vicinity,
the `PrereqOrTerrain/Feature/Improvement` plot prereqs, etc.; there is **NO** separate raw/valid handling. When the
same target arrives via multiple vicinity types, **fold to one and take the HIGHEST value (`combine: max`), never
summed**. **Established 2026-06-16, before the JSON pass.** This DEC exists because successive agents repeatedly
forgot/misinterpreted the ruling (it sat buried in enabler §8, its `max` half split into modifier §7, and absent
from this ledger) — **grep here first.** **Home:** [`reference/cascade/enabler.md` §8](../reference/cascade/enabler.md)
(+ [`reference/cascade/modifier.md` §7](../reference/cascade/modifier.md) for the `combine: max` fold).

### DEC-cascade-bidirectional
The cascade is **bidirectional**, not down-only: the enabler resolves its `requires` by a `require` callback
**UP** the scope chain. `requires` is precisely the **AND** mechanism — it is **mapped on the subset of data
enabled through the enabler chain**, resolved up-chain. Down-only was the *original* design and was abandoned
during iteration because
**(1)** it models **OR** (via enablers) but **cannot reliably model AND**, and **(2)** it forces a modder to
maintain every requirement at the **top of the chain** — a maintenance nightmare. The upward `require`
callback is **load-bearing, not optional**; do not "simplify" back to down-only. **Home:**
[`../architecture/north-star.md` §2](north-star.md) (owner ruling 2026-06).

### DEC-no-guessing
**GENERAL CONDUCT (owner ruling 2026-06-20): do not guess, do not infer, do not assume — an assumption IS a shortcut**,
binding every agent AND every spawned minion, not just modifier work. Never infer a CAUSE you have not mapped, never
assume an earlier verification still holds, and never infer an ANSWER or PERMISSION the owner has not given ("keep going"
authorizes work, never a specific open decision; a question you posed is a HARD STOP until answered). The only moves at a
gap are VERIFY against ground truth, or ASK. The original instances below are applications of this.
Do not hypothesize a divergence's cause and try a fix; emit the full legacy decomposition via the dump and
attribute the divergence to a named source with numbers. If the data isn't emitted, emit it first.
**The same rule binds the data classification/curator** (owner ruling 2026-06-20): the `classify-building` workflow
NEVER invents/best-fits/"SHOEHORN?"s a home — a home is assigned ONLY from a VERIFIED live consumer; anything
ambiguous is emitted as `needsRuling` / `home:"NEEDS_OWNER_RULING"` for the owner to rule (the old `creativeMechanic`
/ `shoehornRisk` guess flags were ripped out). A guessed home is the 30-hours-of-guessed-XML failure this prevents.
**Home:** [`AGENTS.md`](../../../AGENTS.md) — the Conventions bullet "DO NOT GUESS, DO NOT INFER, DO NOT ASSUME — an assumption IS a shortcut" (general conduct) + "THE NO-GUESSING RULE" (divergence mapping); minion brief `.claude/agents/data-reader.md`; workflow `Tools/Migration/workflows/classify-building.js`.

### DEC-all-means-all
When the owner says do **ALL** of something, it means **EXHAUSTIVE — "run over the codebase like locusts in a
cornfield"**: enumerate every item mechanically, **recursing into every aggregate down to its leaf sources**, handling
each — never filtered by a judgment call ("needed / dead / python-only / private / probably fine"). That judgment is
the bug: a single agent's "do I need this?" is *systematically biased toward dropping items*, so a self-certified
"exhaustive" pass is NOT exhaustive — a careful solo pass on the diagnostic dump still missed **77** sources, caught
only by mechanical recurse-everything + an **adversarial re-check** (a second pass that assumes incompleteness and
hunts for a miss; fan it out, one minion per area). Operational: **(1)** go exhaustive *immediately* — partial
passes / per-item asking is the "untold hours of endless wrangling" anti-pattern and is slower; **(2)** prove
completeness adversarially, never by self-assertion. Load-bearing because on the live-shadow parity path (the offline
emulator was dropped) a single un-emitted source hides in an aggregate → the divergence is unattributable → the
guess/despair spiral. **Worked example:** the espionage commerce cascade ran −80 below legacy for every city and
stayed an unattributable mystery for hours; the instant `buildingCommerce100` was split into its four sub-sources
(pure/bonus/tech/perPop — a split that "probably didn't need" decomposing) the −80 named itself: per-pop building
commerce (78) + specialist (1) + playerExtra (1). The aggregate hid it; the exhaustive split named it.
**This is the operational form of the total-observability ("Orwell") bar** (AGENTS.md) and [[DEC-map-before-delete]]:
you cannot delete a legacy maintainer you cannot *fully* observe, and an aggregate that hides one source is a blind
spot in that surveillance — so "more is always better than less" is not a vibe, it is the parity prerequisite.
Sibling of [[DEC-no-guessing]] and [[DEC-proper-once]]; scoping never skips a source (promote
private getters — zero sensitive data in a game mod). **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions
("'ALL' means EXHAUSTIVE — locust mode"); the reusable audit is the `dump-completeness-audit` adversarial workflow.

### DEC-maintenance-bookkeeping
**Maintenance (and inflation) are SEPARATE BOOKKEEPING channels, computed OUTSIDE the commerce chain (owner ruling
2026-06-20)** — added on top of the bottom line like any accounting entry, NEVER folded into the gold-commerce family.
`maintenance` is its own JSON family (city + empire). Three source classes, and the split is *why* every source had to
be mapped ([[DEC-all-means-all]]): **(1) engine hard deductors** — distance-from-capital / numCities / colony /
corporation — engine-computed, NOT in JSON, so the calc-emulator FETCHES them from the live dump as a given base;
**(2) JSON building gold COST** — a building's gold upkeep (legacy `TREAT_NEGATIVE_GOLD_AS_MAINTENANCE`), currently
MIS-HOMED as negative `gold.flat` and to be moved to the `maintenance` family (curator follow-up; routing it out
dropped the gold-commerce divergence from −44.6% to single digits); **(3) JSON maintenance MODIFIERS**
(`maintenance.{city,empire}.percent`, exists). The JSON is only the STATIC truth; the engine adds the rest, so the full
calc is reverse-engineered as JSON-computed parts + engine-fetched parts. `cascade_sim` verified
`baseMaint100 = building+distance+numCities+colony+corp`, `realized = baseMaint100×(100+effectiveModifier)/100`: −0.4%
(capital) and −0.6…−1.9% (non-capitals, distance deductor nonzero). **Home:**
[`reference/observability/gold-maintenance-inflation.md`](../reference/observability/gold-maintenance-inflation.md)
§1-D.1. Sibling of [[DEC-all-means-all]].

### DEC-calc-zero-ride-in
**The cascade calc EMULATES legacy by computing every value from the NEW JSON + game STATE — ZERO legacy-computed
ride-in (owner ruling 2026-06-20).** Riding in a legacy number never validates the new JSON for that part AND collapses
at the atomic cutover when legacy reads are deleted ("the second you ride in any yield, we are royally fucked"). The
**only** raw number taken from the dump is **distance-from-capital maintenance** (map-derived, not JSON, not
reconstructable; even numCities/colony are computable from the city count). Everything else is computed: per-worked-plot
yields (terrain+feature+improvement+bonus+route+river from JSON × the plot's contents) summed up; specialists (JSON
values × assignment counts); commerce (computed yield × slider + JSON deposits); the modifier (JSON). State the calc
*reads* (inputs, not legacy values): tech/city counts, population, worked plots + their contents, specialist
assignment, slider, property values, building age, tally counts; full vicinity for bonus-gates. **Methodology:**
validate each part in **ISOLATION** (one plot, one specialist, one building) to no-diff — the aggregate is then
diff-free by construction; aggregate-diff-chasing is the ghost-hunt (e.g. `getBaseCommerceRateFromBuilding100` is
engine-modifier-inclusive, not JSON-comparable; the `*Raw100` dump field is). **Build DRY-FIRST** — fictitious
plot/loadout in isolation, verify the arithmetic, before wiring to live dumps. **Home:**
[`json-migration/calc-emulator-spec.md`](../json-migration/calc-emulator-spec.md) §2a.2. Sibling of [[DEC-all-means-all]].

### DEC-kraken
**The OVERALL ruling these all serve (owner ruling 2026-06-20).** Skipping something, assuming something, guessing
something, taking a shortcut, or in general **"perceived laziness" is the cardinal sin — punishable, literally, by
the despair index.** *Why (owner, verbatim):* "this codebase is the kraken that will eat your ship, spit you out and
crush you. It is legendary in its lack of standard, coherence, or any reasonable consideration to common sense. So we
act accordingly." In a coherent codebase a small assumption is usually harmless; in THIS one there is no underlying
standard to make it safe, so every shortcut is the move that gets the ship eaten. Operating posture: **maximal rigor
by default** — verify everything, enumerate exhaustively, zero shortcuts; treat any pull toward "probably fine / don't
need it / good enough" as the kraken's bait. **Standing default, not a phase (owner ruling 2026-06-20):** the earliest
this absolute completeness could relax is AFTER a complete refactor has dropped ~half the existing lines — and per the
owner, "honestly, maybe not even then." Until the owner *explicitly declares otherwise*, maximal rigor is the default posture. This is the umbrella WHY behind [[DEC-no-guessing]], [[DEC-all-means-all]],
[[DEC-map-before-delete]], the total-observability ("Orwell") bar, and "nothing is ever just a
one-liner". **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions ("THE KRAKEN RULE").

### DEC-fast-is-slow
**"Fast is slow, slow is fast" (owner ruling 2026-06-21, GLOBAL).** Reading the docs FULLY and completely is **never**
the slower path. If you think skimming a doc — or reading only the section you assume is relevant — is faster, you are
**wrong**: in this codebase the 5% "saved" by skimming routinely costs far more downstream in re-derivation, wrong
fixes built on a misread, and burned context. *Why it is load-bearing (the instance that prompted it):* on the
#428/#430 cascade calc, skimming `migration-renames.md` on the simple/complex **trait split** led
to **five** rounds of reverse-engineering a procedure that was already "clearly laid out, clearly documented, procedure
already nailed" — wasting ~half a context window to save a few minutes of reading. The moment the doc was read in full,
the fix (a `rid→base` translation table) was obvious and landed immediately. So: **read each subsystem doc in
full BEFORE acting**, not the grep-the-keyword shortcut — slow is fast. Sibling of [[DEC-kraken]] / [[DEC-no-guessing]] (skimming
is a shortcut; a misread becomes an assumption). **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions.

### DEC-map-before-delete
You cannot safely delete a maintainer you cannot fully observe; every state behaviour gets a shadow diffing
cascade vs engine until clean, then the legacy is deleted. **Home:** [`AGENTS.md`](../../../AGENTS.md); old
`cascade-mapping-inventory.md` §A. Related: [[DEC-obs-scale]], [[DEC-no-guessing]].

### DEC-parity
**Parity is the ONLY goal — exact match, full stop (owner ruling 2026-06-23, reversing the earlier "parity-not-goal /
parity-adjacent" stance).** No "close / same ballpark," no tolerance band, no agent grading of acceptability — that
framing (and the retired six-rung "care scale") was constantly abused to wave mismatches through as good-enough. No
bug has surfaced in any actual legacy *calculation*, so the math matches; a divergence is therefore a
**data-collection gap** (a source the cascade didn't gather), never a formula difference — map it and close it.
**Home:** [`specs/validation.md`](../specs/validation.md). Related: [[DEC-map-before-delete]], [[DEC-no-guessing]].

### DEC-no-parity-results-in-docs
**Parity-pass results stay out of the durable docs (owner ruling 2026-06-23).** Divergence counts, parity
checklists, per-pass pilot numbers — none of it goes in the docs; stale results **poison contexts** (an agent
fixates on a number and misdiagnoses — a ~1100-building enable diff was repeatedly misattributed to a band-model
change it had nothing to do with). The spec says what the model **is**; the curator code + the live shadow prove it;
the result is ephemeral. This is also why the migration-verification doc set is drastically reduced — the old→new
map lives as **curator comments**, not a documented result. **Home:** [`specs/validation.md`](../specs/validation.md).
Related: [[DEC-parity]], [[DEC-no-guessing]].

### DEC-tally-serializes-nothing
The tally + scope accumulators serialize nothing — rebuilt from authoritative loaded objects on load; true
historical counters live on their owning object. **Home:**
[`reference/cascade/tally.md` §4](../reference/cascade/tally.md). Related: [[DEC-derived-never-trusted]],
[[DEC-save-remove-is-soft]].

### DEC-save-remove-is-soft
The name-keyed save format makes removing a plain member SOFT; Type/XML churn is free for class
enums/arrays; only 4 cases are HARD. **Home:** [`reference/engine/save-load-format.md`](../reference/engine/save-load-format.md).

### DEC-derived-never-trusted
Derived data is never trusted from a save: `reset()` marks it dirty on load and recomputes from live state.
**Home:** [`reference/engine/save-load-format.md`](../reference/engine/save-load-format.md) (the
recompute-on-load model; the repository it came from is superseded — see
[`superseded-ideas.md`](superseded-ideas.md)). Related: [[DEC-tally-serializes-nothing]].

### DEC-obs-scale
Observability Scale: 0 Oblivious · 1 Telescreen · 2 Informant · 3 Big Brother · 4 Thought Police · 5 Meta.
Reconstruction bar: rebuild game state from HTTP + `/events` + gated logs, never the screen. **Home:**
[`reference/observability/README.md`](../reference/observability/README.md).

### DEC-obs-hook-shapes
Three canonical hook shapes: (1) a snapshot field on `/players`|`/cities`|`/units`; (2) a gated `[TAG]` log
teed to `/events` via `streamLogTee`; (3) a mailbox `/diagnostic/*` endpoint. **Home:**
[`reference/observability/README.md`](../reference/observability/README.md).

### DEC-interface-contracts
Depend on interfaces, not concretions. C++03 interface = abstract base, pure-virtuals, no data members; MI
is the `implements` axis; wiring is poor-man's-DI (`if`/switch at a composition root); graft onto
DLL-derived classes, never EXE-bound bases. **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions.

### DEC-proper-once
Build the proper structure once; reject transitional shims that only defer the real design; isolate
components behind interface-bounded surfaces. **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions.

### DEC-keep-unkilled-ideas
Retire a doc ONLY if it is reconstructible-from-code-and-unneeded, or an explicitly KILLED idea. Forward
design intent that has not been killed is **kept** (partitioned if out of active scope) — it is not
reconstructible from code and exists nowhere else; out-of-scope is never a reason to retire. Losing
un-killed intent is the one unrecoverable deletion. **Home:** [`_meta/CONVENTIONS.md` §7](../_meta/CONVENTIONS.md) (owner ruling 2026-06-19).

### DEC-WF-rulings-to-repo
Every owner ruling → the right repo home immediately and unprompted, same work item; memory-only is
unfinished. This ledger is the discoverability half. **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions.

### DEC-WF-no-commit-unmandated
Only branch/commit/PR when tied to an active issue; otherwise edit the working tree only; never switch
branches mid-build; verify the branch immediately before any commit. **Home:** [`AGENTS.md`](../../../AGENTS.md).

### DEC-WF-surface-sprawl
When a change starts to sprawl (serial partial fixes piling up) or you are patching pieces of something whose
target STRUCTURE is undefined, STOP and tell the owner there's a risk of it getting out of hand — don't
agent-overcompensate with more partial fixes. The owner (aware of agentic limits, not omnipotent, expects
trust-but-verify) must be *told* so they can define the structure and authorize ONE proper cleanup; they get
frustrated by inefficiency and by having to restate the same thing — so capture rulings durably the first time
and don't churn or re-litigate. Conduct-level twin of [[DEC-proper-once]]; the docs-rebuild mess is the
motivating case. **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions (owner rulings 2026-06-20).

### DEC-ephemeral-project-folder
A massive ONE-TIME project (a migration, a from-scratch rebuild, a big sweep) gets its own **dedicated,
explicitly-named, front-and-center folder** under `docs/dev/` (worked example: `json-migration/` for #428/#430).
It is **not held to the durable grounding bar** (transient working docs), and is **archived OUT of the active
docs scope when the project completes** — moved out in one step like `old-docs/`, **not deleted** (it stays
referenceable later) — after any durable references are repointed into the durable set
(`reference/`/`explanation/`/this ledger) so the active set doesn't dangle into it. The folder keeps a running
**`TO-BE-MADE-DURABLE.md`** index (append-as-you-go pointers to durable knowledge accumulating inside it);
archiving is gated on working that index to empty. Distinct from
`plans/parked/` (un-killed intent, kept) and `reference/` (durable, permanent): a one-time-project folder is
*active now, archived out when done*. **Home:**
[`_meta/CONVENTIONS.md` §9](../_meta/CONVENTIONS.md) (owner ruling 2026-06-20). Related: [[DEC-keep-unkilled-ideas]].
