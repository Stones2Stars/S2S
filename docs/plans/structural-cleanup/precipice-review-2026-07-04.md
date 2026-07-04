# The precipice review (2026-07-04) — pre-flip adversarial audit worklist

> **What this is:** the owner-ordered adversarial review of the landed scope-package substrate BEFORE the
> Cut-1 demolition ("when we flip this switch, we have to flip it all"). Method: 74 agents — a legacy WRITER
> CENSUS per flipped channel (every feeder of every legacy accumulator vs its cascade counterpart, the
> PROJECT_THE_INTERNET exemplar shape) + a 7-dimension review of the uncommitted landing, every
> missing-source claim and blocker/serious finding independently refuted (13 misses + 1 finding killed with
> counter-evidence, mostly proven data-dead). **This doc is the ACTIONABLE WORKLIST — dispositions pending
> the owner.** Line anchors drift; trust named functions.
>
> ⛔ Nothing here is a parity RESULT (no counts-as-status); every row is a named cut-blocker candidate in the
> [code-cut-map](code-cut-map.md) sense. Each row's disposition = one of: **fold** (build the cascade
> counterpart / persisted-store fold), **accepted** (map to a documented divergence class — and verify the
> documented fold actually EXISTS), **data-dead-guard** (safe today, add the curator/validator guard so
> future authoring can't silently miss), or **later-cut** (feeds an accumulator not in this cut's demolition
> list).

## 1. ⚖ THE RULING NEEDED FIRST — within-turn freshness semantics (the staleness cluster)

Findings 1/6/8/10 (+9) of the review converge on ONE verified code fact: **the landed substrate is a
per-player-slice SNAPSHOT.** Combines are bare fetches (`acc_yieldCombine` reads raw package fields, no
ensure); event sites are MARKS-ONLY (`buildingProcessed`, `markPlayerScopeAndCities` — no ensure); the only
ensures are the boundaries (`playerSliceRebuild` = markAll+ensure at each player's `doTurn` top,
`worldRebuild`, the load warm-up) and internal fill pulls. The facts Set alone self-ensures on read
(`EnablerKernel::cityFacts`). Two consequences, both invisible to the per-turn nets (they sample at
boundary-fresh moments):

- **Any mid-slice mutation serves PRE-EVENT values for the rest of the turn cycle**: a building completing
  inside the owner's own slice (the most common event in the game — cities process before the AI decides),
  a human specialist re-assignment (city screen shows stale yields until next turn), hurry, religion
  convert (`setLastStateReligion` has no mark at all — self-heals at the boundary like everything else,
  since the boundary is markAll the event marks are currently REDUNDANT machinery).
- **A city founded/captured mid-turn serves zero-initialized packages until its owner's next slice**
  (`initCity`/`acquireCity` never ensure; reset marks all-dirty but nothing redeems it until the boundary).

**The doc tension to adjudicate:** [state-repositories.md](../../architecture/state-repositories.md) rules
the turn-snapshot read model as the END-STATE, landing WITH the parked AI build-queue-parity rework —
*"until then … the flipped getters must match what legacy's always-fresh accumulators would have answered
(parity discipline)."* [scope-packages.md](scope-packages.md) §1 landed bare-fetch/no-read-ensure NOW. The
legacy engine is always-fresh; this is an observable behaviour change during the mirror phase. Options:
(a) restore a read-side `ensure(mask)` on the package accessors for the shadow window (a clean-bit branch
per read; refresh fires only when dirty — NOT the old epoch-polling protocol that collapsed automation);
(b) eager-ensure at the mark sites (event-time rebuild, bounded per event); (c) RULE the snapshot semantics
in force now (accept within-turn staleness + UI lag until the turn-end unified pass; nets stay
boundary-sampled).

**✅ RULED (c) — same day:** *"start of next turn is what is expected; getting a yield event in the middle
of a turn is not retroactive"* — the snapshot semantics are in force, captured in
[scope-packages.md](scope-packages.md) §1 (+ the state-repositories.md supersession note), with the ruled
exception: **city creation sets up immediately** (*"so that we can see time to build"*) →
`CascadeAccumulator::cityCreated` eager-ensures at the end of `CvPlayer::found` + `acquireCity` (landed
same day, Assert-clean). Watch-in-play residual: mid-turn city-screen DISPLAY staleness (juggling); the
remedy if play-feel objects is a UI-only refresh hook.

## 2. Verification-instrument repairs (unambiguous, do before the window continues)

