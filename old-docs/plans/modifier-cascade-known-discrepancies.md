# Modifier cascade — KNOWN DISCREPANCIES (shadow vs. live game)

**Purpose (modifier-cascade-shadow-spec §4.1): the durable record of every place the #430 MODIFIER cascade's effective
value differs from the live (legacy) game** — the magnitude sibling of [`cascade-known-discrepancies.md`](cascade-known-discrepancies.md)
(which tracks the *availability* / enabler divergences). Populated AS the shadow runs: each row is `{ channel, scope,
cause-tag, care level, owner note }`. It is the durable home for every **blessed (≤2)** divergence — what the cascade
deliberately corrected and why — and every **open (3–5)** one.

This is the companion to the live tools (see [`../reference/http-server.md`](../reference/http-server.md)):

- **`GET /diagnostic/modifierSweep?player=N`** — the all-cities triage (cap-250 sample) + the UNCAPPED `causeHistogram`
  (`"cause:CareName"`) and `careHistogram` (Fine..Meltdown). `?type=full` = the complete per-cell array; `?channel=` scopes.
- **`GET /diagnostic/modifier?player=N&city=M`** — the per-city on-demand decomposed spot-check.
- the per-turn **`[MODSHADOW]`** line (`cascadeModifierShadow`, every alive player incl. AI) → `Cascade.log` + `/events`.

**The CARE SCALE (§4) — the disposition axis, one word each, severity+action readable cold:**

| Lvl | Name | Meaning | Action / gate |
|---|---|---|---|
| 0 | **Fine** | exact parity / blessed identical-enough | none |
| 1 | **Rounding** | int-rounding / off-by-one within tolerance | accept, note |
| 2 | **Better** | deliberate correction (multiplier composition etc.) — the expected end-state win (R-M2) | accept as a win, document |
| 3 | **Weird** | unexplained — the "ask the owner" bucket | investigate → owner verdict |
| 4 | **Bug** | confirmed cascade wiring bug (deposit missing / extra / mis-scoped) | must-fix before that channel's cutover |
| 5 | **Meltdown** | systemic — whole channel garbage / overflow | stop-the-line; block cutover; DESPAIR_INDEX candidate |

**Cutover rule (§4):** a channel's legacy §9 reads may be deleted only when every remaining Mode-B discrepancy on it sits
at an owner-verdicted rung **≤ 2**. Rungs 3–5 block. The shadow's `cascadeModifierClassify` auto-SUGGESTS a provisional
rung from the cause-tag; the **owner's verdict (R-M3) sets the final one** and is recorded here.

**Status legend:** ✅ verified-MATCH · ⚠ KNOWN-GAP (disposition set) · ❓ UNDIAGNOSED (cause not pinned) · 🔭 UNSHADOWED.

---

## A. City-yields pilot (food / production / commerce) — Mode A (parity)

> **Current expected state (pre-completion): the histograms are still dominated by `missingDeposit` / care 4 `Bug`** —
> NOT (yet) a roster of real bugs, but the **sources not yet deposited**. This shrinks as each source is wired (the R-M1
> incremental method: add a source, watch `missingDeposit` fall toward zero). Genuine per-discrepancy rows get recorded
> here as the owner verdicts what remains.
>
> **Sources WIRED into the city-yield slot so far:**
> - city-scope **BUILDING** flat/percent (+ their `enabled`/`disabled` conditions) — increment 2.
> - the base now includes **`getSpecialistYieldTotal`** so the cascade applies the modifier to (base+specialist), matching
>   legacy `getYieldRate100` (CvCity.cpp:11253) — increment 4A.
> - the player's active **CIVICS'** empire-scope deposits (empire→city roll-down) — increment 4B (124/175 civics carry
>   `<yield>.empire.percent`).
>
> **Sources NOT yet deposited (the remaining `missingDeposit` contributors):** TRAITS (empire), TECH downward deposits,
> BONUS-intrinsic, AREA/POWER/CAPITAL modifiers, building **empire**-scope deposits, civic sub-scopes (`tradeRoute`/
> `improvements`/`capital`), corporation `m_aiExtraYield`, and the legacy clamp `[1, MAX]`. Each is a future source-add.
>
> **NB — `perPopulation` is NOT a pilot gap:** the data has **0** food/production/commerce `perPopulation` deposits (all
> 131 are happiness/health); it belongs to the later health/happiness channel sub-pass, not the yields pilot.

