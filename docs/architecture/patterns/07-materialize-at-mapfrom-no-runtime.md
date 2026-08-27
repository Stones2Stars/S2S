# Materialize at mapFrom — no runtime string reads in info getters (the single-source law's load-time sibling)

> Part of the **[patterns](../patterns.md)** spec.

> Binding: [materialize at mapFrom](#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling). Owner ruling: *"all of these should
> use the standardized jsonreader and be loaded properly into the info — remapping directly from a json read is a
> gigantic nono."*

**The law.** A `CvJson<X>Info` GETTER never does a per-call string-keyed read — no modifier-address sum
(`"happiness.city"` lookups), no bool-block `std::set<string>` walk, no grants/allowed bucket-string fetch, no raw
picojson re-read. Every such value is **materialized ONCE at `mapFrom`** into a typed member (scalar, positional
array, sparse id-keyed map, or a classification-id bitset), and the getter is a **bare member read**. The measured
why: these getters sit under the EXE frame loop (`unit.isInvisible` ~98M calls/turn-window), the pathfinder's
per-step gates, and the AI's per-candidate scans — a heap-string construction + map walk per call was a real
turn-time/FPS tax.

- **The ONE load-time scan source is the compiled `CvModifiers` entry list** (`entries()` — every §3.9 deposit
  as a typed `CvModEntry` with interned family/kind/scope/unit/target axes). A load-time pass (the DepositIndex
  push, the reverse passes, a poco materialization) iterates the typed entries; a getter never walks them — it
  reads the compiled `(family, kind, scope, unit)` slot sums (`sum100`) or its own materialized members.
- **Classification blocks read by GENERATED ID** — the §8/§9 bool blocks resolve their keys to the
  `ClassificationRegistry`'s runtime-minted ids ([the classification-infos registry](../../specs/json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)),
  and the getters are `CLS_HAS`/`CLS_COUNT` bit tests (memoized id + O(1) bitset read; the pre-resolve load window
  falls back to the string set so early consumers stay correct).
- mapFrom is idempotent by contract, so the materialized members are fully redefined on every (re-)map —
  clear-first for accumulating containers, unconditional assignment for scalars.
- **A CROSS-ENTITY value materializes in the REVERSE PASS's post-map derivation step, not at mapFrom.**
  `mapFrom` structurally cannot serve a value derived from *another* info's edges — it runs while the reverse
  view is still being built, so the view it would read is incomplete. The one home is a `rp_derive*` sub-pass
  inside `reversePassRun()` (`Data/CvReversePass.cpp`), calling the type's `deriveAtRegistryComplete()` once
  every entity is mapped and the RELATED/REQUIRED_BY families are landed; where the derivation needs a
  cross-registry fact, the PASS computes it once and FEEDS it in (the DRY shape — a machine never re-derives
  what another can hand it). Idempotent like its siblings: it fully redefines every member it fills.
  ⛔ The alternative — resolving on first read behind a memo — is BANNED, and not as untidiness: a memo puts a
  cache **and a staleness flag** on an info, which the INFO DATA-OUT contract above forbids *by construction*.
  ⛔ And it is ONE step, not a per-type habit: minting a second post-map hook beside this pass is the
  does-the-same-thing failure the enforcement check below exists to catch — reuse `deriveAtRegistryComplete`.
  *(Realized: the unit plane's SM base sums / derived era / upgrade-chain closure, and `CvHeritageInfo`'s
  acquisition prereqs — the tech and predecessor heritages whose `enables.heritages` list it.)*
- The cascade's own gated sums are NOT this surface — they are `MMKernel` over the compiled `DepositIndex`,
  running at mark-rebuild cadence, not per read.

