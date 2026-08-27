# The host — where the state lives

> Part of the **[08-the-machines-shape-components-host](../08-the-machines-shape-components-host.md)** spec.

The machine's state lives on its scope owners, as plain DATA MEMBERS (the guardrail bars adding vtable *bases*
to EXE-bound classes, never members — [cascade.md](../../../cascade.md)):

| owner | member | what it holds |
|---|---|---|
| `CvCity` | `m_enabler` (`CityEnabler`) | the constructible + trainable tri-state domains |
| `CvCity` | `m_operatingBuildings` | the ACTIVE set + provided bonuses at the operate/provides fixpoint (§3.2) |
| `CvPlayer` | `m_enabler` (`PlayerEnabler`) | techs / civics / projects / processes / builds / promotions |

All are **public and mutable** by requirement rather than laxity: the domain enablers write through a
`const CvCity&` / `const CvPlayer&` — the owner holds the STORAGE, the enabler owns the delta LOGIC. **None is
serialized**: every one starts empty and un-ready and is filled by the reseed's events through the same appliers
play uses ([the load reseed](../../../spine/05-the-load-reseed.md#5-the-load-reseed)). Each owner's `reset()` clears them,
which is load-bearing because a `CvCity` is RECYCLED out of an `FFreeListTrashArray` — without it a new city
inherits the previous occupant's frontier.

⛔ **REGISTRATION ORDER IS A CONTRACT: contexts → enabler → modifier.** The enabler's load-end gate pass evaluates
through the CityContext / EmpireContext stores, which the contexts' consumer builds on the SAME
`GAME_LOAD_FINISHED` event; gating ahead of it evaluates against empty stores and every verdict is silently wrong,
with no self-heal to re-derive it ([cascade.md](../../../cascade.md)).

