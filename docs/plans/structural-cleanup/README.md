# Structural cleanup — the #430 work tier

> **The #430 ENGINE is built and wired; its CONSUMER half is not.** The cascade, enabler and spine are in the tree
> and maintained rather than recomputed, and that half's leftovers are polish, tracked per-concept in
> `docs/specs/`/`docs/reference/` rather than as a standing worklist here.
>
> ⛔ **What is NOT polish: the AI/engine consumers that still walk the INFO REGISTRIES instead of reading the
> compiled edges.** `python Tools/verify-registry-scans.py` is that census — ENABLER-DOMAIN sites re-point onto the
> maintained frontier, OTHER-REGISTRY sites invert onto the entity's own compiled entries.
> ⚖ Only a DECISION path is a defect: init, reset, serialization, save/load, UI enumeration and text rendering
> legitimately walk a registry, so triage per site. The counts are a **RATCHET** — they may only fall, and a rise
> means a scan was re-introduced.
> ⚑ This is about INFO registries, not iteration in general: a plot is a game-world object, not an info, so bounded
> plot iteration is normal and is not what this tracks.

## Owner-LOCKED

- **[property-audit.md](property-audit.md)** — the property SOURCE-data migration. The property ENGINE math is
  KEEP-legacy and must NOT be rewritten.

---

**Where the legacy MAPS went:** they are censuses of how the legacy behaves, not work to do, so they live in
[`docs/reference/`](../../reference/) — a legacy map filed under `plans/` reads as planned work, which is exactly
the bait. See `legacy-grant-apply-sites`, `pedia-read-map`, `python-read-map`.
