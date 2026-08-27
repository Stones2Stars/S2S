# 1. The one idea: GENERATE, then GATE

> Part of the **[enabler](../enabler.md)** spec.

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

> **⛔ THE GENERATE TREE IS CONDITIONAL-FREE — every `all`/`any`/`noneOf` lives EXCLUSIVELY in `requires`.** Pass 1 is
> **pure set algebra** — `union(enables) − (disables ∪ obsoletes ∪ replaces)` over HAVE (§2) — with **zero condition
> evaluation**: no combinators, no predicates, no "if". A candidate that *needs multiple things* is **never** a
> conditional edge in the tree — the tree unconditionally proposes it from *any* enabling source, and the AND
> ("actually need T1 **and** T2") is enforced by **`requires.build.all` on the gate** (§2 multi-parent tech; §3). So
> when the parse-time reverse-mapping inverts prereqs into `enables` it must **not** drag AND/OR into the tree — the
> tree stays unconditional; the AND/OR distinction is preserved only for the `requires`-side reconstruction. This is
> the load-bearing split: **generation is a cheap top-down sweep with no calculation; the ONLY calculation is the
> `requires` gate**, and it runs over **just the frontier** — the CAN GET candidates not yet built (the "can I have?"
> set, §6) for `requires.build`, and the built instances for `requires.operate` (§3.2) — never the whole database.

Both passes read **forward** — `enables` forward from the source, `requires` forward from the target — so the
hot path never does a reverse lookup. What is **recomputed on demand is the FRONTIER** — the pure-`f(HAVE)`
CAN GET set (§7) — **never the entire enabler**: the enabler's runtime outputs (the stored availability
vectors, the operating-building set §3.2) are maintained in place by targeted propagation, not recomputed.

---

