# Info rebuild — the toolkit + the three-info rebuild (#430 "make the infos sane")

> Work item under [roadmap.md](roadmap.md) § "How the INFO side hands its data to the cascade". The CONTRACT is
> [patterns.md](../../architecture/patterns.md) (the INFO DATA-OUT contract, the coherent surface, the ONE-reader
> law); this doc is the ordered worklist against it. Current tree state: `CvBuildingInfo` / `CvCivicInfo` /
> `CvTraitInfo` are owner-gutted stubs holding only the typed section members — rebuilt on this toolkit.

## The failure modes this work kills

1. **The legacy-getter shoehorn** — wiring rebuilt members into legacy getter names
   ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface): reusing a legacy getter IS
   the mechanism that produces the half-migrated state). A rebuilt info grows ONLY the new coherent surface;
   no legacy getter name ever returns.
2. **Reader invention** — a second JSON parse site
   ([DEC-one-json-reader](../../architecture/decisions.md#dec-one-json-reader)). Two independent readers exist
   today (census below); the toolkit consolidates to exactly one.
3. **Scope smuggled into kind names**
   ([DEC-scope-is-an-axis](../../architecture/decisions.md#dec-scope-is-an-axis)) — the defect that shaped the
   deleted scalar-kinds header; the reborn vocabulary separates the axes.

## Census — what EXISTS (verified in tree)

| Tool | Where | State |
|---|---|---|
| The typed-section family (`CvRequires`/`CvEdges`/`CvAllowed`/`CvGrants`/`CvProvides`/`CvClassificationBlock`/`CvCondition`/`CvGate`/`CvModEntry`; parse surface `CvJsonParse`/`CvJsonConditionParse`) | `Sources/Infos/` | KEEP — the shared parse + section vocabulary; the stub infos hold exactly these members |
| `ClassificationRegistry` (minted ids → bitsets → `CLS_HAS`) | `Sources/Infos/` | KEEP — the realized classification exemplar |
| `InfoRepo` per-type repos · `DepositIndex`/`DepositRead` (`MMKernel`, interned segments) · `cascadeEvalCondition` + eval ctx · the enabler tree · `ContextDict` + `CityContext`/`EmpireContext`/`PlotContext` · the `readjson.exe` offline driver | various | KEEP |
| **The ONE JSON reader** — `loadJson()`/`loadJsonCategory` (`Data/CvReadJson.cpp`): one walk + one parse per file into a retained store; `LoadGlobalClassInfoJson` is a thin registration against it; the store frees at postmenu end ([engine.md § Info loading](../../reference/engine.md)) | `Sources/Data` | KEEP — toolkit item 1 LANDED (exactly one `picojson::parse` site) |
| `CvTriggers` — the §5 `triggers` section unit (trigger → chance → action typed entries), composed on buildings/features/improvements/traits/units, whole-object `getTriggers()` | `Sources/Infos/` | KEEP — the grants-machine apply-loop implements against it ([grants-machine.md](grants-machine.md)) |
| `CvModifiers` — the compiled §6 container: typed `CvModEntry` list (interned family/kind/scope/unit/target axes, prebuilt condition trees) + the `(family, kind, scope, unit)` unconditioned ×100 slot sums + the family-sorted conditioned ranges | `Sources/Infos/` | KEEP — the retained runtime form (toolkit item 2); the string walk exists only inside its load-time `parseEntity` |

## The toolkit to BUILD

1. **The ONE reader — LANDED** (entry point `loadJson()`, no `cascade` in any reader name; a single pipeline
   in `Sources/Data`, [patterns.md § The ONE reader](../../architecture/patterns.md) /
   [engine.md § Info loading](../../reference/engine.md)): one walk + one parse per file into the retained
   store (~21 MB text → ~70 MB parsed, measured node-count model; freed at postmenu end);
   `LoadGlobalClassInfoJson` is a thin per-category registration; the premenu/postmenu phasing, the
   REUSE-ONLY id rule, both forward-FK guarantees (per-category two-pass + full-registry re-map), the trait
   simple/complex dedup-first-wins + complex-repo keying, and the unconditional fail-loud coverage summary
   (unresolved FKs / unconsumed sections / unknown keys, with the CLOSED family vocabulary in
   `CvJsonParse.cpp` `CJK_FAMILY_KEYS` mirrored from `Tools/Migration/family_census.py`) are all live.
   **The reverse pass — LANDED as the ONE general pass (`Data/CvReversePass.{h,cpp}`, called from `loadJson`):**
   the per-relationship inversions and `rj_buildReverseView`'s legacy-mirror getter reads
   (`getTechYieldChanges100`, `getPrereqAndTech`, …) are deleted whole; `EDGEF_RELATED` generalizes over every
   compiled surface (edges both directions / requires trees / deposit targets + conditions + per-scalers /
   grants / provides / triggers), `EDGEF_REQUIRED_BY` keeps the gate walk, the own-output reverse landing puts
   building/civic/tech yield flats keyed to plot-substrate targets ON the target as compiled conditioned
   entries (`CvModifiers::landReverseEntry`), and the forward compat reconstructions live inside the same pass.
   The DepositIndex push now runs AFTER the pass, so landed entries compile into the index like authored ones
   ([DEC-one-reverse-view]; classification + counts: roadmap § the GENERAL own-output reverse-map).
   **Still open:** runtime verification of the pass's counts/landings on a live load (the red tree does not
   run yet); the enabler's private reverse buckets converging onto `EDGEF_REQUIRED_BY` is separate work
   ([enabler.md §8](../../specs/enabler.md) open item 1).
2. **The load COMPILE pass — LANDED** (`CvModifiers`/`CvModEntry`, `Sources/Infos/`): every string key interns
   to a typed id at parse (family/member → the `CvInfoKinds.h` vocabulary; scope → `CvCascScope`; named-entity
   targets → FK-resolved ids; conditions → prebuilt `CvCondition` trees), and the ONE walk of the §3.9 entries
   produces the runtime forms: null-condition untargeted values folded straight into the packed
   `(family, kind, scope, unit)` unconditioned ×100 slot sums (Σflat vs Σpercent separate slots); conditioned
   entries into the family-sorted compiled list with their trees; the raw authored segments kept as interned
   spell-back ids (the DepositIndex push + `[READJSON]` diagnostics render from them, never a runtime read).
   The string-keyed family map is gone; `DepositIndex::pushInfo`, the `[READJSON]` read-back survey, the
   improvement←route reverse pass, `CascadePropertyBridge::bridgeFamilies`, and the `CvPropertyInfo` own-source
   walk all read the compiled entries. A member outside the vocabulary flows through as an interned segment and
   prints in the `[READJSON] kind-coverage` summary (`unkinded-member` lines — the batch-pending
   condition-as-member/per-scaler authorings land there until the curator batches move the data).
   **The `ai` AUDIENCE axis — LANDED:** the §3.9 `ai` sibling compiles as an ENTRY FLAG
   (`CvModEntry::aiOnly`), never an address segment — the walk consumes the `ai` hop (node form: the handicap
   human/AI dual-leaf shape; entry form: the `{payload, ai:{…}}` sibling), so the handicap `costs.…ai.percent`
   member paths kind-resolve cleanly and drop out of the unkinded diagnostic. aiOnly point-foldables fold into
   a SEPARATE slot-table twin: `sum100` stays HUMAN-audience by default, an aiOnly-inclusive read is the
   explicit `bIncludeAiOnly` parameter; every gated record read audience-filters via `MMKernel::audienceOk`
   (the asking player), the DepositIndex records and the reverse-pass landings carry the flag, and the
   `[READJSON] mod-survey` census counts/tags it (the axis stays observable).
   **The per-info typed getter surfaces + the contexts-taking `expected*` valuation endpoints — LANDED with the
   three-info rebuild** (sequence below), reading these compiled forms.
3. **The `Json` name-fragment law applied — LANDED**: `CvJsonCondition` → `CvCondition` · `CvJsonRequires` →
   `CvRequires` · `CvJsonEdges` → `CvEdges` · `CvJsonAllowed` → `CvAllowed` · `CvJsonGrants` → `CvGrants` ·
   `CvJsonProvides` → `CvProvides` · `CvJsonBoolBlock` → `CvClassificationBlock` (types, files, includes,
   references tree-wide). `Json` remains only on the load-time parse surface (`CvJsonParse`,
   `CvJsonConditionParse`). The unused `JsonModScan` string-address scan surface is deleted —
   load-time passes read the compiled entries instead.
4. **The shared kind-enum vocabulary header — LANDED** (`Sources/Infos/CvInfoKinds.h`, scope-free): the closed
   71-family `ModifierFamily` enum (+ the open `MODFAM_PROPERTY` plane) mirroring `CJK_FAMILY_KEYS`; kind enums
   naming the CONCEPT only, each family's scope-participation mask declared beside its enum; engine-enum
   families keyed by `YieldTypes`/`CommerceTypes`/`DomainTypes` (ruling 1); type-keyed members interned as
   data ids; 1–2-entry stragglers on the one `InfoScalar` enum read via `getScalar` (no `100` suffix — the
   scale-naming ruling, [fixed-point-and-scales.md](../../specs/curators/fixed-point-and-scales.md); the
   suffix-rename sweep is audit-ledger item 21).
5. **Fail-loud key coverage — LANDED with item 1** — every top-level key of every entity accounts to exactly
   one consumer; the three failure counts (unresolved FKs / unconsumed sections / unknown keys) print
   unconditionally in the `Loading.log` coverage summary, and a non-reserved object key outside the closed
   family vocabulary is a per-key `[READJSON] ERROR unknown-key` line. "The info matches the JSON structure"
   is a mechanical check.

## The enum groups — definition rulings (the family→vocabulary walk, owner-ruled per class)

Grounded in the full family census over `Assets/Data` (13,157 files, 90 family keys; per-family scopes/members/
targets/units enumerated — the census script rides `Tools/` when this lands, transient output regenerable).

1. **Families the engine already enumerates REUSE the engine enum** (owner): `food`/`production`/`commerce` →
   `YieldTypes`; `gold`/`research`/`culture`/`espionage` → `CommerceTypes`; domain-keyed members →
   `DomainTypes`. No new vocabulary where the engine already speaks one. (`happiness`/`health` have NO engine
   enum — the four wellbeing channels are cascade channels, so they fall to the new-kind-enum class;
   `PROPERTY_*` stays data-keyed by id, never enum entries.)

2. **The member triage test (owner):** a family member is a KIND only if it answers "WHICH component does this
   modify" (`defense.bombardDefense`, `maintenance.distance`); a member answering "WHEN/WHERE does it apply" is a
   CONDITION-AS-MEMBER rollerskate — the predicate just hasn't been defined yet
   ([DEC-conditions-are-predicates]). First application: `maintenance.empire.{homeArea,otherArea}` re-author as
   conditioned deposits on the NEW `IS_HOME_AREA` predicate ([json.md §3.5](../../specs/json.md); "other areas" =
   `"!IS_HOME_AREA"`) — curator change + regen in the same work item ([DEC-recurate-on-decision]); the
   `MaintenanceKind` enum carries only the genuine calc components (`numCities`, `distance`, `corporation`,
   `colony`, `cap`, the scope-wide amount). The walk applies the same triage to every member the census lists
   (`culture.unit.garrison`, `commerce.empire.tradeRoute`, `empire.specialist`/`perSpecialist`, …);
   `empire.goldenAge` stays — the ledgered permanent carve-out. Census evidence per family: `defense` = 425
   AUTHORING ENTITIES, ONE family, ~13 concept kinds × 3 scopes — the scope axis and the conditions never
   inflate the vocabulary.

3. **`copsAndRobbers` renames to `underworld` (owner)** — the in-city criminal game (criminals hidden in a city,
   investigated, arrested): kinds `insidiousness` (how deep criminals burrow) + `investigation` (the power to
   drag them out), city scope. The stray 2-entity `investigation` family MERGES into it; `espionage`'s
   `insidiousness`/`investigation` members get the ruling-2 triage on the walk (one contest, one home).
   **`detection` stays RESERVED for the hide-and-seek plane** (map-level spotting of hidden units — a different
   system, its own future block per the sizeMatters pattern). Curator rename + regen in the same work item.

4. **`per<X>`-named members ARE §3.7 `per` count-scalers — the name is the verdict (owner):** `perPopulation` →
   `per:{type:POPULATION}` (token exists); `perSpecialist` → `per` over the specialist count (the tally's
   specialist domain); `perCorporationLevel` → `per:{type:CORPORATION_LEVEL}` (mint the §3.1 count token).
   All three leave every family's kind list; curator re-authors + regen. Still on triage from the same member
   set: `specialist` (suspected deliveryguy inversion → own-output on the specialist / trait-keyed carve-out,
   [modifier.md §4](../../specs/modifier.md)) and `headquarters` (suspected corp-HQ FK value plane).

5. **`strength` vs `combat` (owner): `strength` = the unit's BASE value only** (`strength.unit.flat`, absent if
   it cannot fight); **`combat` = everything that MODIFIES that base** — the semantic modifier kinds (`attack`,
   `defense`, `cityAttack`, `cityDefense`, `hillsAttack`/`hillsDefense`, `stealth`, `flanking`, `lunge`, …), the
   type-keyed vs-entries (`UNITCOMBAT_*`/`UNIT_*`/`TERRAIN_*`/`FEATURE_*`/`DOMAIN_*` — interned ids), promotion +
   unitcombat authorings, at unit/empire/team/city scopes. The old grab-bag `combat` members redistribute to
   their concept families via the scope axis: `captureProbability`/`captureResistance` → `capture`;
   `missileCargo`/`navalCargo` → `cargo`; `flightRange`/`missileRange` → `air`/`range`; `espionageDefense` → the
   `espionageDefense` family. Stay in `combat`: `animal`, `barbarian`, `freeWinsVsBarbs`. Flagged for their own
   call: `subdueAnimal` (outcome-plane smell), `nukeInterception` (air-defense smell). Curator re-home + regen.

6. **`text` is NOT a family (owner)** — it is TXT_KEY references (display strings + their template parameters:
   `value`/`change`/`changeAllCities`/`prereqMin`/`prereqMax`), not evaluable data. The reader classifies it as
   a reserved TEXT/intrinsic key (never family-walked, no kinds, never evaluated); the curator re-homes the 7
   authorings under `identity` with the rest of the TXT_KEYs ([json.md §7](../../specs/json.md)).

7. **`odds` is OUTCOME-plane data, not a family (verified: all 6 authorings are `OUTCOME_*` entities)** — the
   per-promotion extra-chance table on `CvOutcomeInfo`
   ([mission-outcome-system.md](../../reference/mission-outcome-system.md)); promotion keys intern as ids; the
   reader classifies it to the outcomes system. **Recorded owner direction for the grants/outcome rework** (out
   of this walk's scope): grants become PURE PAYLOAD ("just what is given"), an EVENT triggers the grant, and
   odds/chance live on the TRIGGER plane — the authoring-shape half of what the grants-engine-as-spine-consumer
   machinery already does ([event-spine.md](../../specs/event-spine.md): a grant is a result of a genuine
   acquisition; [grants-machine.md](grants-machine.md) owns the follow-through).

8. **The grants/triggers rework — IN SCOPE NOW (owner: "we have to do the rework now, to keep this machinery
   consistent"); [json.md §5](../../specs/json.md) is rewritten to it.** The split: **`grants` = pure payload on
   the source's CONSIDERED ACTION** (construct/research/adopt/found/mission — implicit, no trigger field, no
   odds; founder buildings become plain `grants.buildings` on the settler); **`triggers` = `trigger` → `chance`
   → `action`**, in that reading order — trigger is an `on<Happening>` token (the spine's DOMAIN events in
   authoring form: `onCreation`, `onFound`, `onTurn`, … an OPEN registry) and/or a §3 state condition (the
   fire-band exemplar: flammability ≥ 200 → 5% → burn a building); `action` is an open verb registry (`destroy`,
   `grant` = the §5 payload nested whole, shared with the §8 outcome verbs — the outcome verb `triggers` renames
   to `fires`). Odds ALWAYS live on the trigger, never in a payload. **Curator re-emission in this work item:**
   `repeatable`+`interval` → `triggers` entries; `foundBuildings` → `grants.buildings` (settler); building
   `freePromotions` → a `triggers` end-turn-presence entry; property pulses → `triggers` entries with spatial
   intent in the action. [grants-machine.md](grants-machine.md) takes this as the authoring contract — the
   apply-loop (still unbuilt) implements against the new shape once.

9. **`specialist` member — resolved by ruling 4's conversion:** `<c>.empire.specialist.perSpecialist` is ONE
   nested authoring (legacy `SpecialistExtraCommerces`, "+N per employed specialist of any type"); the
   per-scaler conversion dissolves the whole shape, container included. Per-TYPE boosts
   (`empire.specialists.{SPECIALIST_X}` keyed targets) stay deliverer-authored, landed specialist-side by the
   general reverse pass — keyed ids, never vocabulary. `freeSpecialists` is unrelated (the grant/cap system;
   placement stays the engine/AI two-part seam).
10. **`headquarters` member (corporations only — the HQ-city revenue, `shrine`'s corp analog): a WHERE →
   conditioned deposit** on the new parameterized predicate `{IS_HEADQUARTERS: CORPORATION_X}` (the
   `{IS_HOLY_CITY: RELIGION_X}` pattern). Its `perCorporationLevel` half already converts under ruling 4.
11. **`tradeRoutes` is ONE family with conditions (owner):** kinds collapse to `routes` (flat count),
   `modifier` (route-yield %), `max` (the cap); the variant members are CONDITIONS — `foreign`/`foreignModifier`
   → a foreign-partner predicate, `coastal` → `HAS_COAST`, `sharedCivic` → a shares-civic predicate — and the
   `commerce.<scope>.tradeRoute` member (verified: a route-yield modifier) merges in as `tradeRoutes.modifier`.
   Predicates defined with the batch. *(Rulings 9–11 = the SECOND curator batch; the first is in flight.)*

12. **Wellbeing mints zero kinds** — batch-2 worklist only ([DEC-conditions-are-predicates] + §3.4 applied, no
   new ruling): re-author `cityLimit` / `cityOverLimit` / `civicAnger` / `foreignerUnhappy` /
   `nonStateReligion` / `taxRate` / `max` as conditioned deposits (`min:`/`max:` atoms for the limits,
   predicates otherwise).

13. **`healing` was a rollerskate (verified: 3 buildings' `iHealRateChange`, `curate_building.py:66` minting a
   fresh key)** — it folds into the `heal` family at CITY scope (`heal.city.flat`, the engine's own
   `cityContribution` term of the one heal calc). Batch 2.

14. **`perEra` RESOLVED (owner): decompose per consumption site.** `ERA` is the §3.1 counter (1…N, the ordered
   era sequence); what was falsified was only the single-family landing. `iAIPerEraModifier` re-authors as ONE
   `ai`-sibling deposit per affected (family, kind) — the 13 mapped sites in `curate_handicap.py`, each with
   `"per": "ERA"`, the workRate site a NEGATIVE value (the engine's subtraction is a negative deposit). Each
   site's exact math transcribed (`× era` vs `× (era−1)` — the latter = the per-ERA deposit + one flat −value
   entry; mirror per site). Curator batch 3a LANDED (`curate_handicap.py` `PER_ERA_SITES`: all sites are
   ×getCurrentEra() = ×(ERA−1), 11 deposits + companions; the clamped unit-upkeep site CvPlayer.cpp:10354
   `std::max(0, 100 + perEra×era)` is ruling 24's config value — landed with batch 3b).

15. **The corp twins:** `corporationMaintenance` folds into the existing `maintenance.corporation` kind;
   `corporationRevenue` lands as a `corporation` kind on the commerce side (the R11 source-component pattern).
   **LANDED** (data: `commerce.empire.corporation` / `maintenance.empire.corporation`, tech_stock_brokering;
   C++: `CHANNEL_CORPORATION` on the commerce member table + the `CvTechInfo` reads re-pointed).
16. **Chance-modifiers of happenings are TRIGGER-plane data, never family kinds (owner):** `survivor` (onDeath
   survive chance), `cityCapture` (onCapture odds/resistance; distinct from the unit-plane `capture` family),
   `combat.subdueAnimal` (the subdue kill-outcome roll), `combat.nukeInterception` (on-nuke-launched
   interception). Each attaches to its trigger's `chance`; authoring shapes finalize with the trigger system's
   build-out ([grants-machine.md](grants-machine.md)).

17. **`tradeRouteYield` + `foreignTradeRouteYield` (verified: tech percent modifiers on route output) fold into
   ruling 11's `tradeRoutes.modifier`** as conditioned entries — predicate `IS_FOREIGN` (route partner is
   another civ), domestic = `"!IS_FOREIGN"` (the IS_HOME_AREA one-predicate precedent). Batch-2 verify at the
   engine consumption site: is plain `tradeRouteYield` all-routes (unconditioned + foreign extra) or
   domestic-only (`"!IS_FOREIGN"`) — mirror, don't redesign.

18. **The cost cluster — three planes (owner): the ACTUAL cost · what CHANGES a cost · the DERIVED price.**
   (1) Actual cost = the reserved `cost` section + self-data: `hurryCost.city` (verified `iHurryCostModifier`,
   "hurrying ME") → the entity's own cost data; `buildTime` → substrate self-data. (2) ONE `costs` modifier
   family, kinds by category — `train`/`construct`/`create`/`build`/`research`/`improvementUpgrade`/
   `researchCutBelowEra`/`hurry` (absorbs `hurryCost.empire` + `hurry.cost`)/`upgrade` (absorbs
   `unitUpgradePrice` + `upkeep.upgrade*`) — scope as the axis; `techCost` → `costs.research`; `buildCost` →
   `costs.{train,construct,create}` with the `world*`-prefixed kinds retired by the scope axis. (3) Derived
   prices (upgrade gold, hurry gold/pop) = engine-computed from plane-1 × plane-2; formula parameters are
   world/handicap config, never vocabulary. Batch 2.

19. **The building-keyed deposits LAND on the target building (owner)** — the 1,496 wonder/civic/tech →
   building-type boosts (Bank of China Tower → every Bank) become the TARGET building's own conditioned
   entries: landed at CITY scope (building output, §2a), the presence condition carrying the AUTHORED deposit's
   scope (an `empire`-keyed authoring → empire-scope presence atom). `CvReversePass` classification amendment +
   the modifier.md §2a/§2b prose that asserted source-side reading updated in the same change. Pair 2 (the 6
   civic feature-happiness entries) STAYS source-side (the legacy one-term bundling; unruled otherwise).

20. **Slider-rate tokens (owner: "we have happiness per culture rate — no reason we can't have anger per gold
   rate"):** mint `GOLD_RATE` / `RESEARCH_RATE` / `CULTURE_RATE` / `ESPIONAGE_RATE` (§3.1 engine-resolved
   counters = the commerce slider percents). The `taxRate` wellbeing holdout → an ordinary `anger` deposit
   `per: {type: GOLD_RATE, each: N}` (each transcribed at the engine site), and the whole `commerceHappiness`
   family dissolves the same way — `happiness` deposits `per: "<CHANNEL>_RATE"`. Wellbeing stays at zero minted
   kinds. Curator batch 3. **Batch 3b LANDED:** `taxRate` → `happiness.empire` `{−V, per:{GOLD_RATE, each:100}}`
   (CvPlayer.cpp:26526 `V × goldPercent / 100`); the 6 building `commerceHappiness` authorings →
   `happiness.city` `{V, per:{<CHANNEL>_RATE, each:100}}` (CvCity.cpp:12803 `per × commercePercent / 100`);
   the family key is gone from the census. **C++ LANDED:** the four slider-rate tokens resolve in the ONE
   count core (`ev_countCore` = `CvPlayer::getCommercePercent(<channel>)`), and the `commerceHappiness` /
   `perEra` vocabulary rows are deleted (enum + `CJK_FAMILY_KEYS`, both machine-diffed empty against the
   census) with the two keys tombstoned in `CJK_RETIRED_KEYS`.

21. **`cityLimit`/`cityOverLimit` (owner: "min: number, anger per num_cities")** — the engine's
   `V × (cities − limit)` authors EXACTLY with existing vocabulary as the telescoping pair (the batch-3a
   pattern): `{V, per: "CITY", enabled: {type: CITY, min: limit+1}}` + the flat companion
   `{−V×limit, enabled: same}` (the per-civic limit is a curation-time constant), **both entries additionally
   gated on the city-limits GAME OPTION** (owner) — `enabled: {all: ["GAMEOPTION_…", {type: CITY, min: …}]}`,
   the option name transcribed from the engine gate at execution. The proposed `above:` per extension DIES — no
   vocabulary change. Any found-block interplay at the limit is enabler data, checked separately at execution.
   Curator batch 3. **Batch 3b: BLOCKED at execution, reported** — the constant-limit premise is FALSE: the
   engine limit is WORLD-SIZE-SCALED, `CvCivicInfo::getCityLimit` (SourceArchive/Infos/CvCivicInfo.cpp:1015)
   returns `m_iCityLimit × CvWorldInfo::getCityLimitsScalePercent() / 100` (scale 50/75/90/100/110/125/150/200
   per world size, CIV4WorldInfo.xml) under `GAMEOPTION_EXP_OVEREXPANSION_PENALTIES`, feeding
   `V × (numCities − scaledLimit)` at CvCity.cpp:5665-5674 — a curation-time `min:` atom cannot transcribe the
   scale. Members kept as-is; verbatim math documented in `curate_civic.py`.
22. **`foreignerUnhappy` (owner: "a culture_percentage on city driver")** — mint the `CULTURE_PERCENTAGE` city
   counter token (§3.1: the city's OWN-culture percent). The Nationalist civic's `V`-divisor authoring
   converts by reciprocal precompute (`R = 100/V`, exactness verified against the single authored value) into
   the telescoping pair: `anger` flat `R` + `{−R, per: {type: CULTURE_PERCENTAGE, each: 100}}` ≡
   `R × (100 − ownPct)/100`. No inverse semantics minted. Curator batch 3. **Batch 3b LANDED:** Nationalist
   V=10 → R=100/V=10 EXACT (100 % 10 == 0); `happiness.empire.flat` `[−10, {10, per:{CULTURE_PERCENTAGE,
   each:100}}]` (engine CvCity.cpp:5650-5654 live + :8664-8667 what-if, both `(100/V) × (100 − ownPct) / 100`);
   an inexact V falls back to the legacy member + a curator warning. **C++ LANDED:** the `CULTURE_PERCENTAGE`
   counter resolves in the ONE count core (`plot()->calculateCulturePercent(owner)`).

23. **`nonStateReligion` (owner: the NOT predicate already exists)** — no token mint: the deposit per-scales
   over the city's religions filtered by the existing `"!IS_STATE_RELIGION"` composition — the §3.7
   predicate-filtered count (the `unit:` qualifier pattern, religion-typed). Curator batch 3; the evaluator's
   filtered-count leg verified/extended at execution, reported if the per-filter form needs a §3.7 sentence.
   **Batch 3b LANDED:** field spelling `religion:` (the counted-kind field naming pattern); civic + trait
   authorings → `happiness.empire.cities.flat` `{V, religion: "!IS_STATE_RELIGION"}` (engine CvCity.cpp:9407-9418,
   trait feeder CvPlayer.cpp:28516 — same accumulator); the §3.7 non-unit-qualifier sentence added to json.md.
   Traits are LOCKED → 20 files by one-off scripted transform; the 4 `trait_bigot*` files REPORTED undone —
   their `happiness.empire.cities` node is already ranked-qualified (`max`/`orderedByDescending`, the
   largest-cities authoring) and one plural-target node cannot carry two differently-qualified subsets;
   composition shape needs a ruling. **C++ LANDED:** the `religion:` filter parses to a prebuilt tree on the
   compiled entry (`CvModEntry::religionQual`, entry-level + node-level shorthand), the new
   `IS_STATE_RELIGION` predicate tests the counted religion (`CvCascadeEvalCtx::religion`), and the count leg
   is `cascadeCountCityReligions` — applied by the ONE per resolver (`MMKernel::perScale` scales the value by
   the matching-religion count).

24. **Site 7 (AI unit upkeep era stage) = a PLAIN CONFIG VALUE on the handicap (owner):** the engine formula's
   own parameter (the §7 `ai` metadata plane), keeping its clamp and multiplicative stacking in the formula —
   never a deposit. **Batch 3b LANDED:** key
   `ai.unitUpkeepEraModifier` = the legacy `iAIPerEraModifier` on the 6 authoring handicaps (emperor −1,
   immortal −2, deity −3, nightmare/nightmare+/ai_boosted −5). **C++ LANDED:** `CvHandicapInfo` reads the
   config (`m_iAIPerEraModifier` ← `ai.unitUpkeepEraModifier`), feeding CvPlayer.cpp:10354 AND the other 12
   mapped `× getCurrentEra()` consumer formulas, whose math the config reproduces exactly; the dead
   `techCost`/`buildCost`/`upkeep.upgrade` reads re-pointed to the `costs` kinds
   (`costs.{empire,world}.<kind>.ai.percent`), and every scalar mirror over a batch-3a entry LIST reads the
   GAME-START BASE (bare terms + per-ERA at ERA 1 — `hc_leafBase`; the consumers' own formulas carry the
   ramp, so the per-ERA deposits stay the cascade-side twin, never double-applied).

25. **Entry-level target qualifiers restored to the spec (owner: the original json-design intent)** — the §3.3
   ranked qualifiers + counted-kind filters ride individual §3.9 entries; node-level is shorthand for
   unqualified entries ([json.md §3.9](../../specs/json.md)). Unblocks the four `trait_bigot*` R23 holdouts —
   they convert with the next curator pass (locked-folder transform). **Batch 4 LANDED:** the 4 `trait_bigot*`
   files converted (locked-folder transform) — one `cities.flat` list holding the ranked entry
   `{−1, max: TARGET_NUM_CITIES, orderedByDescending: CITY_SIZE}` beside the religion-qualified entry
   `{−1, religion: "!IS_STATE_RELIGION"}`; `nonStateReligion` census count 4 → 0. **C++ LANDED (parse+carry):**
   entry-level `max:`/`orderedBy`/`orderedByDescending` and the node-level shorthand parse onto the compiled
   entry (`CvModEntry::hasRankQual`/`rankMax(Token)`/`orderedBySeg`; a node-level numeric `max` no longer
   misparses as a count-by-type leaf) and ride the reverse-pass landings; ranked-selection EVALUATION stays
   the parked plan ([ranked-target-selection.md](../parked/ranked-target-selection.md)) — a ranked entry
   applies unranked until it lands, exactly as the pre-parse data did.

26. **The over-threshold scaler exists as a first-class expression (owner: "if we don't have expression for
   that, we should have that"):** `per.above` — `value × max(0, count − above)`, threshold literal or token
   ([json.md §3.7](../../specs/json.md)). The city-limit unhappiness authors as ONE entry:
   `{−V, per: {type: CITY, above: "CITY_LIMIT"}, enabled: GAMEOPTION_EXP_OVEREXPANSION_PENALTIES}` with the new
   SOURCE-resolved `CITY_LIMIT` token (civic base-limit config × world-size scale) and the civic's base limit
   emitted as config data. SUPERSEDES the constant-pair shape of ruling 21 for this mechanic (the telescoping
   pair remains the general subtractive idiom). C++ worklist: the `above` leg in the per-resolver + the
   `CITY_LIMIT` token resolution. Curator batch 4. **Batch 4 LANDED:** the 6 government civics author the ONE
   entry (theocracy/republic V=3, monarchy/chiefdom/despotism V=5, anarchism V=6, as `happiness.empire.flat`
   `{−V, per:{CITY, above:"CITY_LIMIT"}, enabled:"GAMEOPTION_EXP_OVEREXPANSION_PENALTIES"}`); the base limit is
   the config datum **`identity.cityLimit`** (15/15/12/5/8/2 — the anarchyLength convention; the CITY_LIMIT
   resolver reads it × `CvWorldInfo::getCityLimitsScalePercent()`/100, and the CvPlayer.cpp:6210 found-block
   reads the same datum). `cityLimit`/`cityOverLimit` members gone from the census. NB the per-source token is
   an intentional semantics choice: legacy summed the limit/V accumulators ACROSS adopted civics
   (CvPlayer.cpp:18216-18217) — indistinguishable in shipped data (all six are the mutually-exclusive
   GOVERNMENT column); a V-without-own-limit civic has no per-source transcription (curator warns + keeps the
   legacy member). **C++ LANDED:** the `above` parse leg (`jsonParsePer`: literal or token) + the eval leg
   (`MMKernel::perScale`: `value × max(0, count − above)`, composing with `each`); `CITY_LIMIT` is
   SOURCE-resolved at load — `CvCivicInfo::mapFrom` reads the new `identity.cityLimit` config
   (`getCityLimit()`, the base) and stamps it onto its own compiled entries
   (`CvModifiers::resolveAboveToken`, the SELF-collapse precedent), the world-size scale applied at eval; an
   unresolved threshold skips the multiply (the SELF guard). **Consumer trio LANDED:** the CvPlayer feed sites
   (18216 processCivics / 18675 load fixup / 8465 canDoCivics) read the RESOLVED limit through the ONE
   engine-side calc (`InfoValuation::resolvedCityLimit` — base × world-size scale, gated on
   `GAMEOPTION_EXP_OVEREXPANSION_PENALTIES` exactly as the archived getter), and the over-limit-anger PRESENCE
   derives from the civic's compiled CITY_LIMIT `per.above` entries (option (a): `hasCityOverLimitAnger`,
   materialized at mapFrom); `m_iCityOverLimitUnhappy` is now a PRESENCE COUNT of in-force anger-carrying
   civics (the anger MAGNITUDE lives in the cascade deposits) — the 6210 found-block reads the accumulators
   unchanged. NB the AI's V-magnitude reads (`CvPlayerAI.cpp:13384/13416/13420/14174` — `iExtraCities × V`
   civic scoring) are the WIDER consumer-rewiring wave's: they need the deposit magnitude (the what-if read),
   not the presence bool.

27. **Trait route-yield boosts (owner: already specced — the percentage rides ON TOP of the engine's incoming
   route yield):** `tradeRoutes.modifier` carries the CHANNEL axis (`YieldTypes` reuse), applied at the §2a
   `tradeYield` input fold. The ~15 trait food/production entries + the earlier commerce merges all author as
   per-channel `tradeRoutes.modifier` entries. Curator batch 4. **Batch 4 LANDED:** the channel shape is
   `tradeRoutes.<scope>.modifier.<channel>.<unit>` (channel = the YieldTypes family word — the
   `shrine:{gold:N}` keyed-axis convention): 22 trait files' `food`/`production` `tradeRoute` members moved
   (32 entries) + 28 batch-2 bare commerce merges re-keyed to `modifier.commerce` (verified entry-by-entry ==
   the pre-batch-2 `commerce.empire.tradeRoute` values; locked traits by one-off transform), and the civic
   curator emits all `TradeYieldModifiers` channels the same way (56 food/48 production ex-`tradeRoute`
   members + 54 commerce). Channel-AGNOSTIC route modifiers (building `iTradeRouteModifier`, the
   `IS_FOREIGN`/`SHARES_CIVIC` conditioned entries — the `CvCity::totalTradeModifier` route-PROFIT stage)
   stay channel-less on `modifier.percent`. Engine transcription: per-channel accumulator
   `m_aiTradeYieldModifier` (feeders CvPlayer.cpp:18058 civic / :28448 trait) applied at
   `CvCity::calculateTradeYield` (CvCity.cpp:11645-48, `profit × mod[ch]/100`), identity 100 on
   `CvYieldInfo::getTradeModifier` (commerce 100, food/production 0). `tradeRoute` member gone from the
   census. **C++ LANDED (vocabulary):** the channel axis interns on the kind vocabulary —
   `TRADE_ROUTE_MODIFIER_FOOD/PRODUCTION/COMMERCE` (contiguous in YieldTypes order, so the parameterized read
   is `TRADE_ROUTE_MODIFIER_FOOD + eYield`), member rows `modifier.<channel>`; kind-coverage reports 0
   unkinded for them. **The §2a fold seam LANDED as its ONE canonical calc function**
   (`InfoValuation::tradeRouteChannelYield100` — `routeYield100 × (channelBase% + Σmod)/100`, the channel
   identity on the base percent; pure static, inputs in / ×100 out): the future package rebuild's rate calc
   and the `expected*` endpoints call this same function — the fold math exists once, ahead of the package
   graft. The infos serve the per-source sum via `getTradeRouteYieldModifier(eYield, eScope)`.

28. **Pop-scaled free unit upkeep (owner, batch 4 + the 4b sign normalization):** the
   `FreeUnitUpkeep{Military,Civilian}PopPercent` fields convert UNCONDITIONALLY as per-population deposits
   on the existing `upkeep.freeMilitary`/`freeCivilian` kinds under the ONE **FREE-AMOUNT sign convention**
   (owner, 4b): entries are free-amount semantics throughout — positive = free upkeep GRANTED, negative =
   free allowance REDUCED; entries sum, the engine nets `max(0, upkeep − Σfree)`. Authored
   `{P, per:{POPULATION, each:100}}` keeping the legacy P's own sign (engine PER_100_POP semantics: the
   source's marginal free upkeep is `pop × P / 100`, CvPlayer.cpp:10218-10226 via
   `getModifiedIntValue(totalPopulation, P)` — P>0 grants, P<0 shrinks; feeders :18033-34 civic /
   :28507-08 trait) — **floored at 0 via family-combine floor metadata** (the modifier.md §2 `min`-member
   mechanism; the engine floors are `free ≥ 0` and `net = max(0, upkeep − free)` per class,
   CvPlayer.cpp:10295/:10315). An INTENTIONAL model change from the legacy `getModifiedIntValue` rounding
   (its asymmetric `mod<0` branch `v×100/(100−mod)` is NOT chased — additive linear is the ruled shape;
   validation.md attributed-intentional). **Batch 4+4b LANDED:** 25 civic authorings (curator, all
   positive) + 54 trait authorings (50 locked files, one-off transform, 26 positive/28 negative — each
   source's direction verified at the pre-batch-4 revision, no blanket flip); `perPopulation` unit keys 0
   across Assets/Data; the pre-existing flats already carried free-amount signs, so the kind is now ONE
   convention (the batch-4 sign tear RESOLVED). **C++ LANDED (metadata):** the family-combine floor table
   lives beside the kind vocabulary — `infoCombineFloorAtZero(family, kind)` (CvInfoKinds), true for
   `upkeep.freeMilitary`/`freeCivilian` (Σfree floored at 0 as a group). **The combine seam LANDED as its ONE
   canonical calc pair** (`InfoValuation::combinedGroupSum100` — the site that consumes the floor metadata,
   applying `max(0, Σ)` to a summed group total; `InfoValuation::netUpkeepAfterFree100` — ruling 28's second,
   engine-side floor `max(0, upkeep − free)`; the two floors are distinct by design): the future package
   rebuild's upkeep calc calls these same functions. The per-pop identity
   (the `BASE_FREE_UNITS_UPKEEP_*_PER_100_POP` GlobalDefines, all currently 0) stays engine formula config.

28. **Free unit upkeep (owner: "just subtract upkeep per pop, with a floor of 0")** — converts as subtractive
   per-population deposits (`upkeep.freeMilitary`/`freeCivilian`, negative value, `per: POPULATION` with the
   transcribed quantum), the zero floor as family-combine metadata (the §2 `min` mechanism). An owner-ruled
   INTENTIONAL model change from the legacy asymmetric rounding helper — attributed, never bit-chased
   ([validation.md](../../specs/validation.md) intentional class). Curator batch 4.

## ⛔ THE GREEN GATE (owner): green is NOT a deliverable until ALL JSON-based infos are set up properly

Compilability becomes a goal only after EVERY JSON-fed info — with the modifier-carrying and enabler-carrying
types as the critical set (units / promotions / unitcombats / techs / buildings / civics / traits / bonuses /
improvements / features / terrains / routes / corporations / religions / projects / processes / handicaps + the
uniformity types) — carries the proper structure: the compiled modifier surface, the enabler sections, its
per-type intrinsics on the [patterns.md § THE GETTER SETUP](../../architecture/patterns.md) exemplar. The
three-info rebuild proves the pattern; the sweep across the remaining types follows it; the green-up (consumer
rewiring waves + the Engine include repair + the full build) is its own stage AFTER the structure is complete.
Structure is never bent to reach green ([DEC-structure-before-shadow] generalized to the build state).

## The stage after the infos: CONTEXTS WORKING WITH THE INFOS (owner)

Once every info is properly structured, the contexts integrate against that surface as their own stage — the
[contexts.md](../../architecture/contexts.md) model made live end to end: the eval-ctx fill seams
(`CityContext::fillEvalCtx` / `EmpireContext::fillEvalCtx`) feeding the ONE evaluator; the HAVE axis read
through the contexts by every gate and atom; `cityContext.plotAttrs` scaling the `plots`-target deposits (and
its event-driven load reseed); the `CvPlotGroup` traded leg; the `expected*` endpoints exercised for every
rebuilt type through the two pass-in scenarios and nothing else. THEN the green-up stage.

**Stage 3B LANDED (the contexts leg):** the evaluator's atoms/counts + the enabler's gate reads ask the
contexts (the ctx's bound pointers are the context bindings — `ev_cityContext`/`ev_empireContext`/
`getPlotContext()`; the enabler fill sites route through the `fillEvalCtx` seams); the plotAttrs load reseed
is built (`CvPlot::read` emits the deserialized working-city fact, `Engine/ContextConsumer` — the contexts'
own spine consumer — buffers the bracket and drains at `GAME_LOAD_FINISHED` through the one applier;
`CvPlot::updateWorkingCity` emits at play); the traded leg rides `CvCascadeEvalCtx::plotGroup` (city-bound
asks stay on the city's plot-group-backed maintained count); the §2 isolated plot-as-base calc is
`InfoValuation::plotBaseYields100` over per-substrate `plotOwnYield100` (plot-eval flags, reverse-landed
conditioned entries included), and `expectedPlotYields` now folds an info's own untargeted plot output beside
the plots-target leg. Per-type what-if fixes: the `IS_HEADQUARTERS` predicate joined the vocabulary (was
UNKNOWN→ignored — HQ revenue applied in every corp city) and `CORPORATION_LEVEL` resolves in the ONE count
core off the corp's SELF-collapsed id (`CvModifiers::resolvePerToken`, the resolveAboveToken precedent).
Open hooks riding the spine plane: the `contextRegisterConsumer()` line in `spineRegisterConsumers`, and the
load-bracket + choke-point emit rebuild the reseed drains on.

## ⛔ THE ORDER: design surface → contexts → THEN the AI calls (owner)

**Exactly ZERO AI calls have been re-wired, BY DESIGN** (owner): *"we nail the design surface, and contexts,
then we wire the AI calls with the new data."* The AI is the LARGEST consumer of the info surface —
`CvPlayerAI` / `CvUnitAI` / `CvCityAI` / `CvTeamAI` are the bulk of the ~4,000-site stage-4 debt — which is
precisely why it goes LAST: wiring thousands of AI reads onto a surface that is still being settled would bake
in a shape we are still deciding, and every later refinement would re-break them.

⚠ **So a dangling AI call site is NOT a defect to fix on sight.** The purge deleted the legacy getters so the
COMPILER would name every consumer ([DEC-playability-not-a-gate](../../architecture/decisions.md#dec-playability-not-a-gate):
the compiler IS the census) — that census is a WORKLIST for a later stage, not a queue of bugs. Reaching into
`Sources/AI/` to "repair" one is the rollerskate: it wires the AI to a moving target and quietly re-legitimises
whatever getter shape happened to exist that day. Read the red as intended output.

Order of operations, and what "done" means at each step:
1. **The design surface** — the infos on the exemplar + the compiled read forms (stage 2, complete).
2. **The contexts** — the live-state read surface the getters and the ONE evaluator ask (stage 3, complete).
3. **THEN the AI calls**, rewired onto the settled surface with the new data — together with the rest of the
   consumer cut below.

## The FINAL stage: Python + getters rewired onto it all (owner)

Last, the consumer cut: the getter surface and the Python boundary move onto the structured backend — **the
Python FRONTEND looks roughly the same; underneath it runs the structured surface.** ⛔ **LITERALLY ALL Python
getters used today are OBSOLETE (owner) — wildly clunky, none survives, none is ported getter-for-getter.** The
new binding surface is designed from a census of what Python actually consumes — the SCREENS **including the
PEDIA as its own mapped surface (owner)** — the largest info-reading screen family; its cross-links map onto
the `EDGEF_RELATED` families, its effect lines onto the per-entry renderer, its requires display onto the
section objects (the read-map: [pedia-map.md](pedia-map.md)) — **and the Python-authoritative gameplay
systems** (Revolution, events: their reads are invisible to engine-side greps — the `revolution.distanceMod`
catch is the standing exhibit) — answered in the structured shapes (group arrays,
section objects — a handful of coherent reads replacing hundreds of scalar calls), with
the legacy `Cy*` info surface DISCONNECTED whole
([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed) — never a widened binding, never two live
surfaces), and every C++ consumer rewired onto the exemplar getters
([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface) — the full cut, no
thin-compat layer). GREEN falls out of this stage as its outcome — the compiler census completes when the last
consumer is rewired — it is never a separate goal pursued ahead of it.

### ⛔ The Python deliverable is ONE COMPLETE DATA-FETCHING LIBRARY (owner) — its own STEP, built before the cut

The Python half is not "rewire the call sites and see what breaks". It is a **library you build, complete, and
then cut over to** — two words carry the whole requirement:

- **ONE SURFACE.** A single data-fetching library IS the Python-facing read boundary — not the scattered
  per-type `Cy*` interfaces it replaces (`CyInfoInterface1..N` + the per-object getter sprawl), not a widened
  binding, and never two live surfaces for one read ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).
  One place a Python author looks; one place the boundary is maintained.
- **COMPLETE.** Every read Python performs today has an answer in the library **before** the legacy surface is
  disconnected. Completeness is the GATE, not an aspiration: a single gap forces a reach-around into legacy,
  and that reach-around IS the second live surface the ruling forbids — the half-migration, re-created at the
  last seam.
- **DATA FETCHING, not gameplay.** It serves reads/payloads; Python-authoritative gameplay (Revolution, events)
  stays Python and becomes a CONSUMER of the library — the boundary redesign never pulls gameplay into the DLL.
- **⛔ ENUM OPERATIONS ARE FIRST CLASS (owner).** Name→type/enum resolution is a SUPPORTED operation of the
  library's surface, never an accident of `getattr` on a module — and it must cover **resolution AND
  EXTENSION**: BUG resolves `WidgetTypes`/`InputTypes`/`InterfaceDirtyBits` by name from config strings *and*
  MINTS new `WidgetTypes` members at runtime (`setattr`), handing them back to the engine as widget ids, so a
  read-only lookup does not serve it. Three engine enums are reachable ONLY this way, so a library lacking it
  forces a legacy reach-around — the second live surface the one-surface ruling forbids. It generalizes what
  the engine already does for infotypes (`getInfoTypeForString`) and is the same shape as the load-minted
  classification registries ([DEC-classification-infos](../../architecture/decisions.md#dec-classification-infos)):
  names minted to ids once, resolved by id thereafter.
- **⛔ MAP SCRIPTS ARE THEIR OWN BOUNDARY (owner) — outside this library entirely.** They read map-gen info
  types nothing else reads, run BEFORE most game state exists, are WRITE-dominated (they build the map; this is
  a read surface), and `eval` script-supplied expressions as an open extension point. Their contract stays the
  named Python CALLBACKS ([engine.md](../../reference/engine.md) — the callback names are the contract, not the
  impl, with the DLL fallback intact), so third-party map scripts are unaffected by the `Cy*` cut and their
  map-gen types leave this library's coverage appendix. A map-gen boundary redesign is its own work item.

**The checklist is the census, not judgement** ([DEC-all-means-all](../../architecture/decisions.md#dec-all-means-all)):
the screens, the PEDIA as its own mapped surface ([pedia-map.md](pedia-map.md)), and the
Python-authoritative systems whose reads are invisible to engine-side greps (the `revolution.distanceMod` catch
is the standing exhibit — a grep-invisible read is exactly what a "complete" claim misses). Each censused read
resolves to a structured shape — group arrays, whole section objects, rendered entry lines, the edge lists, the
per-entity and per-type index payloads — so a handful of coherent fetches replace hundreds of scalar calls.

**This is a REBUILD, not an invention (owner): the `Cy*` wrappers already ARE this library in embryo** — Python
has always fetched its data through a binding layer, and that MECHANISM (the boost::python binding surface) is
fine and stays. What is wrong is the SHAPE: scattered per-type interfaces, one getter per legacy field mirroring
the `CvXInfo` contract, no coherent payload anywhere — which is why it reads as "wildly clunky" rather than as a
library. So the work is the coherent version of a thing that exists, and the existing surface is its INPUT
(the census of what it serves), never its foundation.
⛔ **The ban is unchanged and is the thing to hold onto here** ([AGENTS.md](../../../AGENTS.md) Design;
[DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)): "it kind of exists already" is NOT licence
to widen, extend, or build on a `Cy*` binding — that reflex is exactly how the engine gets shoehorned to fit
Python. New surface, old one disconnected whole.

### ⚑ BUILD IT FOR THE PEDIA — the pedia IS the completeness oracle (owner)

**"Since the pedia shows everything, if we build the reader surface for the pedia we know we have everything."**
That is the build order, and it is a structural argument rather than a convenience: the pedia's whole PURPOSE is
to display every entity exhaustively — every type, every section, every effect family, every relationship — so
it is not a sample of the info surface, it **is** the info surface rendered. Any datum the model carries must
reach a pedia page. Therefore a library that serves the pedia completely has covered the info plane **by
construction**, instead of covering the cases a rewiring sweep happened to touch.

Consequences for stage 4:
- **The pedia surface is built FIRST**, and its read-map ([pedia-map.md](pedia-map.md)) is the library's
  primary specification — not one consumer among many.
- **Completeness is then a PROOF, not a claim** ([DEC-all-means-all](../../architecture/decisions.md#dec-all-means-all)):
  serve the pedia, and every other info consumer is reading a subset of what already exists.
- **⚑ VERIFIED, and sharpened by the census ([python-read-map.md](python-read-map.md), 25,017 call sites over
  208 files): the pedia is a perfect SHAPE oracle, but NOT a coverage oracle.** The distinction is the whole
  ruling's operative half:
  - **SHAPE — complete by construction.** Nothing anywhere in Python needs a payload shape the pedia does not
    already force. So the library's STRUCTURE is settled by serving the pedia; no later consumer introduces a
    new kind of read.
  - **COVERAGE — an enumerated appendix.** The pedia never asks for **293 INFO names / 1,632 sites**, of which
    the real appendix is **59 whole info types that have NO pedia page** (map-gen, game-config,
    diplomacy/victory/vote, command/UI-action) plus **129 per-field reads**. These need serving; none needs a
    new shape.
  - **AND the pedia is 99.7% a static reader**, exercising 1.4% of STATE, 0.1% of COMPUTED, 0% of MUTATION —
    so **54% of the Python surface sits in planes it never touches** (STATE 39% · COMPUTED 10% · MUTATION 6%).
    The pedia strategy completes the INFO plane (35%) and the shapes; it does not complete the boundary.
- **TEXT is a FIFTH read-kind, and ⛔ the library does NOT own it (owner ruling).** Census: 2,578 sites (10%),
  `getText` alone 2,341 — key→string localization, not info data. **The reason is decisive: TXT and ART keys are
  NOT MIGRATED.** Both remain XML-side systems the JSON only REFERENCES — an entity's `identity` carries
  `TXT_KEY_*` references ([json.md §7](../../specs/json.md); ruling 6: text is TXT_KEY references, never
  evaluable data) and its `ui`/`world.art` blocks carry `ART_DEF_*` tag ids resolved by `ARTFILEMGR`
  ([naming.md](../../specs/naming.md): `ART_`/`EFFECT_` are XML-only Types). So TEXT/ART is an **unmigrated
  system boundary, not a hole in the library**:
  - the library serves what it owns — already-localized RENDERED lines (the DLL-side renderer resolves through
    the existing text system) and the raw KEY REFERENCE where a consumer wants the key;
  - resolution stays with the existing managers; Python screen chrome keeps calling `getText` directly;
  - a TXT/ART migration is its own future work item and is NOT a stage-4 prerequisite. (Distinct from ruling 30,
    which parks minting NEW TXT keys for the family/kind/predicate VOCABULARY until the implementation works.)
  - ⛔ **ART is additionally HANDS-OFF (owner): we leave art alone** — including art that falls orphan when a
    consumer is deleted (the `INTERFACE_DEBUG_SCREEN_BUTTON` define left by the TestCode removal is inert and
    STAYS). Roadmap § Scope decisions.

**`TestCode.py` is DELETED, not migrated (owner ruling: "nuke testcode — if we want that we do it properly";
the Python refactor makes it worthless).** It was a DebugScreen tab registering 50 mod-data consistency checks
(requirement/obsoletion ordering, explicit-replacement integrity, replacement quality, cross-entity coherence),
read-only, and the tree's second-largest engine consumer (~2,080 sites) purely because it walked everything —
which is why it was the SOLE consumer of ~90 info names and why its removal shrinks the census appendix ~30%.
The whole feature chain goes with it (`DebugBtn` → `showDebugScreen` → `DebugScreen` → `TestCode`); the shared
`HelperFunctions.py` STAYS (the pedia uses it). ⚑ **The 50 checks encoded real design invariants that the JSON
spec does not currently state** (a requirement may not unlock after the thing requiring it; replacements are
explicit, never implicit; a replacing entity must be better). Those invariants belong in the SPEC first. Not a
stage-4 item.

**Acceptance:** the pedia read-map is the primary tick-list, the census residue its appendix; the legacy `Cy*`
info surface is disconnected WHOLE in the same work item that completes the library — never before it (a gap),
never long after it (two live surfaces).

29. **Per-entry text output, combat-calculator style (owner: "so that tooltips work properly")** — every
   compiled entry renders as one localized detail line via ONE shared renderer (the
   `CvCombatModel::computeCombatPreview` `detailLines` pattern); tooltip/pedia composers consume rendered
   lines. Structural change: `CvModifiers` retains the COMPLETE entry list (unconditioned entries kept as
   entries; folded sums stay the fast plane) — also closes the wellbeing attribution flag.
   **LANDED (renderer + list contract):** the entry list was verified COMPLETE (the parse walk always
   retained unconditioned entries; no drop existed to undo), and the folded `(family, kind, scope, unit)`
   sums are now DERIVED from the retained list at compile end (`CvModifiers::finalizeCompiled` — one
   derivation, list → sums, clear-first idempotent; the inline per-entry folds in `parseLeaf`/
   `landReverseEntry` are gone, so no second fill can drift). The conditioned ranges stay a family-sorted
   view over the CONDITIONED SUBSET — the conditioned-tail walkers never see unconditioned entries.
   **The renderer is `Sources/UI/CvEntryText.{h,cpp}`** (`entryDetailLine(const CvModEntry&)` +
   `entryConditionText(const CvCondition*)` — no prior condition-to-text existed anywhere): sign+magnitude
   ÷100 at the out boundary, TXT-reachable names via the UNCACHED `gDLL->getObjectText` read (channel
   families → Yield/CommerceInfo keys, the property plane → PropertyInfo, FK targets/presence atoms/per
   types → the referenced info; the wellbeing pair renders the game's icon idiom), everything else honest
   segment spell-back (the closed family/kind/member vocabulary and the predicate spellings have no TXT
   keys; `cascadeSpellPredKind` sits beside the parse table in `CvJsonConditionParse` so the vocabulary has
   one home). **Observability proof:** each `[READJSON/mod]` sample line carries its `rendered=` detail line
   (SFT_WSTR field, `CvReadJson.cpp`). **Stage 4 remains:** rewiring the `CvGameTextMgr` composers onto
   rendered entry lines — census: ~15,000 lines / ~1,450 hand-assembled `getText` blocks across the 18
   info-help composer families (`setBuildingHelp` 2805/269, `setBasicUnitHelp*` 2134/224,
   `parsePromotionHelpInternal` 2071/221, `parseCivicInfo` 1555/158, `parseTraits` 1493/179, the rest
   smaller).

30. **Vocabulary TXT keys wait until it all works (owner):** the renderer's spell-back fallback is the accepted
   output through implementation; the localization pass (one TXT key per family/kind/predicate/token) is
   sequenced AFTER the stages complete — polish on a working machine, never before it.

## ⚑ STAGE 2 — the exemplar sweep (adversarially re-audited: 26 of 33 in-scope types verified on the exemplar)

The four waves put 26 of the 33 in-scope types on the exemplar (verified per-header, not asserted); the
post-audit remediation wave completed the rest — `CvVictoryInfo` / `CvVoteInfo` / `CvHurryInfo` /
`CvLeaderHeadInfo` rebuilt whole (the vote/victory boolean blocks and the leaderhead scalar sprawl collapsed
onto typed units + enum-parameterized group reads), `CvCivilizationInfo` / `CvBuildInfo` finished, and
**`CvWorldInfo` cut over to the JSON path** (`curate_world.py` + `Assets/Data/worlds/` + the `CvInfo` poco +
the `LoadGlobalClassInfoJson` line; `WORLDSIZE_` row added to naming.md) — 33/33 in scope, the world orphan
closed. Shared surfaces reconciled census-true (scope-aware
unit verdicts, family-scoped target tokens, the firstStrike-chance census hole and the invisible boolean
containers fixed), six-way census↔`CJK_FAMILY_KEYS`↔`CvInfoKinds` machine-diff EMPTY at 69 families (re-run
fresh at the audit; 69 named + the open `PROPERTY_*` plane). Curator
commits through `b7a72fdd0`. **`revolution.distanceMod` is NOT dead (owner catch):** Revolutions is
Python-authoritative and `Revolution.py:1170` consumes the distance mechanic via the player/city aggregates
(`getRevIdxDistanceModifier` + `getRevIndexDistanceMod`) — invisible to the engine-read census. Both distance
kinds STAY AS-IS, untouched by any stage (owner): Revolutions is due its own rework in the not-too-distant
future — that rework owns every revolution-data question, including the two-spelling nuance. No stage-4
investigation. The `CvDepositIndex`/`CvEventSpine` references to the archived `CPK_*`/
`PSC_*` package enums are the PACKAGE-GRAFT seam — stage 3's package rebuild owns them. Stage-4 consumer debt
ledger: ~4,000+ sites censused across the waves (scratchpad JSONs). The closeout batch LANDED: the keyed-container raw read retired onto compiled targeted entries, the improvement
defense read scope-aware, `readPrereqNatureYield` retired onto the parsed atom, the handicap point-read collapse
+ direct AI-audience reads, and the flanking mis-row fixed (family target token; the member row + `COMBAT_FLANKING`
kind removed — the closeout's specified edit, applied at the vocabulary window). The stage-2 SEAL is retracted
by the adversarial audit (ledger below). Known
still-pending data items (flagged in-code, awaiting their rulings/batches): `culture.unit.garrison`,
`costs.empire.perInstance`, the ruling-16 trigger-plane set. Pre-existing engine-repair debt (green-up stage):
the bare Engine includes, `CvOutcomeMission::mapFrom`, the property-manipulator helpers, `CvCity.h`'s functor
row, the enabler's missing Engine grafts (`CvCity::m_operatingBuildings`, `CvTeam::m_cascadeTeamCaps`).
**Stage 3A LANDED (the package plane):** `Sources/Cascade/` — the uniform `CvCascadePackage` on the 64-bit
`CvDerivedCacheSet`, grafted per the origin rule (area = per-player slots); the channel registry minted from
compiled deposits at push (KEYS ONLY WHERE NEEDED, wellbeing sign twins, observable load census); the mark
derivation from the index incl. CONDITION-DEPENDENCY routes (no hand-wired event masks anywhere); the
modifier's OWN load-active consumer; the ONE gather + the §2a `cityRate100` combine in the calc surface;
the archived `CPK_*`/`PSC_*` regions replaced (CvDepositIndex 29 baseline errors →
0). Named seams for the consumer stage: the §2a trade-route input fold, slider split, golden-age gating, the
specialist own-percent layer. **Logged gap:** game-option flips carry no DOMAIN event — a mid-game toggle would
not re-mark; an emit endpoint is the fix if/when WorldBuilder option toggling is in scope.
**Audit deviations (3A, in the ledger):** five hand-wired blanket masks in `CvModifierConsumer` beside the
route-derived path; a missed invalidation (plot-owner change marks no working-city receiver sums); the plot
PERCENT side is filled but read by no combine leg; the empire/team FLAT reads in the base tier need explicit
§2a grounding (trait free-city yield / golden age ARE genuine empire flats — verify, never assume the
origin-rule shorthand); receiver-bit positions derive from a growing channel count while routes CACHE their
masks (aliasing risk).
**Stage 3B LANDED (contexts working with the infos):** all-FORWARDS context growth (zero new stored aggregates
— stores-vs-forwards held); HAVE through the contexts across the evaluator + the five enabler gate seams (the
deliberate keeps reasoned: pass-1 traversals, game-scope, the future unit scope); the plotAttrs load-reseed
MECHANISM built (the contexts' own consumer, buffer-then-drain-at-LOAD_FINISHED — the §7.1 apply-once option);
the traded leg through CvPlotGroup on the eval ctx; the substrate valuation leg (`plotOwnYield100` + the §2
`plotBaseYields100` plot-as-base calc + `expectedPlotYields` extended); TWO real corp mis-models fixed
(`IS_HEADQUARTERS` absent → HQ revenue everywhere; `per:"CORPORATION_LEVEL"` → 0); the two-pass-in-scenarios
grep-proof holds — though VACUOUSLY on both legs (the `expected*` endpoints have no callers yet; the enabler
leg passes no context across a boundary — the fill seams are called inline on the bound objects), and two
hand-built eval-ctx sites in the grants engine bypass the seams. **Stage 3 is STRUCTURALLY COMPLETE; the audit
ledger below carries the holes.**
**The DOMAIN emit surface is WIRED (verified in tree).** The clean-slate revert had severed every emit call
site engine-wide; the re-wiring is done — **138 `emit*` call sites** across `CvCity` / `CvPlayer` / `CvPlot` /
`CvUnit` / `CvGame` / `CvTeam` / `CvArea` / `CvMap` / `CvPlotGroup` / `CvProperties`, and the load bracket
fires at BOTH ends (`emitGameLoadStarted` in `CvGame::read`, `emitGameLoadFinished` at the end of
`onFinalInitialized` — ledger item 2). So `spineGameLoadInProgress()` reports correctly and `ContextConsumer`
buffers the in-read facts instead of discarding them.
⚠ **Why this paragraph is worth keeping rather than deleting: the failure mode it describes is real and
asymmetric.** Restoring only the FINISHED emit fires the drain against an EMPTY buffer — `plotAttrs` stays
empty with the real cause invisible — so if the bracket is ever touched, BOTH ends move together.

## Sweep status (stage 2)

- **Wave A LANDED (unit plane):** CvUnitInfo / CvPromotionInfo / CvUnitCombatInfo / CvPromotionLineInfo /
  CvSpecialUnitInfo on the exemplar — 835 legacy getter names deleted; §9 vision + sizeMatters as shared typed
  section units; ruling 5 enforced in the reads; ruling 16: no survivor getter. Stage-4 consumer debt: 3,685
  call sites (census in scratchpad, top: CvGameTextMgr 1204 · CvUnit 1048 · CvPlayerAI 901).
- **Wave B LANDED (tech + substrate):** CvTechInfo / CvBonusInfo / CvBonusClassInfo / CvImprovementInfo /
  CvFeatureInfo / CvTerrainInfo / CvRouteInfo on the exemplar — ~40 tech mirrors dead; substrate own-output
  reads off each target's compiled entries; reverse-pass/forward-FK alignments verified; idempotency holes
  (route OR-list, bonus tech-FKs, tech leadsTo) fixed.
- **Reconciliation pass (in flight):** the waves' shared-surface findings — census-driven `infoKindUnit` row
  corrections, `MEMBERS_FIRST_STRIKE.chance`, combat keyed-container target tokens, CvReversePass
  improvement-route sub-pass deletion (writes a deleted mirror), COMMERCE PLOT mask bit, the `natureYield`
  requires atom, the `defense.plot.air` unit verify, and the curator micro-fix dissolving the
  `tradeRoutes.routes` kind/target collision (re-author memberless; the `routes` kind dies).
- **Option-gated values: answered by the spec, not a question** — json.md §9: a game-option system gates AT THE
  CONSUMING SYSTEM; the info serves ungated data (patterns.md: an info never reads game state). Gates move with
  the stage-4 consumers.
- **Wave D LANDED (config types):** the ten config/world types on the exemplar — handicap deep-cut (471 leaves,
  0 mismatches vs legacy math; explicit `bAiAudience` two-leaf reads; ruling-24 config renamed
  `getUnitUpkeepEraModifier`), gamespeed's option-reading `getHammerCostPercent` killed (§9: option gates live
  at the consuming system — the one calc lands as `InfoValuation::hammerCostPercent` at stage 4), leaderhead
  traitless per the roadmap ruling, era/victory/vote undataed members deleted census-grounded, property
  VERIFIED-untouched (LOCKED). Stage-4 debt: ~270 sites (gamespeed 143, handicap 90).
- **Wave D finding F1 — LANDED (the property KEEP-legacy feed restored):** the clean-slate revert had severed
  `CvPropertyManipulators::{clear, addConstantSource ×2, addDecaySource, addAttributeConstantSource,
  addDiffusePropagator}` while `CvPropertyInfo::mapFrom`'s source bridge still called them (property-audit
  increments A/B/4). The five are back with their `BoolExpr`/`IntExpr` forward decls and the `IntExpr` include,
  and the destructor now delegates to `clear()` rather than repeating the delete walk. This is REVERT REPAIR on
  an owner-LOCKED KEEP surface, not a legacy revival: the property engine's internal math is explicitly KEEP
  ([north-star.md](../../architecture/north-star.md)), and the revert overshot on this seam.
- **Wave C LANDED (city-adjacent types):** the nine types on the exemplar — ~95 mirror getters dead;
  corp/religion carry NO point getters by census construction (all entries conditioned — the what-if is the
  read); specialist's in-getter game-option read killed (§9: gates live at consumers); heritage prereq reads
  moved onto its own EDGEF_RELATED lists; specialist `identity.categories` parsed for the first time (authored
  39/39, never read). Reconciliation queue grew: corp maintenance descale (raw ×100 in JSON — §3.6 violation),
  the shrine/HQ registry feed sub-pass, the ruling-16 comment set.
- **Process conversion rows — OWNER CONFIRMED:** processes are hammers→commerce CONVERSION (the idle-production
  fallback), never deposits — re-homed to the [json.md §9](../../specs/json.md) `conversion` section (hurry's
  existing home); `CvProcessInfo::getProductionToCommerce` re-points to the typed section. In the
  reconciliation pass's curator work.

## The rebuild sequence

1. The toolkit (reader consolidation + interning + vocabulary + rename sweep) — LANDED.
2. `CvBuildingInfo` rebuilt to the anatomy (the proven pattern — fattest, both defect and cure historically) —
   **LANDED to the full exemplar surface** (sections · hold-vs-provide classification reads
   (`hasAttribute`/`providesCapability`) · census-grounded per-group point reads over the compiled sums ·
   the conditioned-list access · the `expected*` endpoints · the census identity set as bare typed members
   incl. the shrine/corp-HQ FKs).
3. `CvCivicInfo`, then `CvTraitInfo` (trait reads are active-set-selected — [modifier.md §4](../../specs/modifier.md)) —
   **LANDED to the same surface** (civic: `providesPolicy` + cityLimit/anarchyLength/upkeepLevel/civicOption/
   weLoveTheKing intrinsics + the ruling-26 `hasCityOverLimitAnger` derivation; trait: `providesPolicy` +
   the golden-age member-mirror reads + negativeTrait/impure*/min-maxAnarchy intrinsics — MMKernel's pure
   filter now reads `isNegativeTrait()`). The `expected*` endpoints + the conditioned-list access are declared
   ONCE on the base `CvInfo`, delegating to the ONE calc unit (`Data/CvInfoValuation` —
   [DEC-single-implementation]); the eval ctx fills through the one seam (`InfoValuation::fillEvalCtx` =
   `CityContext::fillEvalCtx` + `EmpireContext::fillEvalCtx` + the enabler's wired operating set).
4. Docs current in the SAME changes: [engine.md § Info loading](../../reference/engine.md) rewritten to the one
   reader when it lands; patterns.md + the ledger already carry the contract.

## The getter aim — settled; full consumer rewiring stays roadmap-sequenced

The EXEMPLAR getter setup every rebuilt info aims at is settled and lives in
[patterns.md § THE GETTER SETUP](../../architecture/patterns.md): sections as whole typed objects ·
classification bitset tests (hold-vs-provide names) · per modifier group THREE reads (the straight point read
over the load-compiled unconditioned sum, the typed conditioned-entry list, the contexts-taking `expected*`
what-if valuation) · bare intrinsic reads + `getScalar(SCALAR_*)` stragglers. No point getter over a
hand-maintained pre-summed MEMBER exists — a point read is the compiled slot fetch, never a second store. The
rebuilt infos implement this shape NOW; what stays roadmap-sequenced is the ENGINE-WIDE consumer rewiring onto
it ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).

## Acceptance

- Grep census: exactly ONE `picojson::parse` call site under `Sources/` (the reader).
- A full load reports **0 unconsumed keys** across `Assets/Data` (the fail-loud coverage).
- `/state/info?type=X` matches the authored JSON for every rebuilt type (the standing loaded≡authored check,
  [validation.md](../../specs/validation.md)).
- `Assert build` green at each step; no runtime string-keyed read anywhere on the rebuilt surface
  ([DEC-materialize-at-mapfrom](../../architecture/decisions.md#dec-materialize-at-mapfrom)).
- After load, no read walks the anatomy or any parse structure — the straight asks (point reads over compiled
  sums, edge lists, the enabler's frontier vectors) are 0-calculation fetches; the only evaluated thing is the
  conditioned tail, at event-driven rebuild + per-decision `expected*` cadence.
- No legacy getter name reappears on a rebuilt info ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).

## ⛔ The adversarial audit ledger — stages 1–3 re-verified (the completion claims retracted where falsified)

An eight-way adversarial audit (one-reader law · legacy-getter intersection vs `SourceArchive/Infos` ·
per-type exemplar conformance ×3 · contexts · package plane · fresh census machine-diff) re-verified the
stage claims against the working tree. **Verified CLEAN with evidence (no action):** the one-reader core
(exactly one `picojson::parse` site, store freed at postmenu end, unconditional fail-loud coverage) · the
fresh six-way vocabulary diff (EMPTY at 69 named families + the open `PROPERTY_*` plane; retired keys
zero-authored; no ×100 value leaks — the corp-maintenance descale holds in data) · the modifier-group getter
surface on the rebuilt types (parameterized `(kind, scope)` point reads over compiled sums; no per-channel
hand getters; the dead legacy array getters genuinely gone) · cross-entity ownership via the reverse pass ·
the ONE-valuation delegation (`CvInfo` → `InfoValuation`, zero per-type re-implementation) · trait active-set
selection by option, `isNegativeTrait()` in the pure filter · rulings 24/26 realized as specced · the
evaluator's HAVE-through-contexts (every remaining direct read classifies to a named keep) ·
stores-vs-forwards (one aggregate per context; `tradedBonusCount` verified a forwarding relay) · the 3A
package structure (one uniform type, all five grafts bound in `reset()`, data-minted channel registry, one
gather + one combine, wellbeing sign-twins routed at fill, zero `CPK_*`/`PSC_*`/`ensure`-protocol remnants).

### Remediation worklist

| # | finding | home |
|---|---|---|
| 1 | **LANDED** — `CvTechInfo::mapFrom` no longer clears `m_leadsTo`; the reverse pass's populator is its single owner. Original finding: the clear ran AFTER `doPostLoadCaching` (its only populator) → `getLeadsToTechs()` empty for the whole game | `CvTechInfo.cpp` |
| 2 | **LANDED** — the load bracket is emitted at BOTH ends (`emitGameLoadStarted` in `CvGame::read`, `emitGameLoadFinished` at the end of `onFinalInitialized`), so `spineGameLoadInProgress()` reports correctly and the contexts' consumer buffers the in-read facts instead of discarding them | `CvGame.cpp` |
| 3 | **HALF LANDED — the multiplier half.** The SM group geometry (`3^(groupRank−1)`) now has ONE derivation, `smGroupMultiplier(iGroupRank)` beside `applySMRank`, and all six sites use it (`CvUnit.cpp` ×5, `CvPlayer::getUnitCountSM`). ⚠ The defect was NOT the "divides by zero" the original finding named — `intPow(3,−1)` returns **0**, so nothing divided by zero: the read side already clamped with `std::max(1,…)` while the four WRITE sides did not, so a rank-0 unit **contributed 0** to the count and was then divided as if it were 1. One quantity, two clamping rules, disagreeing silently; the clamp now lives in the shared function and nowhere else. **The derivation half's premise is FALSE, and the real defect is next to it.** `CvUnit` does NOT re-derive the rank: `CvUnit.cpp:18342` copies the info's single derivation (`setGroupBaseTotal(m_pUnitInfo->getBaseGroupRank())` — the code even says "never re-derived from the single class in hand here"), and `groupRank()` = that base + `getExtraGroup()`, which IS json.md §9's model (derived info base + held-promotion changes + the merge/split accumulators). One derivation, correctly. ⛔ **What IS wrong: the three copies are SERIALIZED** — `m_iQualityBaseTotal` / `m_iGroupBaseTotal` / `m_iSizeBaseTotal` carry `WRAPPER_READ`/`WRAPPER_WRITE` (`CvUnit.cpp:19896-19897`/`:20637-20638`) despite being pure copies of immutable INFO data whose only writer is that one assignment. [DEC-derived-never-trusted]: derived data is never trusted from a save. The live hazard is concrete — re-curate the unitcombat `*Base` ranks (or edit the data) and every existing save keeps the STALE base forever, since nothing re-derives it; and the group rank divides the SM unit count, so a stale one silently mis-counts every unit of that type. **LANDED as the soft-remove** ([save.md §3](../../specs/save.md)): the three reads + writes are gone and the tags are named in `savemigration.txt`. ⚠ The verification that shaped the fix: the load path does NOT reach the `processUnitCombat` assignment — `CvUnit::read` writes the combat-class set straight into the keyed map (the reseed-emit comment says so in place), so the stored copy WAS the only load-time source and deleting the read alone would have left every loaded unit at 0. They are re-derived instead from the unit's own info (`getBaseQualityRank`/`getBaseGroupRank`/`getBaseSizeRank`) at the same point in `read`, where `m_pUnitInfo` is already live (set at `:19533`). The runtime deltas riding on top (`m_iExtraQuality`/`Group`/`Size`) are genuine per-unit state and STAY serialized | `CvUnitInfo` + `CvUnit.cpp`/`CvPlayer.cpp` |
| 4 | **LANDED** — no #430 poco carries a cache or a dirty flag any more. The `CvUnitInfo` half materialized earlier (`getEra`/`canAcquireExperience` are bare member reads); the last memo — `CvHeritageInfo`'s lazily-resolved acquisition prereqs (`m_bPrereqsResolved` + two `mutable` members) — is gone, re-homed onto the EXISTING post-map derivation step — a `rp_deriveHeritagePrereqs` sub-pass in `reversePassRun()` calling `CvHeritageInfo::deriveAtRegistryComplete()`, exactly as the unit plane already did. ⚠ The first attempt minted a SECOND mechanism for this (a generic `CvInfo::materializeCrossEntity()` hook driven from `loadJson`) before the existing one was found; it was withdrawn — a new does-the-same-thing entry point is the failure patterns.md's enforcement check names, and the reverse pass is the one home. The rule the episode fixes in the spec: a cross-entity value materializes in the reverse pass, never at `mapFrom` (which runs while the view is still being built) and never behind a first-read memo (a cache + dirty flag on an info, forbidden by construction) — [patterns.md](../../architecture/patterns.md) § Materialize at mapFrom now states it, and [engine.md § Info loading](../../reference/engine.md) carries it in the pass order. The only surviving `mutable`s are the localized-TEXT render caches on `CvInfoBase`/`CvCivilizationInfo` — the unmigrated TXT boundary (ruling 29/30's TEXT split), not an info-contract item | `CvInfo` + `CvHeritageInfo` + the reader |
| 5 | **LANDED** (verified in tree) — `setDisqualifiedUnitCombatTypes`'s line-level loop reads the VALUE (`lineNotOn[i]`), not the loop index, so the disqualified id is the unitcombat it names | `CvPromotionInfo.cpp` |
| 6 | **LANDED** (verified in tree) — `CvSpecialistInfo::getExperience` resolves its unit scope-AWARE: `infoKindUnit(MODFAM_EXPERIENCE, eKind, eScope)`, so the UNIT-scope percent split no longer reads the FLAT slot | `CvSpecialistInfo.h` |
| 7 | **LANDED** (verified in tree) — the reads are typed/materialized: the base `CvInfo` grant/allowed handles key on INTERNED INTS (`allowedCap(EnAllowedCap)`, `grantList(int)`, `grantPulse(int)`, `grantFlag(int)`), and `CvSpecialBuildingInfo::getMaxPlayerInstances` / `CvCultureLevelInfo::getPrereqGameOption` are bare member reads (`m_iMaxPlayerInstances` / `m_iPrereqGameOption`) — no per-call string compare or map walk survives | `CvGrants`/`CvAllowed`/`CvInfo` + callers |
| 8 | **LANDED, and the residue was a different defect.** All three `CvCascadeEvalCtx` sites in the trigger engine already went through the fill seams (`CityContext::fillEvalCtx` + `EmpireContext::fillEvalCtx`) — the hand-assembled raw-pointer ctx is gone. What remained: two of the three (`tr_promoteFromEntries`, the founder-buildings placement) omitted `EnablerKernel::wireOperatingBuildings`, which the third calls. The enabler's precomputed sets are the THIRD LEG of the eval state, fed in rather than re-derived (patterns.md: the active/dormant verdict is the enabler's and the modifier READS it) — so without it the operating-set legs sat EMPTY and any entry condition asking an active-building or vicinity-provides question evaluated against nothing and quietly answered false. Same shape as the 3B `IS_HEADQUARTERS` mis-model. Both now wire it | `CvTriggerEngine.cpp` |
| 9 | **LANDED (verified — the premise no longer holds).** `PlotContext` stores all 13 `CASC_PRED_*` verdicts — the own-plot block (`IS_WATER`/`IS_LAND`/`IS_FLATLANDS`/`HAS_HILLS`/`HAS_PEAK`/`HAS_RIVER`/`HAS_IRRIGATION`/`HAS_FEATURE`/`HAS_LANDMARK`/**`IS_OWNED`**/`IS_WORKED`) plus the adjacency block (**`HAS_COAST`**/`HAS_FRESHWATER`) — and `CityContext::onPlotChanged` folds the bitset GENERICALLY over all 32 bit positions rather than a hand-listed subset, so every stored verdict reaches `plotAttrs` and a newly-stored one needs no fold change. The named omissions (`IS_OWNED`, `HAS_COAST`) are both stored and both counted; item 23 closed `HAS_LANDMARK`/`HAS_FEATURE`/`HAS_IRRIGATION` earlier | `CityContext.cpp` |
| 10 | **PARTLY LANDED — the missed invalidation is fixed.** The working-city half was already covered (`mc_markPlotFedSums` marks `getWorkingCity()`), but the EMPIRE half was half-blind: that helper reads the plot's LIVE owner, so on an ownership flip it could only ever reach the NEW owner, leaving the DEPARTED empire's plot-fed receiver sums stale with nothing to re-derive them ([DEC-no-self-heal]). `SEVT_PLOT_OWNER_CHANGED` carries the old owner in `iA` exactly so a consumer can act on the delta, and the handler now marks it — the same both-sides shape `SEVT_WORKING_CITY_CHANGED` already used for its two cities. ⚑ **Receiver-bit aliasing: NOT a defect — the premise was wrong, verified.** Bit positions are FIXED, not count-derived (`cr_channelBitOf` = `1 << slot` over 0..58; `cr_receiverBitOf` = `1 << (59 + index)` over 59..62; 63 the tripwire), so a mask cached at any point stays valid across later minting and a route cannot alias as the channel count grows. Both sides clamp coarse-safe (over-mark, never under-mark) and assert. **What WAS real is the silence:** `FAssertMsg` compiles out of Release/FinalRelease — the builds actually played — while the registry is OPEN BY DESIGN (json.md §8) and empire already measures ~50 of 59. So the budget is now checked UNCONDITIONALLY at the channel census (`[CASCADE] ERROR channel-bit-overflow scope=… slots=… budget=…`, no gate, every load), the same fail-loud shape as the readJson coverage counts; the census itself is DIAGNOSTIC-gated and would have shown nothing in a shipped run. ⚑ **The five blanket masks are DONE as specified (verified):** the file header states the governing rule — the only non-derived masks are RULED blankets, each carrying its justifying constraint at the case — and every site does: scope-object LIFECYCLE (`SEVT_CITY_FOUNDED`), ownership COMPOSITION (`SEVT_CITY_OWNER_CHANGED` + `mc_markEmpireWhole`), the plot-substrate whole-package refill, and the golden-age engine member-mirror. All four classes are genuinely not deposit-addressed, so no route exists in the index to derive — which is the stated test, not an excuse attached after the fact. **Both VERIFICATION tasks now DONE, both pass.** (1) The plot-percent leg IS read and IS §2-aligned: `InfoValuation::realizedAtPlot` resolves the plot's flat AND percent in isolation as one base package, and deliberately excludes every upper-scope percent — those apply once, later, to the city's already-summed base, so reading them here too would scale the same magnitude twice. The city/empire roll-ups correctly exclude the plot in turn (modifier.md §1: a plot never enters an upper chain). (2) The empire/team FLAT reads in the base tier are §2a-grounded and correct: `rolledLegsAtCity` accumulates team + empire + city flats, which is what §2a TIER 1 requires — trait free-city yield and golden-age yield are genuine EMPIRE flats. ⚠ This is exactly where the origin-rule shorthand ("modifiers come from everything but plot") would have misled if applied to flats: it governs the YIELD/RATE plane only, and the non-output families author flats at empire freely. **Item 10 fully closed** | `Sources/Cascade/` + `CvDepositIndex` |
| 11 | **LANDED** — the roll moved to the consumption site (`CvMapGenerator::calculateNumBonusesToAdd`). The info now serves the authored WEIGHTS only: `getConstAppearance()` + `getRandAppearance(iBand)` over a `m_aiRandAppearance[NUM_RAND_APPEARANCE_BANDS]` array (the grouped-storage/parameterized-index shape, replacing four hand-named members). ⚠ The defect was not untidiness: a `const` info getter silently drew from the map RNG, so the number of draws depended on how often it was CALLED — an OOS hazard under deterministic lockstep, and it was **exposed to Python** (`CyInfoInterface2`), where any pedia/screen read would have shifted the map stream. That `.def` is removed rather than re-pointed ([DEC-cy-not-fixed]: the `Cy*` info surface is cut, never maintained). Per-band RNG log tags preserved | `CvBonusInfo` + consumer |
| 12 | **LANDED** (verified in tree) — a second multi-member OR-group beyond the single-Or-list forward view now raises `FErrorMsg` **and** a `[READJSON] ERROR or-group-skipped tech=… class=… members=…` line, for the `TECH_` and `BUILDING_` classes alike; the clause itself stays gated by `requires.build`, so the skip is to the VIEW only and is never silent | `CvTechInfo.cpp` |
| 13 | **PARTLY LANDED.** (a) The shrine-registry feed is now CLEAR-FIRST (`CvReligionInfo::clearShrineBuildings`), and the comment claiming the vector "starts empty because the repos are cleared" is gone — that made correctness hinge on whether a repo clear destroys the poco or merely resets the list, which is not something a sub-pass should depend on either way. (b) `CvBuildingInfo::mapFrom` no longer early-returns when the `identity` block is absent: it reads the block against an EMPTY object so every one of the ~22 identity members takes its default, instead of the previous pass's value standing. ⚠ Both matter because `loadJson` runs its full pass TWICE by design (premenu + the postmenu full-registry re-map), so every `mapFrom` and every `rp_*` sub-pass runs at least twice on the same poco — that, not events, is why the idempotency contract exists. (c) The clear-list gaps are closed: `m_szArtDefineTag` now clears first on bonus / feature / improvement / terrain (+ feature's `m_szEffectType`) and `m_szSound`/`m_szSoundMP` on tech — `jsonIdStr` only assigns when the key is PRESENT, so an absent block left the previous pass's string standing. Verified already-clean and left alone: promotion / religion / corporation `m_szSound`, unit / civilization / leaderhead `m_szArtDefineTag`, promotionline `m_bBuildUp`, the special-unit bools. (d) `CvSpecialistInfo::mapFrom` reset its own members BEFORE the base call; reordered to base-first, the documented order every sibling follows (functionally harmless — the base never touches a subclass member — but the deviation invites a wrong assumption). **Item 13 COMPLETE** | various infos |
| 14 | Six types onto the exemplar: `CvVictoryInfo`/`CvVoteInfo`/`CvHurryInfo` (rebuild), `CvLeaderHeadInfo` (rebuild), `CvCivilizationInfo`/`CvBuildInfo` (finish) | `Sources/Infos/` |
| 15 | `CvWorldInfo` cutover: curator + `Assets/Data` folder + poco on the exemplar + the JSON load path (the XML stays curator input, [DEC-no-xml-into-game]) | curator + `Sources/` |
| 16 | **LANDED — and it was not "lift a duplicate to a shared surface".** `jsonReadFkMap(parent, key, map<int,int>&)` ALREADY existed on the shared surface and had been re-implemented twice beside it, byte-for-byte: `vs_readIntensityMap` (`CvVisionSection.cpp`, 4 call sites) and the inner half of `un_readKeyedIntMap` (`CvUnitInfo.cpp`). That is the precise failure patterns.md's enforcement check names — *"a new does-the-same-thing function is the failure — reuse the existing one"* — and a file-`static` copy is the DRY hazard the law calls out, because the next consumer cannot see it and writes a third. The vision copy is deleted (its four sites call `jsonReadFkMap` directly); the unit one keeps only its extra section hop and delegates the read. Also verified NOT duplicated: `jsonReadIdList` / `jsonReadKeyedBoolIdList` / `jsonReadFlavours` have single homes | `CvJsonParse` + callers |
| 17 | **LANDED** — the one surviving play-time offender, `EnablerKernel::jsonFor`, is now `infoFor` (it returns a `CvInfo*` by edge bucket; nothing about it is JSON), renamed across the kernel, the reverse pass and its `CvPlayerAI` caller. `InfoRepo.h`'s `JsonPayload` trait no longer exists. Zero `jsonFor`/`JsonPayload` remain. The `Json` fragment now survives ONLY on the load-time parse surface where [DEC-one-json-reader] reserves it (`loadJson`/`JsonLoadPhase`/`CvJsonParse`/`LoadGlobalClassInfoJson`) | enabler / repos |
| 18 | **PARTLY LANDED** — `upkeep.civicOptions.{CIVICOPTION_RELIGION}` re-authored from the bool `enabler` to the magnitude that states it (`percent: -100`; curator mapping + the one hand-maintained trait file, traits being content-LOCKED). ⚠ counterDamage is **13 authorings, not ×6**, and needs an owner ruling: its `chance` is an on-attack roll, i.e. ruling 16's trigger-plane class, but re-homing needs an `onAttacked` happening AND a `damage` action verb that do not exist. Original finding: unkinded beyond every sanctioned list: `defense.counterDamage` (the `counterDamage.units.unitCombats` membership-list shape compiles no entries) · `upkeep.civicOptions.enabler` ×1 (trait_spiritual3) — curator/vocabulary triage | data triage |
| 19 | **LANDED.** (a) `getUpgradeChain()` IS populated — `deriveAtRegistryComplete` fills `m_aiUpgradeChain` (the transitive `upgradesTo` closure) in the reverse pass's post-map step; the finding predates that pass. (b) The promotion `sizeMatters` deltas were HALF consumed: json.md §9 names four — `quality`/`group`/`sizeModifier`/`maxHP`, all "applied as changes when the promotion is gained" — but only `quality` and `group` reached their appliers. ⚠ **A live defect, not future-proofing: 10 shipped promotions author `sizeMatters.maxHP`** (the self-heal line, +1/+2 …) and it was parsed and then read by nothing, so those promotions granted no max HP at all. Both now apply (`changeExtraSize` / `changeExtraMaxHP`, the latter outside the SM-recalc flag since max HP is not a rank). `sizeModifier` has 0 authorings today and is wired for symmetry with the spec'd set | unit plane |
| 20 | **LANDED.** Both post-load info mutators are gone: `CvGame.cpp:308-317` (halving great-general power on the SHARED `CvUnitInfo` under `!GAMEOPTION_UNIT_GREAT_COMMANDERS`) and `CvUnitInfo::setMilitaryWorth` (zero callers). An info is loaded once and shared, so a post-load mutation is a contract breach by construction — and this one was cumulative across re-inits (a second game in one session would have been ÷100). ⚠ **Correction to the previous entry, recorded because the mistake is instructive:** it specced a gated calc (`InfoValuation::unitPowerValue`) plus five call-site rewires. That was wrong — `getPowerValue`/`setPowerValue` are declared **NOWHERE** on any info, so the block was DEAD CODE calling non-existent methods (the `setQualifiedPromotionTypes` class), there is no member to gate and no calc to build. The error came from verifying the mutation SITE without verifying the DECLARATION. **What survives as real work:** the unit power-value plane was removed from the rebuilt infos and its ~9 readers (`CvUnit.cpp:402`/`:27632`/`:27638`, `CvPlot.cpp:4013`, `CvUnitSort.cpp:80`, `CvTeamAI.cpp:2157`, `CyInfoInterface1.cpp:205`, and the building/tech twins) are ordinary stage-4 consumer debt — dangling call sites on a deliberately red tree, NOT defects to repair on sight | `CvGame` + `CvUnitInfo` |
| 21 | **RENAME HALF LANDED; the AUDIT half needs a method that is not grep — recorded because the naming ruling removed the very marker an audit would search for.** Verified: `sum100` / `getScalar100` / `modifier100` / `expectedModifier100` / `grantPulse100` / `propertySum100` / `gameStartBase100` have ZERO occurrences — the new surface carries no scale in any name. The `*Times100` names that remain (`CvCity`/`CvArea`/`CyCity` — `getMaintenanceTimes100`, `getCommerceRateTimes100`, `getCultureTimes100`, `getEffNumAIUnitsTimes100`, …) are LEGACY surface on the 622-declaration DELETION list; renaming them would be maintaining legacy rather than cutting it, so they are deliberately untouched and die with their surface. ⚠ **The ×100-ALGEBRA AUDIT cannot be completed by name search, and that follows from the ruling itself:** every value is now ×100 and NO name says so, so there is no token to grep for — a mechanical two-×100-multiply search returns zero and proves nothing. The audit must instead run against the KNOWN ×100 surfaces (the calc functions' documented inputs/outputs — `InfoValuation`'s combines, `CvCascadePackage` slot reads, the `CvModifiers` sums), checking each multiply site's two operands against those declarations. **The semantic pass over the calc surface is now DONE** — every multiply on `InfoValuation` walked and each operand checked against its documented scale. Verdict: **no live scale bug, one LOADED TRAP fixed, two implicit contracts made explicit.** (a) `tradeRouteChannelYield` added a PLAIN engine identity percent (`CvYieldInfo::getTradeModifier`, 100/0) to a ×100 deposit sum (`getTradeRouteYieldModifier`). It has ZERO callers today — it is the §2a seam built ahead of the package graft — so nothing is wrong yet, but the first caller would naturally pass the ×100 sum it has and read a +50% modifier as +5000%. Fixed STRUCTURALLY rather than by warning: the function now lifts the base to ×100 and takes both down with one ÷10000, so a caller passes what it holds and cannot get the scale wrong. (b) `cityRate` takes a PLAIN percent while its neighbour `realizedChannel` takes the ×100 sum and de-scales internally — correct in both cases (the single `cityRate` caller pre-divides) but a genuine asymmetry between two functions on one surface; both contracts are now stated at the definition, as `commerceSplit` already did. (c) Verified same-scale and correct: the entry `value × predicate count`, the slider share, the process conversion (`hammers × ratePercent` IS the ×100 commerce), and the city-limit world scale. ⚑ **The method is the deliverable, and it has a sharper form (owner):** the trade-route fold is not merely a place two surfaces happen to meet — it is **THE EDGE**. `tradeYield` is the ONE sanctioned live-yield INPUT (modifier.md §2a): the cascade cannot re-derive the trade NETWORK, so that calculation stays engine-owned ([north-star.md](../../architecture/north-star.md) KEEP — it is none of the four systems' job) and its value is FOLDED IN rather than computed. That is exactly why the scales differ there and why the conversion belongs THERE: **an edge converts**, as readJson converts at the IN boundary and a reader ÷100s at the OUT boundary ([DEC-fixedpoint-x100]). ⇒ **So the audit scope follows straight from [DEC-fixedpoint-x100], which has said it from day one:** ×100 is native EVERYWHERE inside, and human exists at exactly two boundaries. A scale error therefore cannot occur inside the cascade — every magnitude there is ×100 by construction — only where a value crosses a boundary. The audit is an enumeration of those boundaries (the sanctioned engine INPUTS, the readJson IN, the reader OUT), not a sweep over every multiply; both findings landed on one because that is the only place they can. Operands are checked against the DECLARED scale of the surface they come from, never against a name. Original ruling follows.<br>RULED (owner): the `100`-suffix rename sweep — no internal getter/function/member name carries the scale (`getScalar100`→`getScalar`, `sum100`→`sum`, `modifier100`/`expectedModifier100`/`grantPulse100`/`propertySum100`/`gameStartBase100`, the `InfoValuation` calc names, …) — PLUS the mandatory ×100-ALGEBRA AUDIT: **every calculation that multiplies one ×100 value by another ×100 value is FLAGGED and reported** (the product is ×10000; believed zero such sites — verify, never silently rescale). Queued behind the in-flight wave (same files). | surface-wide sweep |
| 22 | **FULLY LANDED — both directions.** The four substrate TYPE facts (`emitImprovementChanged` / `emitTerrainChanged` / `emitFeatureChanged` / `emitRouteChanged`) now carry the DEPARTING value in `iA` beside the arriving one, which is the shape [event-spine.md](../../specs/event-spine.md) already documented as the rule for a type change ("carries the OLD value alongside the new, the plotOwnerChanged shape") and which `emitPlotTypeChanged`/`emitPlotLandmarkChanged` already followed — these four were the deviation, and that deviation was the sole reason the departing type was unaddressable. The consumer routes both ids through ONE shared lookup (`mc_substrateTypeName`), so the two directions cannot drift. ⛔ Deliberately NOT applied to `SEVT_PLOT_BONUS_CHANGED`: that fact carries the placed/removed DELTA in `iB` and leaves `iA` at 0, so reading `iA` as an old id would route bonus id 0 on every placement — its removal case is already covered by its own `-1` delta. The 4 in-read reseed sites pass `NO_<X>` (nothing preceded a loaded value). Without this, a deposit conditioned or per-scaled on the substrate that LEFT kept contributing until something unrelated dirtied it ([DEC-no-self-heal]) | `CvModifierConsumer` + the spine |
| 23 | ~~`HAS_LANDMARK` un-folded in `plotAttrs`~~ **RESOLVED:** `setLandmarkType` now emits a guarded DOMAIN fact (it had NO change guard — `CvGame`'s landmark sweeps re-assert `NO_LANDMARK` across whole regions, so an unguarded emit would have announced a change on every no-op write), and `HAS_LANDMARK` is a stored `PlotContext` bit folded into `plotAttrs`. The 4 option-gated civic `plots` deposits count for real. Same for the `HAS_FEATURE`/`HAS_IRRIGATION` "mutable" carve-outs, which also read 0 forever. | — |
| 24 | **PARTLY LANDED:** an unknown vote `role` now reports through the ONE census (`jsonNoteUnconsumed`) instead of classifying silently to NONE. Remaining: `CvGame.cpp:308-317` mutates the shared unit info through dead power-value names under a game option — the gate moves to the consumer reads when the power plane rewires (stage 4, with item 20) | parse surface / power plane |
| 27 | **LANDED (owner-raised). `cvInternalGlobals::doPostLoadCaching` is DISSOLVED.** The info-member derivations moved into `reversePassRun()` as clear-first `rp_*` sub-passes (`rp_derivePromotionLineMembers` · `rp_derivePromotionQualification` · `rp_deriveTechLeadsTo` · `rp_deriveImprovementAndBonusIndexes`, beside the existing `rp_deriveUnitPlane` / `rp_deriveHeritagePrereqs`), with the clears they needed added (`CvTechInfo::clearLeadsTo`, `CvImprovementInfo::clearBuildTypes`, `CvBonusInfo::clearRuntimeImprovementIndexes`). What remained is genuinely engine-side — GC-owned index lists + the two enabler reverse-indexes + `checkInitialCivics` + the city-output-history sizing — and is now `cvInternalGlobals::buildLoadTimeIndexes()`, **called AFTER `loadJson(JSON_LOAD_POSTMENU)`** instead of at the end of `LoadPreMenuGlobals`, so it reads the fully re-mapped registry. Two DEAD blocks were deleted rather than moved (their setters no longer exist anywhere): the unit `setQualifiedPromotionTypes`/`setCanAnimalIgnores` pair, and the whole trait prereq reverse index — the rebuilt `CvTraitInfo` carries no prereq surface at all, so its consumers (`CvPlayer`, `CvGameTextMgr`) read the edges as stage-4 work ([enabler.md](../../specs/enabler.md) updated: trait prereqs are the deliberate non-reconstructed exception). Also deleted: the `CvInfoBase::doPostLoadCaching(uint32_t)` virtual and its walk — **no info overrode it**, so the loop was a no-op over every info. Two latent bugs fixed in passing: `m_starsigns` was never cleared, and `m_mapBonuses` was appended without a clear. Original finding follows.<br>**`doPostLoadCaching` was a SECOND cross-entity load-derivation pass running at the WRONG TIME.** It fires at the end of `LoadPreMenuGlobals`, but the full-registry `mapFrom` re-run happens at the end of `LoadPostMenuGlobals`, so it derives against an incomplete registry and anything the later re-map clears is silently lost. ⚠ Not hypothetical — **item 1 above WAS this bug** (`mapFrom` cleared `m_leadsTo` after this pass filled it → `getLeadsToTechs()` empty all game), and `CvGlobals.cpp:3378` already concedes the split in a comment ("lives in the readJson reverse pass … this function fires PRE-menu on empty maps"). One job, two homes — [DEC-one-reverse-view] + [DEC-single-implementation]. **The INFO derivations to re-home as `rp_*` sub-passes:** promotion-line members · the all-cities property-source index · promotion qualified/disqualified unitcombats · unit qualified promotions + `canAnimalIgnores` · improvement→bonus `providedBy` · bonus→(improvement,build) trade pairs · the map-bonus list · tech `leadsTo` · the trait prereq reverse index (+ tech→trait) · improvement→builds · the status/starsign promotion lists. ⚠ **Hazard to handle in the move:** `reversePassRun` runs in BOTH load phases, so every re-homed derivation must be CLEAR-FIRST idempotent — several accumulate today (`m_mapBonuses.push_back`, `addTradeProvidingImprovement`, `addBuildType`, `setProvidedByImprovementTypes`) and would double on the second pass; `addLeadsToTech` is safe only because its store is a `std::set`. **Not info work, so NOT part of the move** (they stay engine setup, re-homed to their own call site AFTER the postmenu full pass, never pre-menu): `checkInitialCivics`, `CityOutputHistory::setCityOutputHistorySize`, `buildConstructibilityEnablerIndex`, `buildInvisibleSeerIndex`, the GC-owned index lists (`m_mapBonuses`, `m_aiStatusPromotions`/`m_starsigns`, `m_allCitiesManipBuildings` — engine indexes OVER info data, not info members, so they need GC-side clears rather than a reverse-pass home), and the legacy `CvInfoBase::doPostLoadCaching(i)` virtual walk. ⚠ **Further evidence the pass is STALE, not merely mis-placed:** it calls `CvUnitInfo::setQualifiedPromotionTypes()` and `setCanAnimalIgnores()`, **neither of which exists any more** (`CvGlobals.cpp:3283-3284`) — it is still reaching for a surface the info rebuild replaced. ⚑ **This is NOT the event-driven rule** ([DEC-spine-reseed]/[DEC-no-self-heal]), which governs derived GAME state: an info is write-once-at-load and immutable, so there is no mutation to emit an event about — the fix is one load-time derivation home inside the ONE reader, never an event | `CvGlobals.cpp` → `Data/CvReversePass.cpp` |
| 26 | NEW (verified): the `savemigration.txt` PARSER is PREFIX-FREE — `sm_ensureLoaded` skips only `\|`/`=`/`#` lines, then registers the line's FIRST whitespace-delimited token if it contains `::` (both sides of a `->` for a rename), so the documented `CUT:` / `RENAME:` prefixes in the file's own header are IGNORED. Wrapped prose is therefore a latent drain hazard: any continuation line that happens to BEGIN with a live `Class::m_member` token would silently drain that field on every load. ⚠ **This finding's premise is SUPERSEDED: the file has been restored from `2a9746185` (the pre-revert version) and now registers its full 172 entries**, which drove the re-cut of the 161 serialized fields the clean-slate revert had restored. The two accidental prose-artifact registrations it described (`MMKernel::sumUnit`, `CascadeWellbeing::buildingHealthArea`) went with the stripped version. The PARSER observation still stands and is still an owner call: it is prefix-free, so the documented `CUT:`/`RENAME:` prefixes are ignored and a wrapped prose line beginning with a live `Class::m_member` token would silently drain that field. Fix (owner call — it changes save-load behaviour): make the parser require its own documented prefix. The prose that narrated the reverted-away state is now accurate again: the entries and the member deletions are back in step. | save reader |
| 25 | **LANDED (the source now exists).** `Assets/savemigration.txt` is a REPLACEMENT-OBLIGATION LEDGER ([save.md §3](../../specs/save.md)): its entries cut the per-unit accumulators from every save on the promise of a named gatherer, and the clean-slate revert archived both that gatherer (`MMKernel::sumUnit` — zero occurrences in `Sources/`) and its storage (`m_cascadeUnitPackages`), so **those values had NO SOURCE at all**. Built as the [state-repositories.md](../../architecture/state-repositories.md) UNIT shape rather than a package: `Sources/Cascade/CvUnitResolved.{h,cpp}` — RESOLVED VALUES stored individually (enum-indexed, so a new unit value is a table row, not a member + getter), gathered over the HELD SET (the unit's own info ∪ held promotions ∪ held unit-combat classes) from the COMPILED slot sums (`CvInfo::modifier`/`getScalar` — bare array loads, no anatomy walk, no string address), dirtied by EXACTLY the two facts the model names (`SEVT_UNIT_PROMOTION_CHANGED` / `SEVT_UNIT_COMBAT_CHANGED`, routed through the modifier's own consumer). Never serialized; `gatherInto` is exposed as the from-source oracle. ⚠ 17 slots are wired (strength base + combat percent, withdrawal, first strikes + chance, the five heal legs, evasion, intercept, collateral, capture probability/resistance, extra upkeep); the SM size scaling is deliberately NOT a slot — it is the `sizeMatters` BLOCK (json.md §9), not a modifier family. **Remaining: re-point the consumer getters** onto `resolvedValue()`, which is stage-4 consumer work. |
