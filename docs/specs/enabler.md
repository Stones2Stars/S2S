# The enabler — "can I?"

The cascade machine that decides **what an entity is allowed to do or build right now** — research a tech, train
a unit, construct a building, adopt a civic, lay an improvement. It answers one question per candidate: *"can I
take this action this turn?"* — and as a byproduct, *why not* (greyed / hidden).

It **reads** the availability data authored on entities — `enables`, `obsoletes`, `replaces`, `disables`,
`requires`, `allowed` (the [json spec](json.md) §4 owns their shape). This doc is the **machine** that consumes
them; it does not restate the JSON syntax.

---

## 1. The one idea: GENERATE, then GATE

Availability is **two passes that cannot fold into one** — you must *build the candidate list* before you can
*check each candidate*:

1. **GENERATE** the candidates — from everything you HAVE, what does it unlock? (the `enables` family).
2. **GATE** each candidate — are its `requires` satisfied, and is it under its `allowed` cap?

> `available(X)  =  X was generated  ∧  X.requires met  ∧  X under its allowed cap`

The two passes narrow through three sets:

| set | what it is |
|---|---|
| **HAVE** | what you actually possess — built / researched / adopted |
| **CAN GET** | the candidate frontier — everything HAVE unlocks, minus what's been removed |
| **HAS THE MEANS** | the candidates whose `requires` are met (the buildable set) |

**The two passes are not peers — pass 1 is the authority, pass 2 is the follow-up.** The `enables` family
(`enables` / `disables` / `replaces` / `obsoletes`) is the **sole authority on what is in the tree** (CAN GET) —
what you can *actually do*; it alone adds and removes candidates, and it runs **to completion first**, producing
the final tree. **`requires` runs afterward and CANNOT change tree membership** — it never adds or removes a
candidate, it only decides whether a tree member is **attainable now** (buildable) or **unattainable** (greyed,
or dormant once built). A failed `requires` leaves the thing in the tree, just out of reach.

Both passes read **forward** — `enables` forward from the source, `requires` forward from the target — so the
hot path never does a reverse lookup. The sets are **recomputed on demand**, not cached (§7).

---

## 2. Pass 1 — GENERATE the frontier (the `enables` family)

Four source-side edges drive generation. All read forward over HAVE: `enables` **adds** to the candidate set,
the other three **remove** from it.

| edge | nature | new builds | existing instances |
|---|---|---|---|
| **`enables`** | a permanent unlock | **added** to CAN GET | — (this *is* the unlock) |
| **`disables`** | a reversible ban / effect | removed while the disabler is held | **destroyed** (a law) **or dormant** (an effect) — see json §4.2 |
| **`obsoletes`** | passive supersession | removed | **persist** (an obsolete unit stays on the map); the target decides its own fate |
| **`replaces`** | succession | removed | replaced by the successor (transitive chain) |

So **`CAN GET = union(enables) − (disables ∪ obsoletes ∪ replaces)`**, all over HAVE.

**`obsoletes` vs `disables` kept SEPARATE for clear semantics** (progress-supersedes vs policy-forbids) + the
pedia line ("Obsoleted by [tech]"). `disables` = a hard "be gone" (the source commands; the target gets no say);
`obsoletes` = a soft signal — the instance's fate is authored on the TARGET via `whenObsolete` (wonders/walls
lose their bonus but REMAIN as culture/tourism).

**The fixed run order — `replaces` → `disables` → `enables` → `obsoletes`.** The *membership* of CAN GET is
order-independent (set-difference is commutative); the fixed order matters only for two **propagation** effects:
a possession change (if a law `disables` a building, anything that building `enables` must also drop) and
instance-fate precedence (`replaces` wins over `obsoletes`). So: collapse succession chains, drop
banned/destroyed things from HAVE, *then* generate from the corrected HAVE, *then* prune obsoleted candidates.
**Tech is authored in `enables`** (a tech `enables` what it unlocks) — never as a generation driver in `requires`.

**`disables` — worked cases.** `BUILDING_POLLUTION_BLACKENED_SKIES` disables telescope/observatory (dormant
while blackened, reactivate when clear); a rat-catcher disables the disease pest band; the lone converted
law-disable today is the per-civ Neanderthal research ban (`TECH_SEDENTARY_LIFESTYLE`, reversible).

