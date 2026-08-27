# Poor-man's DI (faking-di)

> Part of the **[patterns](../patterns.md)** spec.

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

