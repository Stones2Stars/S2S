# Empire capabilities — glossary

The catalogue of **empire/team-wide, tech-unlocked abilities** — the **empire counterpart to unit
[skills](skills.md)**. This is the **glossary** (the namings); the **system** is the [json spec](json.md) §8.
Sibling of skills.md.

> **Started; tech-curated (owner 2026-06-29).** The empire capabilities are the **tech `enabler` channels** (a tech
> *unlocks* the ability); `curate_tech.py` now folds them into the `capabilities` block (`enabler_block="capabilities"`
> in `curate_common.apply_channel` — `{cap: true}`, scope implied), so a tech reads `"capabilities": {techTrading: true,
> …}` instead of a top-level `techTrading: {team:{enabler:true}}` family. The **civic** `enabler` channels are the
> sibling case — **policies enacted by a civic** → the `policies` block (already emitted by `curate_civic`). Entity-level
> boolean gates that are neither (a building's `damageAllAttackers`, a wonder's `buildingOnlyHealthy`) stay as-is. The
> full clean-name list still needs grounding against the engine team-flags.

> **⚖ THE MECHANIC — a derived-on-query system, enabler-style; nothing is "granted" (owner ruling 2026-07-02).**
> Capabilities are not handed out per se — no grant event, no application moment, no stored team state. The system
> behaves like the [enabler](enabler.md): the empire's ACTIVE capability set is **derived where consumed**, as the
> union over the **currently live sources** — the same HAVE axis the enabler generates from (team techs + adopted
> civics + active buildings). A source's `capabilities` block is pure data direction ("this source carries the key");
> liveness does the rest — a capability is active iff SOME live source carries it, and it lapses the moment its last
> source does (no lifetime bookkeeping exists or is needed). **In practice we never disable a capability today**
> (owner 2026-07-02): every shipped grant is tech-side and techs are never lost — the lapse-with-source semantic is
> **headroom the model carries for free**, there if ever wanted, not a mechanic in current use. This also
> settles grantor breadth: **any source kind on the HAVE axis participates** (tech / civic / building) — the
> "tech-curated" note above describes the curator mechanics for the tech side, not an only-grantor rule, and the
> `CvJsonTechInfo.h:7` "techs are the ONLY grantor" comment is superseded (fix it when `en_empireHasCapability`'s
> techs-only union widens to the full HAVE). "Monotonic" holds for TECH sources only (a tech is never lost), never
> for the system.

