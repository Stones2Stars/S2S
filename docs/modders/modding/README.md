# Modding reference (imported — pending S2S review)

This section collects modding/player documentation **imported and adapted from the
Caveman2Cosmos (C2C) community** on CivFanatics. Stones2Stars is a direct successor
to C2C and inherits most of its systems, so this material is a useful starting
reference — **but it has not yet been verified against the S2S codebase.**

> ⚠️ **Review status: UNVERIFIED.** S2S has diverged from C2C in significant areas
> (the combat model in `CvCombatModel`, AI architecture and tagged logging, the
> assert/logging infrastructure, build tooling). Treat every specific (XML tag
> name, numeric value, behaviour) as "C2C says…", to be confirmed against current
> S2S source before relying on it. Corrections welcome — fix in place and drop the
> per-section caveat once verified.

## Contents

- [xml-validator.md](xml-validator.md) — how to run `Tools\XmlValidator.exe` to find and fix XML errors before they fail the CI build (S2S-verified, not imported).
- [modders-documentation.md](modders-documentation.md) — adapted from the C2C "Modder's Documentation" thread (XML schema/modules, AI flavors, properties, the expression/outcome system, combat-mod schema, Outbreaks & Afflictions, Size Matters, multimaps, art pipeline, Revolutions).
- [player-guide.md](player-guide.md) — adapted from the "Caveman2Cosmos v44 Player Guide" thread (game options, eras/gateway techs, city/unit/culture/combat/religion/trait/civic/economy systems).

## Provenance

Each document cites its source thread and names the original C2C authors. The
content was synthesised by an automated reading pass over the **opening posts** of
those threads; reply-thread discussion was intentionally excluded.
