# 1. The big picture

> Part of the **[json](../json.md)** spec.

An entity's JSON is a **flat set of top-level keys**. Each key is exactly one of two things:

- a **reserved section** — a fixed keyword with a defined meaning (`enables`, `requires`, `allowed`, `grants`,
  `identity`, `cost`, …); or
- a **modifier family** — a per-turn effect this entity produces (`food`, `production`, `happiness`,
  `maintenance`, one per `PROPERTY_*`, …).

**The classification rule (deterministic, parser-enforced):** a non-reserved top-level key whose value is an
**object** is a **modifier family** (it is scope-keyed, §6); a non-reserved key whose value is a **bare**
bool/string/number is a **capability/skill flag or a text field**, never a family. So the *value's shape* decides
"family vs flag," and a *section's name* decides its meaning. A family colliding with a reserved word is an error.

Everything below is detail under one idea: **one object structure, one shared vocabulary, composed everywhere.**

The three machines read only the **cascade** sections (`enables`-family, `requires`, the modifier families, the
count-bearing clauses, `grants`); intrinsic and auxiliary sections feed their own systems. `readJson` parses all
of them.

---