| channel | scope | cause-tag | care | owner note |
|---|---|---|---|---|
| _(none verdicted yet — populate as the deposit-flow completes and the owner adjudicates the residue)_ | | | | |

## A.1 OFFLINE EMULATOR FINDINGS (cascade_sim vs legacy, 6-city sweep 2026-06-19)

The offline `Tools/ModifierCalc/cascade_sim.py` (x100 import + dormancy + buildings/civics) vs legacy `getYieldRate100`,
swept over 6 cities (London/Keleia/Qart-hadast/Tenochtitlan/Sy Ara/Moscow):

| channel | result | cause |
|---|---|---|
| **production** | ✅ **6/6 parity-adjacent** (mean \|gap\| 4.1%) | the x100 de-scale + import + gating reproduce legacy |
| **food** | ◐ 3/6 (mean 12.8%, ±25%) | variance from over-aggressive dormancy (unevaluable `requires.operate` atoms wrongly mark a building dormant → conservative-OFF removes its yield) + un-wired positive sources |
| **commerce** | ⚠ 0/6 (mean ~29%, +25–38% OVER) | **the cascade counts building commerce-% that legacy's modifier does NOT apply** — concentrated in **property-effect / conditional buildings** (e.g. the `BUILDING_EDUCATION_*` band ladder, +35/+30/+25/… commerce-%). Their continuous gate sits in **`requires.build`** (tech), NOT `requires.operate`, so the cascade's dormancy can't tell they're inactive → over-counts. **This is the documented Phase-F "build-vs-operate" curation gap** (enabler-spec §3 / migration-entity-ranking Phase F): move continuous resource/power/property gates from `requires.build` → `requires.operate`. |

**The scale bug is FIXED** (was ~10× / +1000% overshoot on all channels → production/food now near-parity).

**ROOT CAUSE of the commerce over-count CONFIRMED + the fix (owner rulings 2026-06-19):** the `cityInput` dump's
`buildings` list was `pCity->hasBuilding` (present, **incl. dormant/disabled**, CvHttpServer.cpp:1017), but legacy's
yield modifier only includes **active** buildings — so the cascade summed dormant buildings' commerce-% that legacy
doesn't apply. (NOT the education bands: **property bands are CUMULATIVE — all lower bands stay active, counted by
both sides — by design, for consistent player UX; the "overwrite"/only-current behaviour comes later in the
PropertyEffect remodel.**) Fix (owner: "read the active ones from the dump", don't re-derive dormancy in Python — that
was over-aggressive on unevaluable `operate` atoms, the food variance):
- **`cityInput` now emits the ACTIVE set** (`hasFullyActiveBuilding`) as `buildings`, + `dormantBuildings` for
  observability (CvHttpServer.cpp:1013). **Needs a game rebuild + re-fetch to take effect.**
- **`cascade_sim` trusts the dump's active set** (dropped the Python `requires.operate` re-derivation).
- **VERIFICATION PENDING:** rebuild → re-fetch the city fixtures → re-sweep; commerce expected to drop into
  parity-adjacent (the dormant-building commerce-% over-count removed). The Phase-F build→operate curation is the
  separate longer-term cleanup, but reading the live active set sidesteps it for the emulator.

