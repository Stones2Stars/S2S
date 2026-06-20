# The cascade — the data engine, in full

> **Status:** explanation (the realized exemplar + the data-side core design) · **Grounding:** the owner's
> core model (2026-06-19) reconciled with the cascade specs — `enabler-cascade-spec`, `modifier-cascade-spec`,
> `tally-cascade-spec`, `event-spine-spec`, `data-model-spec`, `cascade-engine-430` (old `docs/dev/plans/`,
> pending rebuild into `../reference/cascade/`). · **Verified state:** **design landed; implementation in
> progress** — do not assume a structure below is built.
>
> The north-star ([`../architecture/north-star.md`](../architecture/north-star.md) §2) is the front-and-center
> *summary*; this is the full mechanism. Atom-level syntax (the exact JSON object shapes) lives in the named
> specs — cited here, not duplicated.

The cascade is the **data side** of the engine (the AI side consumes it — north-star §1). It replaces
~7–8k lines of imperative "maintainer" machinery with **three top-down machines fed by an event spine**.

---

## 1. The scope spine & the three directional flows

State lives on a **scope spine** — a hierarchy (game → team → player → … → city → plot / unit / building).
The cascade is **bidirectional** across it — three flows, and the asymmetry is the whole point
([DEC-cascade-bidirectional](../architecture/decisions.md#dec-cascade-bidirectional)):

| Machine | Question | Flow |
|---|---|---|
| **modifier** | "how much?" | magnitudes deposit **DOWN** |
| **tally** | "how many?" | counts roll **UP** |
| **enabler** (`requires`) | "can I?" | gate resolves by a **callback UP** |

## 2. The enabler — "can I?" (2-step: GENERATE then GATE)

The enabler is **two passes**, narrowing the data through three sets (owner's terms ≡ spec's terms):

| # | owner | spec | what it is |
|---|---|---|---|
| 1 | **have** | **HAS** | what you actually possess |
| 2 | **can get** | **CAN GET** | the candidate frontier — everything HAS unlocks |
| 3 | **can actually do** | **HAS THE MEANS TO** | the candidates whose `requires` are met |

**Stage 1 — HAVE.** Determined by four states: **enabled** *adds* to the list; **replaced**, **obsoleted**,
and **disabled** each *remove* from it (in their own way). (built / researched / possessed.)

**Stage 2 — CAN GET (pass 1: GENERATE, cheap, from HAS).** For each thing you HAVE, look up what it unlocks
via the **`enables` family** → the union is the candidate set, **bounded by HAS**, **minus** what is
removed. The enablers are mostly **techs, traits, and civics**. The `enables` family is **permanent /
irreversible, source-side, read forward** — hard-constructive (`enables`) and destructive
(`obsoletes` / `replaces`) actions. `obsoletes` a unit → removed from CAN GET (no new builds) but **existing
units stay on the map**. (Generation is *always* `enables`-driven; tech unlocks feed the `enables` side, not
a `requires` generation driver — `enabler-spec` §1–§3.)

**Stage 3 — HAS THE MEANS TO (pass 2: GATE).** Each candidate is gated by its **`requires`** — and
`requires` **checks only against the "can get" subset**, never the whole data space, resolved by the
**`require` callback UP the chain** (querying up-scope *enabled* state + the tally counts). `requires` is
**reversible, target-side, read forward** — it covers positive means *and* dormancy negatives. This is **how
the cascade models AND**, and it stays cheap because it only ranges over what is already enabled.

> **`enables` and `requires` COEXIST** — two aspects that both apply to a target, not two homes for one edge.
> Rule: *destructive or hard-constructive → `enables`; reversible enable/disable → `requires`*
> (`enabler-spec` §1). `requires` further distinguishes a **one-time BUILD** gate from a **continuous
> OPERATE** gate (dormancy) — `enabler-spec` §3.

**Why two-pass / bidirectional, not down-only:** down-only was the original design; it models **OR** via
enablers but **cannot reliably model AND**, and forces a modder to maintain every requirement at the **top
of the chain**. The upward `require` callback is load-bearing
([DEC-cascade-bidirectional](../architecture/decisions.md#dec-cascade-bidirectional)).

**Greying falls out for free.** The build-list tri-state is a byproduct of the same gate, no separate "why
greyed" pass: in CAN GET with an unmet `requires` clause → **GREYED**; an unmet `enables`-hiding clause →
**HIDDEN**; permanent removal (`obsoletes`/`replaces`) drops it from CAN GET entirely (`enabler-spec` §4).
The sets are **recomputed on demand** — the bounded two-pass is cheaper than the dozens of scattered
`canBuild`-style checks it replaces, so no aggressive caching.

## 3. The modifier — "how much?" (1-step, deposits DOWN)

Magnitudes deposit **DOWN** the spine in integer fixed-point (×100;
[`../reference/cascade/fixed-point-and-scales.md`](../reference/cascade/fixed-point-and-scales.md),
[DEC-fixedpoint-x100](../architecture/decisions.md#dec-fixedpoint-x100)). Modifiers are organised as
**families × scopes × channels** — authored as `<family>.<scope>[.<member>].<unit>` — with **conditioning**
(`enabled` / `disabled` presence, `per` count-scaling against the tally) deciding whether/how much a deposit
fires (`modifier-spec` §2–§4).

**Ownership = the deliveryguy.** A cross-entity modifier lives on **whoever brings it to the table** (the
*deliveryguy*), keyed by the target — *not* inverted onto the target. Two equally-valid expression modes
chosen by semantic sense: **keep-on-source** vs **fold-onto-deliveryguy**
([DEC-deliveryguy](../architecture/decisions.md#dec-deliveryguy); `modifier-spec` §6/§6.1).

## 4. The tally — "how many?" (counts roll UP)

Counts roll **UP** the spine ("how many of X in this player's cities"). The tally **feeds the enabler's
`require` callbacks and the modifier's `per` scaling**. It **serializes nothing** — rebuilt from the
authoritative loaded objects on load ([DEC-tally-serializes-nothing](../architecture/decisions.md#dec-tally-serializes-nothing);
`tally-spec` §9).

## 5. The event spine & the contract

The machines are fed by an **event spine** and program to one contract, **`IEventConsumer`** — the spine,
the tally, `grants`, and logging are independent implementations pluggable behind it (`Sources/Cascade/`;
the realized exemplar of the [composability](../architecture/patterns/composability.md) /
[faking-DI](../architecture/patterns/faking-di.md) patterns). `grants` deliver effects (e.g. a free bonus)
**on delivery**. Build order: **readJson + substrate → tally → modifier → enabler** (`cascade-engine-430`).
`readJson` is a **pure consumer** of the JSON data *shape* (defined by `data-model-spec`, not by readJson).

## 6. Fixed-point & determinism

All math is integer ×100; the single human→×100 conversion is in `readJson`; no float (OOS-load-bearing —
Civ4 MP is deterministic lockstep). The scale registry is the canonical home:
[`../reference/cascade/fixed-point-and-scales.md`](../reference/cascade/fixed-point-and-scales.md).

## 7. Verification — the Orwellian shadow (map-before-delete)

You cannot safely delete a maintainer you cannot fully observe. Every behaviour gets a **shadow** diffing
the cascade against the live engine, turn over turn, until clean — *then* the legacy is cut
([DEC-map-before-delete](../architecture/decisions.md#dec-map-before-delete)). That requires the
**Orwellian** total-observability surface ([DEC-obs-scale](../architecture/decisions.md#dec-obs-scale);
[`../reference/observability/README.md`](../reference/observability/README.md)).

## See also
- [`../architecture/north-star.md`](../architecture/north-star.md) — the front-and-center summary this expands.
- [`../reference/cascade/fixed-point-and-scales.md`](../reference/cascade/fixed-point-and-scales.md) — the scale registry.
- the cascade specs (old `docs/dev/plans/`, pending rebuild) — atom-level syntax + the demolition map.