1. ✅ **FIXED (2026-07-04)** — **The buildRate net was TAUTOLOGICAL** (two dimensions found it
   independently): the endpoint's `buildRateLeg` field (`Tools/CvHttpServer.cpp`) called the FLIPPED
   dispatcher, comparing cascade-vs-cascade. Now emits the head-order `getProductionModifierLegacy`
   overload (unit/building/project).
2. ✅ **FIXED (2026-07-04)** — **The four flipped wellbeing getters lacked the `isFinalInitialized()`
   guard** every sibling flip carries — pre-init reads returned zeroed packages; they now route to the
   `*Legacy` bodies pre-init.

## 3. The confirmed census misses (33 — survived independent refutation; disposition pending per row)

Census coverage (rows / covered / accepted): yields 21/13/3 · commerce 39/26/4 · happiness 35/20/10 ·
health 28/25/1 · gp-rate 13/10/2 · **defense 9/1/0** · maintenance-mod 11/6/0 · buildRate 35/23/3 ·
tradeRoutes 14/8/4.

**Likely LIVE-DATA (flipped getter may be serving without the source — verify against save data first):**

| # | channel | missing source | legacy site |
|---|---|---|---|
| L1 | commerce | trait `NonStateReligionCommerce` (collect ALL religions' commerce; live non-zero complex-trait data) | `CvPlayer.cpp:28458` processTrait → OR-gate `CvCity.cpp:~12xxx` |
| L2 | commerce | Process production→commerce conversion (`processProcess`; the cascade carries a TODO stub hardcoded 0) | `CvCity.cpp:5192` → `getProductionToCommerceModifier` |
| L3 | happiness | building-authored building-keyed happiness (`BuildingHappinessChanges`, Royal Tomb → Palace class, ~11 wonders; curator gated the deposit on the SOURCE-city side instead of the target building) | `CvPlayer.cpp:7490` processBuilding → `changeExtraBuildingHappiness` |
| L4 | happiness | specialist TECH-gated happiness (`m_iExtraTechSpecialistHappiness` = Σ count × `getTechHappiness`) | `CvCity.cpp:23468` updateExtraTechSpecialistHappiness |
| L5 | happiness | `CommerceInfo.iInitialHappiness` (city-founding constant — every city, every game) | `CvCity.cpp:363` init → `changeCommerceHappinessPer` |
| L6 | gp-rate | trait `iGreatPeopleRateChange` → `m_iNationalGreatPeopleRate` (player scalar) | `CvPlayer.cpp:28654` processTrait |
| L7 | maintenance | tech `iMaintenanceModifier` (empire percent) | `CvPlayer.cpp:30927` processTech |
| L8 | maintenance | civic `iHomeAreaMaintenanceModifier` / `iOtherAreaMaintenanceModifier` (area overlays) | `CvPlayer.cpp:18043/18044` processCivics |
| L9 | maintenance | project `iGlobalMaintenanceModifier` + `iConnectedCityMaintenanceModifier` | `CvTeam.cpp:4329/4332` processProjectChange |
| L10 | buildRate | corporation `getMilitaryProductionModifier` (city, active-in-city) | `CvCity.cpp:15504` applyCorporationModifiers |
| L11 | buildRate | trait LIVE walks: `SpecialBuildingProductionModifiers` + maxGlobal/maxTeam/maxPlayer wonder-category modifiers | `CvPlayer.cpp:7274-7302` getProductionModifier(Building) |
| L12 | tradeRoutes | PROJECT `iWorldTradeRoutes` (PROJECT_THE_INTERNET, curated `tradeRoutes.world.flat:1`; cascade world walk covers buildings only) | `CvTeam.cpp:4341` |
| L13 | defense | the channel breadth (9 rows, 1 covered): building `iBombardDefense`+`iMinDefense`, cultureLevel `iCityDefenseModifier` (naturalDefense), building `iAllCityDefense` (empire), civic `iExtraCityDefense`, trait `iCityDefenseBonus`+`iBombardDefense` — ⚠ several feed getters OUTSIDE the flipped `getBuildingDefense`; per-row check whether each is in this cut's demolition list at all | `CvCity.cpp:5142/4840/10290`, `CvPlayer.cpp:7446/18130/28562/28572` |

**⚖ Owner dispositions received (2026-07-04) on the live-data rows:**
- **L1 `NonStateReligionCommerce`** — *"in essence a civic-instated POLICY that allows a civ to get
  commerces from non-state-religion buildings"* → wire as the policy read: the commerce fill's SR-gated
  terms consult the policy state (derived cascade-side from the civic + trait grantors — the json.md §9
  `nonStateReligionCommerce` policy, already verified a pure STATE), never the legacy counter.
- **L13 defense breadth** — *"these are ALL additive percentages, or in a very few buildings things that
  raise the FLOOR of defense. The defense object is sound — wire it properly so cities don't lose defense
  on flip."* → wire the missing feeders into the defense family (additive percents + the `min`-floor
  member the family metadata already models: bombardDefense, minDefense, cultureLevel naturalDefense,
  empire allCityDefense, civic extraCityDefense, trait cityDefense/bombardDefense) before this channel's
  demolition.