**✅ CONFIRMED 2026-06-19 (6-city sweep, both ways) — the path is ALL-PRESENT + INCREMENTAL BANDS, not active-exclusion.**
Two sweeps settled it: (a) **active-only** (`hasFullyActiveBuilding`) → commerce 5/6 BUT **production regressed to 0/6
(−25%)** — legacy DOES count "dormant" buildings' production, so excluding them is wrong; (b) **all-present** →
**production 5/6 + food 3/6 near-parity**, commerce isolated at +33% = **entirely the property-effect bands at full
value**. The math closes: London education bands full +140% vs incremental +35% = −105%·base ≈ the +168k commerce gap →
fixing bands lands commerce at −1.9% (parity). **So: count all present buildings (production parity); fix commerce by
INCREMENTAL band authoring (below).** `cascade_sim` uses `buildings + dormantBuildings` (all present); the cityInput
`hasFullyActiveBuilding` split is kept only for observability. **NEXT TASK = the band-aware incremental curation.**

**BAND VALUES — INCREMENTAL "cumulative effort" authoring (owner ruling 2026-06-19):** legacy counts only the
**highest** active band at its **full** value (e.g. education ENLIGHTENED = +35% total). The new model keeps **all
bands cumulatively ACTIVE** (clearer player UX — you see every level achieved), so to match legacy's total each band
must be authored as its **INCREMENTAL delta** (the "cumulative effort" added at that level), NOT the full per-band
value — summing the active cumulative bands then equals the highest band's full value. **Action: a band-aware JSON
rebuild** (convert full per-band values → deltas, ordered by band rank). The active-set re-fetch (above) will confirm
whether legacy is highest-only (→ author increments) vs already-cumulative — diagnostic + design in one. *(This is the
clearer-UX design choice, distinct from just matching legacy; it lands with the PropertyEffect remodel direction.)*

**✅ DONE 2026-06-19 — EDUCATION pulled in line + the REAL crux: pseudobuildings must NOT `replace` EACH OTHER (owner
rulings 2026-06-19).** Ground-truth scope (probe over `CIV4PropertyInfos.xml` `PropertyBuildings` × the store
`replaces` index): of 188 property-effect pseudobuildings, **crime (86) / disease (16) / tourism (10) /
water-pollution (12) already use the cumulative threshold→∞ model (0 `replaces`)** — *"all work on bands from the
threshold to infinity, only disabling below threshold."* **EDUCATION is the lone outlier**: its author built a
parallel succession system — 4 ladders (positive-era, negative-era, argumentative-awareness, blissful-ignorance), each
13 bands chained by legacy `ReplacementBuildings` and carrying the FULL per-band value. The fix (curator
`Tools/Migration/curate_building.py` `apply_property_bands`): **(1)** strip pseudo→pseudo `replaces` (the 48
successor bands) so the bands STACK; **(2)** re-author each ladder band as its INCREMENTAL delta
(`full[rank] − full[rank−1]`, zero-increments pruned), so the cumulative active bands reproduce the top band's
intended total ("nerf each building for UX"). **Pseudobuildings NEVER `replace` anything** (replace = REMOVE): a
pseudo→pseudo replace is dropped (bands stack); a pseudo→**REAL** replace becomes a **reversible `disables`** —
`BUILDING_POLLUTION_BLACKENED_SKIES` now `disables` (not removes) the 24 telescope/observatory buildings, so they go
**dormant** while the skies are blackened and **reactivate** when the air clears ("an observatory should become
dormant from it, not get nuked from orbit" — the reversible effect-disable, enabler-spec §5; a genuine
pollution-blocks-astronomy mechanic, owner: a *cool idea to use more* later). Pseudobuildings remain first-class
cascade participants for the OTHER edges: a normal building may still `disables` a band (e.g. a rat-catcher disabling
the disease-tree pest band), and the model already supports future negative-side bands (e.g. negative crime bands
that activate on over-policing).