> **✅ MAPPED into the cascade (2026-06-30).** readJson now maps the `capabilities` block onto the entity's `CvJsonInfo`
> (`std::set<std::string> capabilities` — the granted names; `[READJSON/cap]`); it was previously parsed-but-skipped.
> Verified live: the block appears on **24 techs** (e.g. `techTrading`, `openBordersTrading`, `permanentAllianceTrading`,
> `dcmAirBomb2`). The empire's **ACTIVE** capability set is the union over the team's held grantor techs — **derived
> where consumed** (the enabler's `canFound`/`canBuild` gates + the team-ability systems), per the static(info)/live(state)
> split; it is not stored on a team object. Wiring those consumers is the next step.

## What a capability is (recap)
- **Team / empire scope** — applies to the whole civilization, not one unit (the section name carries the scope).
- **Source-derived, never granted** — active iff some **live** HAVE source (tech / civic / building) carries the
  key; derived on query, enabler-style (the ruling above). Monotonic only insofar as the source is (a tech is;
  a building isn't).
- The empire analogue of a unit `skill` (a `skill` is the *unit* ability; a `capability` is the *empire* one).

> **⚖ PARAMETERIZED abilities (owner rulings 2026-07-02).** Two shapes, decided by the value's shape (json.md §2):
> - **Per-commerce sliders — discrete capability keys** (`setScienceRate`/`setCultureRate`/`setEspionageRate`,
>   owner 2026-07-01): after the split each is a genuine bare-bool ability, so the flat set carries them.
> - **Per-terrain trade — NOT a capability: the root `canTradeOn` block** (owner ruling 2026-07-02 — named
>   `canTradeOn`, not `canTrade`, to avoid confusion with trading RESOURCES; supersedes the same-day
>   `canTradeOn<Class>` discrete-keys idea). Flat `canTradeOnX` booleans *"will end up having to be
>   individual hard-code gates, with 0 modularity"* — every key needs its own C++ gate + a hardcoded terrain→key
>   table. Instead the grantor (tech) carries a bespoke **`canTradeOn`** block with REAL `TERRAIN_` references
>   (FK-resolved by readJson), **which the trade-route system goes through**: the empire's tradable-terrain set is
>   the derived-on-query union over live sources' `canTradeOn.terrains` (same mechanic as capabilities), and the
>   consumer (`CvPlot.cpp:5641` `CvTeam::isTerrainTrade`) asks generic set-membership — new tradable terrains are
>   pure data, zero code. Live data: raft-building (lake-shore), sailing (coasts+lake), seafaring (seas),
>   navigation (oceans+trenches). **The COMMON (baseline) tradable terrains are homed on `TECH_GAME_START`'s
>   `canTradeOn` block** (owner 2026-07-02) — the universal start node every civ holds (it already carries
>   `setScienceRate:true` the same way), so the from-game-start tradable set rides the SAME union mechanic, no
>   engine special-case for "always tradable". **`riverTrade` is semantically DISTINCT (owner 2026-07-02):** it
>   defines whether a river can be used as a "trade ROAD" (a connectivity conduit, like routes) — NOT whether you
>   can trade on a river tile — so it is not terrain-list data. **Ruled (owner 2026-07-02): `riverTrade` IS a
>   capability** — it stays the bare bool it already is (a river-interaction ability like `bridgeBuilding`),
>   outside `canTradeOn`, which stays purely "which plot types carry trade".

> **⚖ The `canTrade` block — the whole `-Trading` family re-homes out of flat capabilities (owner ruling
> 2026-07-02).** The semantic model first: `openBorders` is FULLY open — a civilization-to-civilization **"tradeable
> pact"** (all units pass); `limitedBorders` means only CIVILIAN units (merchants and such) can pass — **in-game
> name: "Right of Passage"** (verified: `TXT_KEY_MISC_LIMITED_BORDERS`). Each is a capability only in the sense that
> **you can trade FOR it** — the unlock is the *ability to negotiate that pact type*; actually *having* it with
> another civ is a **traded agreement** (diplomatic state, outside this system). That model generalizes into a root
> **`canTrade`** block — *"what may appear on your trade table"* — booleans for items AND agreements: `techs`,
> `openBorders`, `rightOfPassage` (the player-facing name, not limitedBorders), `embassy`, `bonuses`,
> `freeTradeAgreement` *"and so on"* (owner keys) — plus the grounded legacy re-homes `gold`, `maps`,
> `defensivePact`, `vassals`, `permanentAlliance`. The diplomacy/deal system (`CvPlayer::canTradeItem` + the
> per-item gates) goes through it **generically** — no per-key hardcoded gate — via the same derived-on-query union
> over live sources. Sibling of `canTradeOn` (below); the flat capability set keeps only the non-trading abilities.
> Data consequence: the curator emits BOTH `canTrade.openBorders` AND `canTrade.rightOfPassage` from the single
> legacy `isOpenBordersTrading` flag (legacy couples them in one `processTech` branch) — two keys so the coupling
> lives in DATA, not a hardcoded engine implication.
>
> **The grounded tradeability map (locust pass 2026-07-02 — owner: map ALL of them; a miss is easy to add after,
> the block is additive).** Legacy has exactly **8 tech-side flags** (`CvTechInfo.h:57-74`):
> `bMapTrading`→`maps` · `bTechTrading`→`techs` · `bGoldTrading`→`gold` · `bOpenBordersTrading`→`openBorders`+
> `rightOfPassage` · `bDefensivePactTrading`→`defensivePact` · `bPermanentAllianceTrading`→`permanentAlliance` ·
> `bVassalStateTrading`→`vassals` · `bEmbassyTrading`→`embassy`. (`CvTeam::isLimitedBordersTrading` is the coupled
> team counter with NO own XML flag — the double-emit covers it.) The wider trade-TABLE item space
> (`TradeableItems`, `CvEnums.h:2156` — resources/bonuses, cities, workers, military units, contacts, corporations,
> votes, `TRADE_FREE_TRADE_ZONE`, `TRADE_RITE_OF_PASSAGE`, war/peace/embargo/civic/religion) is game-option/
> state-gated today, NOT tech-flagged — each becomes a `canTrade` key (`bonuses`, `freeTradeAgreement`, …) as/when
> its gate goes data-driven.

> **⚖ The `canWorkOn` block (owner rulings 2026-07-02).** *Which plot classes a city's citizens may WORK.*
> Deliberately **coarse plot classes, not terrain lists** (owner: no per-terrain detail needed here) — in essence
> **`water` · `ocean` · `peaks` · `space`**. The `CvCity::canWork` gate queries the block generically (derived
> union over live sources). Grounded legacy sources:
> - **water/ocean** — `bWaterWork` (`TECH_TRAP_FISHING` → `CvTeam::isWaterWork`, the `canWork` `isWater()` gate,
>   `CvCity.cpp:1753`) is the ONE direct work gate found. The owner half-remembers a separate ocean (and
>   deepOcean) tech requirement — NOT found in `canWork` this pass (all water terrains carry positive base yield,
>   so it is not the `hasYield` gate either); **trace the actual ocean-working realization at port time**, do not
>   assume the single-flag model.
> - **peaks** — need **`TECH_MOUNTAINEERING`** (owner; grounded: `bCanPassPeaks` → `CvTeam::isCanPassPeaks`). The
>   legacy realization is INDIRECT — peaks are impassable without it (`CvPlot::isImpassable`, `CvPlot.cpp:5785`);
>   there is no direct `canWork` peak test — trace the exact hop at port time.
> - **space** — **semi-modelled today / to be modelled in the future** (owner); the block is its ready home.
> Same magically-free modularity as `canTrade`/`canTradeOn`: a new workable plot class is data, not a new
> hardcoded gate. **If terrain-level explicitness is ever needed here, rework it THEN (owner 2026-07-02)** — the
> coarse classes are the model until a real need says otherwise.

> **⚖ Dual-plane abilities — same name on both planes (owner ruling 2026-07-02).** An ability can exist as BOTH a
> unit **skill** and an empire **capability**, under the **same clean name**. The exemplar: **`canPassPeaks`** — a
> promotion grants the unit skill (legacy `bCanMovePeaks`) to a specific unit, and `TECH_MOUNTAINEERING` makes it
> universal as the empire capability (legacy `bCanPassPeaks`) — "everyone can do it at mountaineering". The
> effective check is the OR of the planes: unit-has-skill ∪ empire-has-capability (legacy AI already treats them as
> one mechanic — it zero-values the promotion once the team flag is up, `CvPlayerAI.cpp:28313`). The distinct
> `canLeadThroughPeaks` (lead a whole stack through) stays its own skill.

## Capabilities — the CANONICAL list (⚖ ruled 2026-07-02: clear-semantics names — the name says what it does)

> Naming convention (owner): **`can<Verb><Object>` / `has<Thing>`** — e.g. `canSetScienceRate`, `hasRiverTrade`,
> `canIgnoreIrrigation`, `canSpreadIrrigation`. Grounded from the shipped data (24 emitted keys) + the engine
> flags; the `-Trading` family lives in `canTrade`, terrain trade in `canTradeOn`, workability in `canWorkOn`
> (rulings above). `moveOnWater` is DROPPED (exists in neither data nor engine; `canWorkOn.water` is the
> water-working ability, a different thing).

| capability (canonical) | was (emitted) | legacy source | meaning |
|---|---|---|---|
| `canFoundOnPeaks` | `canFoundOnPeaks` | `bCanFoundOnPeaks` (TECH_ALGEBRA) | can found cities on peak tiles (has the one live capability shadow) |
| `canPassPeaks` | `canPassPeaks` | `bCanPassPeaks` (TECH_MOUNTAINEERING) | move through peaks — **dual-plane** with the unit skill (ruling above) |
| `canMoveFastOnPeaks` | `moveFastPeaks` | `bMoveFastPeaks` (TECH_COLONIALISM) | faster movement over peaks |
| `canFarmDesert` | `desertFarming` | `bEnablesDesertFarming` | can farm desert tiles |
| `canSpreadIrrigation` | `irrigation` | `bIrrigation` | irrigation spreads / chains from fresh water |
| `canIgnoreIrrigation` | `ignoreIrrigation` | `bIgnoreIrrigation` | farms work without an irrigation chain |
| `canBuildBridges` | `bridgeBuilding` | `bBridgeBuilding` | roads cross rivers |
| `hasRiverTrade` | `riverTrade` | `bRiverTrade` | a river acts as a trade ROAD (conduit — ruling above) |
| `canRebaseAnywhere` | `rebaseAnywhere` | tech flag | air units may rebase to any friendly plot |
| `canSeeFurtherFromWater` | `extraWaterSeeFrom` | `bExtraWaterSeeFrom` | see FROM water plots one level higher (`CvPlot::seeFromLevel`) |
| `hasCenteredMap` | `mapCentering` | `bMapCentering` (tech + buildings) | minimap centered on your civ + round-globe view; arrive-and-stay latch (ruling above) |
| `hasWholeMapRevealed` | `mapVisible` | `bMapVisible` | reveals the ENTIRE map on acquire (`setRevealedPlots`, `CvTeam.cpp:5292`) |
| `hasLanguage` | `language` | `bLanguage` (TECH_LANGUAGE) | civ has developed language — gates `needLanguage` heritages (`CvPlayer.cpp:30970`) |
| `canSetScienceRate` | `setScienceRate` | commerce-flexible (TECH_GAME_START) | the science slider |
| `canSetCultureRate` | `setCultureRate` | commerce-flexible (TECH_DRAMA) | the culture slider |
| `canSetEspionageRate` | `setEspionageRate` | commerce-flexible (buildings) | the espionage slider |
| ⏳ `dcmAirBomb1` / `dcmAirBomb2` | same | `bDCMAirBombTech1/2` | DCM air-bomb target tiers — **rename pending the 1-vs-2 semantic pin** (see Grounded meanings) |

## Grounded meanings (2026-07-02 walkthrough — feed the C5 glossary rewrite)

- **`waterWork`** — cities may WORK water tiles at all (`CvCity::canWork` gate, `CvCity.cpp:1753`); granted by
  `TECH_TRAP_FISHING`. → re-homed to `canWorkOn.water` (ruling above).
- **`extraWaterSeeFrom`** — see FROM water plots one level higher (`CvPlot::seeFromLevel`, `CvPlot.cpp:2562`) —
  ships stop being nearsighted; second consumer in AI settle scoring (`CvCity.cpp:6327`). ⚖ Owner 2026-07-02: it
  *could* be modelled in the vision system proper (a visibility-on-water-units shape), but **stays a capability —
  solve if/when it is a problem**, not now. Expected surfacing point (owner): **when the visibility system is
  modelled properly during shadow/cutover** — whoever builds that system should revisit this key then (it sits
  next to the BLOCKED unit visibility/invisibility accumulators and the building `lineOfSight` channel). Vision-semantic but a
  single bare bool → stays a flat capability; rename candidate `seeFurtherFromWater`.
- **`mapCentering`** — ⚖ RULED a capability, canonical name **`hasCenteredMap`** (owner 2026-07-02): *"when it arrives, map gets centered,
  and stays centered"* — an arrive-and-stay latch in practice, even if the source is never really rechecked (the
  never-disabled-in-practice ruling above). It *could* technically behave as a grant (a one-shot pulse), but the
  owner is deliberately NOT minting a special grant for a thing like this — do not reclassify. *(Pre-named future
  path, owner 2026-07-02: if it ever shows we need to grant a player ONE-TIME EFFECTS, mapCentering lands cleanly
  in that grant type — until then it stays here.)* Pure presentation, no map reveal: minimap renders centered on your civ
  (`CvGameInterface.cpp:2907`) + globe view goes round-with-stars (`CvGame.cpp:2760`). Granted by one tech +
  `bMapCentering` buildings.
- **`dcmAirBomb1`/`dcmAirBomb2`** — DCM air-bombing target tiers: consumers `CvUnit::airBomb2At`/`airBomb4At`
  (`CvUnit.cpp:22493/22684`) do an O(all-techs) held-tech rescan per strike (the rewire collapses it to the
  capability query). ⚠ The parent toggle `DCM_AIR_BOMBING` is a **split-brain pseudo-option**: a GlobalDefine
  shipped ON (`GlobalDefinesAlt.xml`) that a buried BUG checkbox (RevDCM tab, default False) OVERWRITES at init
  (`RevDCM.py:60,108`) — effective state depends on the Python init, which explains "can't strike this city"
  scenarios (with DCM active, `canAirBombAt` branches to DCM target logic, `CvUnit.cpp:7123/7186`). Names are
  mod-heritage numbering — pin the 1-vs-2 semantic from the target-filter code before renaming.

## ✅ VERIFIED — the full-surface parity run (2026-07-02)

Owner method: **direct HTTP parity** — `/computed/teamFlags` (the engine flags by canonical name +
`canTrade`/`canTradeOn`/`canWorkOn`) diffed against the offline derived-on-query union (held techs' JSON blocks +
`TECH_GAME_START`), per player. **Result: 0 diverging** across every capability, the whole `canTrade` family, the
per-terrain `canTradeOn` set, and `canWorkOn.water`. Grounded finds captured:

- **`canSetScienceRate` + `canSetEspionageRate` are UNIVERSAL defaults** — `CIV4CommerceInfo.xml` marks research
  and espionage `bFlexiblePercent=1` (a system global, no grantor); their data home is `TECH_GAME_START`'s
  `capabilities` (both now there). Culture stays tech-gated (TECH_DRAMA); gold has no slider.
- **`canTrade.vassals` / `canTrade.permanentAlliance` compose GAME OPTIONS engine-side** (`CvTeam.cpp:3262/3279`:
  the flag getter = capability ∧ `GAMEOPTION_ENABLE_PERMANENT_ALLIANCES` / ¬`GAMEOPTION_NO_VASSAL_STATES`). The
  capability DATA is the unlock; the option gate stays an engine-side composition at the consumer (like era-scaling
  on `allowed` caps). Any parity harness must fold the options.
- `isCommerceFlexible` additionally gates espionage on met-civs and everything on founded-first-city — runtime UI
  conditions, not capability data.

## ✅ CUT LANDED — Gate-3 wire #1 complete (2026-07-02, third pass)

**The 22 CvTeam capability getters RUN ON the cascade and the 21 legacy counters are DELETED** (commit
`b1cf1edb7`), proven by a recorded live turn: state loads whole through the named skips, the trade network holds
across the boundary (stack at its accepted residue), every cut-gate at 0, and the fastest turn pace of the
campaign. The pieces: `CascadeCapabilities` (per-team cached union, O(1) precomputed flags + per-terrain bit
vector, invalidated at `setHasTech`/`reset`); NPC guard + game-option compositions preserved in the getters;
`processTech` keeps the side effects — `updateYield` (peaks), the improvement-validity cache round
(farming/irrigation/water-work), and the trade-NETWORK recompute (`updatePlotGroups` + `MarkBridgesDirty`) the
deleted changers carried; serialization retired via **named `WRAPPER_SKIP_ELEMENT`s** (the save doc's
IGNORE-by-field-name mechanism) with the fields ledgered in **`savemigration.txt`** (repo root — the conversion-
step list). ✅ **ONE union (folded 2026-07-02):** the enabler kernel's techs-only duplicate
(`EnablerKernel::empireHasCapability`) is DELETED — `CascadeCapabilities` is the sole derived-on-query union.
Its only consumer was the enabler's `cap:canFoundOnPeaks` shadow, itself deleted as tautological post-cut (its
oracle — the legacy counter — is gone; the flipped getter IS the cascade). The `[CAPSHADOW]`
machinery stays as the net for the pending flips (`isMapCentering`, the commerce sliders, building attributes,
`hasLanguage`).

