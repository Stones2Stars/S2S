# 8. The machine's shape — components, host, and the read surface

> Part of the **[enabler](../enabler.md)** spec.

> The structural half of the design: what the machine decomposes into, where its state lives, and the contract its
> readers get. ⛔ It carries no build status and no worklist — what is NOT done belongs in a short todo list, never
> woven into this spec ([a doc is a SPEC or a TODO, never both](../../../AGENTS.md#docs)).

### The components

The enabler lives in **`Sources/Enabler/`** — its own tree, carrying no `Cascade` prefix
(the enabler and the modifier cascade are two separate systems):

- **`EnablerDomain`** (`CvEnabler.{h,cpp}`) — the §7.1 shape: the tri-state array + the two membership refcount
  planes + the removal-wins formula. One component, instantiated per scope owner.
- **`EnablerKernel`** (`CvEnablerKernel.{h,cpp}`) — the shared edge-apply (`applyEdges`), the `requires` gate
  (`requiresMet` → `cascadeEvalCondition`), the `allowed` cap (`allowedOk`), and the operating-building fixpoint.
- **The eight per-domain enablers** — `CvTechEnabler` / `CvBuildingEnabler` / `CvUnitEnabler` / `CvCivicEnabler` /
  `CvProjectEnabler` / `CvProcessEnabler` / `CvBuildEnabler` / `CvPromotionEnabler`, each its domain's seed +
  event-delta calculator, all routed through the ONE `applyEdges`.
- **`CvEnablerConsumer`** — the enabler's OWN spine consumer, registered by `enablerRegisterConsumer()`. It is
  **LOAD-ACTIVE**: the reseed's in-read emits BUILD the domains through the same appliers play uses
  ([the load reseed](../../spine/05-the-load-reseed.md#5-the-load-reseed)) — there is no warm-up seed walk. One
  consumer per system; it never routes modifier work.
- **`OperatingBuildings`** (`CvOperatingBuildings.h`) — the §3.2 set type (`active` + `provided` + `obsolete`).

⛔ **The empire-capability union is NOT one of these** — it is a keyed store the PLAYER holds, fed by the tech /
civic / building facts ([capabilities.md](../capabilities.md),
[plot/city/player each own one live-state context](../../cascade/10-contexts.md#the-contexts--the-per-scope-live-state-read-surface)). The enabler is a SOURCE of those facts,
never the home of that answer.

### RESIDENCY — the network count lives on the PLOT GROUP, and only there

> **⛔ A PLOT GROUP IS A PURE OWNERSHIP QUESTION, AND IT IS ALWAYS FUNNELED THROUGH THE CITIES / FORTS THAT
> PARTICIPATE IN IT — NEVER THROUGH THE PLOT.** It answers *"does this city HAVE this bonus"* — feeding
> `requires` gates, the `connection:"trade"` atom and any deposit conditioned on `HAS_BONUS`. It never
> contributes a MAGNITUDE to anything, and it never answers for a tile: the city is the asker
> (`CvCity::getNumBonuses` relays through the city's plot-group pointer), a fort participates as a city-like
> member via the `actsAsCity` characteristic ([json.md §8](../json.md)), and the plot is merely where the resource
> sits.
> ⛔ **THE ROLLERSKATE THIS EXISTS TO STOP — CONFLATING THE PLOT GROUP WITH THE LOCAL PLOT SCOPE.** Both say
> "plot", and they are unrelated: a plot GROUP is a connectivity object spanning the map answering possession;
> plot SCOPE is one tile's own output. ⚑ **The measured consequence when they were conflated:** the connection /
> vicinity / network facts were routed into the PLOT package plane, where — carrying no plot — they fanned a mark
> over every plot of every city of the owner, dominating the entire load bracket. A connection fact moves no
> tile's output at all: the resource was already on its tile producing it.
> ⚑ **And a bonus's own yield reaches ONE tile — its own.** A resource changing a NEIGHBOURING tile's output is
> the deliveryguy's ([the deliveryguy ownership rule](../../cascade/18-ownership.md#4-ownership--the-deliveryguy-rule)) and is authored on that
> tile's IMPROVEMENT, conditioned on the bonus — never on the bonus. ⇒ A plot-scope deposit is authored only by a
> PLOT-RESIDENT source, so a plot-scope route with no named plot has no target by construction, and declining to
> fan drops nothing.

> **⛔ NO BONUS LIST IS SERIALIZED, ANYWHERE — the plot group's, `onSite`, any of them — and the plot group is
> populated EXCLUSIVELY BY EVENTS ON LOAD.** A resource list is DERIVED at every scope it appears at, so
> it answers to [derived data is never trusted from a save](../save.md#5-derived-data-serializes-nothing-) with no
> per-list judgement to make.
> ⚖ **THE ONE EXCEPTION IS A TRADE, AND IT IS THE DEAL THAT PERSISTS, NEVER THE LIST: *"bonuses traded
> away needs to be serialized, otherwise the trade is lost — so that is the current trade DEAL itself being
> serialized, not the list."*** An agreement between two players is genuine non-derivable state (the event-store
> class, [save.md §5](../save.md)); the per-bonus import/export COUNTS that follow from it are derived and are
> re-derived from the held deals on load, exactly as the network is.
> ⚑ **The test the exception gives you is general:** ask whether the thing is an AGREEMENT or a CONSEQUENCE of
> one. The agreement is state; every count downstream of it is derived.

**⛔ The `CvPlotGroup` is the ONLY authoritative list for trade resources, and NOTHING mirrors it.** Its content
is placed by the member CITIES (and `actsAsCity` forts) — never by a plot, which only holds the resource — so the
group is where the number is formed; every reader below it RELAYS. A `connection:"trade"` gate reads that list
and nothing else.

- **`CvCity::getNumBonuses` is a relay**, not a stored count: it reads the group through the city's plot-group
  pointer and applies the three things that are genuinely per-asker — the bonus's `TechCityTrade` gate, the
  player's minted-percent suppression, and the city's own corporation add-on. **The city declares no
  bonus-count member.**
- **`CityContext::tradedBonusCount` FORWARDS to that read** — it is the object's own O(1) data, so the
  STORES-vs-FORWARDS rule ([contexts.md](../../cascade.md)) puts it on the forward side. A stored
  copy re-swept every bonus on every fact that could move one, for a number a pointer hop already answers.
- **What the crossing fan-out is FOR.** `CvPlotGroup::changeNumBonuses` still fans into its member cities, and
  the city's plot-group moves still announce — but only to fire the **presence CROSSING** (`processBonus` + the
  corporation re-check), never to maintain a value. A count moving between two non-zero values announces
  nothing, by ruling ([spine.md](../../spine.md)).

⚑ **Why a per-city mirror is the wrong answer even though the read is hot.** Three copies of one number
(group → city → context) is duplicated authoritative state with only drift to gain — the read-not-store rule
([tally.md](../tally.md): *"creating something new when we already have it is pointless"*). And the cost that
argued for it is gone: the group maintains its holdings as a sparse `id → count` map, so the relay is a pointer
hop and a lookup, not the group SUM the mirror was built to avoid.

⚖ **VICINITY belongs to the CITY and is a plain local-presence fact:** it satisfies `connection:"onSite"`
atoms and NOTHING else — it never adds a second owned count (one pasture is ONE horse, not vicinity+network=2).

### The host — where the state lives

The machine's state lives on its scope owners, as plain DATA MEMBERS (the guardrail bars adding vtable *bases*
to EXE-bound classes, never members — [cascade.md](../../cascade.md)):

| owner | member | what it holds |
|---|---|---|
| `CvCity` | `m_enabler` (`CityEnabler`) | the constructible + trainable tri-state domains |
| `CvCity` | `m_operatingBuildings` | the ACTIVE set + provided bonuses at the operate/provides fixpoint (§3.2) |
| `CvPlayer` | `m_enabler` (`PlayerEnabler`) | techs / civics / projects / processes / builds / promotions |

All are **public and mutable** by requirement rather than laxity: the domain enablers write through a
`const CvCity&` / `const CvPlayer&` — the owner holds the STORAGE, the enabler owns the delta LOGIC. **None is
serialized**: every one starts empty and un-ready and is filled by the reseed's events through the same appliers
play uses ([the load reseed](../../spine/05-the-load-reseed.md#5-the-load-reseed)). Each owner's `reset()` clears them,
which is load-bearing because a `CvCity` is RECYCLED out of an `FFreeListTrashArray` — without it a new city
inherits the previous occupant's frontier.

⛔ **REGISTRATION ORDER IS A CONTRACT: contexts → enabler → modifier.** The enabler's load-end gate pass evaluates
through the CityContext / EmpireContext stores, which the contexts' consumer builds on the SAME
`GAME_LOAD_FINISHED` event; gating ahead of it evaluates against empty stores and every verdict is silently wrong,
with no self-heal to re-derive it ([cascade.md](../../cascade.md)).

### The availability READ surface

**⚖ THE NEW SURFACE IS BUILT WITHOUT WAITING FOR THE LEGACY DISCONNECT:** *"assume it is already
disconnected, add the new."* The disconnect is its own sweep; gating the replacement on it is what leaves the
machine unreachable indefinitely. Build the new surface as if the legacy one were already gone.

**⚖ BUILDING CONSTRUCTION AND UNIT TRAINING ARE THE SAME PLANE** — one design, two domains, never two
designs. Both are **CITY** concerns for the same concrete reason: the gate needs *"what resources are in VICINITY,
and in the PLOT GROUP"* — city-local supply that no other scope can answer. ⛔ **There is therefore no
player-level construct/train verdict**, and a player-scope `canTrain`/`canConstruct` is not merely redundant, it
is asking at a scope that cannot know. A caller with a city in hand asks that city; a caller genuinely meaning
"anywhere" fans over the player's cities. ⛔ Do NOT mint a maintained player-level union to make the old shape
work: it is duplicated state that must never drift — the same argument that keeps projects/processes player-held
rather than copied per city (§7.1), running the other way.

**ONE READ PAIR PER DOMAIN** — the domain IS the group, and the existing engine enum
(`BuildingTypes`/`TechTypes`/…) is the consumer's vocabulary. The domain set is fixed and small, so the surface
grows by DOMAIN, never by candidate; there is no per-candidate getter and no what-if argument.

| owner | verdict (tri-state) | frontier (caller-owned fill) |
|---|---|---|
| `CvCity` | `getBuildingAvailability` · `getUnitAvailability` | `getAvailableBuildings` · `getAvailableUnits` |
| `CvPlayer` | `getTechAvailability` · `getCivicAvailability` · `getProjectAvailability` · `getProcessAvailability` | `getAvailableTechs` · `getAvailableCivics` · `getAvailableProjects` · `getAvailableProcesses` |
| `CvPlayer` (carve-outs) | `getBuildUnlocked` · `getPromotionUnlocked` | `getUnlockedBuilds` · `getUnlockedPromotions` |

⛔ **Every read is a BARE O(1) FETCH of the maintained tri-state** — no gate runs, no calculator is called, and
`requires` is never evaluated (§7). A missed propagation therefore leaves a visibly wrong verdict instead of
being silently recomputed away ([self-heal is not a backstop](../../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)).

**The tri-state is returned WHOLE, answering TREE + GATE only.** HIDDEN vs GREYED is the "why not" the build list
needs (§6), so reducing it to a bool would force a second read to recover it. ⛔ The **QUEUED overlay is
deliberately not folded in**: the domain keeps `FLAG_QUEUED` separate from `FLAG_GATE_FAILED` precisely so
"already queued" stays distinguishable from "requires unmet", and collapsing a queued candidate onto GREYED would
destroy that and misreport why it is not offered. The overlay rides only the two reads that care — the FRONTIER
(fresh offer, queued excluded) and `CvCity::isBuildingContinuable` (reads past it, so the production-check sweep
does not cancel every in-progress build).

### ⛔ WHAT THE ENABLER IS NOT — tech-tree PATHING AND QUEUING BELONG TO THE TECH-PICKING LOGIC

The enabler answers **"can I, right now?"** and stops there. Two research features are **NOT its concern**:

- **QUEUING FURTHER THAN THE TREE** — a player may queue a tech that is not in CAN GET yet (several steps away).
- **THE EASIEST PATH** to a chosen tech — the cheapest prerequisite chain from what is currently held.

*"That is NOT the enabler's concern; that is the concern of the actual tech-picking logic."* Both are
**research-only and only needed inside the TECH-TREE BROWSER**. They are structurally impossible for the
enabler anyway: its maintained frontier holds only what is unlocked NOW, so it cannot see a candidate three steps
out — that answer comes from the **static compiled `enables`/prereq edges** the infos carry
([patterns.md § THE WHAT-IF DRIVER](../../architecture/patterns.md)), walked COLD by the picking logic. A path search
is a genuine graph walk, which is acceptable on a browser path and would be unacceptable on the frontier.

⛔ So do NOT grow path-finding, queue projection, or a reachability closure inside the enabler. The enabler
supplies the FACTS (held / statically barred / removed / the gate verdict); the picking logic composes the route.
This is the [north-star](../../architecture/north-star.md) test applied — ask *whose job is this?* and the answer
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
membership through the same `EnablerDomain::isMember` ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
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
over-valued-enablement problem ([AGENTS.md](../../../AGENTS.md) § AI valuation of ENABLEMENT — relaxing enablement
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
target ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
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
.** *"For all unit/promotions that rely on game options, and anything else the enabler deals with, it is
the enabler's job to call `hasGameOption`."* A whole-entity game-option bar authors as the entity-level
`enabled`/`disabled` pair ([the whole-entity applicability gate](../json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity)), so answering "is this
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
  stale with nothing to re-derive it ([self-heal is not a backstop](../../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). The last
  enabler-facing live options went with the ranged-bombard removal
  ([superseded-ideas #24](../../architecture/superseded-ideas.md)), so the hazard is absent from this surface rather
  than merely avoided. ⛔ Do not gate an enabler entity on a live option; if one is ever wanted, it needs its emit
  first.

> **⛔ A TRANSFORMATION ASKS `everAvailable` + THE TARGET'S `requires` — NEVER THE QUEUE-OFFER VERDICT.**
> `STATE_LISTED` means *"offered in the production queue, in this city, right now"*. That is the right question
> for a BUILD and the wrong one for an **UPGRADE**, a gift, a merge or any other `modifyUnit` transformation —
> none of which is a creation ([triggers.md](../triggers.md): a transformation stands a successor up in place of a
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
data — a consuming-system calc ([engine.md](../../reference/engine.md)), which no entity gate carries and which an
info structurally cannot answer. Run that test on any future "ever" bar: a plain entity gate is the enabler's; a
composition over game state plus authored data belongs at the consuming system.

⚠ The two **carve-out** domains answer the UNLOCKED half only, and a consumer treating either as the whole verdict
over-offers: a BUILD's plot-validity half and a PROMOTION's per-unit applicability are evaluated LIVE at their
decision points (§7.1). EMPIRE-capability reads are not here either: they are asked of the PLAYER's own keyed
union ([capabilities.md](../capabilities.md)), which no availability read duplicates.

⛔ Do not re-attach the machine ad hoc — a per-site `can*` rewire is the half-migration this rebuild exists
to avoid ([build a new getter surface, never widen a legacy one](../../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)). Every consumer reads
through this surface, never around it.

### The gate stages, by domain

The gate verdict is a per-id flag (`setGateFailed`): a failed gate flips a tree member LISTED → GREYED, membership
untouched. **A domain whose gate stage has not landed never sets the flag, so its members stay LISTED** — the
enable-side over-offer, which is a VISIBLE defect to fix, never a reason to fall back to legacy.

Every domain carries all three stages — membership, the `requires` gate, and an `allowed` cap — with the cap
taking its domain's own shape:

| domain | what its `allowed` cap bounds |
|---|---|
| techs | world-unique founder techs |
| buildings | world/team/empire self-caps + the per-city wonder-CATEGORY cap (§4) |
| units | world lifetime-created; empire era-scaled national cap |
| projects · civics · processes · builds | the plain per-scope cap |
| promotions | none — and the gate is on demand, not a maintained flag (§7.1 carve-out) |

**Promotions are the exception to the over-offer:** they set no gate flag, but `requires` + the unit-state
applicability leg (unitcombat QUALIFIED/DISQUALIFIED, game options, promotion-line prereq tech, and the runtime
spy/pillage/commander/commodore/blend + intercept/evasion/XP caps) are enforced ON DEMAND at level-up, so the
promotion offer is not over-inclusive.

### The settled model rulings

- **HAVE model:** the enabler owns NO HAVE store — it ties into the object-owned has-lists that already exist
  (city buildings/religions/corps, player civics/traits/heritages, team techs). Presence stays on the objects; the
  [tally](../tally.md) stays the count accessor.
- **Evaluator depth:** `cascadeEvalCondition` reads raw object-owned state (legitimate live reads). What is
  event-driven is the MAINTENANCE — which dependents re-gate, when — never the read source.
- **Component model:** one unified component, instantiated per §7.1 owner; delta-apply, never
  mark-then-recompute — no such path exists at all (§7).
- **The root rule:** no implicit "no-edge ⇒ available" engine rule. Start-available entities are authored onto
  `TECH_GAME_START`'s `enables` (§2, curator-derived), the tree is fully connected, a missing edge fails closed.
  The load backfill of `TECH_GAME_START` itself is the ONLY engine special case the model needs.
- **The BONUS axis is GATE-ONLY**: a plot-group-carried bonus NEVER drives tree membership. The
  curator keeps authoring bonus `enables` edges (the reverse-mapped view of the target's retained `requires`
  atom), but the runtime consumes bonus events as pure stateless gate re-checks over the bonus's
  `EDGEF_REQUIRED_BY` dependents. Membership rides tech/building/civic edges + the root; an entity whose only
  inbound edges are bonuses ROOTS, sitting visible-GREYED on its bonus requirement. The one carve-out — a bonus ON
  a plot enabling an improvement's placement (`enables.builds`) — is a live per-plot gate, no domain involvement.

### The reverse index, and what is deliberately NOT one

**The canonical reverse axis is `EDGEF_REQUIRED_BY`** ([reverse lookups are populated once, at load](../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)),
and a per-id bucket that duplicates it is a defect. ⛔ But the axis-flag lists (power / golden age / state
religion / the coarse religion-civic-tech lists) and the PROPERTY band index are **NOT** convergence targets and
must not be swept into one: the reverse pass deliberately excludes engine tokens, the plot substrate and
`PROPERTY_` bands, and **a coarse list matches a coarse event**. Reading the two populations as one uniform
"operate index" is exactly the mistake the spelled-out naming rule exists to prevent
([Sources/AGENTS.md](../../../Sources/AGENTS.md) § Code Style).

⚑ **`civicAny` is coarse by the same logic, and that coarseness is a known gap for AI VALUATION, not just
re-gating.** `CascadeCondDeps::civicAny` unions every `requires civic` clause into one bool — enough to re-gate,
but not enough to answer "which civic gates this candidate." `CvPlayerAI::AI_civicValue`'s civic-choice building
valuation dropped its cross-category half-value damper (owner: civic valuations are linearly combined across
categories, so a building gated by civics in two options could be counted at full value from both, risking
oscillating choices) without replacing it with per-civic precision. If choices start oscillating, the principled
fix is an id-keyed `civics` set on `CascadeCondDeps` — never reviving the whole-civic-database sweep that such a
set would replace.

⛔ **THE PLOT PLANE CARRIES NO `EDGEF_REQUIRED_BY` AT ALL, AND ITS COARSE LIST IS THE `(kind, id)` PLOT-ATOM
INDEX.** `CvReversePass::rp_requiredByRefInfo` routes nine infotype prefixes and returns NULL for every other,
so **no terrain / feature / improvement / route / mapcategory info ever gains a REQUIRED_BY edge.** The coarse
list this section prescribes is therefore built by the enabler itself: `scanCondDeps` records each substrate id
the `requires` names, and each domain compiles `(PlotAtomKind, id) → candidates` — read by
`onPlotAtomChanged`, fanned over the plot's own `workableByCities()`.
⚑ **A TERRAIN fact also seeds the MAPCATEGORY atoms**, because a plot's categories are derived from its terrain
(`CvPlot::getMapCategories` forwards to the terrain info) and have no fact of their own; `plotAtomSeeds` is the
one place that hop lives.
⚑ **The bare plot BITS ride the verdict fact, not a substrate id.** `HAS_RIVER` / `HAS_COAST` / `IS_WATER` and
their kin name no entity, so they index by their `CASC_PRED_*` id and re-gate off
`SEVT_PLOT_PREDICATE_ADDED / _REMOVED` — which is exactly why that fact exists beside the substrate ones
([spine.md](../../spine.md): one says what the tile CARRIES, the other what it MEANS).
⚠ **Reading the empty reverse edge instead FAILS SILENTLY, which is why this is spelled out**: the walk
succeeds, finds nothing, and re-gates nobody — indistinguishable from "no candidate needed re-gating" at every
observation point, including a census read taken when nothing has changed since load. The index
therefore reports its own size at load (`[ENABLER/plotatoms] atomKeys=… atomEntries=…`), so an index that
compiled EMPTY says so.

⚑ **And this is what keeps `GATE_DYNAMIC` meaning what §7.1 says it means.** `scanCondDeps` marks `dynamic` for
any atom it does not NAME, so every axis that later gained a precise route must also gain a case there — or it
keeps marking the catch-all, and the "small load-compiled set" becomes the whole registry (the plot substrate
alone put every building in it, and every fact routed through the class then re-gated everything). ⛔ So when
you wire a new route, remove its axis from the catch-all in the same change; the residue is the genuinely live
state — `existedFor`, `IS_CAPITAL`, the count tokens, connection.

> **⛔ AN AXIS HAS TWO SPELLINGS AND THEY MUST NOT DISAGREE — this is the failure mode, not a tidiness point.**
> `scanCondDeps` meets most axes twice: as a PRESENCE atom (`BONUS_IRON`) and as a PREDICATE
> (`{HAS_BONUS: BONUS_IRON}`). Narrowing one and leaving the other keeps the whole axis in the catch-all while
> the code reads as though it were routed — and the note justifying the surviving half is typically the one
> already retired beside it. ⚑ **Measured: the bonus axis had exactly that split, and closing it took the class
> from 2,674 of 5,180 buildings to 423.** ⇒ When you route an axis, grep BOTH branches.
>
> **⚖ THE THIRD DISPOSITION IS *STATIC*, and forgetting it is what puts a never-moving axis in a live class.**
> §3.2's rule is that an axis either has a fact and is routed on it, or is STATIC for the city's life and gated
> once at creation. A static axis therefore marks **nothing at all** — a plot's LATITUDE cannot change and a city
> cannot move, and a VICTORY condition is fixed at setup, so neither has a crossing to wait for and marking them
> dynamic bought a re-gate that could never change a verdict.
> ⚠ **`existedFor` is the neighbour that is NOT static and must stay in the residue:** the game YEAR advances, so
> an age-gated candidate genuinely crosses a threshold with no fact naming it.
>
> ⚑ **THE CLASS SIZE IS INSTRUMENTED, so a widening is observable rather than suspected** —
> `[ENABLER/gateclass] domain=… class=… members=… of=…` at load, beside `[ENABLER/plotatoms]`. Read `members`
> against `of`: a class approaching the registry size is not a bounded re-gate set, and every fact routed through
> it re-gates nearly everything. ⛔ Do not narrow this class by reasoning alone — the number is one line in
> `Cascade.log`, and the last two attempts to estimate it from the authored JSON were both wrong.

### Load-end reconciliation

- **Neither the counts NOR plot-group MEMBERSHIP are trusted from a save** (membership is derived state: routes +
  terrain-trade capabilities + ownership). The deserialized groups are drained and discarded; a load-end rebuild
  RE-COLORS membership from current state (`CvPlotGroup::colorRegion`, a flood fill from each plot) and folds
  the counts through the live entry points as each plot joins, announcing every bonus fact as a genuine crossing
  emit before the `GAME_LOAD_FINISHED` gate pass.
  ⛔ **This full demolish-and-repaint is the LOAD PATH ONLY** (`reInitialize` has exactly one caller,
  `CvGame::onFinalInitialized`) — every in-play group change is incremental (`recalculatePlots`'s early-out,
  `CvPlot::updatePlotGroup`'s targeted join). Reading the load teardown as the ordinary shape invites
  "optimizing" a full rebuild that does not run during play.
  > **⛔ THE RE-COLOR RE-FOLDS THE TILE HALF ONLY, SO THE BUILDING-SUPPLIED HALF MUST BE RE-PUSHED BEHIND IT.**
  > `CvPlot::updatePlotGroupBonus` folds a plot's own extracted resource, a city's free bonuses and the capital's
  > import/export — and nothing else. Every resource an ACTIVE BUILDING supplies through `provides.bonuses`
  > (§5a) was pushed into the DESERIALIZED group as that building resolved its dormancy in-read, and the
  > demolish-and-repaint throws it away: by re-color time the operating set has already CONVERGED, so
  > re-confirming a dormant/active verdict is a no-op that crosses and announces nothing (§3.2) — the
  > `GAME_LOAD_FINISHED` gate pass re-confirms `provided` and the supply is simply gone. The signature is a whole
  > CLASS of resource going invisible, never a wrong number: a resource supplied only by an active building reads
  > ≤ 0 in every member city's traded store, while tile-supplied resources beside it are unaffected.
  > ⇒ **The fix is a load-end re-push through `CvPlotGroup::changeNumBonuses`** (the same live entry point
  > `provides.bonuses` normally uses) — walking each city's converged `providedCount` into its NEW group, after
  > the re-color, so the crossing is announced as a genuine `SEVT_PLOTGROUP_BONUS_ADDED` rather than seeded
  > ([the load reseed](../../spine/05-the-load-reseed.md#5-the-load-reseed) bans a warm-up walk that leaves consumers
  > deaf; a real crossing emit is not one).
- **The DORMANCY VERDICT is the operating-building fixpoint** (§3.2,
  [the pollution guardrail](../validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)) — applied through the engine's
  disabled-building flag, never a hand re-derivation from legacy prereq getters, plus the two runtime-state legs
  the authored data does not carry (employed-population composition; the banned-non-state-religion policy). The
  load-end cross-city fixpoint — iterate {re-fixpoint each city's operating set → apply flips → the provides
  injections adjust the network} until stable — reconciles the serialized flags to the computed verdict inside
  the load bracket (a manufactured chain lights tier by tier: ore → wares → firearms). The iteration is
  WORK-LIST driven, each flip keeps the FULL per-flip side-effect surface (power, freshwater, employed
  population, traits, provides), and convergence is declared ONLY by a quiet FULL verify pass.
  ⛔ **BAKED-CONSUMER RE-RUNS:** an engine consumer that BAKES state on modifier changes (the trade-route
  ASSIGNMENT) runs during this fixpoint against not-yet-warmed packages and its baked result self-heals never;
  every such consumer is re-run ONCE after the load-end package warm.
- **The dynamic operate axes ride their events** — connectivity via the plot-group/network bonus events,
  vicinity (radius growth) via the culture-level event — routed into the operate re-check of dependents.

⚠ **A WHAT-IF asker can never iterate the frontier.** The frontier answers the CURRENT verdict only, so a gate
called with hypothetical arguments is served by `EnablerOverlay` (§8, "WHAT THE ENABLER IS NOT") — not by a swap
to `listedIds`.

---

