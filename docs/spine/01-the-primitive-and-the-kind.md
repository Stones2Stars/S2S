# 1. The primitive and the KIND firewall

> Part of the **[spine](../spine.md)** spec.

A caller `emit`s an event; every consumer that registered interest in that event's **KIND** receives it. KIND is
declared **at the call site, never inferred**.

Civ4 multiplayer is deterministic lockstep, so an authoritative count that differs per machine is a desync. KIND
keeps the synced and unsynced streams apart:

| KIND | meaning | synced? | consumed by |
|---|---|---|---|
| **`DOMAIN`** | game **state** changed (building built, unit created, tech researched) | yes — deterministic | logging + grants + cache-invalidation + out-of-process replay (NOT the in-engine tally — it reads the object-owned counts) |
| **`SAVELOAD`** | a fact was **read off the save stream** — a log of LOADING, never what sets state | no | **logging only** — never counted, never gates |
| **`DIAGNOSTIC`** | **code** ran (a function entered, a decision re-evaluated) | no — execution trace | **logging only** — never counted, never gates |
| **`TRACE`** | fine-grained "every step" | no | logging only |

> **⛔ `SAVELOAD` IS ITS OWN KIND AND NOT A `DIAGNOSTIC`, AND THE DIFFERENCE IS LOAD-BEARING.**
> `DIAGNOSTIC` means CODE RAN. A save-load fact is a record of what the STREAM CONTAINED — a different
> statement — so filing it under `DIAGNOSTIC` would put *"the save says this plot is `TERRAIN_GRASS`"* in the
> same bucket as *"this function was entered"*, after which only convention separates them.
> ⚑ **Its own kind makes the rule STRUCTURAL rather than remembered:** a state-building consumer registers for
> `DOMAIN`, so *"nothing derives held state from the load log"* is enforced by the interest mask, not by
> reviewer memory — the contract-not-prohibition shape ([patterns.md](../architecture/patterns.md)).
> ⚑ It also gets its own volume story for free: the load record is the highest-volume stream in the engine, and
> as a separate kind it never has to ride the `DIAGNOSTIC` firehose to be watched.
> ⛔ **It needs NO gate knob of its own** — volume rides the event's own `iLevel` through the existing file
> (`gPlayerLogLevel`) and stream (`gStreamLogLevel`) gates. Only `DOMAIN` streams unconditionally; a load
> record must never spend the bounded SSE slots during ordinary play.
> ⚠ **The load's DOMAIN facts are NOT these.** The save read populates base state through the objects' own
> INTERNAL SETTERS, and those setters emit the ordinary `DOMAIN` facts that build the cascade, the enabler and
> the contexts — one mechanism for load and for play (§5). A `SAVELOAD` line is testimony ABOUT the read, beside
> them, and nothing folds on it.

Only `DOMAIN` events carry authoritative synced state-changes (for observability, cache-invalidation, and the
out-of-process replay). The in-engine [tally](../specs/tally.md) does **not** consume them — it reads the object-owned
counts directly. The payload is **raw** (typed fields, never a pre-formatted string) so the costly index→text
formatting defers to the gated logging consumer (§8) — when a gate is off, nothing expensive ran.

