# ⛔ WHAT THE ENABLER IS NOT — tech-tree PATHING AND QUEUING BELONG TO THE TECH-PICKING LOGIC

> Part of the **[08-the-machines-shape-components-host](../08-the-machines-shape-components-host.md)** spec.

The enabler answers **"can I, right now?"** and stops there. Two research features are **NOT its concern**:

- **QUEUING FURTHER THAN THE TREE** — a player may queue a tech that is not in CAN GET yet (several steps away).
- **THE EASIEST PATH** to a chosen tech — the cheapest prerequisite chain from what is currently held.

Neither is the enabler's concern; both belong to the tech-picking logic. Both are
**research-only and only needed inside the TECH-TREE BROWSER**. They are structurally impossible for the
enabler anyway: its maintained frontier holds only what is unlocked NOW, so it cannot see a candidate three steps
out — that answer comes from the **static compiled `enables`/prereq edges** the infos carry
([patterns.md § THE WHAT-IF DRIVER](../../../architecture/patterns.md)), walked COLD by the picking logic. A path search
is a genuine graph walk, which is acceptable on a browser path and would be unacceptable on the frontier.

⛔ So do NOT grow path-finding, queue projection, or a reachability closure inside the enabler. The enabler
supplies the FACTS (held / statically barred / removed / the gate verdict); the picking logic composes the route.
This is the [north-star](../../../architecture/north-star.md) test applied — ask *whose job is this?* and the answer
names the picking logic, not availability.

