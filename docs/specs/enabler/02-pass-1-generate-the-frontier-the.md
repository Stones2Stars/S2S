# 2. Pass 1 — GENERATE the frontier (the `enables` family)

> Part of the **[enabler](../enabler.md)** spec.

Four source-side edges drive generation. All read forward over HAVE: `enables` **adds** to the candidate set,
the other three **remove** from it.

| edge | nature | new builds | existing instances |
|---|---|---|---|
| **`enables`** | a permanent unlock | **added** to CAN GET | — (this *is* the unlock) |
| **`disables`** | a **law / ban** (policy forbids) | removed while the disabler is held | **destroyed** — torn down; rebuilt on repeal. *(Dormancy is NOT a `disables` — it's the target's `requires.operate.dormant`, §3.)* |
| **`obsoletes`** | passive supersession | removed | **persist** (an obsolete unit stays on the map); the target decides its own fate |
| **`replaces`** | succession **removal** — a superseder removes the predecessor (`replacedBy`; e.g. a unit's `SupersedingUnits`) | removed | dropped from buildable once the superseder is itself buildable; the *building* `ReplacementBuildings` is instead *dormancy* (`requires.operate.dormant`, §3) |

So **`CAN GET = union(enables) − (disables ∪ obsoletes ∪ replaces)`**, all over HAVE (`replaces`' one live use
is unit succession — §3).

**The tree is fully connected — `TECH_GAME_START` is the universal root.** Every entity enters CAN GET via an
inbound `enables` edge; there is **no** implicit "no-edge ⇒ always available" engine rule. Entities available
from the start of the game — the Palace's ongoing constructibility, the starter units (`UNIT_BRUTE`), the base
promotions, `PROCESS_IDLE`, the base civics — are authored onto **`TECH_GAME_START`'s `enables`** (the synthetic
start node every player holds), derived by the curator (no prereq in legacy ⇒ enabled from game start). A missing
edge therefore fails **closed** (the entity is unreachable — loud in validation), never silently-available.
The dead workarounds for this — whole-domain frontiers, hardcoded always-available whitelists — are tombstoned
([superseded-ideas #18](../../architecture/superseded-ideas.md)). *(The Palace's FIRST placement is not the
enabler's doing: the settler's `grants.foundBuildings` places it at founding — the grants machine,
[json](../json.md) §5.)*

**`TECH_GAME_START` is guaranteed into HAVE at load.** It is a newly-added concept — a tech that will **never be
in the tech tree** — so: **new games hold it by default; an existing save does not** and gets it **added on save
load if absent** (the backfill that keeps an old save's generation rooted). Without it, generation on a
pre-concept save produces an empty tree. **This backfill is the ONLY engine special case the root model needs**
— everything else is pure data + the generic machine.

> **`replaces` is the UNIT succession edge; building "replacement" is dormancy (engine-verified).**
> A unit's superseders ARE genuine removal-on-succession (the legacy engine dropped the predecessor once a
> superseder was buildable) → modeled as the unit's `replacedBy.units` replace edge (§ units, below).
> The legacy *building* `ReplacementBuildings` (A lists the buildings that supersede it) *looks* like removal, but the legacy engine only
> DISABLED A while the successor was present and re-enabled it when the successor was gone — reversible **dormancy**, never removed. So it is mirrored as the **target's
> `requires.operate.dormant: [successor]`** (§3) and leaves CAN-GET membership untouched — *not* a `replaces` edge.
> This unifies the education ladders (a lower band dorms while a higher is present = only-highest-active) with the
> pollution effects (blackened-skies dorms the observatory). `replaces` stays a defined family member for a future
> genuine-removal source; there is none today.

> **⛔ NO BUILDING EVER OBSOLETES A BUILDING — a building→building relation is an UPGRADE CHAIN.** A building obsoleting another building is not good design; such relations are upgrade chains. So a building's `obsoletedBy` carries **`techs` only**, and the
> successor relation is carried by the chain's reversible dormancy (above). ⛔ Do not emit, author, or wire
> `obsoletedBy.buildings` for a building.
> ⚑ **The two mechanisms CANNOT COEXIST on one pair, and obsolescence wins silently.** Obsolescence is evaluated
> BEFORE the operate verdict (`EnablerKernel`'s `EK_OBSOLETE` — *"obsolete regardless of operate, so it is
> checked FIRST"*), and pass 1 alone decides tree membership (§1), so an `obsoletedBy.buildings` edge HARD-REMOVES
> the predecessor the instant its successor exists — `whenObsolete` being absent — and the dormancy on the very
> same pair becomes unreachable. The upgrade chain stops being reversible and the predecessor is destroyed rather
> than parked.
> ⚠ **The inversion that produced it, so it is not re-derived:** legacy `ObsoletesToBuilding` is the **SWAP
> DESTINATION** — what this building turned INTO when its OWN `ObsoleteTech` fired — so it was never a cause, never
> a destroy, and never keyed on the successor's presence. Reading it as a "superseding building" turns a
> destination into a trigger and invents a presence-keyed removal the engine never had. A CONSTRUCTIBLE
> `ObsoletesToBuilding` target therefore emits **nothing**: the tech edge already carries the obsolescence, and the
> successor relation is the dormancy. (The NON-constructible relic-shell target still becomes `whenObsolete` —
> [json.md §4.2](../json.md).)
> ⚑ **Measured, which is why this is a rule and not a note: 1,521 of the 1,522 buildings carrying
> `obsoletedBy.buildings` named that same building in `requires.operate.dormant`** — every upgrade ladder in the
> mod (bridges, gatherers, medicine, arenas) asserting both fates at once. It was inert only because nothing reads
> the buildings bucket; wiring it would have deleted all 1,521 predecessors on the turn their successor was built.
> ⚑ **THE CHAIN HAS TWO HALVES AND THIS IS ONLY ONE OF THEM.** Dormancy is the PRESENCE half — the successor
> being built parks the predecessor, reversibly. The TECH half is the UPGRADE: when the predecessor's own
> `obsoletedBy.techs` fires it BECOMES its successor, authored on `whenObsolete`
> ([json.md §4.2](../json/04-availability.md#42-obsoletes--replaces--disables--removal-permanent-source-side)). Different triggers,
> different directions, and a building carries both — the Forge parks when a Foundry is built, and turns into one
> when `TECH_NANOMINING` lands.
> ⚖ **DIRECTION: the upgrade chain is a concept to LEAN INTO further** — see
> [plans/parked/upgrade-chains.md](../../plans/parked/upgrade-chains.md) for what is still not modelled (chain
> identity, tier order, a chosen-and-paid upgrade). Do not build chain machinery ahead of that work.

**`obsoletes` vs `disables` kept SEPARATE for clear semantics** (progress-supersedes vs policy-forbids) + the
pedia line ("Obsoleted by [tech]"). `disables` = a hard "be gone" (the source commands; the target gets no say);
`obsoletes` = a soft signal — the instance's fate is authored on the TARGET via `whenObsolete`, a **separate full
modifier tree** applied while obsolete ([json](../json.md) §4.2): empty ⇒ the building is fully gone; non-empty ⇒ its
normal families stop and this tree applies instead (wonders/walls keep culture/tourism, most buildings vanish).

**The fixed run order — `replaces` → `disables` → `enables` → `obsoletes`.** The *membership* of CAN GET is
order-independent (set-difference is commutative); the fixed order matters only for two **propagation** effects:
a possession change (if a law `disables` a building, anything that building `enables` must also drop) and
instance-fate precedence (`replaces` wins over `obsoletes`). So: collapse succession chains, drop
banned/destroyed things from HAVE, *then* generate from the corrected HAVE, *then* prune obsoleted candidates.
**Tech is authored in `enables`** (a tech `enables` what it unlocks) — never as a generation driver in `requires`.

**`disables` — the worked case.** The lone law-disable today is the per-civ Neanderthal research ban
(`TECH_SEDENTARY_LIFESTYLE`, reversible — bars the tech while active). **NB the blackened-skies → observatory case
is NOT a `disables`:** blackened skies don't nuke the observatory from orbit — it goes **dormant** and wakes when the
skies clear, so the *observatory* carries `requires.operate.dormant: BLACKENED_SKIES` (§3). (`BLACKENED_SKIES` is
itself a tech-created pseudobuilding, dormant via its own air-pollution band until pollution gets *really* bad — only
then does it dorm the observatory.) Dormancy is always the **target's `requires.operate.dormant`**, never a source
`disables` (the disease-band / rat-catcher case is the same shape).

**Multi-parent tech.** A child tech carries `requires.build.all:[T1,T2]` (AND) or `.any:[T1,T2]` (OR — a plain `||`
over its members, [json](../json.md) §3.4; NOT a list-of-groups). `enables` proposes
the child from one parent, `requires.build` confirms all parents forward from HAVE. The curator must RETAIN
`AndPreReqs`/`OrPreReqs` as `requires.build.all`/`.any` **because the store's prereq-inversion flattens them
into other techs' `enables` for generation and does not keep them on the child** — so the curator re-reads them
off the child. `requires.build` only —
techs are monotonic, no `operate`.

> **Reverse-mapping the forward compat views (standing direction: reverse-map everything on load).** The
> store's prereq-inversion is *for the GENERATE pass* (it flattens each entity's prereqs into the prereq entity's
> `enables`). But many legacy consumers still read the **forward** view off the child — a route's `getPrereqBonus`
> (the `CvPlot` build gate), a trait's `getPrereqTrait`, a tech's `leadsTo`. These are **reconstructed AT LOAD from
> the inverted `enables`** (never stubbed, never re-authored on the child): the tech `leadsTo` and the route
> bonus prereqs in the ONE general reverse pass (`Data/CvReversePass.cpp`) — a cross-entity reconstruction has
> exactly ONE home, and it is never `mapFrom` (which runs while the view is still being built) nor a second
> load-time pass beside the reader. ⚠ **The TRAIT prereqs are the deliberate exception and their forward GETTER is
> NOT reconstructed:** the rebuilt `CvTraitInfo` carries no prereq getter at all, because re-adding
> `getPrereqTrait`/`getPrereqOrTrait1/2` would be a legacy getter name returning
> ([build a new getter surface, never widen a legacy one](../../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)). Their consumers
> (`CvPlayer`, `CvGameTextMgr`) read the trait's own edge families instead, as stage-4 consumer work.
> ⛔ **"Not reconstructed" is about the GETTER ONLY — the prereqs THEMSELVES are live and load-bearing. Reading
> this line as "trait prereqs are inert" is the misreading this callout exists to stop.** A trait's `TraitPrereq`
> and `PrereqTech` INVERT at the store onto the SOURCE's `enables`: trait→trait becomes the developing-ladder
> edge, and tech→trait becomes `tech.enables.traits` — §2's rule that a tech is authored in `enables`, never as a
> generation driver in `requires`. ⚑ The TECH leg is not a curiosity: every rank ±2/±3 rung carries one, so it is
> the gate on advancing a developing line at all, and a tech JSON missing those edges leaves every upper rung
> permanently unreachable — silently, since nothing reads a gate that was never emitted.
> **The inversion must keep AND vs OR
> in DISTINCT buckets** or the reverse map loses the distinction — a single AND prereq inverts to its own bucket
> (`enables.routesAnd` / `enables.traitsAnd`), the OR-list to another (`enables.routes` / `enables.traitsOr`), and
> the load pass rebuilds each forward getter separately. *(The tech case reconstructs from the child's retained
> `requires.build.all`/`.any` instead — same goal, the two reconstruction sources.)*
>
> ⚖ **WHAT DECIDES RECONSTRUCT-vs-EDGE-FAMILIES: CAN THE THING PHYSICALLY MOVE?** A **ROUTE** is pinned to
> its plot, so a static forward list on the info describes something that cannot change place — reconstruction is
> coherent, and the route side KEEPS it. A **UNIT** physically moves, so its consumers read the unit's own **edge
> families** rather than a reconstructed forward list, and no `unitsAnd` bucket is minted for one.
> ⛔ Apply this test to any future forward-view question; it is not a per-case preference.
> ⚠ **The cost to price in: an edge family is ONE MERGED BUCKET.** `EDGEF_RELATED` lands every authored family's
> references together, so a unit's `EDGEB_BONUSES` does NOT preserve the mandatory-vs-one-of split, and its
> `EDGEB_TECHS` mixes ENABLING techs with OBSOLETING ones. A consumer with **ANY** semantics is safe (a superset
> only loosens); a consumer with **ALL** semantics is NOT — reading a merged bucket as "every one of these is
> required" silently demands a unit's own obsoleting tech before it may be trained. Keep the exact predicate over
> the family ([reverse lookups are populated once, at load](../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)); where ALL semantics are
> genuinely needed the answer is the owning info's own `requires` section, never the merged family.

**Empire/team-scope constructables need NO new machinery** (the scope spine already has team/empire): stage-gates
via `enables` (the space line), doctrine bans via `disables` + empire modifiers.

> **⚖ THE EMPIRE-LEVEL BUILDING (`identity.empireLevel`) — a building the PLAYER holds, once.** The home
> of [empire-level buildings](#2-pass-1--generate-the-frontier-the-enables-family).
> **Membership is EMPIRE-UNIFORMITY BY CONSTRUCTION:** a building whose presence cannot vary per city — the
> class that self-granted a copy into every city (`grants.buildings` on itself at empire scope: the folklores,
> the elemental-knowledge and requirement markers), plus the `notConstructible` effect markers whose ONLY
> arrival is another source's empire-wide grant. A per-city copy of such a building carries no information
> beyond "the empire has it", so the copies are the wrong shape: the building is held by the PLAYER and is
> never IN any city. The curator derives the tag from that membership rule — it is never hand-authored per
> entity.
> - **HAVE is the player's held set** — genuine non-derivable state, serialized on `CvPlayer`
>   ([save.md §5](../save.md)), announced as `SEVT_EMPIRE_BUILDING_ADDED / _REMOVED` with the in-read half
>   emitted from `CvPlayer::read` ([the load reseed](../../spine/05-the-load-reseed.md#5-the-load-reseed)), and
>   forwarded through `EmpireContext` beside the civic / trait / heritage axes
>   ([contexts.md](../../cascade.md)).
> - **An atom naming one resolves at EMPIRE scope by the implied-scope rule** ([json.md §3.4](../json.md): scope
>   is implied from the type's DOMAIN, and the tag IS the type's domain) — so the bare `requires`/`per` atoms
>   naming class members stay bare and answer from the held set; a count atom reads the tally's empire domain,
>   which the held set feeds.
> - **Its deposits author at EMPIRE scope** and roll down to every city at the read, exactly as a civic's do
>   ([modifier.md §1](../../cascade.md)) — no fan, no per-city copies. Per-city variation stays expressible as
>   city-conditioned entries (the city-realization law,
>   [cascade.md](../../cascade.md)).
> - **The BUILD path is the PROJECT precedent (§7.1), exactly:** a constructible member is offered on the CITY
>   production queue; its axes are empire-held so the frontier is the PLAYER's domain (byte-identical
>   duplication risk otherwise, per §7.1) — and a city-local atom (the plot map-category
>   gate) stays a live check at the gate, the same split projects already use. Completion acquires it for the
>   PLAYER, and it leaves every city's offer at once (the built leave-rule, keyed on the held set). A
>   `notConstructible` member is placed by its own system (the grants machine, a unit's construct mission)
>   through the ONE placement choke point, which routes an empire-level target to the player — the placing
>   systems never learn the tag exists.
> - **The self-grant is DELETED with the copies.** Holding at the player IS the empire-wide effect, so the
>   `grants.buildings` self-entry, the construction fan over standing cities and the city-starts-existing fold
>   all cease to exist for this class — and a city capture or culture flip moves NOTHING for it, which is the
>   point (it *"vastly reduces the amount of domain-event flips we have to handle when a city gets
>   taken).
> - **Removal is the ordinary machinery at the holder:** `obsoletedBy`/`whenObsolete` and `disables` evaluate
>   against the player exactly as they evaluate against a city. An empire-level building's ONGOING gate is
>   empire-evaluable by construction; a per-city atom in one's `requires.operate` is a data shape the curator
>   resolves **by the constructibility split**: on a CONSTRUCTIBLE member it MOVES into `requires.build` — the
>   build-time gate is `build ∧ operate` (§3), and `build` is evaluated in the BUILDING city, so dropping it
>   deletes the member's whole build-city requirement (the obsidian-gatherer over-offer — a gatherer offered
>   with no resource anywhere near) — and on a `notConstructible` marker it DROPS (nothing reads its build
>   gate; the per-city half lives on the per-city consumers' own gates).
> - ⚠ **An old save's per-city copies NORMALIZE at load:** the city read routes an `empireLevel` id to the
>   owner — idempotent, so N city copies fold to held-once — and the city keeps nothing.
>
> ⛔ **The per-city GRANT stays the model for everything else.** A wonder granting an ordinary constructible
> building to every city (a Granary, Irrigation Canals) grants real per-city copies whose presence genuinely
> varies — that is the two-leg apply of
> [triggers.md](../../specs/triggers.md), unchanged. The line is
> EMPIRE-UNIFORMITY: presence that cannot vary per city moves up; presence that can stays down.

> **⚖ A GRANTOR→MARKER PAIR IS ONE BUILDING — the split is legacy grant machinery, not a design.** Many
> empire-level effects are authored as TWO ids: a `notConstructible` **vehicle** whose only job is to deliver a
> `grants.buildings` marker, and the `identity.empireLevel` **marker** carrying the effect. That is one concept
> wearing two ids because of how the legacy grant machine worked. The correct shape is a single empire-level
> building the award path grants directly — no vehicle, no grant hop — and the survivor carries the UNION of
> the pair (the vehicle's award-path identity, the marker's effects/amenities/requires/triggers).
> - **⛔ THE MEMBERSHIP TEST IS "DOES THE SOURCE HAVE ANY OTHER JOB?", AND IT IS NOT THE MECHANICAL SCAN.** A
>   scan for *"single-grant vehicle whose target is `empireLevel`"* finds the CANDIDATES; a scan for *"and the
>   vehicle carries no modifier family of its own"* is a **narrower and WRONG** worklist, because the survivor
>   absorbs those families anyway. ⚑ Worked: `BUILDING_TRADITION_WORK_ETHIC` carries its own `culture` deposit
>   and still collapses — the culture rides onto the survivor. Read the family list as information about what
>   the union must carry, never as an exclusion.
> - **⛔ WHAT DOES NOT COLLAPSE — three kinds, each for a different reason:**
>   - **Worldviews.** The repeal outcomes remove the ACTIVE marker while the vehicle stands, so the pair
>     expresses two genuinely distinct states. The split is load-bearing.
>   - **The culture chain (`C_N`/`C_L`/`C_AD` → `C_AC`).** Base culture is CITY-PLANE information — an African
>     culture city does not become European base culture — so those stay per-city, and the `C_AC` ACCESS marker
>     stays a separate empire-level building fed by per-city bases acquired through conquest
>     ([culture-religion-research.md](../../reference/culture-religion-research.md)).
>   - **A real wonder that grants a marker.** The source has an in-city job of its own, so it is not a vehicle
>     at all; the pair stays split.
> - **⚖ COLLAPSING IS AN ID REKEY ON THE STORE, NEVER A PER-CURATOR MERGE.** A collapse-candidate marker is
>   referenced from far outside its own record — other buildings' `disabled` clauses, repeal outcomes, requires
>   atoms, triggers and text — and a reference the merge misses becomes an id no record defines, silently. So
>   the rename is declared ONCE where the inverted edges are handed out (`Tools/Migration/store.py`, the
>   `trait_rekey` shape) and every referencing curator picks it up
>   ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
> - **⚠ A COLLAPSED ID IS RENAMED, NOT REMOVED**, so each dead id takes a bare `INFOTYPE` entry in
>   [`Assets/savemigration.txt`](../save.md) — the saveload mechanism translates it at the one stored-Type
>   resolution point, and a save holding either half then loads holding the survivor, once, empire-level.

> **The two fates are two mechanisms — nothing to declare.** `disables` = **destroy** (a law/ban
> removes it; rebuilt on repeal); the target's `requires.operate.dormant` = **dormant** (it stays put,
> inactive while the condition holds — §3). There is no flag on `disables`: the choice of *mechanism* IS the fate.

---

