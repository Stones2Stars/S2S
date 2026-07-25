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
| `InfoRepo` per-type repos · `DepositIndex`/`DepositRead` (`MMKernel`, interned segments) · `cascadeEvalCondition` + eval ctx · the hostless enabler tree · `ContextDict` + `CityContext`/`EmpireContext`/`PlotContext` · the `readjson.exe` offline driver | various | KEEP |
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
   **Still open:** the per-info-type typed member arrays + the contexts-taking `expected*` valuation endpoints
   land with the three-info rebuild (sequence below), reading these compiled forms.
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
   data ids; 1–2-entry stragglers on the one `InfoScalar` enum read via `getScalar100`.
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
   `corporationRevenue` lands as a `corporation` kind on the commerce side (the R11 source-component pattern) —
   landing awaiting owner nod. Batch 2.
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
   the family key is gone from the census.

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
   an inexact V falls back to the legacy member + a curator warning.

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
   composition shape needs a ruling.

24. **Site 7 (AI unit upkeep era stage) = a PLAIN CONFIG VALUE on the handicap (owner):** the engine formula's
   own parameter (the §7 `ai` metadata plane), keeping its clamp and multiplicative stacking in the formula —
   never a deposit. Curator emits it in batch 3b; the `CvHandicapInfo` read re-points with the next Sources
   pass (until then the site reads 0 — on the enumerated dead-reader worklist). **Batch 3b LANDED:** key
   `ai.unitUpkeepEraModifier` = the legacy `iAIPerEraModifier` on the 6 authoring handicaps (emperor −1,
   immortal −2, deity −3, nightmare/nightmare+/ai_boosted −5); re-point sites: CvHandicapInfo.cpp:121 (reads
   the dissolved `perEra.empire.ai.percent` slot → 0) feeding CvPlayer.cpp:10354.

25. **Entry-level target qualifiers restored to the spec (owner: the original json-design intent)** — the §3.3
   ranked qualifiers + counted-kind filters ride individual §3.9 entries; node-level is shorthand for
   unqualified entries ([json.md §3.9](../../specs/json.md)). Unblocks the four `trait_bigot*` R23 holdouts —
   they convert with the next curator pass (locked-folder transform). **Batch 4 LANDED:** the 4 `trait_bigot*`
   files converted (locked-folder transform) — one `cities.flat` list holding the ranked entry
   `{−1, max: TARGET_NUM_CITIES, orderedByDescending: CITY_SIZE}` beside the religion-qualified entry
   `{−1, religion: "!IS_STATE_RELIGION"}`; `nonStateReligion` census count 4 → 0.

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
   legacy member).

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
   census. C++ worklist: the channel axis on the modifier kind + the per-channel fold at the §2a tradeYield
   input.

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
   convention (the batch-4 sign tear RESOLVED). C++ worklist: the combine-side floor leg (freeX group ≥ 0,
   net ≥ 0) + the per-pop identity (1 gold/pop × the `BASE_FREE_UNITS_UPKEEP_*_PER_100_POP` GlobalDefines,
   all currently 0) staying engine formula config.

28. **Free unit upkeep (owner: "just subtract upkeep per pop, with a floor of 0")** — converts as subtractive
   per-population deposits (`upkeep.freeMilitary`/`freeCivilian`, negative value, `per: POPULATION` with the
   transcribed quantum), the zero floor as family-combine metadata (the §2 `min` mechanism). An owner-ruled
   INTENTIONAL model change from the legacy asymmetric rounding helper — attributed, never bit-chased
   ([validation.md](../../specs/validation.md) intentional class). Curator batch 4.

## The rebuild sequence

1. The toolkit (reader consolidation + interning + vocabulary + rename sweep).
2. `CvBuildingInfo` rebuilt to the anatomy (the proven pattern — fattest, both defect and cure historically).
3. `CvCivicInfo`, then `CvTraitInfo` (trait reads are active-set-selected — [modifier.md §4](../../specs/modifier.md)).
4. Docs current in the SAME changes: [engine.md § Info loading](../../reference/engine.md) rewritten to the one
   reader when it lands; patterns.md + the ledger already carry the contract.

## The getter aim — settled; full consumer rewiring stays roadmap-sequenced

The EXEMPLAR getter setup every rebuilt info aims at is settled and lives in
[patterns.md § THE GETTER SETUP](../../architecture/patterns.md): sections as whole typed objects ·
classification bitset tests (hold-vs-provide names) · per modifier group exactly TWO reads (the typed entry list
+ the contexts-taking `expected*` what-if valuation) · bare intrinsic reads + `getScalar(SCALAR_*)` stragglers.
No per-slot point getter over a pre-summed member exists. The rebuilt infos implement this shape NOW; what stays
roadmap-sequenced is the ENGINE-WIDE consumer rewiring onto it
([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)) and the
[contexts.md](../../architecture/contexts.md) OPEN items the valuation bodies depend on (the
`CityContext.plotAttrs` load reseed).

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