**The road here — two reverted attempts whose findings BIND any future serialized-member cut:**
1. **⛔ Deleting `WRAPPER_READ` entries DESYNCED the save load** — the testsave's counter tags were not skipped
   softly; every read after the first orphaned tag landed wrong and GUTTED the loaded state (no techs, no
   buildable lists; the "every city can't find anything to build" grind). The `cascade-engine-430.md` §5 claim
   "removing a serialized member is soft" is **WRONG as stated for this path** — verify
   `CvTaggedSaveFormatWrapper`'s actual unknown-tag semantics before ANY serialization-touching cut. The correct
   retirement is **two-stage**: (a) drop the `WRAPPER_WRITE` + replace the read with a named
   `WRAPPER_SKIP_ELEMENT` (drains the stale tag on old saves; no-ops on new) + ledger the field in
   `savemigration.txt`; (b) flush skips + ledger together at the next save-compat break. Grounded mechanism
   (`Expect()` never consumes an unexpected element): [engine.md](../reference/engine.md) §Save/load.
2. **⛔ The deleted CHANGERS carried side effects the applies audit missed**: `changeTerrainTradeCount` /
   `changeRiverTradeCount` call **`updatePlotGroups()`** per team player (the trade-network recompute!) and
   `changeBridgeBuildingCount` marks bridges dirty. Post-cut, `setHasTech` must still fire those when a tech
   carries the relevant flags — without them the network goes progressively stale (this, WITH the NPC guard,
   fully explains the first flip's breakage). Audit EVERY deleted function's body for side effects, not just the
   apply sites.

The `[CAPSHADOW]` machinery stays as the net for the pending flips (`isMapCentering`, the commerce sliders,
building attributes, `hasLanguage`).

## ⚠ THE FIRST FLIP ATTEMPT — REVERTED; the wiring lesson (2026-07-02)

A full Gate-3 cut was landed and **REVERTED the same day** (commits `eccbb8e9d`+`d93d7d834`, reverts
`304645b9c`+`f6fdc5ef6`): all 22 CvTeam capability getters flipped to a `CascadeCapabilities` derived-on-query
union (per-team cached, `setHasTech`-invalidated, O(1) precomputed hot-path flags), the 21 legacy counters
deleted. Offline parity was 0-diverging pre-flip — **and the first real turn broke the TRADE NETWORK in-game**:
the connectivity getters (`isTerrainTrade`/`isRiverTrade`/`isBridgeBuilding`) feed plot-group computation, cities
lost bonus connections, and the ENGINE mass-dormed bonus-gated buildings (a sampled city's engine-disabled count
doubled). Root cause NOT yet mapped (candidates: NPC/barb teams the offline parity never sampled; a load-order
read before tech deserialization; tech seeding that bypassed `processTech` so legacy counters ≠ f(held techs)
for some team).

**The binding lesson: for getters feeding DERIVED ENGINE STATE (plot groups, trade network, caches), the offline
parity leg is NECESSARY but NOT SUFFICIENT.** The getter-contract strategy's step 1 — the IN-BODY instrument
(cascade-vs-legacy diffed at the real call moment, through real turns) — is mandatory before any flip; it was
skipped here. Re-wire plan: keep the legacy counters, add the in-body `[GETTER]`-style shadow to the 22 getters,
run turns until clean (which also NAMES the root cause above), THEN flip. The `CascadeCapabilities` design
(union + cache + O(1) flags + option-composition-in-getter + side-effects-survive) is validated and comes back
as-is at that point.

## Open
- ~~**`dcmAirBomb1/2` renames**~~ — **MOOT: DCM air bombing is slated for whole-system REMOVAL** (owner 2026-07-02;
  audit: effectively off by default, human-only, not load-bearing — see
  [structural-cleanup.md](../plans/structural-cleanup/structural-cleanup.md) Tier 2 for the full removal surface).
  The `dcmAirBomb1/2` capability channels drop with it.
- ~~**The curator rename pass**~~ — ✅ EXECUTED 2026-07-02 (TechInfo.json channels + `apply_channel`
  block/refList/flexArray + building sliders + `tech_game_start.json` + `CJK_INTRINSIC_KEYS`; regenerated. No C++
  query strings existed for the renamed keys — verified by grep; `canFoundOnPeaks`/`canPassPeaks` unchanged, the
  shadow survives).
- **Ocean-working trace** — the half-remembered ocean/deepOcean requirement (see the `canWorkOn` ruling).

## See also
- [json.md](json.md) §8 — the system. · [skills.md](skills.md) — the unit counterpart. · [tags.md](tags.md) ·
  [state.md](state.md).