- **§4 area-scope data-dead** — expected (*"not terribly surprising"*); the REAL area-scope user is
  **POWER** (Hoover-Dam-class: power to all cities on the continent — 2 wonders) → ties to the known-open
  `isPower` self-containment item; the area machinery matters there, not on yields.
- **L5 `iInitialHappiness`** — owner confirms it exists, *"the base we start from"* → ✅ **FIXED
  (2026-07-04)**: verified it is the per-COMMERCE CommerceInfo constant (culture=10 → +1 happy per 10%
  culture slider; NOT handicap — the difficulty base is the separate, covered `HandicapInfo::HappyBonus`).
  `CascadeWellbeing::gatherCityTerms` now SEEDS the per-commerce pool from the constant (static
  system-Info config, the sanctioned read class) instead of 0; the building `commerceHappiness` deposits
  add on top as before. ⚠ Expected side effect: the "accepted" happy +1..+3 residue (increment D) matched
  slider×seed — it should largely COLLAPSE with this fix; re-attribute on the next live read.
- **L3 Royal-Tomb class** — ✅ **FIXED (2026-07-04, both halves)**: curator moved `BuildingHappinessChanges`
  to `TARGET_KEYED` → `happiness.empire.buildings.{B}.flat` (the commerce-twin shape; 12-file regen == the
  XML author census; the three NAMED targets per the owner — never generic gov-center); DLL:
  `playerAreaEmpire` gained the keyed ledger + one `foldBuildingKeyed` realization (fill + oracle paths;
  `CPK_WB` sibling fan-out on the keyed mark). **Live-verified ARITHMETICALLY EXACT:** every probe city's
  post-fix happy delta == its authored keyed sum (Toledo +4/4, Avaris +4/4, Abydos +3/3; player-0 grantors:
  Tokyo Tower / Royal Tomb / Bayreuth / Giverny / Orient Express). **⚠ OPEN ATTRIBUTION — the legacy side:**
  post == pre + keyed EXACT on 3/3 measured cities ⇒ legacy's happy verdict provably does NOT pay these
  building-authored keyed amounts there (its `extraBuildingGood` emit is nonzero — the civic-keyed part —
  but the building part is absent), and the legacy HEALTH `extraBuildingGood` equals the authored keyed
  HAPPINESS sums city-exactly (7/7/4/3/4 on all five probes) — either a legacy cross-application or a
  drifted/never-fed player table; the apply-chain code reads correct (`CvPlayer::processBuilding:7499` →
  player table → self-healing city recompute), so the NEXT INSTRUMENT STEP is the emit extension: expose the
  player table (`getExtraBuildingHappiness/Health` per target) + the cascade's `extraB` term on the
  wellbeing endpoint, then name it with numbers. The cascade side matches the authored data + the owner's
  stated intent; candidate outcome = the attributed engine-wrong class.
- **⚖ The general disposition direction (owner 2026-07-04):** *"these are the kinds of things that we
  should be able to settle via plugging gaps in the JSON"* — census-gap classes settle as DATA wherever
  the model allows; an engine-side Info-constant read (the L5 seed) is the mirror-phase interim, and the
  durable home is a JSON plug when the owning system ports (for the culture-slider seed: the
  `TECH_GAME_START` universal-start-node precedent — the same home as the baseline `canTradeOn` terrains —
  or the commerce system config block; requires widening the `commerceHappiness` walk beyond buildings).

