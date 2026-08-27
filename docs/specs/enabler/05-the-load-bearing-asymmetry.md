# 5. The load-bearing asymmetry — bidirectional, not down-only

> Part of the **[enabler](../enabler.md)** spec.

The cascade is **bidirectional**: generation flows down from sources, but the `requires` gate resolves by a
**callback UP the scope chain** — a city-scope candidate asks its empire/team/world about civics, counts, state
religion. This is **how the model expresses AND** (every clause must hold, possibly at different scopes), and it
is **not optional**:

A pure down-only design (sources push everything onto targets) was tried and abandoned — it can model **OR**
(many sources enable one thing) but **cannot reliably model AND**, and it forces a modder to maintain every
requirement at the top of the chain. The upward `require` callback is load-bearing. Do not "simplify" it back to
down-only.

⚑ **The DATA proves it, so this is not a stylistic preference:** across the curated set, **~75% of building
`requires` and the large majority of unit `requires` are AND** — multi-condition, often at different scopes, with
live predicates (connected / `IS_CAPITAL` / count thresholds). A top-down single-enable inversion cannot flatten
that, so the up-walk STAYS. What makes it cheap is that it re-runs **INCREMENTALLY over only the affected
candidates** via the `EDGEF_REQUIRED_BY` reverse index (§7.1), never over the whole frontier.

⛔ **CORRECTNESS *IS* THE TARGETED INVALIDATION — there is no self-heal net behind it**
([self-heal is not a backstop](../../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). The reverse index plus targeted propagation is
the WHOLE correctness mechanism: every HAVE-change re-gates exactly its dependents, and nothing blanket-rebuilds
behind it absorbing misses. ⚑ The asymmetry to hold onto: **over-inclusion in the reverse index is SAFE** (a few
harmless extra re-checks), while a **MISS is a bug to close, never an accepted one-slice lag** — it must surface as
a live divergence (a wrong `can*` verdict, or the operating-set census disagreeing with what state expects), and
that divergence is the signal to fix the reverse-index hole.

---