**Multi-parent tech.** A child tech carries `requires.build.all:[T1,T2]` (AND) or `.any` (OR); the `.any` here **is**
the [json](json.md) §3.4 combinator (a list of OR-groups), so a single OR of techs is authored as ONE group —
`requires.build.any:[[T1,T2]]`, not a flat `[T1,T2]`. `enables` proposes
the child from one parent, `requires.build` confirms all parents forward from HAVE. The curator must RETAIN
`AndPreReqs`/`OrPreReqs` as `requires.build.all`/`.any` **because the store's prereq-inversion flattens them
into other techs' `enables` for generation and does not keep them on the child** — so the curator re-reads them
off the child. `requires.build` only —
techs are monotonic, no `operate`.

**Empire/team-scope constructables need NO new machinery** (the scope spine already has team/empire): stage-gates
via `enables` (the space line), doctrine bans via `disables` + empire modifiers — replacing the `FreeBuilding`
autobuild clunk (~345 uses) with one empire-scope building. (INTERIM — a later issue; the per-city machinery
still works.)

> **Known-open design point (not your gap):** the `disables` **law-vs-effect** fate — the two fates themselves
> (destroy-and-rebuild vs go-dormant) are **settled**; what is OPEN is **how a source declares which fate it
> carries**. That declaration mechanism is the deferred/post-migration part (it lands with the engine
> implementation), not the fates or their intent.

---

## 3. Pass 2 — GATE each candidate (`requires`)

`requires` answers *"do I have the means?"* — checked **forward** (is this atom in HAVE?). It is authored on the
**target**, in two timings ([json](json.md) §4.3):

