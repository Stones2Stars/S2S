# The INFO DATA-OUT contract — what an info hands to the cascade

> Part of the **[patterns](../patterns.md)** spec.

> **This section is the home of [parsing/holding info data is info-side, never cascade-side](#the-info-data-out-contract--what-an-info-hands-to-the-cascade)** — parsing and holding
> the info data is INFO-side, and cascade runtime never lives on an info.
>
> The **infos** row of [EACH IS ITS OWN SYSTEM](../north-star.md): readJson puts data into infos, infos SERVE that
> data, the cascade sums, the enabler resolves availability. This section is that row's concrete surface.
> **It is stated as a CONTRACT, not a prohibition** — a prohibition has to be remembered by every future agent,
> the enforcement model this project keeps watching fail; a contract makes the violation unsayable rather than
> forbidden, because there is no member to write to.

**An info is a pure DATA SOURCE with one outbound surface.** It is loaded once, immutable thereafter, and shared
by every player — so it can carry authored data and nothing else. Concretely:

1. **What an info holds** — the availability model (the `enables` family, `requires`/`allowed`, the load-derived
   reverse edge families) and its own authored modifier data, resolved to typed members at `mapFrom`.
2. **What an info hands out** — its data, ASKED FOR BY CHANNEL: *"give me your flats / your percents for these
   channels."* The cascade points at a LIST of infos and sums what comes back. It never reaches inside an info's
   per-type shape, and an info never learns what a cascade, a scope, or an owner is.
3. **What an info CANNOT hold** — per-owner state, a computed total, a staleness flag, a cache. Not by rule: by
   construction. There is nowhere on the object to put it, because the outbound surface is the only surface.

**Why the boundary is load-bearing, not tidiness.** An info is write-once-at-load and shared; cascade runtime is
per-owner mutable derived state. Storing the latter on the former silently makes an immutable, shared object
mutable **per game rather than per load** — and it is the third copy of the same static numbers, after the
authored JSON and the compiled deposit index.

⛔ **The pages below ARE the spec — this page is a map and carries no ruling of its own.**
Read the parts your work touches END TO END; the count that applies is something you FIND, not something
you decide ([AGENTS.md](../../../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[write once at load a read never](04-the-info-data-out-contract-what-an/01-write-once-at-load-a-read-never.md)** | ⛔ WRITE-ONCE-AT-LOAD — A READ NEVER CREATES, AND AN UNANSWERABLE READ FAILS LOUD |
| **[an info is styled for the json not](04-the-info-data-out-contract-what-an/02-an-info-is-styled-for-the-json-not.md)** | An info is STYLED FOR THE JSON, not the legacy field set |
| **[the coherent surface grouped](04-the-info-data-out-contract-what-an/03-the-coherent-surface-grouped.md)** | The coherent surface — grouped storage, parameterized getters (CLARITY AND PREDICTABILITY IS KING) |

