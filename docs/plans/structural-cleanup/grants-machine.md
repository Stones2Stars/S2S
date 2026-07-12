# The grants machine — in-DLL build plan ("provisions")

> The cascade's **provisions** consumer: an `IEventConsumer` on the event spine ([event-spine.md](../../specs/event-spine.md))
> that, on a `DOMAIN` state-change, resolves the **source entity's genuine grants** off the mapped `CvJsonInfo`
> (`grantLists`/`grantPulses` in the per-type `InfoRepo`) and — at the cutover — applies them; the apply-loop must
> MANIFEST in-game ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)). Design authority:
> [json.md](../../specs/json.md) §5 (`grants`). The **classification pass** (2026-07-01, [data-migration-remaining.md](data-migration-remaining.md))
> cleaned the surface: `grants` now holds **only genuine provisions handed out on a trigger** — the mis-homed keys
> moved out, and the unit activated-MISSION keys are a **PERMANENT carve-out (missions pass)** ([mission-outcome-system.md](../reference/mission-outcome-system.md)).

## The genuine grant inventory (post-classification) — trigger + legacy apply-site

| grant (holder) | trigger (→ DOMAIN event) | legacy apply-site | shape |
|---|---|---|---|
| unit `promotions` | unit created | `CvUnit::init`→`doSetFreePromotions` | one-shot on-create |
| unit `foundBuildings` | city founded | `CvPlayer::found` (NewCityFree) | one-shot on-settle |
| building `repeatable` (spawn/heal) | per-turn | `CvCity::doPropertyUnitSpawn` / `doUnitFullHeal` | recurring |
| building `freePromotions` | end-turn | `assignPromotionsFromBuildingChecked` | recurring |
| building `freeTechs` / `population` / `goldenAge` | first build | `CvCity` `bFirst` block (:14803/:14724/:14764) | one-shot |
| trait `freePromotions`(dict) / `goldenAgeOnBirthOfGreatPerson` / ~~`eraAdvanceFreeSpecialist`~~ | unit-init / GP-birth / ~~era-advance~~ | `CvPlayer` trait paths | recurring/pulse |

