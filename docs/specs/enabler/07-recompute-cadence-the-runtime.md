# 7. Recompute cadence + the runtime realization — event-maintained vectors over `f(HAVE)`

> Part of the **[enabler](../enabler.md)** spec.

**What is recomputed on demand is the FRONTIER — never the entire enabler.** The frontier is a pure function of
HAVE — conditional-free set algebra (§1) — so it is **recomputed when HAVE changes**, not cached with deltas. The
dominant cadence is once per turn, but same-turn HAVE-changes must trigger a mid-turn recompute (the AI finishes
building A then builds B the same turn; religion spreads; a bonus connects; a city is conquered). This stays
cheap: the bounded two-pass over the affected scope is *less* work than the scattered legacy checks it replaces,
which already re-scan the whole database constantly. Any caching is a separate optimization layer wrapped around
the pure `HAVE → frontier` function, never leaking into the model.

**The runtime realization (LOCKED) — a CONSTANTLY-UPDATED VECTOR, not recompute-on-read.** The
`canConstruct` / `canResearch` / `canTrain` / … lists are **stored vectors the ENABLER OWNS**, built **once** at
load by the **reseed events** (the in-read emits stream through the same appliers as play,
[the load reseed](../../spine/05-the-load-reseed.md#5-the-load-reseed) — never a warm-up walk beside the event
stream) and **updated in place on events** (a tech researched adds its `enables` / removes its
obsoletes; a building built leaves `buildable`; …). Every read is a **pure O(1) lookup that NEVER calls a
calculator**, and the enabler consumes ONLY events precisely so a missed emit surfaces as a visibly wrong
enabler. ⛔ **There is no from-source recompute to diff that against, and none comes back** — the
fresh-seed-and-diff statics that once served one are DELETED
([superseded-ideas #33](../../architecture/superseded-ideas.md)): an endpoint cannot replay the event chain, so its
recompute side was never comparable, and the replay it would need is minutes of work — disqualifying for an
endpoint call twice over. A wrong verdict is caught by the THREE-LEG check
([http-endpoints.md](../http-endpoints.md)), and DECOMPOSED for a reader by the enabler's own stored-side censuses
(`/computed/enabler/operating` · `/buildings` · `/verdict` · `/units`), which serve the maintained verdict term
by term and never recompute it. The `requires` gate re-runs **incrementally over only the affected candidates** (via the reverse
index), and the operating-building set (§3.2) is maintained the same way — this is
[cascade.md](../../cascade.md)' targeted propagation applied to the availability
machine. The representation is deliberately primitive: **the HAS list, and the enabler list built from HAS, are
literally TWO SETS OF INTS (enum ids)** — set algebra over int sets, nothing richer.

**Per-scope instantiation — EACH CITY owns its OWN enabler object (buildings + units).** The
buildable/trainable lists are per-city derived state, so every `CvCity` carries its own enabler object — exactly
as it carries its package set — and the player carries the player-domain lists (researchable / adoptable /
hurries / …). It is **ONE unified enabler component**, instantiated per scope owner and fed by the eventspine
consumer. A value cache recomputes on its mark; the enabler
**fundamentally behaves differently: the CAN-HAVE set is built PURELY on
the events of ALREADY-HAS** — each HAVE-event applies its `enables`/removal edges in place, the load reseed's
events are the one full build, and no mark-then-recompute path exists at all. A component's `requires` gate resolves
cross-scope atoms by reading its parent scope's state up the chain (§5's upward callback, realized).

**HAVE is NOT a new store — and its READ SURFACE is the per-scope CONTEXTS**
([contexts.md](../../cascade.md), owner). The object-owned has-lists that ALREADY EXIST (the city's
buildings-present / religions / corporations, the player's civics / traits / heritages, the team's techs) stay
where they are — the object owns its presence state, the [tally](../tally.md) rule ("let an object care about
itself") applied to presence — and each scope's CONTEXT forwards them (storing only a homeless aggregate, e.g.
`policies`), so every reader — the evaluator's atoms, the gates — asks the context, never reaches into the game
object ad hoc. The DOMAIN event carries the delta that triggers the in-place list update, and the enabler stores
only what it **derives** (the lists + the operating-building set). Predicates/atoms read HAVE through the
contexts; what is event-driven is the **maintenance** (which dependents re-gate, when), never a read-side
recompute.

**Event-fed, the end-state:** the enabler's derived sets — the **domain lists**, the **operating-building set**
— are built by the **load reseed** (the in-read DOMAIN events populate them,
[the load reseed](../../spine/05-the-load-reseed.md#5-the-load-reseed)) and **maintained incrementally by play-time
events** (building built → the city's lists re-gate its dependents; tech researched → its `enables`/`obsoletes`
edges apply; bonus network shift → operate re-check) — never re-reading live game objects wholesale and never a
per-turn blanket re-check. Exactly the modifier caches' model, applied to the "can I?" machine.

**Mid-turn HAVE-change triggers** also include **inquisition** (which retracts a RELIGION, not just a building —
disproving "buildings-only" state-retraction), nuke, and `doAutobuild` add/remove.

**Gather order — "right-then-down".** Pass 1 gathers in dependency order: sticky top (techs/civics) first, then
volatile bottom (resources/bonuses/buildings), so derived have-entries resolve against what's already gathered.

**Game-option gates are the ENTITY-LEVEL `enabled`/`disabled` pair, evaluated LIVE ([the whole-entity applicability gate](../json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity)).**
The legacy engine checks the option tags at USE time, and the gate mirrors that: an entity whose `enabled` fails (or
`disabled` holds) is simply never offered/valid while the option state says so. LOAD-STABLE machinery that genuinely
resolves at load (the legacy whole-Info replacement swap — dissolved into the curated trait sets, see
[modifier.md](../../cascade.md) — WorldBuilder/BUG, a per-civ research ban) is engine-side, not entity data.

### 7.1 The concrete structure + the delta algorithm

**Storage — one per-domain TRI-STATE ARRAY per owner** (semantically the two int-sets of §7; physically flatter):
`state[id] ∈ {HIDDEN, GREYED, LISTED}`, a byte-array indexed by enum id, one per domain on its owner (city:
buildings, units; player: techs, civics, projects, processes). **Hurries are NOT an enabler domain** — whether
a hurry type is usable is a civic-enacted ability (the capabilities/policies side, [capabilities.md](../capabilities.md));
the city `canHurry` gold/population/progress arithmetic is a live stats check. Neither half is this machine's.
**The owner is where the domain's HAVE
axes live, NOT where the gate is asked:** projects/processes are chosen and built on the CITY's production list
(`canCreate`/`canMaintain` — a project builds exactly like a unit/building/wonder, one city queue with a
team-wide effect; the engine's apparent multi-city project production does not actually work), but their axes
are team-scope, so the domain is PLAYER-held — per-city copies would be byte-identical duplicated state that
must never drift — and the city gate reads through its owner (a dynamic `getOwner()` lookup, conquest-safe;
never a stored pointer). The one city-local project fact (the plot map-category gate) stays a live check at
the gate, the same split as worker builds below. CAN GET = `state ≥ GREYED`; the
gate-passed set = `LISTED`; §6's tri-state IS the array. Chosen over two `std::set<int>`s deliberately: O(1)
reads on the AI's hottest gates (vs O(log n) + ~20 B/entry tree-node overhead), O(delta) writes, ~8.5 KB per city
for both big domains, and frontier iteration is a linear byte scan. The **only mutable state is these arrays**
(plus the operating-building set §3.2); the reverse indices are static load-compiled data; **nothing serializes**
— the load reseed is the one full build.

**The delta algorithm — per HAVE-event H, everything O(delta):**

1. **Generation — membership is the FORMULA, never the operation sequence.** A candidate is in CAN GET iff
   `(≥1 held source enables it) ∧ (0 held sources remove it)`, maintained as **two per-candidate refcounts**:
   H's `enables.<bucket>` entries increment their candidates' enable-count; H's
   `obsoletes`/`disables`/`replaces` edges increment the remove-count; a **lost** source decrements (civic
   swaps, bonus disconnects). Membership = `enableCount > 0 && removeCount == 0` — **REMOVAL WINS regardless of
   arrival order**. ⛔ The naive sequenced add/erase delta ("insert on enables, erase on removes") is BANNED: an
   enables-add arriving after an obsoletes-remove re-inserts the candidate (the `TECH_GAME_START`-arrives-last /
   obsoleted-`UNIT_BRUTE` edge case — the remove was a no-op on the absent element, then the late add resurrects
   it). Same refcount shape as the operating-building set's provided-bonus counts. Entering CAN GET gates
   **once** (→ GREYED or LISTED); leaving → HIDDEN, with §2's instance-fate side effects.
2. **Re-gate:** the requires-reverse-index (HAVE-atom id → dependent candidate ids) names the in-tree
   candidates whose `requires` references H; **only those** re-evaluate, flipping GREYED↔LISTED. Its canonical
   home is **`EDGEF_REQUIRED_BY` on the referenced info**, populated by the readJson reverse pass
   ([reverse lookups are populated once, at load](../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)) — never a bespoke side index
   inside an enabler.
3. **Caps / queue / built:** a count event re-checks `allowed` for that one type; queueing/completion is the
   targeted single-id erase. The leave-rules differ per domain: a **building** leaves the frontier when built; a
   **unit** stays trainable (it leaves only on a cap or supersession).
4. **Operate ripple:** operate-atoms referencing H drive the operating-building work-list fixpoint (§3.2).

**⛔ ORDER-INDEPENDENCE is a HARD INVARIANT of the delta algorithm.** Events are facts, not causal steps
([spine.md](../../spine.md)) — the sets must converge to the same content whatever order the events arrive
in (`TECH_GAME_START` last, first, or anywhere). The algorithm guarantees it because every piece is commutative:
generation is the **refcounted membership formula** (step 1 — removal wins; sequenced add/erase is banned);
gating is gate-on-entry *against current state* + re-gate via the reverse index when a referenced atom later
changes. Three implementation failure modes are therefore BANNED: (a) any ordering assumption in the delta
("parents before children" — prerequisite logic belongs only in `requires`, which re-gates); (b) the sequenced
add/erase membership delta (step 1's edge case); (c) a load reseed that gates-on-entry against half-built state
while SKIPPING re-gates during the load window — during the reseed either every event's re-gates apply as they
arrive, or gating runs once after the stream ends; both are correct, the mix is the bug.

**Two deliberate maintained-set EXCEPTIONS (efficiency — maintain only where reads are hot and the owner-space
is small):** **promotions** keep no per-unit maintained sets (thousands of units × hundreds of promotions,
churned on every tech, for a decision that only happens at level-up) — the player maintains one
unlocked-promotions set and `canAcquirePromotion` evaluates on demand at level-up; **worker builds** — the player
maintains the unlocked-builds set, and the plot-validity half stays a live per-plot gate (a maintained set over
~10k plots is waste; worker decisions already iterate plots).

---

