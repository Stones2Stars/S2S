# Cascade fixed-point + the scale-conversion boundary — reference (owner-LOCKED 2026-06-19)

**The cascade does ALL value math in integer fixed-point with 2 decimals (×100). Numbers are human-readable in the
JSON; the single human→integer conversion lives in readJson; the curator is what resolves the legacy XML's
per-100-vs-normal ambiguity into uniform human numbers.** This doc is the canonical translation table + the
responsibility split. Owner rulings 2026-06-19 (the home for them).

Companions: [`../plans/modifier-cascade-spec.md`](../plans/modifier-cascade-spec.md) (§2 arithmetic),
[`../plans/data-model-spec.md`](../plans/data-model-spec.md) (§2.3 units, the cold-modder rule),
[`../plans/cascade-engine-430.md`](../plans/cascade-engine-430.md) (§5 "readJson converts readable→int×100 ONCE at
load"), [`../plans/calc-emulator-spec.md`](../plans/calc-emulator-spec.md) (the offline tester that models this first).

**Indexed in the [decisions ledger](../decisions.md):** [DEC-fixedpoint-x100](../decisions.md#dec-fixedpoint-x100),
[DEC-per100-closed-set](../decisions.md#dec-per100-closed-set),
[DEC-curator-owns-descale](../decisions.md#dec-curator-owns-descale) — this doc is their authoritative home; other
docs link the ID rather than restate the ruling.

---

## 0. The THREE layers — conversion lives in EXACTLY ONE (owner 2026-06-19)

The whole point: a modder writes plain numbers; the math engine sees only integer fixed-point; nobody in between
guesses scales. Each layer has ONE job:

| Layer | Job | Sees ×100? |
|---|---|---|
| **XML (legacy, frozen)** | the inherited data — MIXED scales: some fields per-100 (`*Changes100`), some normal (`YieldChange`) | n/a (the mess we're leaving) |
| **CURATOR** (`Tools/Migration/`) | **resolve the XML per-100-vs-normal ambiguity → emit UNIFORM HUMAN-READABLE numbers** to JSON. *"that job falls on the curator."* | reads ×100 XML, **writes human** |
| **JSON** (`Assets/Data/**`, modder surface) | human-readable numbers ONLY — `7`, `25` (a percent), `1.5`. **No ×100, no scale markers.** A modder never multiplies by 100. | **NO** |
| **readJson** (the IMPORT, `CvCascadeReadJson` + `cascade_sim`'s reader) | **the ENTIRE human→integer-fixed-point (×100) conversion**, and percent semantics. *"that entire conversion process belongs in readJson."* Hands the math side data already prepared for integer math. | **converts → ×100** |
| **CASCADE** (`CvCascadeModifier` + helpers) | pure integer math on prepared data. **Never checks, decides, or knows about ×100.** *"it is not the constraint of the cascade calculator to check whether the numbers are already multiplied by 100."* | **NO (just integers)** |

**Consequence:** a ×100 value in a JSON file (e.g. `commerce.city.flat: 700` for "+7") is a **bug in the JSON layer**
— the curator leaked an integer-math representation onto the human surface. The fix is curator-side (emit `7`), and
readJson re-applies the ×100.

### 0.1 The conversion is ONE-TIME — no legacy scale mess survives it (owner 2026-06-19)

XML→JSON happens **once, finally**; thereafter the JSON is the source of truth and the XML is gone. So **we do NOT
carry the legacy per-100-vs-normal mixing forward anywhere** — the curator must absorb ALL of it in that single run,
emitting uniformly human numbers. The payoff for each downstream layer:

- **readJson has ZERO per-field scale knowledge.** Because every JSON number is already uniform-human, the import is a
  blanket `× 100` (+ percent semantics) over all values — no "is this field per-100?" branch, ever. The per-field
  scale map (§2) is a **curator-only, used-once** concern; it must not leak into readJson or the cascade.
- **The cascade is pure integer math**, as in §0.
- So the §2 de-scale table is the curator's one-time checklist; its *correctness* (did we catch every per-100 field?)
  is proven by the tester reaching parity (§3) — after which the legacy scale distinction simply ceases to exist.

---

## 1. The fixed-point model — ×100 ("2 decimals"), integer throughout

- **Scale = ×100.** Internal value `V100 = round(human × 100)`. So `1.00 → 100`, `7 → 700`, `0.5 → 50`. Two decimal
  places of precision; no floats anywhere (OOS determinism — cascade-engine-430 §5).
- `FIXED_ONE = 100` (the representation of 1.00).
- Applies to **every magnitude family** (yields, commerce, health/happiness, maintenance, defense, unit stats, …) —
  this is the cascade-wide convention, established on the city-yields pilot and reused by every later channel.

### 1.1 The unit translation table (readJson does this conversion)

| JSON (human) | meaning | internal (×100) | combine |
|---|---|---|---|
| `flat: 7` (or `7.5`) | additive +7.00 / +7.50 | `700` / `750` | summed: `Σflat100` |
| `percent: 25` (or `25.5`) | +25.00% / +25.50% | `2500` / `2550` | summed: `Σpct100` |
| `multiplier: 2` (or `1.5`) | ×2.00 / ×1.50 factor | `200` / `150` | product: `Π(mult100/100)` |

### 1.2 The effective formula (integer, ×100, the active calc-flow)

`CALCFLOW_LEGACY_FLAT_OUTSIDE` (current — matches legacy flat placement so parity reaches zero):

```
base100 = (getBaseYieldRate + getSpecialistYieldTotal) × 100      // the pre-modifier base, ×100
eff100  = base100 × (10000 + Σpct100) / 10000 + Σflat100          // percent applied, then flat OUTSIDE
```

Compared **directly** against legacy `getYieldRate100` (also ×100) — no lossy `/100` round-trip. **This is exact
parity** when all sources are deposited: legacy `getYieldRate100 = (base+spec)×(100+Σ%) + 100×extra`; substitute
`base100=(base+spec)×100`, `Σpct100=Σ%×100`, `Σflat100=extra×100` ⇒ `eff100 = (base+spec)×(100+Σ%) + 100×extra`. ∎

`CALCFLOW_UNIFIED_FLAT_INSIDE` (deferred; needs a data rebalance): `(base100+Σflat100)×(10000+Σpct100)/10000 ×
Π(mult100/100)`. Selected by the single dispatch const `cascadeModifierCalcFlow` (modifier-spec §, calc-emulator §1).

### 1.3 The standardized helpers (the only place the arithmetic lives)

Pure integer, C++03, header-light (DLL: `Sources/Cascade/`; mirrored in `cascade_sim.py`):

- `FIXED_ONE = 100`, `toFixed(human) = human × 100`, `fromFixed(v100) = v100 / 100` (display only).
- percentage-adder: accumulate `Σpct100`; `applyPercent(base100, sumPct100) = base100 × (10000 + sumPct100) / 10000`.
- multiplier-compose: `Π` via `m = m × mult100 / 100` (identity 100).
- the calc-flow combine (`cascadeModifierApply`) — the single swappable dispatch point.

---

## 2. The per-field LEGACY-SCALE map — what the curator must de-scale (figured from the math, not eyeballed)

**Rule (how to figure a field's scale from the code, owner: "you will have to figure that out from the math"):** a
legacy field is **per-100 (÷100 to humanize)** iff its accessor is named `...100()` AND/OR its value flows
**directly into a ×100 accumulator** (`changeBuildingExtraYield100`, `m_buildingExtraYield100`, the `getExtraYield100`
bucket) with no `× 100` on the way in. A field is **normal (×1, already human)** iff the engine multiplies it by 100
when depositing (e.g. base `YieldChange` via `updateYieldRate`). Verified against `CvCity::processBuilding`
(`CvCity.cpp:4945-4983`) + `getExtraYield100` (`:11323-11333`) + `getYieldRate100` (`:11246-11254`).

| legacy XML field | accessor | scale | curator action | status |
|---|---|---|---|---|
| `YieldChange` / `CommerceChange` | `getYieldChange` / `getCommerceChange` | **×1 (human)** | emit as-is | ✅ |
| `YieldModifier` / `CommerceModifier` | `getYieldModifier` … | **integer % (human)** | emit as-is (`percent`) | ✅ |
| `YieldPerPopChange` | `getBaseYieldPerPopRate` | ×1 (human) — but legacy folds `×pop` into the ×100 bucket (a latent /100 weakening; health/happiness only, not yields) | emit as-is; flag the legacy quirk | ✅ |
| **`TechYieldChanges`** (Building) | **`getTechYieldChanges100`** | **×100** | **÷100 → human** | ✅ DONE (`curate_building.PER100_TAGS`, FLAT) |
| **`TechCommerceChanges`** (Building) | **`getTechCommerceChanges100`** | **×100** | **÷100 → human** | ✅ DONE + flat-vs-percent FIXED 2026-06-19: it is a **FLAT** change (`changeBuildingCommerceTechChange`→`getBaseCommerceRate100`, CvCity.cpp:12136; the XML sub-tag "CommercePercents" is a misnomer), was mapped `percent` → now `flat` |
| `EraCommerceChanges` / `CentiCommerce` (Heritage) | `getEraCommerceChanges100` | **×100** | **÷100 → human** | ✅ DONE 2026-06-19 (`curate_heritage._era_commerce`; was "carried FAITHFULLY (#432)" — that was the gap) |
| `iExtraUpkeep100` (Promotion / UnitCombat) | `getExtraUpkeep100` | **×100** | **÷100 → human** (upkeep channel) | ✅ DONE 2026-06-19 (`FAMILIES` applier de-scales any `tag.endswith("100")`; member renamed `extra100`→`extra`) |

**The ×100-space ADDENDS that LACK a `…100()` getter (the accessor-sweep's blind spot — mapped against
`Sources/Engine/CvCity.cpp` 2026-06-19, #432):**
- `BonusCommercePercentChanges` (Building) — **×100, and FLAT (not a percent/rate)**: added raw beside
  `100 * getBuildingCommerce` inside `getBuildingCommerce100` (`CvCity.cpp:12135`); the actual rate modifier is
  the SEPARATE `m_aiBonusCommerceRateModifier`. Curator: **÷100 de-scale + relabel `percent`→`flat`** — the
  `Percent` in the tag name is a misnomer.
- `YieldPerPopChange` / `CommercePerPopChange` (per-pop) — **×1 human, NOT ×100**: added raw into the ×100-space
  `getExtraYield100` / `getBuildingCommerce100` (`CvCity.cpp:11331` / `:12137`), the legacy "latent /100
  weakening." Curator: **emit as-is; do NOT de-scale** (÷100 here would corrupt `1/pop` → `0.01/pop`). This
  retires the handover's tentative "de-scale perPopulation" plan — it was wrong; verified ×1.

**COMPLETENESS — the per-100 set is CLOSED (verified 2026-06-19).** `grep -rE "get[A-Za-z_]+100 *\(" Sources/Infos/*.h`
returns EXACTLY SIX `...100()` accessors across ALL Info headers: `getTechYieldChanges100` + `getTechCommerceChanges100`
(Building), `getEraCommerceChanges100` (Heritage), `getExtraUpkeep100` (Promotion + UnitCombat), and
`getTotalModifiedCombatStrength100` (CvUnit — a COMPUTED accessor, not an XML field, nothing to de-scale). Literal `*100`/
`Centi*` XML tags = `<CentiCommerce>` (in EraCommerceChanges) + `<iExtraUpkeep100>` only. All data fields are now
de-scaled in their curators — so NO ×100 representation survives in the JSON. The shared one-time de-scale lives in ONE
place (`curate_common.descale100`), used by building/heritage/promotion/unitcombat, so it cannot drift per-entity.

**The DRIFT that caused the "I was assured it was fixed" failure (owner 2026-06-19): the scale rework done ~2 sessions
ago de-scaled BUILDING only** (`PER100_TAGS`, 555 JSONs) — Heritage `EraCommerceChanges` and Promotion/UnitCombat
`iExtraUpkeep100` were left carrying ×100 ("faithfully (#432)"), even though THIS §2 table already listed them as ÷100.
The doc was right; the implementation was partial. Lesson: the `...100()` accessor sweep above is the EXHAUSTIVE source
of truth — run it across ALL Info headers, not just the entity you're on. (The offline tester reaching parity, §3, is
the secondary check — a mis-scaled field shows up as residual divergence localized to entities that carry it.)

---

## 3. Verification — the offline tester proves the scales (no manual JSON review)

The owner cannot eyeball thousands of JSONs; **the math verifies them.** Workflow (calc-emulator-spec §3):

1. Curator de-scales per §2 → regenerate human-readable JSON (`Assets/Data/**`).
2. `cascade_sim.py` IMPORTS the human JSON (human→×100, §1.1) and computes `eff100` (§1.2) for a real loadout.
3. Compare `eff100` vs **legacy** `getYieldRate100` (the loadout's live legacy output, e.g. `samples/london.json`)
   — **parity-adjacent** is the bar (modifier-shadow-spec §3.1a); exact for the wired sources in Mode A.
4. Residual divergence localizes the next mis-scaled field or un-wired source → fix curator/import → **regenerate +
   re-run until right.** Then port the proven model to the DLL (`readJson` import + `CvCascadeModifier` helpers).

Two known compounding bugs this fixes (both surfaced 2026-06-19): **(a)** the ×100 scale leak (this doc), and **(b)**
the DLL pilot not condition-gating tech-gated deposits (calc-emulator §2a.1) — the import/cascade must gate each
deposit's `enabled`/`disabled` so a deposit for an un-researched tech does not fire.