**✅ DONE 2026-06-19 (PART 2) — the `requires.operate` PROPERTY-in-band GATING is AUTHORED + the bands are placed
CUMULATIVELY (owner ruling, repeated 4×: pseudobuildings COMPOUND, set up by `requires`, NEVER `replace`).** This
finishes the band model that PART 1 (increment-convert + strip-replace) set up — it is NOT deferred (the earlier "NOT
added / deferred PropertyEffect concern" line was the stale text that caused this to be re-discovered four times; it is
gone). Concretely:
- **Curator** (`curate_building.py` `apply_property_bands` + `property_band_atoms`): EVERY pseudobuilding (all 188 —
  crime/disease/education/tourism/pollution) now carries a `requires.operate.all:[{type:PROPERTY_X, scope:city,
  min:iMinValue, max:iMaxValue}]` atom read straight from its `CIV4PropertyInfos.xml` `<PropertyBuilding>` band
  (data-model §2.1 ATOMDOMAIN_PROPERTY, enabler-spec §3). Merged into the band's `requires.operate`; any
  `requires.build` tech gate is preserved.
- **`cascade_sim`** places a pseudobuilding by EVALUATING that band atom against the city's CURRENT property VALUE
  (from the dump's `properties`), IGNORING the dump's active/dormant split — so all in-band bands stack. **NB the
  property value already includes UNIT contributions** (units stationed in a city change its properties — owner
  2026-06-19); the dumped `getValueByProperty` is the live value, so band placement is correct without
  reconstructing it. Non-pseudo buildings stay ACTIVE-only (legacy removes a dormant building's modifier via
  `processBuilding(-1)` — code-trace verified, `setDisabledBuilding`/`setReligiouslyLimitedBuilding` →
  `getBuildingYieldModifier` is an active-only accumulator; this REFUTES the "spicy" note below).
- **Validation** (`cascade_sim --glob`, 6-city sweep, within ±10%): **commerce 1/6 → 5/6** (mean |gap| 15.3% → 8.8%,
  London −16.5% → −7.8%), production 1/6 → 2/6, food 4/6 (unchanged — education deposits **no** food). All gaps are
  NEGATIVE (cascade UNDER) — no over-count, confirming cumulative placement is right and tech-unreachable higher
  bands are correctly excluded (the sim iterates only bands legacy actually placed).

**⛔ TOLERANCE BAR (owner ruling 2026-06-19): ±10% is NOT parity-adjacent — the bar is MUCH SHARPER.** So the band
fix is necessary but not sufficient; the residual must be driven down. GROUNDED per-source attribution (cascade_sim
percent/flat vs the dump's legacy `mod{Building,Player,Capital,Bonus,Power,Area}` + `extraYield` decomposition, all 6
cities):
- **PERCENT gap is SMALL** (cascade ~5–40 points under legacy of 150–650 total): bonus/power-gated building deposits
  DO fire (via `enabled`), so most is wired; the residual is **TECH-DOWNWARD deposits** (authored on the tech, keyed
  by building/scope — `cascade_sim` does not load tech JSONs nor handle the keyed sub-scopes yet) + a few per-building
  bonus/power deposits + AREA. WIREABLE (care `Bug`).
- **FLAT gap is the dominant residual and consistently NEGATIVE** (e.g. London production extraYield 648 vs cascade
  434): substantially **CORPORATIONS**, which calc-emulator-spec §2a EXCLUDES from the modifier cascade BY DESIGN
  ("a demon we do not want to add" — a flat post-add, later) → that portion is a **`Better`/by-design omission, NOT a
  bug**; the rest is **tech-dependent building flats** (same tech-JSON gap as the percent side).
- **Next source-wiring phase** (to hit the sharp bar): load tech JSONs + handle keyed sub-scopes (`<y>.<scope>.
  {buildings|improvements|specialists}.{TARGET}`) for the TECH-DOWNWARD deposits; then decide corporation handling
  (by-design-exclude vs the spec's flat post-add). Food's residual is dormancy variance + the same tech-downward.

**❌ SUPERSEDED 2026-06-19 (code-trace) — the "spicy" claim that legacy counts DORMANT buildings' percent is WRONG.**
The earlier empirical read (active-only → production 0/6) was the EDUCATION-band undercount, not general dormant
counting; once bands are placed cumulatively-by-value and non-pseudo buildings are active-only, the sweep is
all-NEGATIVE (no over-count). Authoritative code trace: `hasFullyActiveBuilding = !isReligiouslyLimitedBuilding &&
isActiveBuilding`; both the disabled path (`setDisabledBuilding`→`processBuilding(-1)`) and the religious path process
the building OUT, so `getBuildingYieldModifier` (`getBaseYieldRateModifier`, CvCity.cpp:11217-11222) is an
ACTIVE-ONLY accumulator — legacy does NOT count a dormant building's percent. Active-only (non-pseudo) is correct; no
post-parity dormancy-cleanup pass is owed for this. Original (now-refuted) note kept below for history:

**⚠ DEFERRED LEGACY-BUG — legacy applies DORMANT buildings' PERCENT modifiers (owner 2026-06-19, "spicy"; fix AFTER
parity).** The all-present-vs-active-only decomposition (London) showed legacy's `getBaseYieldRateModifier` includes
the % modifiers of buildings that are NOT `hasFullyActiveBuilding` — i.e. a dormant building still contributes its
yield *percent* to the city (its *flat* `getExtraYield` behaves more as expected). So **the cascade must COUNT dormant
buildings' percent to reach parity** (it already does, via all-present), even though that is a latent legacy bug. Owner:
*"that legacy counts dormant buildings % modifiers is… spicy — we need an actual pass to fix crap like that after we
got parity."* So: match it now (parity scaffold, R-M1), then a dedicated **post-parity cleanup pass** corrects the
dormancy semantics deliberately (a `Better`, §4) rather than chasing it mid-parity. Captured here so it is not lost.

## A.2 PER-BUILDING ATTRIBUTION (the cityInput decomposition — calc-emulator §5)

The residual flat/percent divergence is per-source, so `/diagnostic/cityInput` now emits a **`buildingYields`** array
(`CvHttpServer.cpp`): per ACTIVE building, its legacy flat (`getBaseYieldRateFromBuilding100`, ×100) and static
percent (`getYieldModifier`) per food/production/commerce. The offline emulator diffs this against the cascade's
per-building JSON deposit to attribute each +/- to a NAMED building, replacing the aggregate guesswork. Needs a
`Release` rebuild + a fixture re-fetch. (Diagnostic: even active-only flat OVER-counts food +102 / commerce +187 on
London, while production flat UNDER-counts — so it is per-source attribution, not a present-set/dormancy fix.)

- **⛔ UNIFY THE CALCULATION, FIX THE DATA — do NOT replicate legacy's per-property calc quirks (owner ruling
  2026-06-19, the governing principle here).** Legacy very likely calculates its pseudobuildings DIFFERENTLY per
  property (crime / disease / education / tourism / pollution — "it would not surprise me in the slightest"). **We do
  NOT match that.** The cascade has **ONE unified calculation** (cumulative-active bands + incremental "cumulative
  effort" values, the §1 ×100 model); ALL property-band DATA is **re-authored to fit that one calc** — we never keep
  legacy's inconsistent data and branch the calc per-property. *(cascade-engine-430 §2b "the mod fits the new structure,
  not the reverse"; modifier-spec §0.7.)* Consequence: the result is **parity-ADJACENT, not legacy-exact** — legacy's
  per-property inconsistency is a corrected `Better` (§4 care scale), not a divergence to chase to zero. The emulator's
  role shifts accordingly: it's the tool to find WHERE the data needs fixing + confirm parity-adjacency under the one
  unified model, NOT to infer-and-match each property's legacy scheme. *(Supersedes the earlier "verify per-property to
  match legacy" framing.)*

## B. Later channels (commerce-split / health-happiness / defense / maintenance / unit-plane — §2.2)

🔭 UNSHADOWED — built channel-by-channel in §2.2 order; each gets its rows here as its deposit-flow + shadow land.