**EVENT/VOTE class — ⚖ RULED (owner 2026-07-04): the CLEAN persisted stores RIDE IN as raw saved state.**
*"We are past looking for parity, we now have to ensure we got everything — if cascade does not at this
point read the stored event yields, that has to be rectified, and that would have been missed on the
flip."* ✅ **E1/E2/E3 FIXED same day**: `BuildingPackage::buildingFlat` (the ONE per-building flat function
for yields AND commerce) folds `m_aBuildingYieldChange` + `m_aBuildingCommerceChangeEvents` per active
building, ×100 at the legacy tiers (`CvCity:4733` / `getBuildingCommerceByBuilding`) — fill + oracle in one
site; live proof case: EVENT_FULLERENES_1 (+10 research on Oxford's Chemistry Lab, owner-played).
`http-endpoints.md`'s honest-divergence stance amended (clean stores fold; mixed/dead stores + the offline
StoneBase leg keep the boundary rule). STILL OPEN in this class (the mixed-accumulator members, need the
extraction treatment, not a fold): E4 event commerce-rate PERCENT modifiers (city+player), E5 event
spaceProduction, E6 WB stateReligion production pokes, E7 tradeRoutes stores (its own Cut-1 step).

| # | channel | source |
|---|---|---|
| E1 | yields | event per-building yield grants (`m_aBuildingYieldChange` keyed store; city + player applyEvent) |
| E2 | yields | VOTE-source religion-linked per-building yield grants (`processVoteSourceBonus` — the COMMERCE sibling has the persisted store; the YIELD side has nothing and is named nowhere) |
| E3 | commerce | event/vote per-building commerce FLAT (Event `BuildingCommerceChanges`; VoteSource `ReligionCommerce`) — the `m_aBuildingCommerceChangeEvents` fold is documented as plan, not present in Sources/Cascade |
| E4 | commerce | event commerce-rate PERCENT modifiers, city + player scope (`EventInfo CommerceModifier`) |
| E5 | buildRate | event `SpaceProductionModifier` (player, non-city applyEvent) |
| E6 | buildRate | WorldBuilder pokes on stateReligion unit/building production modifiers |
| E7 | tradeRoutes | `INITIAL_TRADE_ROUTES` define (currently 0) + the WB pokes (CvGame/CvCity) — fold as raw inputs at the tradeRoutes flip |

**Bonus/civic percent tags (verify data-live, then fold or guard):** bonus own `CommerceModifier`
(`processBonus :4539`), civic `BonusCommerceModifiers` (`:18175`), civic `BonusMintedPercents` →
`getMintedCommerceTimes100` (`:18172`, COMMERCE_GOLD). Plus happiness structurally-absent-but-zero-valued
today: improvement own happiness + civic `ImprovementHappinessChanges`; health: feature radius health
(`updateFeatureHealth` — verify vs the wellbeing plot walk); yields: `m_aiExtraYield` framing — it is NOT
folded as an input anywhere (the legacy oracle itself excludes it); only the corporation share is
owner-ruled-accepted, the event/vote share is an open TODO.

## 4. Refuted-but-caveated (safe TODAY, each proven data-dead — future-authoring traps)

AREA cross-city yield percents (`AreaYieldModifiers` — 0 authored in all 14 XML files + all 5238 curated
JSONs; the `CvCascadePercentStack.cpp:24` comment naming `areaPercentByArea` is stale/aspirational — the
walk does not exist), trait `SpecialUnitProductionModifier`, building `BonusDefenseChanges`, and the
event-modifier rows above where the refuter's ground was "honest-divergence stance documented". **Each
becomes a real live gap the day someone authors the data.** Follow-up: a validator/curator guard that flags
authoring into an unwalked family.

## 5. Structural findings (confirmed; not cut-gating by themselves)

- **No generic family-metadata combine engine exists** — nine hand-written per-channel combines, vs the
  scope-packages Phase-1 commitment (the one-path law). Each further channel migration compounds it.
- **buildRate read path does `std::map::find` tree lookups** (`acc_brLookup`) — violates the
  generic-code-static-storage law on a documented hot path (correct values, latent perf).
- **`refreshPlayerScope` evaluates player-scope fills in the capital's ctx** — a latent Burdigala instance
  if a city-conditioned deposit is ever authored at player scope (none today; the guard belongs in the
  validator).

## 6. Full detail

The complete per-row evidence (every covered row's proof, refutation texts, reviewer summaries) lives in the
session artifacts (74-agent workflow `wf_1f3dc3e4-dbc`, 2026-07-04) — transient by design; everything
actionable is in the tables above. Dispositions land back into [code-cut-map.md](code-cut-map.md) rows /
[duplicate-surface.md](duplicate-surface.md) as they are ruled.