> **⚖ freeSpecialists reclassified OUT of grants (owner ruling 2026-07-02):** a free specialist is alive only as
> long as its source is (building/civic/trait active) — the continuous **modifier** shape
> ([modifier.md](../../specs/modifier.md) §Specialist counts), so the freeSpecialist-shaped entries (incl. the trait
> `eraAdvanceFreeSpecialist` row above) belong to the modifier plane's `freeSpecialists` family, not this machine.
> Pin the exact legacy lifetime semantics of the era-advance path against the modifier family when that channel is built.
| civic `revolution` | civic switch | RevolutionDCM **Python** (no DLL apply) | one-shot pulse |
| civ `civics` / `techs` / `buildings` | game start / first city | `CvPlayer` init / `CvCity::init` | game-start |
| tech `firstFreeUnit` / `firstFreeProphet` / `freeTechs` | first to discover | `CvTeam::setHasTech` (5452+) | one-shot on-discover |
| religion `numFreeUnits` / `freeUnit` | religion founding | `CvPlayer::setHolyCity` block | one-shot on-found |
| era / handicap `starting*` / `freePopulation` / `ai` | game start | `CvPlayer::initFreeUnits`/`initFreeState` | game-start |
| property `buildings` | property present | (#430 pending — auto-built) | continuous |
| feature/improvement property-pulses | per-turn | `CvPropertySolver::doTurn` (spatial, #429) | recurring |

## The machine — `CvCascadeGrants` (`Sources/Cascade/CvCascadeGrants.{h,cpp}`)

An `IEventConsumer` (`wantedKinds` = DOMAIN), registered at the composition root (`cascadeRegisterConsumers` →
`cascadeRegisterGrants`). `onEvent` dispatches by the DOMAIN event, resolves the source entity's genuine grants off
`InfoRepo<CvXInfo>::get().get(id)` (`grantLists`/`grantPulses`, the carved-out missions-pass keys simply not read), and emits a
`[GRANTS]` diagnostic via the spine (`SD_GRANTS`). **Resolution only — it does NOT apply** (legacy applies); the
resolved set is observable via `[GRANTS]`, and the apply-loop (increment 5) must MANIFEST in-game once built ([DEC-done-is-observable]).

## Build increments (each compiles before the next)

1. **Slice-1 ✅ DONE** — the consumer + the `[GRANTS]` domain + resolution over the DOMAIN events the spine emits
   **today** (building-built `CASCADE_EVT_BUILDING_COUNT` delta>0, unit-created `CASCADE_EVT_UNIT_COUNT` delta>0):
   building genuine grants (`repeatable`/`freePromotions`/`freeTechs`) + unit genuine grants (`promotions`/
   `foundBuildings`). Emits `[GRANTS/building]` / `[GRANTS/unit]` (gated). Assert green.
2. **The richer grant MAPPING**
   - **2a ✅ DONE** — `rj_walkGrants` now captures the **bool grants** (`goldenAge` → `CvJsonInfo::grantFlags`) and the
     **scoped-pulse dict grants** (`population {city|empire:N}` → `grantScopedPulses`, ×100); object-valued dicts (the
     carved-out missions-pass key `greatPersonAction`) are correctly skipped (no number leaves). The machine's `[GRANTS/building]`
     now surfaces `goldenAge` + `population` (pulses shown ÷100 = human count). No JSON regen — pure readJson enrichment.
   - **2b ✅ DONE** — the **`repeatable` STRUCTURE** is now captured: `rj_parseRepeatable` fills
     `CvJsonInfo::grantRepeatables` (a `CvCascadeGrantRepeatable` per entry) with the payload (unit spawn / unitCombat
     heal / PROPERTY_* pulse), `interval`, the `chance:{per}` scaler id, and the #429 spatial `on`/`relation`/`distance`.
     This fixed a real gap — the old generic id-only capture missed the **149 heal + 49 property** entries entirely (only
     the 26 `{unit}` spawns resolved). `[GRANTS/building]` `repeatable` now counts the full structured set. Pure readJson.
     *(Followed for consistency: `heal:"full"` + property-amount ×100 vs raw; the enabler/#429 consumers
     that ACT on these — the per-turn spawn/heal apply + the spatial property distribution — are increments 3/5.)*
3. **The remaining DOMAIN triggers** — the spine emitted only building/unit-count + name-change; the grant inventory
   needs more. Each is a new synced `DOMAIN` event emitted from a deterministic engine state-change site.
   - **3a ✅ DONE — tech first-discover** (`CASCADE_EVT_TECH_ACQUIRED`): emitted from `CvTeam::setHasTech` inside the
     `bFirst && countKnownTechNumTeams==1` block (the exact site where `firstFreeUnit`/`firstFreeProphet`/`freeTechs`
     fire), carrying the tech + discovering player. The machine resolves them → `[GRANTS/tech]`.
   - **3b ✅ (religion + civic done)** — `CASCADE_EVT_RELIGION_FOUNDED` (`CvPlayer` at `setHolyCity`, the founder-grant
     site → `numFreeUnits`/`freeUnit` → `[GRANTS/religion]`) and `CASCADE_EVT_CIVIC_ADOPTED` (`CvPlayer::setCivics`, a
     newly-adopted non-NPC civic → `revolution` pulse → `[GRANTS/civic]`).
   - **3c ✅ (game-start done)** — `CASCADE_EVT_PLAYER_INIT` (`CvPlayer::initFreeUnits`, where the player's civ/era/
     handicap are set): the machine resolves the whole game-start set at ONE trigger — civilization `civics`/`techs`/
     `buildings` + era/handicap `startingGold` → `[GRANTS/gameStart]` (the legacy apply is spread across init sites;
     the cascade resolves it in one place off `InfoRepo<CvCivilizationInfo>`/`<CvEraInfo>`/`<CvHandicapInfo>`).
   - **All the clean event triggers are now wired** (building/unit/tech/religion/civic/game-start). What remains is NOT
     a resolution trigger: city-founded is minor (`foundBuildings` already resolve on unit-created), and the **per-turn
     recurring** apply (`repeatable` spawn/heal + `freePromotions`) belongs to increment 5 (the cutover apply-loop).
4. **Live manifestation check** — grants are event-driven side-effects; the acceptance is that the resolved grant-set is
   observable via the `[GRANTS]` diagnostic, and — once the apply-loop lands (increment 5) — the granted effect MANIFESTS
   in the running game per channel ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)); not a re-run of parity/shadow.
5. **Apply + cutover** — replace the legacy grant application; the grants machine applies. Atomic with the cascade cutover.

## See also

- [json.md](../../specs/json.md) §5 (`grants`) · [event-spine.md](../../specs/event-spine.md) (the `IEventConsumer` front door) ·
  [mission-outcome-system.md](../reference/mission-outcome-system.md) (the carved-out missions-pass keys, NOT grants) ·
  [data-migration-remaining.md](data-migration-remaining.md) (the classification pass).
