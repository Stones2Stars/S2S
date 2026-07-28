# Triggers & grants — the provisions machine

> The cascade's **provisions** consumer: an `IEventConsumer` on the [event spine](event-spine.md) that, on a
> `DOMAIN` state change, resolves the source entity's payload off its info and APPLIES it. The AUTHORING shapes are
> [json.md §5](json.md); this doc is the **machine** that consumes them.
>
> **⚖ TRIGGER IS THE TOP-LEVEL CONCEPT — A GRANT IS A TRIGGER WITH A NULL CONDITION (owner).** One plane, one
> engine, one spine domain. `grants` stays a first-class AUTHORING shape (the overwhelmingly common "acquiring me
> gives this"), but nothing about it needs its own machinery.

## ONE compiled plane — how `grants` and `triggers` meet

Both authoring shapes compile into the **same entry list** (`CvTriggers` on the info); there is no separate grants
section and no `m_grants` member anywhere. A `grants` block becomes ONE entry with `consideredAction = true`, no
condition and no roll — the degenerate trigger — and its payload lives in that entry exactly as an explicit
`action.grant` payload does. The considered-action entry is an O(1) read (its index captured at parse, never
searched for).

⚑ The implicit happening is a compiled FLAG, not an `on*` token: it is never authored, so there is no token to
collide with, and the dispatching event already names which considered action it is — a building's construction, a
tech's research, a civic's adoption.

## ⛔ A GRANTED ENTITY IS AN ORDINARY ENTITY (owner)

*"The only difference between a building granted and a building constructed is that we didn't use production if
granted."* So the machine gets **no** parallel apply path, no "granted" flag, no distinct lifecycle, no ledger of
its own: it places the entity through the **SAME creation mechanism** as normal creation, and the ONLY divergence
is that the production/cost step is skipped. Settled by this, not open:

- **A grant fires the ordinary DOMAIN events** — *"like anything else"* — so the enabler, the modifier packages and
  the tally see a granted building exactly as they see a constructed one. The machine FEEDS the spine; it never
  bypasses it ([event-spine.md](event-spine.md): the spine is the SINGLE place a state change is announced).
- **A granted building runs its own first-build block**, because a construct would. The resulting grant→event→grant
  chain is intended behaviour, not re-entrancy to guard against.
- **Nothing downstream may branch on "was this granted?"** — there is no such state to read.

## ⛔ THE MACHINE REPLACES THE PER-TURN WORK — and the spine is its ONLY way in (owner)

It is not a resolver running beside legacy: the per-turn work MOVES onto the machine, and the legacy call sites are
DELETED, not re-pointed. Their ledgers become derived and are cut by
[DEC-accumulator-cut-uniform](../architecture/decisions.md#dec-accumulator-cut-uniform) via the `savemigration.txt`
soft-remove ([save.md §3](save.md)) — never a `@SAVEBREAK`.

⛔ **The per-turn apply arrives as a spine EVENT, never a direct call from `doTurn`.** The machine is an
`IEventConsumer` and that is its only front door; a machine taking events through `onEvent` *and* per-turn work
through a bespoke entry point has two front doors, which is the scattered-endpoint disease it exists to cure. The
player-scoped turn event is the natural grain — legacy ran the per-turn work inside the city loop within the
player's turn, so consuming the player boundary and walking that player's cities preserves the ordering.

**Perf constraint:** it must NOT re-create a per-city list of pending provisions (that list is the ledger being
deleted). It gates on the enabler's already-maintained **operating-building set**
([enabler.md §3.2](enabler.md)) — required for correctness anyway, since a dormant building must grant nothing.

⛔ **It never reads the legacy collapse members**, which LOSE `interval`/`enabled`/`chance` at map time. It reads
the composed entries, which carry the full structure. Never widen a legacy member to carry the missing fields.

## Registration order — the machine registers LAST

After the contexts, the enabler and the modifier: it READS the contexts (every entry condition evaluates through
the fill seams) and the enabler's operating set (a dormant building grants nothing), and unlike those machines it
**APPLIES** — so a stale read hands out a wrong GRANT, not merely a wrong number.

⚠ Every eval context it builds must be filled through the ONE seam AND wired with the enabler's operating set. The
enabler's precomputed sets are the THIRD LEG of the eval state, fed in rather than re-derived; without them the
operating-set legs sit EMPTY and any condition asking an active-building or vicinity-provides question evaluates
against nothing and quietly answers false.

## What the plane must NOT do

- ⛔ **The per-turn applier must NOT apply property pulses.** Their carriers bridge theirs at load; applying them
  again would double the value AND land it outside the solver's ordered pass, where spread resolves against
  PRE-source values ([engine.md](../reference/engine.md)) — and that engine's math is owner-LOCKED.
- ⛔ **Do not build machinery for a hypothetical verb.** A verb with zero authorings is an EXAMPLE in the spec, not
  live data; it lands if and when its authoring direction is taken.
  ⚑ **The worked case — building counter-damage (owner): IF WE WANT IT, IT IS A TRIGGER; UNTIL THEN IT IS
  NOTHING.** A trap building damaging a unit that attacks its city is a trigger by shape — a happening, a roll,
  an effect on the attacker. Modelling it needs an **`onAttacked`** happening and a **`damage`** verb, neither of
  which exists. The ruling takes neither of the two tempting shortcuts: the verbs are NOT minted speculatively for
  one mechanic, and the legacy member is NOT kept alive in the meantime — **the data goes out and the mechanic is
  authored fresh when the rework is taken.** ⚠ So being trigger-SHAPED is not on its own a reason to re-home
  something, and it is equally not a reason to preserve the old shape while waiting: a member parked on the
  `defense` family is a half-migration that reads as done. *(The UNIT-side trap subsystem is separately dead —
  [skills.md](skills.md).)*
- ⚠ **A promotion that stops being valid is dropped by the PROMOTION SYSTEM itself** (owner). So a granted
  promotion needs no take-away verb, and "the payload plane cannot revoke" is NOT an argument for re-homing the
  free-promotion shapes.

## ⛔ A DROPPED TRIGGER ANNOUNCES — every skip goes through the ONE census

**If a trigger fails to parse or to land, say so** (owner). The plane is fail-closed in several places — the bridge
refuses a source it cannot faithfully translate rather than applying it under a wrong condition, and the parser
refuses malformed input — and being fail-closed is right. Being fail-closed *and silent* is not: authored data that
loads, never applies, and reports nothing is invisible on both axes at once.

Every drop routes through the ONE load-time census
([DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)) — the same mechanism the
parser already uses for unknown verbs and keys, surfacing on readJson's coverage counts. ⛔ Do NOT add a second
reporting path or a bespoke spine domain for this.

⚑ The case that was genuinely invisible: a conditioned pulse whose condition falls outside the bridge's known
predicate set, dropped with nothing said. What the census reports is a real question to ASK of a loaded game.

---

## Game-start provisions — START PACKAGES

The game-start sequence (what a player begins the game holding) is a set of JSON-authored **start packages** the
machine applies, replacing the hardcoded engine selection. Owner intent: *"this gives modders a chance to set up
how they want."*

**The problem it solves.** Today the start is split between data and hardcoded engine logic, and the engine half is
the problem: the COUNTS are curated, but the **unit identity is not authored anywhere** — it is picked at runtime by
scanning the whole unit database, filtering on trainability and scoring with an AI valuation — and the **settler is
not in data at all**. A modder therefore cannot say what a start looks like; they can only nudge counts and hope the
scorer picks the unit they meant.

A start package is an ordinary entity — one JSON object per file — essentially a named `grants` block plus the
condition that decides when it applies:

```jsonc
{
  "type": "STARTPACKAGE_ANCIENT_DEFAULT",
  "enabled": { "type": "ERA", "max": 1 },          // the ordinary entity-level gate
  "grants": {
    "units": [ { "unit": "UNIT_SETTLER", "count": 1 },
               { "unit": "UNIT_BRUTE",   "count": 2 } ],
    "startingGold": 40
  }
}
```

- **Reuses `grants` wholesale** — no parallel vocabulary ([json.md §5](json.md)).
- **The settler stops being hardcoded** — it is just a `units` entry.
- The per-role starting counts are superseded by explicit unit entries.

> **⚖ THE POINT: author the shared start ONCE, not per civilization (owner).** `grants` can already express a start,
> but putting it on the CIVILIZATION means repeating the same block with the same conditionals across ~50 civ files
> that all start identically — *"kinda dumb, when it's the same package for all of them."* A package inverts that:
> **the condition lives ON THE PACKAGE, evaluated once**, and every civ it applies to gets it without authoring
> anything. A civilization authors something only when it DEVIATES, and that deviation is its own package stacking
> on top of the shared default.

**Packages STACK (owner).** Applicable packages sum, exactly like any other grants deposit — they are not mutually
exclusive alternatives. Stacking SUBSUMES single-selection, so the modder chooses the granularity: one coherent
package, or era + handicap + civilization composing. Single-selection could not express the second, and the engine
already adds era + handicap counts today, so stacking is the behaviour-preserving choice as well as the flexible one.

⛔ **"Conditionally loaded" means the ENTITY GATE, never a load-time prune.** The applicability condition is the
entity-level `enabled`/`disabled` pair evaluated LIVE ([DEC-entity-gate](../architecture/decisions.md#dec-entity-gate)).
Do NOT build a "load these files, skip those" prune — that is the killed `loadPrune`
([superseded-ideas](../architecture/superseded-ideas.md) #3). Every package loads; the gate decides which APPLY.

**What a new entity type requires** (skipping any of these is how an entity ends up half-wired): a folder under
`Assets/Data/`, one object per file · the `STARTPACKAGE_` infotype prefix registered in [naming.md](naming.md) · a
row in the ONE per-type repo dispatch (which earns it the full-registry re-map, the DepositIndex push, and a
`/state/info` home) · an `_order.json` manifest · and authoring through `_additions`, since entity curation is
complete and there is no legacy XML to convert — the unit identities never existed as data.

## See also
- [json.md §5](json.md) — the authoring shapes (`grants`, `triggers`).
- [event-spine.md](event-spine.md) — the `IEventConsumer` front door and the DOMAIN facts this dispatches on.
- [enabler.md §3.2](enabler.md) — the operating-building set the per-turn apply gates on.
- [legacy-grant-apply-sites.md](../reference/legacy-grant-apply-sites.md) — where the legacy engine hands
  provisions over today (the surface this replaces).
