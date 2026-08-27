# 7. Triggers: the plane that acts

> Part of the **[overview](../overview.md)** spec.

→ [specs/triggers.md](../specs/triggers.md)

The spine announces what happened; the cascade works out how much. Neither *does* anything — and a game is
mostly things being handed over.

### A grant is a trigger with a null condition

One plane, not two. Three named parts in reading order: **when/why → odds → what it does**.

```jsonc
"triggers": [
  { "trigger": { "type": "PROPERTY_FLAMMABILITY", "min": 200 },
    "chance": 5,
    "action": { "destroy": { "building": "random" } } },

  { "trigger": "onTurn",
    "chance": { "value": 5, "per": { "type": "PROPERTY_CRIME", "scope": "city" } },
    "action": { "grant": { "units": ["UNIT_PROPERTY_CRIMINAL"] } } }
]
```

A `grants` block is the degenerate case: the happening is implicit — the source's own considered action — with
no condition and no roll. It stays a first-class authoring shape because "acquiring me gives this" is
overwhelmingly the common case, but underneath there is one compiled entry list and one engine.

**Odds are data; the roll is not.** The draw comes off the synchronized stream, which is shared save state, so
no JSON authors a seed, a stream or a draw.

### A granted entity is an ordinary entity

> The only difference between a building granted and a building constructed is that we didn't use production if
> granted.

No parallel apply path, no "granted" flag, no separate lifecycle. It fires the ordinary state facts, so the
enabler, cascade and tally see it exactly as a constructed one, and it runs its own first-build effects.
**Nothing downstream may branch on "was this granted?"**

The same rule binds units, and there it fixed something measurable:

```
  WAS                                          IS
  production order ─▶ gives the city's XP      production order ┐
  religion founder ─▶ gives none               any trigger      ├─▶ createUnit()
  property spawn   ─▶ gives the city's XP      capture · bribe  │   settles what
  first discoverer ─▶ a third path entirely    the map editor   ┘   creation owes
```

A unit's starting experience depended on which payload created it. **The tell is a divergent side-effect set,
not a divergent call** — every route already reached the same low-level function; each had simply remembered a
different subset of what a new unit is owed.

Transformations — upgrade, gift, merge — must **not** ride the creation step, and the reason is concrete: the
step settles what a city owes a unit *born* there, so routing an upgrade through it would hand out free
experience on every upgrade and turn a barracks city into an XP faucet.

> If there is one place that can create a unit in other ways, that is a rollerskating surface — particularly for
> modders.

Which is why the map editor goes through the same step. An alternate path is exactly where the wrong lesson
gets taught.

### Why it registers last

Consumers dispatch in registration order, and this one goes after the contexts, the enabler and the cascade. It
reads all three, and gates on the enabler's operating-building set because **a dormant building must grant
nothing**. Unlike those machines it *applies*: a stale read hands out a wrong grant, not merely a wrong number.

The spine is its only front door. Per-turn work arrives as a turn fact, never a direct call from the turn loop —
two front doors is the scattered-endpoint disease it exists to cure.

### Telling a trigger from a modifier

A number whose value is a **threshold deciding whether something happens** is a trigger condition. A number that
is **added, scaled or stacked** is a modifier. If two copies would never sum, no channel was ever involved.

### What the plane refuses to build

The standing temptation is a verb for a mechanic somebody might author later. A trap building damaging attackers
is a trigger by shape, but modelling it needs an `onAttacked` happening and a `damage` verb, neither of which
exists. **If we want it, it is a trigger; until then it is nothing.** Neither shortcut is taken: verbs are not
minted speculatively, and the old data member is not kept alive meanwhile — a member parked on the wrong family
is a half-migration that reads as finished.

A trigger that fails to parse or land *says so*. Fail-closed is right; fail-closed **and silent** is not.

### Start packages: engine logic becoming data

The starting units are half data and half hardcoded: the counts are curated, but **the unit identity is not
authored anywhere** — chosen at runtime by scanning the whole unit database, filtering on trainability and
scoring with an AI valuation. The settler is not in data at all.

```jsonc
{
  "type": "STARTPACKAGE_ANCIENT_DEFAULT",
  "enabled": { "type": "ERA", "max": 1 },
  "grants": {
    "units": [ { "unit": "UNIT_SETTLER", "count": 1 },
               { "unit": "UNIT_BRUTE",   "count": 2 } ],
    "startingGold": 40
  }
}
```

No new vocabulary — it reuses `grants` wholesale, and packages **stack**: the condition lives on the package and
is evaluated once, so every civilization it applies to gets it without authoring anything. A civilization writes
something only when it deviates.

### And this is what the events become

The event system is the half that has not moved. A large amount of gameplay still lives in Python scripts —
per-wonder effects, combat reactions, respawn handouts — hardcoded, invisible to every C++ consumer, and
impossible to author without writing code.

> Having scripts like this in Python is the root of all evil. Most of it can even be expressed as triggers.

Those scripts are *already* trigger-shaped: a happening, a chance, an effect. The bridge is built and
deliberately left unfinished in one specific way:

- **The reporting hub now emits a spine fact beside every Python call**, so a happening that used to reach only
  Python is on the wire for every consumer.
- **Those facts are raw on purpose** — the reporter's own arguments, inventing nothing. The authoring vocabulary
  should be settled when the events are genuinely formalized; anything designed now gets undone by that.
- **It already pays for itself in provenance.** A captured unit used to announce only "a unit was created",
  indistinguishable from one trained, granted or editor-placed — and no flag could express a capture's *second
  party*.

From here: happenings get names, the verbs they need get defined in data, and a Python script becomes an
authored entry a modder can read and change without touching code. The discipline meanwhile is not to start
opportunistically — writing one as a trigger requires its happening and verb to exist.

---

