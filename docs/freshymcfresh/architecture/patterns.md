# Patterns — interface contracts in C++03 (poor-man's DI)

> The concrete shape of [DEC-interface-contracts](decisions.md) under the frozen C++03/VC7.1 toolchain. Condensed
> from `composability.md` + `faking-di.md`.

## The interface shape (composability)
- A C++03 **interface** = an abstract base class with only pure-virtuals + a virtual dtor and **NO data members**
  (`IEventConsumer` is the realized model).
- **MI as `implements`:** one concrete satisfies several role-contracts via MI of their stateless interface bases —
  the compose-roles axis, **NOT** a DI substitute.
- **Two guardrails:** (1) MI **only** of stateless pure-virtual bases — MI of stateful concretes invites the
  diamond / layout / virtual-base mess; (2) graft interfaces onto the **DLL-internal derived** classes
  (`CvCityAI`/`CvUnitAI`), **never** onto EXE-bound bases (`CvCity`/`CvUnit` — the closed `.exe` binds their
  vtable/layout). The derived side is the safe lane and the lever for shrinking the god-classes.
- **Isolate-systems recipe:** when two systems entangle, give each its own data block + predicate query-surface,
  have both implement the one shared contract, and switch at the composition root. (Worked example: simple traits vs
  complex/Thunderbrd traits.)

## Poor-man's DI (faking-di)
No DI container exists (C++03/VC7.1; the EXE binds concretes), so:
1. Define the dependency as an **interface** (pure-virtual base, no data).
2. The consumer holds a **pointer to the interface**, never to a concrete.
3. At the **composition root**, a literal `if`/`switch` picks the concrete and assigns it — that `if`/`switch` is
   the manual "container." (Canonical use: game-option override-by-design swaps — one option check selects the impl;
   the consumer sees only the contract.)
- **Guardrails:** MI is not a DI substitute (you still inject via a base pointer); the decoupling is real even
  without a container ("no container" is never an excuse to `#include` the concrete into the consumer); the
  composition root is the **only** place that names concretes (a leaked concrete = the root is no longer the single
  wiring point).