**⚑ AND IT NEEDS NO NEW MACHINERY EITHER — the picking logic just HYPOTHETICALLY FINISHES a tech.** It
takes the maintained planes, overlays "as if this tech were held" (which contributes that tech's `enables` edges),
re-applies the §7.1 membership formula, and repeats — walking outward until it reaches the target. That is the
whole of both features: queuing beyond the tree is one such step, the easiest path is the cheapest chain of them.
The raw membership reads (`enableCount` / `removeCount`) are public precisely so a composite gate can OVERLAY
per-instance planes on the maintained ones before applying the formula; `EnablerOverlay` is the ONE
implementation of that shape and every hypothetical asker is a consumer of it, never a second overlay.
⛔ The overlay is the CALLER's, held in the caller's own scratch: it never writes the maintained planes. A
hypothetical that mutated the domain would leave the real frontier describing a game state that never happened.
⛔ **The formula itself is NOT re-implemented alongside it** — the overlay and the maintained refresh resolve
membership through the same `EnablerDomain::isMember` ([the DRY single-implementation law](../../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
A second copy would diverge the first time the formula gained a term, and a hypothetical that disagrees with the
frontier it is overlaid on is worse than no hypothetical at all.

⚖ **A WHAT-IF ASKS BOTH HALVES, AND THEY ARE ASKED SEPARATELY.** *"Would I be able to build X if I adopted this
civic"* resolves as **membership** (`EnablerOverlay` over the enable/remove planes) **AND** the **gate**
(`requiresMetInCity` with the hypothetical). A candidate can be gate-satisfiable under a hypothetical and still
not be in the tree, and the reverse — so collapsing the two into one test silently answers a different question.
⚑ Adopting a civic is a **SWAP**, so each side states both halves: the civic held and the one it displaces
dropped. An empty option slot displaces nothing.

⛔ **A BONUS IS NOT AN OVERLAY SOURCE, and the overlay refuses one.** The curator authors bonus `enables` edges
(the reverse-mapped view of the target's retained `requires` atom) but the runtime never counts them — the bonus
axis is GATE-ONLY (§8, the settled model rulings). Folding them would hand the hypothetical an edge class the maintained
planes have never had, so every HIDDEN candidate whose inbound edge is that bonus would read as newly unlocked
when acquiring it changes no membership whatsoever. *"Would this bonus let me build X"* is a **`requires`-GATE**
question — re-evaluate the candidate's `requires` with the bonus injected into the eval ctx — and it is a
separate mechanism from this one, never a widening of it.

**⚖ THE RESEARCH SEARCH DEPTH IS A LEADER VARIABLE.** It bounds both the candidate walk and every
path-length test in the tech pick, so it is the ONE knob that tunes how far ahead an AI commits — and it is
therefore PERSONALITY, never a constant. It is authored as `ai.personality.researchSearchDepth` on the
LEADERHEAD; an unauthored leader takes the default, so per-leader values are pure data.
⚑ **This is the dial that governs BEELINING**, which is why it is worth having at all: the depth is exactly how
many hops past the researchable frontier a single distant unlock can pull an AI, so it is the lever on the
over-valued-enablement problem ([AGENTS.md](../../../../AGENTS.md) § AI valuation of ENABLEMENT — relaxing enablement
pull is only ever an improvement).
⚠ **The picker's other depth arguments are OVERRIDES, not depths** — a human's picker and a committed
culture-victory AI both ask for the immediate best (depth 1) rather than a plan, and neither becomes
personality-driven.
It belongs to the picking logic, like everything else in this section — never to the enabler.

**⚖ THE "EVER" QUESTION IS THE PICKING LOGIC'S, AND IT ALREADY OWNS IT.** HIDDEN conflates *"nothing enables it
YET"* with *"it can never be offered"*, and a research QUEUE asks precisely that difference — a target is chosen
now and researched later, so "not currently offerable" is not a refusal. ⛔ That is **not a gap in the tri-state
to fill**: per the boundary above it is a picking concern, and `CvPlayer::canEverResearch` is its existing, single
implementation, carrying the PERMANENT bars the enabler does not model as membership — the game-option bars
(`NO_FUTURE`, a tech's `PrereqGameOption`), the world-unique rule (*"religion techs are global and can only be
invented once by one player in a game"*) and the limited-religion hoarding guard.
⚠ **Do not re-derive it on the availability surface.** A second "ever" predicate reading only the membership
planes silently drops those bars — it would call a religion tech already invented elsewhere a legitimate queue
target ([the DRY single-implementation law](../../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
⚑ **It is published to Python as `CyEnabler::canEverResearch`, and the tech-tree browser MUST use it** — the
plane is `CyEnabler` because the QUESTION is availability, while the answer delegates to the picking logic;
the binding is not the enabler machine answering "ever".
⛔ **A consumer that reads HIDDEN as "never" is the failure this exists to prevent, and it is not hypothetical:**
the browser did exactly that, so every tech past the immediate frontier rendered permanently barred AND refused
its queue click — one state driving both the colour and the gate. A tech further along is HIDDEN for the
ordinary reason that nothing held enables it YET, which is precisely the difference a queue asks about.
The split, stated once: **the enabler answers CAN-I-NOW (the tri-state); the picking logic answers CAN-I-EVER and
BY WHAT PATH.** The two membership bars that ARE the enabler's — `identity.disable` and a civilization's own
never-researchable list — are static for a player's life and sit on the static-exclusion plane at `initDomain`.

⚖ **BUT WHERE THE BAR *IS* AN ENTITY GATE, THE EVER QUESTION IS THE ENABLER'S — AND SO IS THE OPTION READ
.** For any unit or promotion relying on game options — and anything else the enabler deals with — calling
`hasGameOption` is the enabler's job. A whole-entity game-option bar authors as the entity-level
`enabled`/`disabled` pair ([the whole-entity applicability gate](../../json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity)), so answering "is this
barred for the whole game" is just evaluating that gate — availability data, read by the availability machine.
`EnablerKernel::everAvailable(bucket, id)` is that ONE implementation, parameterized over the domain axis rather
than split per domain, and it is where the option read lives for every entity-gated domain.

- **It is TOTAL by construction.** `CvInfo::getGate()` is declared on the BASE returning `NULL` and
  `cascadeGateOk(NULL, …)` is true, so a domain whose data authors no gate answers "never barred" and a
  newly-authored gate lights up as pure DATA — no engine change, no per-domain variant.
- **Evaluated against a bare ctx, deliberately.** Every authored entity gate in the tree is a `GAMEOPTION_` leaf,
  which reads the live options and consults no scope context — which is precisely what makes the verdict the same
  for every player and city, i.e. what "ever" means.
- ⚑ **The verdict is STABLE for the game, and that is load-bearing: nothing the enabler gates rides a
  BUG/live option.** A game option is fixed at setup, whereas a live option (`setDefineINT`) is changeable
  mid-game and its flip carries **no DOMAIN event** — so a maintained verdict gating on one would go permanently
  stale with nothing to re-derive it ([self-heal is not a backstop](../../../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). The last
  enabler-facing live options went with the ranged-bombard removal
  ([superseded-ideas #24](../../../architecture/superseded-ideas.md)), so the hazard is absent from this surface rather
  than merely avoided. ⛔ Do not gate an enabler entity on a live option; if one is ever wanted, it needs its emit
  first.

> **⛔ A TRANSFORMATION ASKS `everAvailable` + THE TARGET'S `requires` — NEVER THE QUEUE-OFFER VERDICT.**
> `STATE_LISTED` means *"offered in the production queue, in this city, right now"*. That is the right question
> for a BUILD and the wrong one for an **UPGRADE**, a gift, a merge or any other `modifyUnit` transformation —
> none of which is a creation ([triggers.md](../../triggers.md): a transformation stands a successor up in place of a
> predecessor and deliberately does NOT ride the creation step), so what the queue is willing to OFFER has no
> bearing on it.
> ⚠ **The failure is total and silent, because a whole population can never reach LISTED.** A unit carrying
> `identity.spawnOnly` (legacy's `iCost == -1` sentinel) is excluded from the trainable set outright (§3), so
> gating a transformation on LISTED bars it permanently rather than conditionally. ⚑ **Measured: every
> great-person CONVERSION in the game — 49 units, the whole `MASTER_SAILOR_*` chain plus
> `MASTER_HUNTER → MASTER_RANGER → MASTER_WARDEN`** — while the SETTLE action kept working, because that is a
> `grants` payload that never asks the enabler. The tell to recognise: *one action on a unit works and another
> is missing*, rather than the unit being broken.
> ⇒ **The pair is the answer, and each half is doing its own job:** `everAvailable(bucket, id)` is the
> whole-game bar, and `requiresMetInCity(city, bucket, id)` is the target's own tech/resource gate asked where
> the transformation would happen — which is what keeps an upgrade chain following the RESEARCH the data gates
> it on. ⛔ Neither half substitutes for the other, and neither is `STATE_LISTED`.

⛔ **TECHS stay the picking logic's, and the reason is the distinction to apply elsewhere: their bar is a
COMPOSITION, not a gate.** `CvGame::canEverResearch` composes `NO_FUTURE` against the tech's own era and `isRepeat`
data — a consuming-system calc ([engine.md](../../../reference/engine.md)), which no entity gate carries and which an
info structurally cannot answer. Run that test on any future "ever" bar: a plain entity gate is the enabler's; a
composition over game state plus authored data belongs at the consuming system.

⚠ The two **carve-out** domains answer the UNLOCKED half only, and a consumer treating either as the whole verdict
over-offers: a BUILD's plot-validity half and a PROMOTION's per-unit applicability are evaluated LIVE at their
decision points (§7.1). EMPIRE-capability reads are not here either: they are asked of the PLAYER's own keyed
union ([capabilities.md](../../capabilities.md)), which no availability read duplicates.

⛔ Do not re-attach the machine ad hoc — a per-site `can*` rewire is the half-migration this rebuild exists
to avoid ([build a new getter surface, never widen a legacy one](../../../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)). Every consumer reads
through this surface, never around it.

