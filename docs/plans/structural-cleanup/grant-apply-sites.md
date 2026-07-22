# The grant APPLY-SITE map — where provisions are actually handed over today

> **Why this exists.** [grants-machine.md](grants-machine.md) specs the machine and carries a trigger→legacy-site
> *inventory*. That inventory was checked domain-by-domain against live code and was found to **understate the
> surface in every single domain**, with drifted line numbers throughout and at least one row citing a function
> that does not exist. The apply surface is not reconstructible from memory — it accreted over fifteen years
> across two languages — so it is mapped here, once, with `file:line` for each site.
>
> **This doc is the map. [grants-machine.md](grants-machine.md) stays the machine's spec.** Do not fold them
> together: one is "what the machine is", this is "what it must replace".
>
> ⛔ **The apply cannot be moved before the CLASSIFICATION below is ruled on.** Several sites look like grants and
> are not; several that were classified as not-grants provably are.

## 0. ⚖ THE SCOPE RULE (owner) — what the machine is for

> **The grants machinery's job is to UNIFY ALL THE RANDOM PLACES THAT ADD RANDOM THINGS TO THE MAP, OUTSIDE
> "NORMAL CREATION"** — construct / research / train / adopt / purchase.

That is the membership test, and it is deliberately broad: the qualifier is *how the thing arrived*, not which
info class declared it. If something appears in the world and the player did not build, research, train, adopt or
buy it, the machine owns it. Consequences that settle most of §5:

- **IN** — goody huts, random events, espionage payouts, vote awards (including a whole city), outcome reward
  payloads, NPC/barbarian spawners, combat loot / pillage / capture units, plot bonus discovery, leader-level-up
  traits, the trait provisions curated as identity keys (`cityStartCulture`, `bonusPopulationinNewCities`,
  `citiesStartwithStateReligion`, `draftsOnCityCapture`, `extraGoody`), `barbarianInitialDefenders`, the
  start-era `freePopulation` / `FreeStartEra` buildings. **A missing `grants` block is not an exemption** — where
  the curator filed a provision elsewhere, that is a curation question, not a scope question.
- **OUT — normal creation.** A built building, a trained unit, a researched tech, an adopted civic, an
  advanced-start *purchase*. (The advanced-start *budget* is granted; what it buys is normal creation.)
- **OUT — not an arrival.** A transfer or transformation of something already owned: `CvUnit::convert`
  (upgrade/merge promotion carry-over), unit merge/split, scrap refunds, production→gold overflow, negotiated
  deal/gift transfers, improvement upgrades.
- **OUT — modifiers.** Anything alive only while its source is (§4) never "arrived" independently; it is the
  source's ongoing effect, and it belongs to [modifier.md](../../specs/modifier.md).