- **`build`** — needed to construct; **greys** the candidate if missing. Checked once, at build.
- **`operate`** — needed to construct **and** to keep running; re-checked every recompute. Lose it after
  building and the thing goes **dormant** — inactive, not destroyed — and wakes when the condition returns.
  (Units carry `build` only; they're leaf actions that exit the model once built.)

So the build-time gate = `build ∧ operate`; the ongoing dormancy gate = `operate` only. A `noneOf` clause is the
**dormancy negative** ("dormant *while* X is present") — distinct from a source-side `disables` ban by fate
(dormant-and-reversible vs destroyed-and-rebuilt) and author (the target vs the law).

**Pseudobuilding bands.** Legacy `CvPropertyInfo` `iMinValue`/`iMaxValue`/`BuildingType` + `checkPropertyBuildings`
(each turn) adds/removes a building as the property value enters/leaves the band. End-state: retire the per-turn
churn — model the band as uniform `requires.operate` dormancy (enabled once; toggles active/dormant); `notConstructible`
(an `identity` flag, [json](json.md) §7) is the **interim** until the `requires.operate` dormancy end-state lands. ⛔ **Bands REPLACE legacy parity (the Education ladder) — a higher band supersedes a lower one;
do NOT stack.** Bands are **bidirectional** — effect-buildings can spawn on the **negative** side, not just the
positive ladder; a negative band is being considered for **every property** (education is the live case).

**`requires.operate` on a UNIT** (FUTURE — e.g. tanks need fuel) would reversibly disable an existing unit while
it stays on the map; the structure supports it, but it is not modelled now.

**VICINITY** (enabler-specific) = the city's current workable radius, which **grows with culture** (1→2→3 rings),
NOT fixed; a plot can lie in two overlapping cities' vicinity (counts for both). The terrain/feature/improvement
plot scan has NO ownership/worked filter — deliberately MORE permissive than the two legacy checks it replaces;
`workedBy:SELF` is the tightening path if the overlap is later judged wrong.

### 3.1 The cache-friendly two-stage evaluation

Every `requires` resolves the same way, so it's cacheable as a pure function of clause-shape + state:

1. **combinator** — the `all`/`any`/`noneOf` structure ([json](json.md) §3.4), verified vs `CvCascadeCondition.cpp`:
   **`all` = AND** (a flat list of leaves), **`any` = a list of OR-groups** (within a group OR, between groups AND),
   **`noneOf` = NONE**. One level, no nesting — AND-of-ORs is `any`'s multiple groups (or a sequential `requires`/`disabled`).
2. **conditions** — each leaf: a presence/count **atom** (`min`/`max` at a scope) or a **predicate**. A count at
   `city`/`plot` reads the live object; at `empire`/`team`/`world` it reads the [tally](tally.md). A missing
   predicate is **ignored**, never false (json §3.5) — so retiring a system never spuriously disables data.
   **Tally-bucket routing is by TYPE PREFIX** (`BUILDING_`/`UNIT_`/`BONUS_`/…), no separate `kind` field; author
   resource presence as `min(BONUS_X,1)` (the N=1 case) — volumetric-ready.

---

## 4. The `allowed` cap

`allowed` ([json](json.md) §4.4) is a separate gate from `requires` — "how many of **me** may exist," not "what
I need." A build is permitted while **`count(me, scope) < allowed`**; the count comes from the [tally](tally.md).
The engine owns ignoring caps under game options / era-scaling — the machine just compares.

---

## 5. The load-bearing asymmetry — bidirectional, not down-only

The cascade is **bidirectional**: generation flows down from sources, but the `requires` gate resolves by a
**callback UP the scope chain** — a city-scope candidate asks its empire/team/world about civics, counts, state
religion. This is **how the model expresses AND** (every clause must hold, possibly at different scopes), and it
is **not optional**:

A pure down-only design (sources push everything onto targets) was tried and abandoned — it can model **OR**
(many sources enable one thing) but **cannot reliably model AND**, and it forces a modder to maintain every
requirement at the top of the chain. The upward `require` callback is load-bearing. Do not "simplify" it back to
down-only.

---

## 6. Greying — the build-list tri-state falls out for free

The same gate that decides buildability yields **why** a thing isn't buildable — no separate "why greyed" pass.
Each clause carries a disposition (set once by its kind):

| state | condition |
|---|---|
| **HIDDEN** | not in CAN GET — generation never reached it (or it was obsoleted/replaced/banned away) |
| **LISTED** (buildable) | in CAN GET ∧ all `requires` met ∧ under `allowed` |
| **GREYED** | in CAN GET ∧ only *greyable* clauses unmet — a connectable resource, an unadopted civic (named to the player) |

Grey vs hide is a **UI choice per clause**, not engine behaviour: author a resource on `requires` to **grey**
(surfacing "go get copper"), or on `enables` to **hide** until present. General lean: grey on resources.

**The frontier is one shared choice set — UI *and* AI.** It is computed once per recompute; the UI greys from
it, and the AI's production decision iterates **only this small frontier** instead of scoring the whole entity
database. That consolidation — one recompute replacing dozens of scattered ad-hoc `canBuild` checks — is the
biggest systemic win.

---

## 7. Recompute cadence — `f(HAVE)`, recomputed when HAVE changes

The frontier is a pure function of HAVE, so it is **recomputed when HAVE changes**, not cached with deltas. The
dominant cadence is once per turn, but same-turn HAVE-changes must trigger a mid-turn recompute (the AI finishes
building A then builds B the same turn; religion spreads; a bonus connects; a city is conquered). This stays
cheap: the bounded two-pass over the affected scope is *less* work than the scattered legacy checks it replaces,
which already re-scan the whole database constantly. Any caching is a separate optimization layer wrapped around
the pure `HAVE → frontier` function, never leaking into the model.

**Mid-turn HAVE-change triggers** also include **inquisition** (which retracts a RELIGION, not just a building —
disproving "buildings-only" state-retraction), nuke, and `doAutobuild` add/remove.

**Gather order — "right-then-down".** Pass 1 gathers in dependency order: sticky top (techs/civics) first, then
volatile bottom (resources/bonuses/buildings), so derived have-entries resolve against what's already gathered.

**Load-time gate — `loadPrune` (a separate axis).** LOAD-STABLE gates (`loadPrune`, the `CvInfoReplacements`
swap, WorldBuilder/BUG, a per-civ research ban) resolve at load and never materialize if false; DYNAMIC gates
(tech/civic/bonus presence) flip in play → matched each recompute.

---

## See also
- [json.md](json.md) — the data this machine reads: `enables`/`obsoletes`/`replaces`/`disables` (§4.1–4.2),
  `requires` build/operate (§4.3), `allowed` (§4.4), and the `all`/`any`/`noneOf` + atom/predicate vocabulary (§3).
- [tally.md](tally.md) — the count machine the `requires` count-atoms and the `allowed` cap read at cross-city scopes.
- [modifier.md](modifier.md) — the sibling "how much?" machine. A dormant/unavailable entity (per this doc)
  simply deposits no modifiers.
- [naming.md](naming.md) — the `INFOTYPE_NAME` ids that fill the `enables` buckets and `requires` atoms.
