# Composability — small interface contracts over deep inheritance

> **Status:** landed pattern · **Grounding:** [`AGENTS.md`](../../../../AGENTS.md) architectural rulings; the
> realized model is `IEventConsumer` (`Sources/Cascade/`). · **Ruling:**
> [DEC-interface-contracts](../decisions.md#dec-interface-contracts).

**Intent.** Build behaviour by composing small, explicit contracts — not by extending the inherited Civ4
god-classes. This is the lever for the standing goal of dissolving `CvCityAI` / `CvUnitAI`.

**The C++03 interface shape.** An interface here = an **abstract base class with only pure-virtuals + a
virtual destructor and NO data members.** `IEventConsumer` is the model (`Sources/Cascade/`). No state on
an interface — state lives on the concrete.

**MI is the `implements IA, IB` mechanism.** One concrete class can satisfy several role-contracts via
multiple inheritance of their interface bases. This is the *implements* axis (composition of roles), **not**
a DI substitute (you still inject — [faking-di.md](faking-di.md)).

**Two load-bearing guardrails:**
1. **MI only of stateless pure-virtual interface bases.** MI of stateful concretes invites the
   diamond / layout / virtual-base mess. Interfaces have no data members, so they compose cleanly.
2. **Graft interfaces onto the DLL-internal DERIVED classes (`CvCityAI`/`CvUnitAI`/…), NEVER widen an
   EXE-bound base (`CvCity`/`CvUnit`).** The closed `.exe` binds the base classes' vtable/layout; adding a
   base there risks the ABI. The derived side is the safe lane **and** the lever for shrinking the
   god-classes.

**The isolate-systems-behind-one-contract recipe.** When two systems entangle: give each its own **data
block + predicate query-surface**, then have both **implement the one shared contract**. The simple traits
and the complex/Thunderbrd traits are the worked example — once both implement the single trait contract,
the composition root selects one ([faking-di.md](faking-di.md)) and every consumer depends only on the
contract. Apply this wherever two concerns leak into each other: isolate, unify behind one contract, switch
at the root.

**Realized example.** The cascade: `IEventConsumer` is the contract; spine / tally / grants / logging are
independent implementations composed behind it; the three machines (enabler / modifier / tally) are
interface-bounded, each built once.

## See also
- [faking-di.md](faking-di.md) — how the chosen implementation is wired in.
- [`../north-star.md`](../north-star.md) — the Clean-Architecture destination this serves.
