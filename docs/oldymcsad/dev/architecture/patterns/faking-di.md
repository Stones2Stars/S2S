# Faking DI — poor-man's dependency injection in C++03

> **Status:** landed pattern · **Grounding:** [`AGENTS.md`](../../../../AGENTS.md) architectural rulings; the
> realized contract is `IEventConsumer` (`Sources/Cascade/`). · **Ruling:**
> [DEC-interface-contracts](../decisions.md#dec-interface-contracts).

**Intent.** Get real dependency inversion — a consumer that depends only on an interface, with the concrete
chosen elsewhere — in an engine that has **no DI container** and a frozen toolchain.

**Context.** C++03 / VC7.1, and the closed Firaxis `.exe` **binds concrete classes**. There is no
container to resolve `IFoo → FooImpl`; templates/lambdas/modern reflection are unavailable. So the wiring
is manual — but the *decoupling* is fully real.

**The pattern.**
1. Define the dependency as an **interface** (abstract base, pure-virtuals + virtual dtor, no data
   members — see [composability.md](composability.md)).
2. The consumer holds a **pointer to the interface**, never to a concrete.
3. At the **composition root**, a literal **`if`/`switch`** picks the concrete and assigns it to that
   pointer. That `if`/`switch` *is* the "container" — a one-line manual resolve.

```cpp
// composition root — the only place that knows the concretes
ITrait* pTraits = useComplexTraits ? (ITrait*)&g_complexTraits   // §5a game-option override
                                    : (ITrait*)&g_vanillaTraits;
// the game object is handed pTraits; it depends ONLY on ITrait, never learns which it got.
```

**Realized example.** The cascade's `IEventConsumer` is the contract; the spine / tally / grants / logging
are pluggable behind it (`Sources/Cascade/`). The game-option **override-by-design** swaps (e.g. complex
vs vanilla traits) are this pattern's canonical use: one option check at the root selects the
implementation; every consumer sees only the contract.

**Guardrails / what it is NOT.**
- **MI is not a DI substitute.** Multiple inheritance lets one concrete *implement* several contracts (the
  `implements IA, IB` axis — [composability.md](composability.md)); it does **not** replace injection. You
  still inject by holding a base pointer assigned at the composition root.
- **The decoupling is real; only the wiring is manual.** Do not let "no container" become an excuse to
  `#include` the concrete into the consumer — that re-couples them and throws the pattern away.
- **The composition root is the only place that names concretes.** If a concrete type leaks into a
  consumer, the root is no longer the single wiring point.

## See also
- [composability.md](composability.md) — the interface shape this depends on, and MI-as-`implements`.
- [`../north-star.md`](../north-star.md) — why this is the direction (Clean Architecture in C++03).
