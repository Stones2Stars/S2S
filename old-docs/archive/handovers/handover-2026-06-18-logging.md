# Handover 2026-06-18 (session 2) — §14 H shadows landed; logging consolidated onto the event spine

> A handover is a TRANSIENT task relay (work done + work next), NOT a source of truth. Every durable fact/ruling below
> already lives in the committed docs cited; this file is deletable-without-loss. **Read the durable docs first.** This
> session followed the prior `handover-2026-06-18.md` (whose NEXT was "build the §14 H maintainer shadows").

## Where we are
Two arcs this session, both on branch `json-data-migration`, **all Assert-clean, working-tree, UNCOMMITTED** (owner
inspects before every commit — serial discipline):

1. **§14 H runtime-maintainer SHADOWS + the live-state camera suite** (the runtime twin of the buildability sweep):
   B-i auto-placement (`placementSweep` + `[PLACEMENT]`) and B-ii dormancy (`dormancySweep` + `[DORMANCY]`) shadows
   built + live-verified (human AND AI players); the `[STATE/game|fin|dip|city]` live feed + `/diagnostic/game` +
   `/diagnostic/tally` endpoints; the `autoBuild` parse + the `PROPERTY_X` band atom.

2. **A complete LOGGING-SYSTEM consolidation onto the event spine** (the owner's "total-observability / 1984" direction):
   the spine is now domain-agnostic with per-domain field resolvers; the raw-field event carries a per-line level + the
   0–5 surveillance scale; 8 AI-logging domains shadow-wired (HAI live-verified; WAR/WAI/CIT/UNT/COM/GRP/FND/DIP/ESP/CTB/ENG),
   110+ sites + 21 recovered names; verified live on `/events` at level 3.

## Read FIRST — the durable homes (this handover holds NO facts)
- **`AGENTS.md`** — Conventions + the "Cascade observability" Key-Subsystem block: the total-observability ("Orwell")
  bar, the cheap **`data-reader`** sub-agent rule (`.claude/agents/data-reader.md`), Clean-Architecture + interface-
  contracts + **poor-man's-DI (`if`/`switch` wiring)** + build-proper-once/no-debt + isolate-components, and the
  `CvInfos.h`-retire flag.
- **`docs/dev/reference/ai-logging-reference.md` §0** — the CANONICAL logging structure + the 0–5 surveillance scale
  (0 Oblivious · 1 Telescreen · 2 Informant · 3 Big Brother · 4 Thought Police · 5 Meta), the held-open-file rule
  (`/events` is the live channel; `.log` only post-close), the level↔tag mapping.
- **`docs/dev/plans/event-spine-spec.md`** — §3 (payload contract RESOLVED: generic typed-field array + per-domain
  resolvers) + §8 (the full migration progress, the recovery pass, the remaining threads).
- **`docs/dev/reference/logging-surface-inventory.md`** — the R-1..R-6 owner rulings + the consolidation plan +
  anomalies (C2.log delete, OutputDebugString-replace, dead Python exports deferred).
- **`docs/dev/reference/logging-field-catalog.md`** — the per-domain field catalog (Stage-0).
- **`docs/dev/reference/http-server.md`** — `/diagnostic/*` (sweep/placementSweep/dormancySweep/game/tally), the
  `EVENT_QUEUE_CAP=65536` bump, the `[STATE/*]`/`[PLACEMENT]`/`[DORMANCY]` per-turn lines.
- **`docs/dev/plans/cascade-mapping-inventory.md`** — §A observability bar + AI-as-cleaner-test + autoplay-measurement,
  §B-i/B-ii maintainer shadows (BUILT), §D the scale (references ai-logging-reference §0), the dated self-assessment.
- **`docs/dev/plans/cascade-known-discrepancies.md`** — B-rows (B1 match, B3/B4/B5 shadowed).
- **`docs/dev/plans/state-mapping-2026-06-18.md`** + **`docs/dev/reference/observability/`** (22 per-system maps) — the
  expanded state-map (~15–20% observable baseline) + each system's hooks for the climb to Tier 4/5.

## NEXT TASKS (the relay)
**Logging consolidation — finish it (event-spine-spec §8 has the detail):**
- **Verify each migrated domain on `/events`** (legacy-vs-spine parity, like HAI), then **CUT the legacy `log<Domain>AI`
  calls** per domain (and retire the helper) — the cutover that ends the shadow.
- **`[DAI]`** civ/flavor wide-strings (nominal — 0 live emits) + the **3 CTB `SFT_UNITAI` lines** (add `SFT_UNITAI` to
  the renderer if wanted).
- **The ~31 genuinely-unrecoverable lines** (instance `getName()`/`getDescription()` + caller-composed strings): a
  DESIGN decision — carry entity-IDs + resolve at display, or enum-ify the composed strings. Owner call.
- **Rework the Autolog BUG option → one 0–5 "Surveillance level" knob** (max bumpable to 5/Meta), retire the phantom
  per-scope knobs; use the `add-bug-option` skill. The user-facing finish line ("make it current").
- Flagged separate dragons: the **`[HAI]`/`HunterAI` rename** (it's the roaming-attacker module, not a UNITAI_HUNTER
  plugin), retiring the **`CvInfos.h`** umbrella, the **Python logging structure** (deferred per owner).

**§14 H demolition — the actual goal (cascade-mapping-inventory §B, enabler-spec §14 H):**
- **B-iii group-gate shadow**; **B3 property-band data curation** (author `autoBuild` + `requires.operate` PROPERTY band
  on the property-effect buildings → kills `placementSweep` `reason=noMarker`); **B5 resource prereqs `build→operate`**
  (the dormancy shadow surfaces `cascade-active/legacy-disabled`).
- Then **DELETE the §14 H maintainers**, re-deriving behaviour from the cascade — the over-reach-biased cutover. Shadows
  must run CLEAN first (map-before-delete).
- Climb to Tier 5: give the §A opaque systems (food wastage/espionage/culture/religion-spread/corporations) live surfaces.

## Verification recipes (run/verify boundary: owner launches the game; agent reads)
- **Never read `.log` live** (game holds them open). LIVE = capture `/events` to your OWN file across a turn boundary:
  `curl -s --max-time 150 http://127.0.0.1:7227/events > <tmp>` then grep your file. `.log` only post-close.
- Delegate bulk reads to the **`data-reader`** agent (Haiku) — distill, never raw-dump into an expensive context.
- Endpoints: `/diagnostic/placementSweep` `/dormancySweep` `/game` `/tally` `/sweep`. Level 3 = full surveillance
  (`Autolog__LogLevelPlayerBBAI`); `Autolog__HttpServer` on.