> **⛔ THE MACHINE REPLACES LEGACY — IT NEVER WIDENS IT (owner ruling).** The goal is a FULL replacement and a
> **vast reduction of endpoints**: many scattered apply sites collapse into one. So a change that *widens a legacy
> apply path to accommodate the machine* is backwards by construction — it grows the endpoint count in service of
> a machine whose whole purpose is to shrink it. The concrete instance that keeps tempting agents: the building
> `repeatable` data loss (§3.1) looks like it wants `CvBuildingInfo::mapFrom` widened to carry `interval`/`enabled`
> into the legacy collapse members (`m_iNumUnitFullHeal`, `m_iPropertySpawnUnit`/`Property`, `m_healUnitCombats`).
> **It does not.** Those members are the legacy shape being deleted; the machine reads the composed
> `getGrants()->repeatables()` — which already carries interval, chance, the spatial intent and the `enabled`
> condition in full ([CvJsonGrants.h](../../../Sources/Infos/CvJsonGrants.h) `CvJsonGrantRepeatable`) — and the
> collapse members die with the city ledgers. Widening them would be the transitional shim
> [DEC-proper-once](../../architecture/decisions.md#dec-proper-once) bans, applied to a member already condemned.
> ⚠ Do NOT confuse the two `getNumUnitFullHeal`s: the **info-side** `CvBuildingInfo::getNumUnitFullHeal()` is
> static data with live READ consumers (AI valuation `CvCityAI.cpp:5473/12752/13255`, pedia
> `CvGameTextMgr.cpp:15923`) and **STAYS**; only the **city-side** accumulator `CvCity::m_iNumUnitFullHeal` is cut.

## 1. The classification that decides ownership

Three distinctions, each of which has already been got wrong at least once:

- **GRANT** — handed over once, then persists with no living source. The machine's business.
- **MODIFIER** — alive only while its source is; refcounted or toggled with the source's presence. NOT the
  machine's business ([modifier.md](../../specs/modifier.md) — the `freeSpecialists` precedent).
- **READ** — a consumer of the same info data for pedia/AI/UI. Not an apply, but **must keep working when an
  apply moves**, so it is mapped separately rather than discarded.

And, orthogonally:

- **LEGACY-THAT-STOPPED** — a legacy behaviour whose apply is dead today (usually a stubbed poco getter).
  Needs a ruling: was the drop intended?
- **NEW-DESIGN-NOT-YET-BUILT** — authored data for a mechanic that never had an apply, awaiting the machine.
  Not a regression. `grants.foundBuildings` (settlers seeding buildings at settle time) is this: a **new
  mechanic coined for this rework** (owner), not a port of the legacy `bNewCityFree`.

> **⚖ Reuse the engine's own partition — do not re-derive one.** `CvPlayer::applyEvent` takes
> `iEventTriggeredId == -1` to mean "replay the MODIFIER effects only" (`adjustModifiersOnly`,
> `CvPlayer.cpp:21270`, twin in `CvCity.cpp:17856`), and every one-shot handout in the event surface is already
> guarded by `!adjustModifiersOnly`. That flag is an existing, load-bearing, hand-audited grant-vs-modifier split
> over the whole event surface. Adopt it.

## 2. Apply sites by domain

Line numbers verified against the live tree at the time of writing; treat them as leads, re-confirm the function.

### Unit-created
| granted | site | function |
|---|---|---|
| free promotions (unit `grants.promotions` + player registries + trait dict) | `Engine/CvUnit.cpp:414` → `:26009` → `:25938` | `init` → `doSetFreePromotions` → `setFreePromotion` |
| free-to-unitcombat promotions | `Engine/CvUnit.cpp:25672` | `checkFreetoCombatClass` |
| default status promotions | `Engine/CvUnit.cpp:30007` → `:25768` | `doSetDefaultStatuses` → `statusUpdate` |
| random starsign promotion | `Engine/CvUnit.cpp:30773` | `doStarsign` |
| free XP + building free promotions | `Engine/CvCity.cpp:3006`, `:3013` → `:21495` | `addProductionExperience` → `assignPromotionsFromBuildingChecked` |
| golden age on GP birth (trait) | `Engine/CvPlayer.cpp:20123` | `createGreatPeople` |
| the Great Person unit itself | `Engine/CvPlayer.cpp:20116` | `createGreatPeople` |

### City-founded
| granted | site | function |
|---|---|---|
| free population (start-era) | `Engine/CvCity.cpp:352` | `init` |
| civilization buildings | `Engine/CvCity.cpp:371`; `CvPlayer.cpp:11094/11098`, `:5740` | `init`, `setCapitalCity`, `findNewCapital` |
| `FreeStartEra` buildings | `Engine/CvPlayer.cpp:6257` | `found` |
| trait: city-start culture / bonus population / state religion | `Engine/CvPlayer.cpp:6265`, `:6272`, `:6279` | `found` |
| NPC initial defenders (handicap) | `Engine/CvPlayer.cpp:6246` | `found` |
| free defenders on revolt/culture flip | `Engine/CvPlot.cpp:6542` | `setOwner` |

### Building
| granted | site | function |
|---|---|---|
| first-build block: population, free tech (`grants.techs`), golden age, empire population, `freeTechs` | `Engine/CvCity.cpp:13599-13697` | `setupBuilding` (`bFirst`) |
| per-turn unit spawn / full heal / per-unitcombat heal | `Engine/CvCity.cpp:22081`, `:20220`, `:4348` | `doPropertyUnitSpawn`, `doHeal`, `processBuilding` |
| free bonuses (two sites — **both must move together**) | `Engine/CvCity.cpp:4238`, `:12175` | `processBuilding`, `addProvidedBonusesToGroup` |
| autobuild placement/removal | `Engine/CvCity.cpp:1425`, `:1438` | `doAutobuild` |
| corporation HQ free unit | `Engine/CvCity.cpp:4881` | `setHeadquarters` |

### Game start
| granted | site | function |
|---|---|---|
| starting gold | `Engine/CvPlayer.cpp:1801`, `:1807` | `initFreeState` |
| start-era free techs; civ free techs | `Engine/CvGame.cpp:1526`, `:1535` | `CvGame::initFreeState` |
| initial civics (4 sites) | `CvPlayer.cpp:467`, `:1466`, `:18372` (raw write on LOAD) | `initMore`, `resetCivTypeEffects`, `read` |
| starting units + creation | `CvPlayer.cpp:1861-1889`, `:1935` | `initFreeUnits`, `addStartUnitAI` |
| advanced-start budget | `CvPlayer.cpp:1834`; pool `CvInitCore.cpp:1664` | `initFreeUnits` |

### Tech / religion / civic
| granted | site | function |
|---|---|---|
| first-discoverer free unit / prophet | `Engine/CvTeam.cpp:5173`, `:5187` | `setHasTech` |
| first-discoverer free techs (AI / human) | `AI/CvPlayerAI.cpp:6393` / `UI/CvMessageData.cpp:469` | `AI_chooseFreeTech` / `CvNetResearch::Execute` |
| religion founder free units | `Engine/CvPlayer.cpp:8651` | `foundReligion` |
| holy city religion + influence | `Engine/CvGame.cpp:5855` | `setHolyCity` |
| civic `revolution` pulse | **Python**, `Revolution/Gameready/Revolution.py:929` | `checkCivics` (polled) |

### Subsystems with NO inventory row at all
| granted | site | function |
|---|---|---|
| goody huts: gold, research, tech, XP, heal, reveal, free unit, barbarians | `Engine/CvPlayer.cpp:5915-6065` | `receiveGoody` |
| random events: gold, esp, research, golden age, bonus, religion, units, pop, culture, promotions | `CvPlayer.cpp:21261+`, `CvCity.cpp:17851+`, `CvUnit.cpp:21743`, `CvPlot.cpp:12542` | `applyEvent` |
| espionage: stolen gold/tech, bribed worker, culture | `Engine/CvPlayer.cpp:16000+` | `doEspionageMission` |
| votes: **an entire city**, pacts | `Engine/CvGame.cpp:8275`, `:8389` | `processVote` |
| outcome reward payloads (`outcomes.kill[]`/`actions[]`) | `Engine/CvOutcome.cpp:1045+` | `execute` |
| NPC per-turn spawns | `Engine/CvGame.cpp:6653` | `doSpawns` |
| combat loot / pillage / capture / blockade gold | `CvUnit.cpp:2456`, `:7564`, `:1537`; `CvPlayer.cpp:2279` | various |
| leader trait granted on culture level-up | `Engine/CvPlayer.cpp:29249` | `doPromoteLeader` |
| plot bonus discovery | `Engine/CvPlot.cpp:812` | `doBonusDiscovery` |

### Python (genuine granting, in a layer scoped to stay Python)
`CvEventManager.py` — promotions on unit built (`:2010`), free units from popups (`:580`), settle-time culture
buildings (`:2443`, the only *live* settle-time seed), settler population (`:2459`), tech-triggered free units
(`:2229`, `:2257`), Nazca building bonuses (`:2301`) · `InitMilitaryPromos.py:131` · `RevEvents.py:740` ·
`BarbarianCiv.py:580`.

## 3. Live defects this map surfaced

1. ~~Building `grants.repeatable` drops `interval` / `enabled` / `chance`.~~ **FIXED — by ownership, not by
   widening.** `CvBuildingInfo::mapFrom` still collapses each entry into the legacy members (which keep NO
   interval/enabled/chance), but those members now serve only the AI/pedia READ consumers. The per-turn APPLY
   moved to the grants machine, which reads the composed `getGrants()->repeatables()` and honours
   `intervalPerTurn`, the `enabled` condition (through `cascadeEvalCondition`) and the property-scaled chance —
   gated on the operating-building set, so a dormant building grants nothing. The legacy sites
   (`CvCity::doPropertyUnitSpawn`, `CvCity::doHeal`, `changePropertySpawn`/`changeNumUnitFullHeal` and their
   `processBuilding` feeds) are DELETED. ⚠ Still open: **`m_aPropertySpawns`'s serialization survives as an inert
   stream drain.** Its shape is a count tag named `iNumElts` — **shared by 23 variable-length blocks in
   `CvCity::read`** — followed by N raw untagged records, so naming it in `savemigration.txt` would drain the
   wrong block, and dropping the write would orphan a tag old saves carry (leaving `iNumElts` holding the previous
   block's value and reading garbage). It retires when those blocks get per-block tags.
   `m_iNumUnitFullHeal` — a plain named scalar — WAS fully soft-removed (`Assets/savemigration.txt`).
2. **Three `changeFreeSpecialistCount` pushes are silently dropped.** The body no-ops unless `bUnattributed`
   (`CvCity.cpp:13163`, default `false` at `CvCity.h:1079`), and the cascade side sums only building/civic/trait
   deposits — so event grants (`CvCity.cpp:17979`), vote-source grants (`:14185`) and the espionage
   assassinate-specialist `-1` (`CvPlayer.cpp:16055`) all vanish.
3. ~~The game-start resolver reads the wrong era.~~ **FIXED** — `gr_resolvePlayerInit` now reads
   `GC.getGame().getStartEra()`, matching every legacy site. (Verified by compile only: era-sourced game-start
   grants fire at NEW GAME, so they cannot be exercised on the standing late-game save.)
4. **`SEVT_PLAYER_INIT` does not fire for every player who receives grants** — `initFreeUnits` early-returns on
   a null starting plot *before* the emit, and is only called for players with zero units and zero cities.
   Gold is also applied 21 lines *before* the emit (`CvGame.cpp:994` vs `:1015`).
5. **The religion founder grant is dead under `GAMEOPTION_RELIGION_DIVINE_PROPHETS`** — `foundReligion`
   early-returns (`CvPlayer.cpp:8543`) and `CvUnit::spread` founds instead, granting nothing and never emitting
   `emitReligionFounded`. It also mixes `eSlotReligion` (count) with `eReligion` (unit type).
6. **Both property-band machines have live code and zero data** — `m_aPropertyBuildings` /
   `m_aPropertyPromotions` are never written; the bands are live instead through the F5 watermark →
   `requires.operate` dormancy, a different semantic (operating, not presence).
7. **Legacy-that-stopped:** `getFreeBuilding`/`getFreeAreaBuilding` → `-1` (404 authorings, chain + save fields
   intact); `isApplyFreePromotionOnMove` → `false`, making `CvCity::doPromotion` unreachable so a unit that walks
   into a city never gains the building's promotions.

## 4. Classification results (settled by reading the code)

**MODIFIER, not grant** — `getFreeBuilding`/`getFreeAreaBuilding` (refcounted ±1 with source presence);
building `getFreeTraitTypes` ("conferred while active"); vote `tradeRoutes`/`isFreeTrade`/`isNoNukes`/`forceCivic`
(reversed on repeal); vote-source religion yields; building/civic/trait `freeSpecialists`.

**GRANT, contradicting the earlier reclassification** — the **unattributed** free-specialist ledger
(`m_paiFreeSpecialistCountUnattributed`) is genuine one-shot state: Great-Person `join` consumes the unit so no
source survives (`CvUnit.cpp:8778`), city acquisition carries it (`CvPlayer.cpp:2606`), and **era-advance free
specialists are a persisted pulse, not a while-active modifier** (`CvPlayer.cpp:12187`) — which pins the lifetime
question [grants-machine.md](grants-machine.md) left open.

## 5. Open rulings (blocking the apply)

1. ~~Python boundary.~~ **SETTLED (owner): Python events do NOT use grants yet.** The first pass of the machine
   is **DLL-scoped**, and the Python granting catalogued in §2 stays where it is — a KNOWN and accepted parallel
   for now, not an oversight and not a gap to close opportunistically. ⛔ Do NOT wire `CvEventManager` /
   Revolution / `BarbarianCiv` handouts into the machine, and do not claim "one place" without the DLL
   qualifier. "Yet" is deliberate: the boundary moves when the owner says so, and the map above is what that
   later pass will work from.
2. **Conquest re-grant.** `bFirst=false` on city acquisition (`CvPlayer.cpp:2572`) is the engine's deliberate
   "don't re-fire grants on conquest" switch. **CODE STATE — the carrier is BUILT:** `SEVT_BUILDING_CHANGED` now
   carries `bFirst` (`emitBuildingChanged(..., bool bFirst)`, `CvEventSpine.h:322`) — the real flag from
   `CvCity.cpp:13501`, hard `false` from the load reseed at `:16752` (a load RESTORES, it is not an acquisition) —
   and the machine consumes it (`s_bFirstAcquire = (e.iA != 0)`, `CvCascadeGrants.cpp:256`), emitting it as
   `firstAcquire` beside `suppressed` so the withholding REASON is on the wire. What the apply must then honour is
   the engine's own semantic; the ruling itself is not closed here.
3. ~~Serialized ledgers.~~ **SETTLED (owner): the machine REPLACES the existing per-turn work**, so the ledgers
   feeding it become DERIVED. All three are written only by `CvCity::processBuilding` (`changePropertySpawn`
   `:4255`, `changeHealUnitCombatTypeVolume` `:4352`, `changeNumUnitFullHeal` `:4370`, all
   `kBuilding.getX() * iChange`) — no event/vote/espionage writer exists — so each is a Σ over the city's buildings
   of a static info field: the STORED-ACCUMULATOR DRIFT class, cut by
   [DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform) via the
   `Assets/savemigration.txt` soft-remove ([save.md §3](../../specs/save.md)) — delete member + read + write, name
   the tag, no `WRAPPER_SKIP_ELEMENT`, **no `@SAVEBREAK`** (field removal is not a save break).

   ⚠ **But they are NOT one class — the replacement OWNER differs (§1: "several sites look like grants and are
   not"):**
   | ledger | mechanic | replaced by |
   |---|---|---|
   | `m_aPropertySpawns` | per-turn unit spawn — an arrival outside normal creation | **the grants machine** |
   | `m_iNumUnitFullHeal` | discrete per-turn action (fully heals up to N damaged units, `CvCity::doHeal` `:20202`); [json.md §5](../../specs/json.md) names a heal as a `repeatable` payload | **the grants machine** |
   | `m_paiHealUnitCombatTypeVolume` | **continuous heal-RATE contribution**, not a discrete event — consumed at `CvUnit.cpp:6232` (`iTotalHeal += pCity->getHealRate() + pCity->getHealUnitCombatTypeTotal(...)`) and already a named term in the `/computed/units/heal` decomposition | **the MODIFIER heal channel** (alive-with-source ⇒ modifier, the `freeSpecialists` precedent) — F4's unit-side heal channel is built; this is its city-scope deposit |
4. ~~Scope of "grant".~~ **SETTLED by the §0 scope rule** — arrival-outside-normal-creation decides it. What
   remains is not scope but CURATION: several in-scope provisions are not authored in a `grants` block at all
   (the five trait keys, `barbarianInitialDefenders`, the advanced-start budget, `FreeStartEra`), so the machine
   cannot resolve them off `getGrants()` until the curator emits them there
   ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision)).
5. **Start-era grants applied forever.** `freePopulation` and `FreeStartEra` buildings fire at *every* city
   founding, not at game start — both are in scope per §0; the open question is only which TRIGGER owns them
   (city-founded, not player-init). Grounded evidence for the ruling: legacy fires `freePopulation` at
   `CvCity::init` (`:352`, reading `GC.getGame().getStartEra()`) and `FreeStartEra` at `CvPlayer::found` (`:6257`)
   — both at city-founding. ⛔ **Blocked on a missing EMIT, not only on the ruling:** there is no city-founded
   DOMAIN event. The three `emitCityOwnerChanged` sites are `CvCity::read:16269` (the load reseed,
   `NO_PLAYER`→owner), `CvPlayer.cpp:2306` (dispose) and `:2696` (acquire) — a settler founding a city in live play
   emits nothing, so the machine has no trigger to hang these on. Per
   [event-spine.md](../../specs/event-spine.md) the fix is to EMIT the fact, never to work around its absence.

## See also
- [grants-machine.md](grants-machine.md) — the machine + its build increments (its inventory table is superseded
  by §2 here). · [json.md §5](../../specs/json.md) — the `grants` vocabulary. ·
  [mission-outcome-system.md](../../reference/mission-outcome-system.md) — the missions carve-out, which covers
  the four hardcoded ability keys and **not** the outcome reward payloads.
