# State-mapping sweep findings — 2026-06-18 (multi-agent, Sonnet fan-out)

> Durable capture of the `state-mapping-sweep` workflow output (8 slices, 219 behaviours). The EXPANDED map the
> owner asked for after judging the §14 H inventory too shallow — it confirms only ~15-20% of the per-turn/per-event
> state surface is observable today. Companion to `cascade-mapping-inventory.md` (curated design table) + the
> per-system `docs/dev/reference/observability/` maps. Agent-generated; claims cite file:line — trust-but-verify.

# S2S Engine State-Mapping Sweep — Consolidated Observability Inventory

## 1. State of the Map

The per-turn and per-event state surface of the S2S engine is roughly 15-20% fully observable from outside the process today. The three snapshot endpoints (/units, /players, /cities) expose a coarse high-water-mark reading of unit damage/level/role, player gold/tech-count/score, and city population/yield-rates/culture-level/three-property-values — all up to 5 seconds stale. The /events SSE stream adds turn-boundary signals and gated log tees ([SPINE/DOMAIN] building/unit count deltas, [CIT/proplevel] property snapshots, [CIT/produced] completions, [UNT/role] role changes) that push the coverage higher for the narrow cascading-count domain. The /diagnostic family (canConstruct, sweep, placementSweep, whyNot) fills the on-demand gate-eval gap for buildings and units. Against this, the sweep finds eight major opaque clusters that together cover the majority of per-turn state mutations: (a) all city anger/happiness/timer fields (eight timer decrements per turn, WLTK, war weariness, occupation), (b) all dormancy state for present-but-inactive buildings (religious disabling, resource disabling, replacement suppression — none of which are in any endpoint), (c) the full culture pipeline (plot culture arrays, city culture balance, revolt probabilities, culture rates — only the discrete level tier is visible), (d) religion presence and spread/decay mechanics, (e) the entire corporation system (presence, influence, maintenance, bonus chain), (f) espionage economy (point accumulators, per-team weights, mission costs, counterespionage timers), (g) AI diplomatic memory (attitude counters, memory counts, contact timers, war-plan state), and (h) the cascade's own tally — there is no /tally snapshot endpoint, making the accumulated empire/team/world counts unreconstructible from a point-in-time query. The property solver is partially observable (crime/education/disease on cities only) but operates across six scopes and many more property types with zero delta visibility. The prior §14 H mapping is confirmed shallow: it omitted the anger/timer cluster, the food-pipeline internals, the per-player finance breakdown (maintenance, unit upkeep, civic upkeep, hurry inflation), the AI diplomatic counter cluster, and the CvTeam diplomacy subsystem (deal verification, vote timers, circumnavigation, WW decay) entirely.

---

## 2. Deduplicated Behaviour Table

### CvCity::doTurn

| System | Behaviour (file:line) | State maintained | Trigger | Observable today | Cascade mapping | Priority |
|---|---|---|---|---|---|---|
| CvCity | Property level snapshot [CIT/proplevel] (CvCity.cpp:1234) | None — read-only observability hook | perTurn | partial — log only; /cities exposes crime/education/disease only | tally: property values drive requires.operate property-band atoms | med |
| CvCity | Defense damage recovery + flag resets (CvCity.cpp:1292) | m_iDefenseDamage, m_bBombarded/Plundered/Drafted/AirliftTargeted | perTurn | none | unmapped | med |
| CvCity | doPromotion — building-granted free promotions (CvCity.cpp:1304) | Unit promotion flags for units on city plot | perTurn | partial — /units shows level/type, not promotion set | unmapped | low |
| CvCity | doVicinityBonus — building yield on vicinity-bonus change (CvCity.cpp:1306) | Building yield rates via updateYieldRate | perTurn | partial — /cities shows resulting rates, not delta or cause | requires.operate: vicinity-bonus continuous gate (§14 F) | med |
| CvCity | checkBuildings — per-turn dormancy gating (CvCity.cpp:1308) | m_vDisabledBuildings (setDisabledBuilding true/false) | perTurn | none | requires.operate: §14 B-ii dormancy maintainer — shadow not yet built | high |
| CvCity | checkFreeBuildings — free-building reconciliation (CvCity.cpp:1309) | Free building presence (setFreeBuilding) | perTurn | partial — /cities building count changes, no attribution | autoBuild | low |
| CvCity | doAttack — city damages adjacent enemy units (CvCity.cpp:1312) | Enemy unit damage (changeDamage) | perTurn | partial — /units damage after-the-fact; no log tag | unmapped | low |
| CvCity | doHeal — city heals up to N friendly units (CvCity.cpp:1314) | Friendly unit damage (setDamage(0)) | perTurn | partial — /units damage = 0 afterward; no cause log | unmapped | low |
| CvCity | doCorporation — corporation spread/decay (CvCity.cpp:1316) | m_pabHasCorporation; competing corps removed on spread | perTurn | none | unmapped (§A opaque) | high |
| CvCity | doDisabledPower — power timer countdown (CvCity.cpp:1318) | m_iDisabledPowerTimer | perTurn | none | unmapped; feeds isPrereqPower dormancy in checkBuildings | med |
| CvCity | recalculatePopulationgrowthratepercentage (CvCity.cpp:1320) | m_fPopulationgrowthratepercentageLog | perTurn | none | tally/autoBuild: per-building growth-rate modifier | med |
| CvCity | doWarWeariness city-level (CvCity.cpp:1322) | m_iWarWearinessTimer (−20/turn), m_iEventAnger | perTurn | none | unmapped; anger inputs needed for Orwell bar | med |
| CvCity | doCheckProduction — maxed conversion + queue cleanup (CvCity.cpp:1327) | m_progressOnUnit/Building/Project, player gold, order queue | perTurn | partial — [CIT/cancel] logs dropped orders; no maxed-conversion attribution | unmapped (maxed path); requires gate (queue cleanup) | med |
| CvCity | Food accumulation / growth / starvation (CvCity.cpp:1329) | m_iFood, m_iFoodKept, m_iPopulation | perTurn | partial — /cities population; food rate visible; net delta/wastage/threshold not exposed | tally: food yield / population / health are DOMAIN quantities | high |
| CvCity | Food wastage nonlinear formula (CvCity.cpp:5923) | Effective foodDifference (surplus burned) | perTurn | none | unmapped; nonlinear modifier on surplus yield | high |
| CvCity | doCulture — per-turn culture accrual (CvCity.cpp:1331) | City culture changeCultureTimes100 | perTurn | partial — /cities cultureLevel tier only; no balance or rate | tally: culture accrual gates culture-level thresholds | high |
| CvCity | doPlotCulture — culture spread to plots (CvCity.cpp:1335) | m_aiCulture[ePlayer] on surrounding plots | perTurn | none | unmapped (§A opaque) | high |
| CvCity | doAutobuild — property-band + autoBuild placement (CvCity.cpp:1341) | Building presence (changeHasBuilding) | perTurn | partial — [PLACEMENT] + /diagnostic/placementSweep; property-band shows noMarker | autoBuild + requires.operate property-band (B-i; shadow running) | high |
| CvCity | doProduction — hammer accumulation + completion (CvCity.cpp:1343) | m_progressOnUnit/Building, overflow, player gold/research/culture | perTurn | partial — [CIT/produced]/[CIT/waste]/[SPINE/DOMAIN]; no banked-progress or overflow snapshot | enables (completion fires cascade); tally tracks counts | high |
| CvCity | doDecay — queued-but-inactive build decay (CvCity.cpp:1347) | m_progressOnBuilding/Unit for non-head queued items | perTurn | none | unmapped | low |
| CvCity | doReligion — religion spread/decay (CvCity.cpp:1349) | m_pabHasReligion; prereq-religion buildings removed on decay | perTurn | none | requires.operate: STATE_RELIGION_IN_CITY atom; unmapped for spread | high |
| CvCity | doGreatPeople — GP progress + spawn (CvCity.cpp:1351) | m_iGreatPeopleProgress, m_aiGreatPeopleUnitProgress | perTurn | none | tally: GP points accumulator toward threshold | med |
| CvCity | doMeltdown — nuclear building removal (CvCity.cpp:1353) | Building presence (changeHasBuilding(false)); plot nuked | perTurn | none | unmapped | low |
| CvCity | updateEspionageVisibility (CvCity.cpp:1355) | Per-team espionage visibility flags | perTurn | none | unmapped (§A opaque espionage) | med |
| CvCity | doPropertyUnitSpawn — property-driven unit spawning (CvCity.cpp:1359) | New units on map (crime→barb, positive→owner) | perTurn | partial — /units shows spawned unit; no spawn-event tag | unmapped | med |
| CvCity | Anger/timer decrements × 8 (CvCity.cpp:1369) | occupationTimer, hurryAngerTimer, revRequest/SuccessTimer, conscriptAngerTimer, defyResolutionAngerTimer, happinessTimer, landmarkAngerTimer | perTurn | none | unmapped; anger-input cluster — major Orwell bar gap | high |
| CvCity | espionageHealthCounter / espionageHappinessCounter (CvCity.cpp:1409) | m_iEspionageHealthCounter, m_iEspionageHappinessCounter | perTurn | none | unmapped (espionage §A) | med |
| CvCity | We Love the King Day (CvCity.cpp:1419) | m_bWeLoveTheKingDay; maintenance suppressed when true | perTurn | none | unmapped; WLTK inputs (anger, health, pop) need tally exposure | med |
| CvCity | Vicinity bonus snapshot save (CvCity.cpp:1432) | m_pabHadVicinityBonus/HadRawVicinityBonus | perTurn | none | requires.operate: delta-detection state for yield adjustment | low |
| CvCity | setCurrAirlift(0) (CvCity.cpp:1357) | m_iCurrAirlift | perTurn | none | unmapped | low |
| CvCity | cultureUpdateTimer countdown (CvCity.cpp:1364) | m_iCultureUpdateTimer | perTurn | none | unmapped | low |
| CvCity | City maintenance calculation — updateMaintenance (CvCity.cpp:7560) | m_iMaintenance (lazy) | onChange | partial — /players gold rate aggregate only | tally: per-city maintenance cost modifier | high |
| CvCity | Corporation maintenance per city (CvCity.cpp:21662) | Player gold (via total maintenance) | onChange | none | unmapped (§A corporations) | med |
| CvCity | Commerce rate per type — getCommerceRateTimes100 (CvCity.cpp:11854) | m_aiCommerceRate[COMMERCE_TYPE] | onChange | partial — /cities single commerce value; no per-type breakdown | tally: each commerce type is DOMAIN count | high |
| CvCity | Trade route commerce (CvCity.cpp:21801) | Commerce rate (via lazy commerce computation) | onChange | none | unmapped; building-enabled commerce modifiers | high |
| CvCity | Yield rate — getYieldRate / getYieldRate100 (CvCity.cpp:11231) | Computed from baseYieldRate × modifier + extra | onChange | partial — /cities food/production/commerce; no component breakdown | tally: yields are core DOMAIN quantities | high |
| CvCity | Building dormancy via replacement (CvCity.cpp:20656) | Building effective-active state (replacement suppresses predecessor) | perTurn | none | obsoletes/replaces (DESTRUCTIVE enables family; B-ii) | med |
| CvCity | Religious dormancy (CvCity.cpp:14921) | m_pabReligiouslyDisabledBuilding + processBuilding(±1) | onChange | none | requires.operate STATE_RELIGION; B-ii shadow not yet built | high |
| CvCity | checkBuildings — resource/bonus/war/civic/power dormancy (CvCity.cpp:20625) | m_vDisabledBuildings | perTurn | none | requires.operate (B-ii); no shadow | high |
| CvCity | Python cityDoTurn event (CvCity.cpp:1448) | Arbitrary Python-driven state mutations | perTurn | none | unmapped; black-box risk | med |

### CvPlayer::doTurn

| System | Behaviour (file:line) | State maintained | Trigger | Observable today | Cascade mapping | Priority |
|---|---|---|---|---|---|---|
| CvPlayer | Gold treasury accrual + bankruptcy cascade (CvPlayer.cpp:15472) | m_iGold, m_bStrike, m_iStrikeTurns | perTurn | partial — /players gold, goldRate | requires/tally: goldRate is composite; strike = bankrupt dormancy signal | high |
| CvPlayer | Research beaker accrual + tech completion (CvPlayer.cpp:15507) | CvTeam::m_paiResearchProgress, m_iOverflowResearch, m_pabHasTech | perTurn | partial — /players scienceRate, current research key; no progress int or overflow | enables: tech acquisition fires enables chain | high |
| CvPlayer | Espionage point accumulation + per-team distribution (CvPlayer.cpp:15580) | CvTeam::m_iEspionagePointsEver, m_aiEspionagePointsAgainstTeam | perTurn | none | unmapped (§A) | high |
| CvPlayer | AI espionage slider + spending-weight update (CvPlayerAI.cpp:16657) | COMMERCE_ESPIONAGE percent, espionageSpendingWeightAgainstTeam | perTurn | none | unmapped (§A) | high |
| CvPlayer | Anarchy timer countdown (CvPlayer.cpp:3783) | m_iAnarchyTurns, m_iRevolutionTimer, m_iNumAnarchyTurns | perTurn | none | requires: anarchy = failed-requires.operate for civics | high |
| CvPlayer | Golden age countdown (CvPlayer.cpp:3850) | m_iGoldenAgeTurns | perTurn | none | tally/requires: golden age = player-scope positive-requires state | high |
| CvPlayer | Hurry-inflation decay — doAdvancedEconomy (CvPlayer.cpp:27833) | m_iHurryCount (decaying per-player modifier on all costs) | perTurn | none | unmapped; decaying cost multiplier | high |
| CvPlayer | Civic verification + forced reset (CvPlayer.cpp:4079) | m_paecivics[eCivicOption] | perTurn | none | requires: civic legality = requires.operate gate | med |
| CvPlayer | War-weariness derived anger recompute (CvPlayer.cpp:10910) | m_iWarWearinessPercentAnger, CvTeam::m_aiWarWearinessTimes100 | perTurn | none | tally/requires: WW per-team accumulator with decay | high |
| CvPlayer | Trade-route yield assignment (CvPlayer.cpp:4272) | Per-city trade-route slot assignments | perTurn | partial — /cities commerce aggregate | unmapped; building-enabled commerce modifiers | med |
| CvPlayer | History record snapshots (CvPlayer.cpp:3963) | m_mapEconomyHistory etc. (turn-indexed maps) | perTurn | none | unmapped; culture accrual + stability need /players fields | med |
| CvPlayer | Population growth rate cache rebuild (CvPlayer.cpp:26576) | m_fPopulationgrowthratepercentageLog | perTurn | none | tally/autoBuild: per-building/civic growth-rate modifier | med |
| CvPlayer | Resource consumption statistics rebuild (CvPlayer.cpp:27129) | per-bonus resource consumption array | perTurn | none | unmapped (gated on MODDERGAMEOPTION_RESOURCE_DEPLETION) | low |
| CvPlayer | Corporate maintenance recompute (CvPlayer.cpp:27689) | m_iCorporateMaintenance | perTurn | none | unmapped (§A corporations) | low |
| CvPlayer | Gold commerce auto-correction (CvPlayer.cpp:17974) | m_aiCommercePercent[COMMERCE_GOLD] | perTurn | partial — /players goldRate shows result | unmapped; needs /players slider field | med |
| CvPlayer | AI diplomatic counters decay — AI_doCounter (CvPlayerAI.cpp:16348) | m_aiSameReligionCounter, DifferentReligionCounter, FavoriteCivicCounter, BonusTradeCounter, ContactTimer | perTurn | none | unmapped; AI-internal social tally | med |
| CvPlayer | AI memory count decay (CvPlayerAI.cpp:16420) | m_aaiMemoryCount[player][MemoryType] | perTurn | none | unmapped; AI-internal; RNG-gated per-type decay | med |
| CvPlayer | Unit upkeep lazy recompute (CvPlayer.cpp:10327) | m_iFinalUnitUpkeep | onChange | none | tally: per-unit upkeep-type cost; needs /players field | med |
| CvPlayer | City maintenance total lazy recompute (CvPlayer.cpp:10722) | m_iTotalMaintenance | onChange | none | tally: per-city maintenance costs; needs /players or /cities field | med |
| CvPlayer | Civic upkeep (CvPlayer.cpp:14260) | Computed from m_paecivics; feeds getFinalExpense | onChange | none | tally: civic upkeep per active civic | med |
| CvPlayer | Score computation Python delegate (CvPlayer.cpp:4414) | CvGame::getPlayerScore (cached) | perTurn | full — /players score | unmapped: component breakdown not in endpoint | low |
| CvPlayer | Key finance cache refresh (CvPlayer.cpp:8018) | m_iMinTaxIncome, m_iMaxTaxIncome | perTurn | none | unmapped: derived advisory cache | low |
| CvPlayer | Conscript count reset (CvPlayer.cpp:3793) | m_iConscriptCount = 0 | perTurn | none | unmapped | low |
| CvPlayer | Upgrade round count reset (CvPlayer.cpp:3795) | m_iUpgradeRoundCount | perTurn | none | unmapped | low |
| CvPlayer | Espionage commerce zeroed when no met civs (CvPlayer.cpp:3799) | m_aiCommercePercent[COMMERCE_ESPIONAGE] | perTurn | none | unmapped | low |
| CvPlayer | Random event trigger/expiry/countdown (CvPlayer.cpp:22470) | m_mapEventsOccured, m_mapEventCountdown, arbitrary player/city state | perTurn | none | unmapped; arbitrary state mutations | med |
| CvPlayer | Stability index EMA (CvPlayer.cpp:1842) | m_iStabilityIndexAverage | perTurn | none | unmapped; revolution risk completely opaque | med |

### CvTeam::doTurn + CvGame::doTurn

| System | Behaviour (file:line) | State maintained | Trigger | Observable today | Cascade mapping | Priority |
|---|---|---|---|---|---|---|
| CvTeam | Hominid tech diffusion (CvTeam.cpp:1025) | m_paiResearchProgress[eTech] for barb/neanderthal teams | perTurn | partial — /players techs count; no raw progress int | unmapped; NPC-only beaker trickle | high |
| CvTeam | Stolen-visibility timer decay (CvTeam.cpp:1056) | m_aiStolenVisibilityTimer | perTurn | none | unmapped | med |
| CvTeam | Counterespionage timer + mod reset (CvTeam.cpp:1061) | m_aiCounterespionageTurnsLeftAgainstTeam, m_aiCounterespionageModAgainstTeam | perTurn | none | unmapped (§A espionage) | med |
| CvTeam | War weariness decay (CvTeam.cpp:5770) | m_aiWarWeariness[iI] per opposing team | perTurn | none | unmapped; per-team-pair continuous decay counter | high |
| CvTeam | Circumnavigation bonus grant (CvTeam.cpp:5839) | CvGame::m_eCircumnavigatedTeam | perTurn | partial — /players: no field; globe unlock only via UI | unmapped; one-time enables at team scope | med |
| CvTeam | AI diplomacy counters increment — AI_doCounter (CvTeamAI.cpp:3789) | m_aiWarPlanStateCounter, AtWarCounter, AtPeaceCounter, HasMetCounter, OpenBordersCounter, DefensivePactCounter, ShareWarCounter | perTurn | none | unmapped | high |
| CvTeam | AI worst-enemy update (CvTeamAI.cpp:2957) | m_eWorstEnemy | perTurn | none | unmapped | low |
| CvTeam | AI area strategy update (CvTeamAI.cpp:227) | CvArea::m_aiAreaAIType[team] | perTurn | partial — [WAR/area] log on change only; no snapshot | unmapped (AI decision state) | med |
| CvTeam | AI war decision — AI_doWar (CvTeamAI.cpp:3884) | m_eWarPlan[iI]; may declare war | perTurn | partial — [WAR/*] logs on transition; war-plan type opaque | unmapped | high |
| CvTeam | Era advance via setHasTech (CvTeam.cpp:5306) | CvPlayer::m_eCurrentEra | perEvent | full — /players era field | enables: derived consequence of tech enables chain | med |
| CvTeam | AI research adjacency update (CvTeam.cpp:5289) | m_adjacentResearch vector | perEvent | partial — /players research shows current target only | enables: adjacentResearch = the forward-enables frontier | med |
| CvTeam | Tech share — updateTechShare (CvTeam.cpp:5788) | m_pabHasTech for allied teams | perEvent | partial — /players techs count changes; no share event | enables: conditional forward propagation | med |
| CvGame | Player/team score ranking update (CvGame.cpp:2425) | m_aiRankPlayer, m_aiPlayerScore etc. | perTurn | full — /players score | unmapped for rank ordinal | med |
| CvGame | Culture-victory cache reset (CvGame.cpp:9292) | m_iNumCultureVictoryCities, m_eCultureVictoryCultureLevel | perTurn | none | unmapped: derived config cache | low |
| CvGame | Deal AI peacetime-value accrual (CvDeal.cpp:323) | m_aiPeacetimeTradeValue, m_aiPeacetimeGrantValue | perTurn | none | unmapped; needs /deals endpoint | med |
| CvGame | Deal verification + expiry (CvDeal.cpp:371) | Active deal list; deal kill reverses open-borders/peace/resource effects | perTurn | none | unmapped; needs /deals endpoint | high |
| CvGame | Property system solve — PropertySolver::doTurn (CvPropertySolver.cpp:448) | All CvProperties on all game objects (all scopes) | perTurn | partial — /cities crime/education/disease only | partial: B-i mapped; non-city scopes unmapped | high |
| CvGame | Global warming terrain mutation (CvGame.cpp:6581) | DEAD CODE (#define GLOBAL_WARMING commented out) | perTurn | none | unmapped (dead; future ties to air_pollution property) | low |
| CvGame | Auto-found corporation HQ (CvGame.cpp:6847) | Corporation HQ city assignment | perTurn | partial — /cities building count changes; no event | unmapped (§A corporations) | med |
| CvGame | Realistic-corp auto-founding (CvGame.cpp:11156) | Same as doHeadquarters (gated on advanced-corps option) | perTurn | none | unmapped | low |
| CvGame | Diplomacy vote resolution (CvGame.cpp:9525) | m_votesTriggered; war/peace/trade state mutations | perTurn | none | unmapped; listed §A opaque | high |
| CvGame | Vote timer decay + new vote scheduling (CvGame.cpp:9730) | m_aiVoteTimer, m_aiSecretaryGeneralTimer | perTurn | none | unmapped | med |
| CvGame | ForcedAIAutoPlay counter decrement (CvGame.cpp:6011) | m_aiForcedAIAutoPlay, m_aiAIAutoPlay | perTurn | none | unmapped | low |
| CvGame | Turn counters increment (CvGame.cpp:6034) | m_iGameTurn, m_iElapsedGameTurns | perTurn | full — /events turnEnd/turnStart; X-S2S-Turn header | unmapped: fundamental counter | low |
| CvGame | Victory countdown tick (CvGame.cpp:7696) | CvTeam::m_aiVictoryCountdown, m_iMercyRuleCounter | perTurn | partial — /players score for mercy threshold; countdown opaque | unmapped; needs /diagnostic/victory | high |
| CvGame | Increasing difficulty ratchet (CvGame.cpp:10061) | CvPlayer::m_eHandicap for human players | perTurn | partial — /players handicap string; no event | unmapped | low |
| CvGame | Flexible difficulty adjustment (CvGame.cpp:10143) | m_aiFlexibleDifficultyTimer, player handicap | perTurn | partial — /players handicap string; C2C.log not in /events | unmapped | low |
| CvGame | Final Five elimination (CvGame.cpp:10007) | m_iCutLosersCounter; player setAlive(false) | perTurn | partial — /players: player disappears; no warning event | unmapped | low |
| CvGame | Map visibility brute-force rebuild (CvGame.cpp:5996) | All CvPlot::m_aiVisibilityCount | perTurn | none | unmapped; plot visibility not in any endpoint | med |
| CvGame | Barbarian city creation (CvGame.cpp:6952) | New barbarian city on map | perTurn | partial — /cities: city appears in next snapshot | unmapped | med |
| CvGame | NPC unit spawn — doSpawns (CvGame.cpp:5971) | New units for non-PC players | perTurn | partial — /units: units appear in next snapshot | unmapped | low |
| CvGame | SSE turn boundary events (CvGame.cpp:6030) | None — emits turnEnd/turnStart SSE frames | perTurn | full — /events turnEnd/turnStart | unmapped: observability infrastructure | low |
| CvGame | Turn order activation — setTurnActive (CvGame.cpp:6042) | Team/player turn-active flag | perTurn | partial — /events: human player turns only | unmapped: game flow control | low |
| CvGame | previousRequest flag reset (CvGame.cpp:6101) | m_abPreviousRequest[player] = false | perTurn | none | unmapped | low |

### CvUnit / CvSelectionGroup per-turn state

| System | Behaviour (file:line) | State maintained | Trigger | Observable today | Cascade mapping | Priority |
|---|---|---|---|---|---|---|
| CvUnit | Movement-point reset setMoves(0) (CvUnit.cpp:1767) | m_iMoves = 0 | perTurn | none | unmapped: gates healing and fortify | high |
| CvUnit | Healing — doHeal / changeDamage (CvUnit.cpp:1713) | m_iDamage; healer m_iHealSupportUsed | perTurn | partial — /units damage post-heal | requires.operate: hasMoved=false + isAlwaysHeal | high |
| CvUnit | Forced-march damage +10% (CvUnit.cpp:1676) | m_iDamage (+10% maxHP/turn) | perTurn | partial — /units damage; cause invisible | requires.operate: promotion active → damage source | med |
| CvUnit | Fortify-turn counter increment (CvUnit.cpp:1718) | m_iFortifyTurns [0, MAX_FORTIFY_TURNS] | perTurn | none | tally: unit-scope age counter gating combat bonus | high |
| CvUnit | Build-up timer + promotion grant (CvUnit.cpp:1722) | m_iBuildUpTurns, m_bIsBuildUp, promotion flags | perTurn | none | tally: unit-scope age counter → enables promotion grants | med |
| CvUnit | Commander/Commodore control-point restoration (CvUnit.cpp:1646) | UnitCompCommander/Commodore::m_iControlPointsLeft | perTurn | none | unmapped | med |
| CvUnit | m_bRevealed flag reset (CvUnit.cpp:1656) | m_bRevealed = false | perTurn | none | unmapped: within-turn combat flag | low |
| CvUnit | HN-capture promotion removal (CvUnit.cpp:1657) | m_bHasHNCapturePromotion + promotion flags | perTurn | none | requires.operate: capture promotion requires enemy territory | low |
| CvUnit | Spy interception check (CvUnit.cpp:1689) | Spy unit may be killed or gain XP | perTurn | none | unmapped (§A espionage) | med |
| CvUnit | Insidiousness vs investigation check (CvUnit.cpp:1662) | m_pPlayerInvestigated, wanted promotions, XP | perTurn | none | unmapped (§A espionage) | low |
| CvUnit | City passive XP accumulation (CvUnit.cpp:1603) | m_iExperience100 (+5/turn for build-up teach/disease-control units in own city) | perTurn | partial — /units level (indirectly after threshold) | tally: unit-scope XP counter | med |
| CvUnit | Promotion readiness test — testPromotionReady (CvUnit.cpp:1683) | m_bPromotionReady | perTurn | none | tally: unit XP vs threshold → enables promotion | med |
| CvUnit | Immobile timer decrement (CvUnit.cpp:1744) | m_iImmobileTimer | perTurn | none | tally: countdown timer gating movement | med |
| CvUnit | Per-turn attack/defense count reset (CvUnit.cpp:1746) | m_iAttackCount, m_iDefenseCount | perTurn | none | tally: per-turn attack counter; requires.operate for multi-attack limits | low |
| CvUnit | madeAttack / madeInterception flags reset (CvUnit.cpp:1758) | m_bMadeAttack, m_bMadeInterception | perTurn | none | unmapped: within-turn combat bookkeeping | low |
| CvUnit | reconPlot reset (CvUnit.cpp:1761) | m_pReconPlot = NULL | perTurn | none | unmapped | low |
| CvUnit | canRespawn / survivor flags cleared (CvUnit.cpp:1728) | m_bCanRespawn, m_bSurvivor | perTurn | none | unmapped: transient combat result flag | low |
| CvUnit | Exile unit repositioning (CvUnit.cpp:1763) | Unit plot (x/y) | perTurn | partial — /units x/y post-move | unmapped | low |
| CvUnit | Spy sleep timer reset on max fortify (CvUnit.cpp:1738) | m_iSleepTimer, group woken | perTurn | partial — /units activity post-wake | tally: fortify-turns threshold → enables wake | low |
| CvUnit | Healer support tracking (CvSelectionGroup.cpp:408) | m_iHealSupportUsed | perTurn | none | tally: unit-scope heal-support-used counter | med |
| CvUnit | Activity-type transitions in CvSelectionGroup::doTurn (CvSelectionGroup.cpp:259) | m_eActivityType (HOLD/HEAL/SENTRY wake-up) | perTurn | partial — /units activity field; reason invisible | requires.operate: unit healed/danger → waking transition | high |
| CvUnit | Unit-level promotion grant on level-up (CvUnit.cpp:10148) | m_iLevel, m_iDamage (heals 50%), m_bPromotionReady, promotion flags | perEvent | partial — /units level + damage; no promotion-grant event | enables: XP tally → level → promotion access | high |
| CvUnit | XP gain from combat (CvUnit.cpp:1920) | m_iExperience100, m_iLevel, player fractionalCombatExperience | perEvent | partial — /units level (indirectly) | tally: unit XP counter, player GG-XP counter | high |
| CvUnit | Combat damage (CvUnit.cpp:2305) | m_iDamage | perEvent | partial — /units damage post-combat, stale | tally: unit HP tally; requires.operate for damage-threshold abilities | high |
| CvUnit | UNITAI role reassignment (CvUnitAI.cpp:1540) | m_eUnitAI | perEvent | partial — /units ai field + [UNT/role] log | unmapped (AI decision state) | high |
| CvUnit | Merge — doMerge / mergeUnits (CvUnit.cpp:27046) | Unit set: 3→1; promotions/XP/damage merged | perEvent | partial — /units snapshot + [UNT/merge] log | tally update (unit counts); [SPINE/DOMAIN] emitted | high |
| CvUnit | Split — doSplit (CvUnit.cpp:27338) | Unit set: 1→3; promotions distributed | perEvent | partial — /units snapshot + [UNT/split] log | tally update (unit counts); [SPINE/DOMAIN] emitted | med |
| CvUnit | Property values on units — CvPropertySolver (CvGame.cpp:5944) | CvUnit::m_Properties (crime/disease etc.) | perTurn | none | requires.operate: property-band dormancy at unit scope (B-ii analogue) | med |
| CvUnit | Blockade gold collection (CvUnit.cpp:1684) | Player gold, enemy gold, city plundered flag | perTurn | partial — /players gold aggregate | unmapped | low |
| CvUnit | Mission-timer countdown — updateMission (CvSelectionGroup.cpp:662) | m_iMissionTimer | perEvent | none | unmapped: frame-rate UI timer | low |
| CvUnit | Group force-update flag (CvSelectionGroup.cpp:324) | m_bForceUpdate | perTurn | none | unmapped: AI scheduling flag | low |

### CvPlot / CvMap per-turn state

| System | Behaviour (file:line) | State maintained | Trigger | Observable today | Cascade mapping | Priority |
|---|---|---|---|---|---|---|
| CvPlot | ownershipDuration increment (CvPlot.cpp:656) | m_iOwnershipDuration | perTurn | none | unmapped: fort revolt threshold | low |
| CvPlot | doBonusDiscovery — RNG bonus reveal (CvPlot.cpp:707) | m_eBonusType: NO_BONUS → BonusTypes | perTurn | none | unmapped: improvement enables discovery | med |
| CvPlot | doBonusDepletion — RNG resource exhaustion (CvPlot.cpp:821) | m_eBonusType → NO_BONUS | perTurn | partial — C2C.log line (not in /events) | unmapped: needs depletion event emission | med |
| CvPlot | doImprovementUpgrade — improvement chain progression (CvPlot.cpp:882) | m_iUpgradeProgress, m_eImprovementType | perTurn | none | unmapped: improvement lifecycle; setImprovementType is cascade-relevant state change | high |
| CvPlot | Super-Forts defense-damage heal + bombarded reset (CvPlot.cpp:679) | m_iDefenseDamage, m_bBombarded | perTurn | none | unmapped | low |
| CvPlot | doFeature — feature disappearance (CvPlot.cpp:10749) | m_eFeatureType → NO_FEATURE | perTurn | none | unmapped: feature change affects yields and improvement eligibility | med |
| CvPlot | doFeature — feature spread/growth (CvPlot.cpp:10765) | m_eFeatureType: NO_FEATURE → FeatureTypes | perTurn | partial — DLL message to plot owner; not in /events | unmapped | med |
| CvPlot | doCulture — improvement culture emission (CvPlot.cpp:10836) | m_aiCulture[ePlayer] on plots within improvement range | perTurn | none | unmapped: improvement culture feeds ownership flip | high |
| CvPlot | doCulture — per-player culture decay (CvPlot.cpp:10842) | m_aiCulture[ePlayer] decremented per decay formula | perTurn | none | unmapped (§A culture opaque) | high |
| CvPlot | doCulture — plot ownership flip (CvPlot.cpp:10882) | m_eOwner → calculateCulturalOwner() result | perTurn | none | unmapped | high |
| CvPlot | checkCityRevolt — city revolt / culture flip (CvPlot.cpp:1022) | m_aiNumRevolts, m_iOccupationTimer, m_eOwner | perTurn | partial — DLL messages only; no HTTP event | unmapped; [CULT/revolt] log tag needed | high |
| CvPlot | checkFortRevolt — fort culture flip (CvPlot.cpp:1133) | m_eOwner for isActsAsCity improvements | perTurn | none | unmapped: deterministic ownership flip | low |
| CvPlot | doCulture — culture-rate buffer rotation (CvPlot.cpp:10885) | m_cultureRatesLastTurn, m_influencedByCityByPlayerLastTurn | perTurn | none | unmapped: internal delta-detection buffer | med |
| CvPlot | doTerritoryClaiming — deferred ownership apply (CvPlot.cpp:12968) | m_eOwner ← m_eClaimingOwner | perTurn | none | unmapped | low |
| CvPlot | verifyUnitValidPlot — unit displacement on invalid plot (CvPlot.cpp:1527) | Unit positions; group cohesion | perTurn | partial — /units x/y; displacement cause invisible | unmapped | low |
| CvPlot | Dead-player ownership cleanup (CvPlot.cpp:695) | m_eOwner → NO_PLAYER for dead-player plots | perTurn | none | unmapped | low |
| CvPlot | CvPropertySolver — plot property values (CvPropertySolver.cpp:448) | CvPlot::m_Properties (air_pollution, water_pollution, flammability etc.) | perTurn | none | partial: property thresholds drive B-i placement; plot values unmapped | high |
| CvMap | updateIncomingUnits — multi-map transit (CvMap.cpp:422) | TravelingUnit arrival → new unit spawned | perTurn | partial — /units: unit appears in next snapshot | unmapped | low |
| CvPlot | updateIrrigated — irrigation connectivity (CvPlot.cpp:6290) | m_bIrrigated (BFS-derived) | onChange | none | unmapped: improvement enables irrigation chain | med |
| CvPlot | River/freshwater edges (CvPlot.cpp:11114) | m_bNOfRiver, m_bWOfRiver (static) | onChange | none | unmapped: static terrain prerequisite for freshwater requires | low |

### Property System (CvProperties / CvPropertySolver)

| System | Behaviour (file:line) | State maintained | Trigger | Observable today | Cascade mapping | Priority |
|---|---|---|---|---|---|---|
| PropertySolver | Global property solve — CvPropertySolver::doTurn (CvPropertySolver.cpp:448) | All CvProperties on all game objects all scopes | perTurn | partial — /cities crime/education/disease only | The solver IS the tally's forward computation; all sources map to tally MODIFIERs | high |
| PropertySolver | Propagator phase — Spread/Gather/Diffuse (CvPropertyPropagator.cpp:245) | Property values on target objects via cross-object flow | perTurn | none | tally MODIFIER with cross-city/cross-scope topology (#429 leakage) | high |
| PropertySolver | Interaction phase — ConvertConstant/InhibitedGrowth/ConvertPercent (CvPropertyInteraction.cpp:183) | Two property values on same object (source reduced, target increased) | perTurn | none | paired tally MODIFIER entries; InhibitedGrowth is non-linear | high |
| PropertySolver | Source phase — Constant/ConstantLimited/Decay/AttributeConstant (CvPropertySource.cpp:166) | Property value on host object | perTurn | none | each source type = tally MODIFIER: Constant=fixed additive; Decay=percent decay above threshold; AttributeConstant=attribute-scaled | high |
| PropertySolver | Per-object manipulator gathering — foreachManipulator (CvGameObject.cpp:626) | Which PropertyManipulators are active for each city/player/unit/plot | perTurn | none | MODIFIER authoring surface: each building/civic PropertyManipulators → cascade modifiers entry | high |
| PropertySolver | CvProperties::propagateChange — instantaneous cross-scope propagation (CvProperties.cpp:230) | Property values on related game objects (city → player etc.) | perEvent | none | tally MODIFIER with cross-scope emission at configured percentage | med |
| PropertySolver | clearForRecalculate — source-drain reset before solve (CvGame.cpp:11661) | Non-source-drain property values erased; source-drain persists | perTurn | none | determines tally persistence: source-drain=accumulated; non-source-drain=re-derived | med |
| PropertySolver | CvGameObjectUnit::eventPropertyChanged — unit promotion on property band (CvGameObject.cpp:753) | Unit promotion set (setHasPromotion) when property enters/exits band | perEvent | none | requires.operate on promotion: {PROPERTY_X: {min:N, max:M}} — unit-scope B-ii analogue; unmapped | high |
| PropertySolver | CvCity::checkPropertyBuildings — property-band building placement (CvCity.cpp:1490) | City building set (changeHasBuilding) | perTurn | partial — [PLACEMENT] + /diagnostic/placementSweep; property reason=noMarker not yet JSON-flagged | autoBuild + requires.operate ATOMDOMAIN_PROPERTY (B-i; shadow running) | high |
| PropertySolver | CvCity::doPropertyUnitSpawn — stochastic property-driven unit spawning (CvCity.cpp:23496) | New units spawned; m_aPropertySpawns roster | perTurn | none | unmapped; probabilistic outcome outside deterministic enabler model | high |
| PropertySolver | AI property needs cache — getCachedPropertyNeed (CvCity.cpp:23378) | m_cachedPropertyNeeds[], m_icachedPropertyNeedsTurn | perTurn | none | unmapped; targetLevel concept from properties-first-class.md | med |
| PropertySolver | Property values at non-city scopes (player/team/game/plot/unit) (CvPropertySolver.cpp:313) | Property values at all non-city scopes | perTurn | none | all non-city scopes need endpoint coverage for Orwell bar | high |
| PropertySolver | m_aiPropertyChange delta accumulator (CvProperties.cpp:188) | Per-turn net delta for each PropertyType on each object | perTurn | none | exactly what tally DOMAIN event stream should emit: (PropertyType, scope, objectId, delta) | high |
| PropertySolver | Building dormancy gate on city property manipulators (CvGameObject.cpp:668) | Which buildings contribute PropertyManipulators to solver | perTurn | none | requires.operate dormancy gate naturally propagated into property MODIFIER chain (B-ii) | med |

### §A Opaque Systems

| System | Behaviour (file:line) | State maintained | Trigger | Observable today | Cascade mapping | Priority |
|---|---|---|---|---|---|---|
| Food | Food net-rate — foodDifference (CvCity.cpp:5970) | Per-city per-turn food delta | perTurn | partial — /cities iFood = gross food rate only; net delta/wastage/healthRate not exposed | enables/requires: food yield drives growth; needs /cities foodDifference, foodWastage, foodConsumed | high |
| Food | Food wastage curve (CvCity.cpp:5923) | Portion of surplus burned asymptotically per turn | perTurn | none | unmapped nonlinear modifier; candidate /cities diagnostic field foodWastage | high |
| Food | Food storage + pop growth/shrink — changeFood (CvCity.cpp:9713) | m_iFood, m_iFoodKept, population | perTurn | partial — /cities population; m_iFood/m_iFoodKept/growthThreshold opaque | unmapped as cascade concept; needs /cities: food (stored), foodKept, growthThreshold | high |
| Espionage | Espionage point accumulation (CvPlayer.cpp:15580) | EspionagePointsEver, EspionagePointsAgainstTeam | perTurn | none | unmapped; needs /players fields | high |
| Espionage | AI espionage slider + weight update (CvPlayerAI.cpp:16657) | COMMERCE_ESPIONAGE percent, spending weights | perTurn | none | unmapped; needs [ESP/econ] log tag | high |
| Espionage | Counterespionage timer + mod (CvTeam.cpp:1061) | CounterespionageTurnsLeft, CounterespionageMod | perTurn | none | unmapped; needs /players or /teams field | med |
| Culture | City culture accrual — doCulture (CvCity.cpp:16301) | City m_aiCulture[owner], m_iCultureLevel | perTurn | partial — /cities cultureLevel tier only | requires.operate for culture-level-gated buildings; accumulator unmapped; needs /cities: culture, cultureRate, cultureThreshold | high |
| Culture | Plot culture spread from city — doPlotCulture (CvCity.cpp:16308) | m_aiCulture[ePlayer] on surrounding plots | perTurn | none | unmapped; needs /plots or /culturemap endpoint | high |
| Culture | Plot culture decay (CvPlot.cpp:10844) | m_aiCulture[ePlayer] per decay formula | perTurn | none | unmapped; core precondition for revolt/flip observability | high |
| Culture | City revolt / culture flip — checkCityRevolt (CvPlot.cpp:1022) | m_aiNumRevolts, m_iOccupationTimer, m_eOwner | perTurn | none | unmapped; needs [CULT/revolt] log tag + /cities fields: numRevolts, occupationTimer | high |
| Culture | Fort culture revolt — checkFortRevolt (CvPlot.cpp:1133) | m_eOwner of fort plots | perTurn | none | unmapped; deterministic ownership flip | low |
| Religion | Religion spread — doReligion spread path (CvCity.cpp:16792) | m_pabHasReligion | perTurn | partial — /cities building count indirect; no religion field | unmapped; needs /cities religions[] + [REL/spread] tag | high |
| Religion | Religion decay — doReligion decay path (CvCity.cpp:16729) | m_pabHasReligion removed; prereq buildings removed | perTurn | none | unmapped; must log before §14 building-set mutation demolition; needs [REL/decay] tag | high |
| Corporations | Corporation spread — doCorporation spread path (CvCity.cpp:21474) | m_pabHasCorporation | perTurn | none | unmapped; needs /cities corporations[] + [CORP/spread] tag | high |
| Corporations | Corporation decay — doCorporation decay path (CvCity.cpp:21553) | m_pabHasCorporation removed | perTurn | none | unmapped | high |
| Corporations | Corporation influence score — getCorporationInfluence (CvCity.cpp:21586) | Derived score (gates isActive and spread/decay probability) | perEvent | none | unmapped; should be /cities diagnostic field | high |
| Corporations | Corporation isActive gate + bonus chain — isActiveCorporation / updateCorporationBonus (CvCity.cpp:13419) | Effective resource counts via processBonus ±1; corp yield/commerce | onChange | none | partial: requires.operate (resource prereq); multi-iteration chain unmapped | high |
| Corporations | Corporation maintenance — calcCorporateMaintenance (CvCity.cpp:21662) | Player gold drain | perTurn | none | unmapped; should be /cities corpMaintenance field | med |
| Corporations | Auto-founding HQ — CvGame::updateCorporation (CvGame.cpp:11169) | Corporation HQ city assignment | perEvent | none | unmapped; needs [CORP/founded] log tag | med |

### Current Observability Surface

| System | Behaviour (file:line) | State maintained | Trigger | Observable today | Cascade mapping | Priority |
|---|---|---|---|---|---|---|
| HTTP | /units snapshot (CvHttpServer.cpp:221) | Per-unit: id, owner, x/y, type, ai, group, missionAI, activity, damage, level | perTurn (5s) | full | Feeds cascade context: unit presence at player/city scope | high |
| HTTP | /players snapshot (CvHttpServer.cpp:270) | Per-player: score, era, techCount, research, civ, gold, goldRate, scienceRate, production, population, cities, units | perTurn (5s) | partial | Tech count coarse proxy; civic/religion/bonus/espionage not exposed | high |
| HTTP | /cities snapshot (CvHttpServer.cpp:320) | Per-city: id, owner, x/y, pop, yields, production item, building count (total), culture level, crime/education/disease | perTurn (5s) | partial | Building count total unusable for per-type cascade atoms; three properties only | high |
| HTTP | /events SSE — turn boundaries (CvGame.cpp:6032) | turnEnd/turnStart/playerTurnStart/playerTurnEnd | perEvent | full | Turn-number axis for log correlation | med |
| HTTP | /events SSE — log tee via streamLogTee (BetterBTSAI.cpp:27) | All [TAG/*] lines at level <= gStreamLogLevel | perEvent | partial | Primary out-of-process channel; [SPINE/DOMAIN] is key | high |
| Cascade | [SPINE/DOMAIN] — building/unit count deltas (CvPlayer.cpp:13736) | Per-player per-type building/unit count delta | perEvent | full | tally DOMAIN inputs for COUNTDOMAIN_BUILDING and COUNTDOMAIN_UNIT | high |
| Cascade | [SPINE/DIAGNOSTIC] — tally shadow verify (CvCascadeTally.cpp:154) | Per-domain mismatch per turn | perTurn | partial — raw int; type index not resolved to name | Shadow mechanism; readability gap in DIAGNOSTIC line format | med |
| Cascade | [READJSON] — per-turn buildability canary (CvCascadeReadJson.cpp:837) | Two sample buildings per turn: cascade vs legacy verdict | perTurn | partial — two samples only | Smoke signal; not exhaustive; /diagnostic/sweep covers full roster | med |
| Cascade | [PLACEMENT] — auto-placement shadow (CvCascadeReadJson.cpp:957) | cascade-would-place vs legacy hasBuilding for B-i roster | perTurn | partial — active player only; noMarker masks real divergences | B-i maintainer shadow; running. B-ii/B-iii/B-iv have no shadow at all | high |
| HTTP | /diagnostic/canConstruct (CvHttpServer.cpp:798) | Per-building: legacy bool + legacyReason + cascade bool + cascadeReason + cap shadow | perEvent | full | Per-building cascade-vs-legacy divergence diagnosis | high |
| HTTP | /diagnostic/sweep (CvHttpServer.cpp:649) | Full-roster buildability comparison for one player/city | perEvent | full | Full-roster cascade-vs-legacy shadow; capital only by default | high |
| HTTP | /diagnostic/placementSweep (CvHttpServer.cpp:714) | All cities × auto-placed roster: cascade vs legacy, per-cell | perEvent | full | Orwell-bar fulfillment for B-i auto-placement | high |
| HTTP | /diagnostic/canTrain (CvHttpServer.cpp:818) | Per-unit: legacy canTrain + cascade verdict | perEvent | partial — no legacyReason or cascadeReason breakdown | Covers COUNTDOMAIN_UNIT cascade side | med |
| HTTP | /diagnostic/whyNot (CvHttpServer.cpp:870) | Per-unit: inputs to canTrain (tech/obsolete/bonus prereqs) | perEvent | full — but undocumented in http-server.md | Diagnoses why a unit is hidden; bonus prereqs map to cascade requires.build BONUS atoms | med |
| HTTP | /diagnostic/canResearch (CvHttpServer.cpp:832) | Per-tech: legacy canResearch + cascade requiresBuild/Operate | perEvent | partial — no reason breakdown; tech domain not in tally | Tech researched status is most common cascade enabler condition | med |
| HTTP | /diagnostic/canDoCivics, canCreate, canMaintain (CvHttpServer.cpp:844) | Per-civic/project/process: legacy bool + cascade (partial) | perEvent | partial — no sweep; no cascade for processes | Civic state not in /players; one-at-a-time only | low |
| Tally | /tally snapshot endpoint — NOT YET BUILT | Accumulated empire/team/world counts for COUNTDOMAIN_BUILDING and COUNTDOMAIN_UNIT | continuous | none | SINGLE LARGEST CASCADE OBSERVABILITY GAP — count state unreconstructible from point-in-time query | high |
| Logs | [CIT/proplevel] (CvCity.cpp:1234) | Per-city per-active-property value + per-turn change at turn start | perTurn | full — log only (not in /cities snapshot for non-crime/education/disease properties) | Property values = input to ATOMDOMAIN_PROPERTY cascade atoms | high |
| Logs | [CIT/prop] — property-control production gate trace (CvCityAI.cpp:14855) | Per-property AI gate inputs (eval, check, proj, fire) | perEvent | partial — level 2 only; not headline | Explains checkPropertyBuildings decisions; shadow cross-ref for B-i divergences | med |
| Logs | [CIT/produced] / [CIT/cancel] / [CIT/waste] | Building/unit completion + cancellation + overflow-gold | perEvent | full | Corroborating log for [SPINE/DOMAIN] building-count events | high |
| Logs | [UNT/role] (CvUnitAI.cpp) | UNITAI role transitions | perEvent | full | Complements /units ai field; regression signal for garrison retype | low |
| Logs | [UNT/merge] / [UNT/split] | 3→1 merge and 1→3 split events | perEvent | full | Explanation layer for tally unit-count deltas that cannot be inferred from delta value alone | med |
| Logs | [PERF/phase] (BetterBTSAI.cpp) | Per-phase wall-clock ms | perTurn | partial — dual gate: gPerfLogLevel AND gStreamLogLevel both required | unmapped to cascade; relevant for shadow overhead diagnosis | med |
| Logs | B-ii religious dormancy — setReligiouslyLimitedBuilding (CvCity.cpp:21279) | m_pabReligiouslyDisabledBuilding + processBuilding(±1) | onChange | none | requires.operate STATE_RELIGION; B-ii shadow not yet built | high |
| Logs | B-ii resource dormancy — setDisabledBuilding (CvCity.cpp:21239) | m_pabDisabledBuilding + isActiveBuilding | onChange | none | requires.operate BONUS; B-ii shadow not yet built | high |
| Logs | B-iii group gate — isSpecialBuildingNotRequired (CvPlayer.cpp:13927) | Civic-driven SpecialBuilding waiver per player | onChange | none | Uniform group-gate; shadow NEEDED not built | med |
| Logs | B-iv — hasAllReligionsActive waiver (CvPlayer.cpp:30299) | Civic-driven AllReligionsActive flag | onChange | none | Folds into B-ii religion dormancy waiver; currently moot | low |

---

## 3. Prioritized Gap List

The following high-priority behaviours are NOT reconstructible from outside today. Ordered by risk to the hard switch (§14 H deletion + §A tally replacement). Items the prior §14 H / §A inventory missed are marked **[MISSED]**.

**Gap 1 — B-ii dormancy: no shadow for present-but-inactive buildings [MISSED in §14 H]**
The prior inventory documented that B-ii (requires.operate dormancy) is "needed but not built" but did not enumerate the three distinct dormancy mechanisms that must all be covered: (a) resource disabling via setDisabledBuilding (triggered by bonus resource gain/loss), (b) religious disabling via setReligiouslyLimitedBuilding (triggered by state-religion change), and (c) replacement suppression (building present but suppressed because successor is also present). All three leave the building in hasBuilding=true while isActiveBuilding=false, invisible to /cities which reports only total building count. The §14 H deletion cannot proceed safely until a per-city "active vs dormant" building roster shadow is built and runs clean. Risk level: blocking — the cascade's requires.operate atom cannot be verified without knowing the current active/dormant split.

**Gap 2 — /tally snapshot endpoint does not exist [PARTIALLY in prior inventory]**
The prior mapping noted this as "planned" in http-server.md but did not assess the consequence: an agent cannot reconstruct the empire/team/world building or unit counts at a point in time. The only signal is replaying all [SPINE/DOMAIN] delta events from session start. This means the cap shadow in /diagnostic/canConstruct (which shows tally.count for one building) is the only point-in-time tally read available. The allowed cap atoms for any building with world/team/empire scope are unverifiable as a batch. Risk: high for any requires count-threshold or allows cap atom.

**Gap 3 — Anger/happiness timer cluster: eight per-city timers + WLTK, fully opaque [MISSED entirely in §14 H]**
The prior inventory does not mention occupationTimer, hurryAngerTimer, revRequestAngerTimer, revSuccessTimer, conscriptAngerTimer, defyResolutionAngerTimer, happinessTimer, landmarkAngerTimer, warWearinessTimer, or eventAnger. All eight per-city anger/happiness timers decrement every turn and drive angryPopulation(), which feeds WLTK, maintenance suppression, and the revolt test precondition. An agent cannot determine city disorder state from outside. Any cascade atom that requires knows-the-city-is-in-disorder (e.g. a building that requires angryPopulation==0) has no external verification path.

**Gap 4 — City culture pipeline: balance, rate, and plot arrays are all opaque [MISSED in §14 H, partially in §A]**
The prior §A entry flagged culture as opaque but the §14 H inventory did not connect this to the dormancy/requires chain. City culture balance (m_aiCulture[]) is not in /cities (only the discrete level tier is). The requires.operate atom for culture-level-gated buildings (noted in /diagnostic/canConstruct cascadeReason=cultureLevel) cannot be shadow-verified against a continuous value because the continuous value is invisible. Additionally, the revolt pipeline (checkCityRevolt) is a perTurn stochastic mutation of occupation state that must be logged before the §14 deletion of any city-ownership or building-placement maintainer that reads isOccupation().

**Gap 5 — Religion presence per city: no /cities field [MISSED in §14 H]**
The prior inventory does not flag this explicitly, but isHasReligion per city is a requires.operate atom (STATE_RELIGION_IN_CITY) used in many building prereqs. /cities exposes no religion array. The doReligion spread and decay paths mutate this state stochastically each turn with no log emission. Any building gated on STATE_RELIGION_IN_CITY (e.g. temple → cathedral prereq chain) cannot have its cascade dormancy shadow verified. Risk: blocking for B-ii religious dormancy shadow, since you cannot compare cascade dormancy verdict vs isActiveBuilding for religion-gated buildings without knowing which religions are present in each city.

**Gap 6 — Civic state not in /players [MISSED in §14 H]**
Which civics are currently active for a player is not exposed by any endpoint. Civic presence drives multiple requires.operate atoms (CIVIC_ domain), several building prereqs, and the B-iii special-building group waiver. The /diagnostic/canDoCivics endpoint exists but is one-at-a-time without a sweep. The verifyCivics forced-reset (which can silently change civics) fires with no log event.

**Gap 7 — Finance internals: maintenance, unit upkeep, civic upkeep, hurry inflation not broken out [MISSED in §14 H]**
goldRate is exposed but is a black box: maintenance, unit upkeep, civic upkeep, corporate maintenance, and the hurry-inflation multiplier (getInflationMod10000) that scales all of these are individually invisible. The cascade's requires.operate operating-cost atoms cannot be verified because the expense breakdown is opaque. This is a hard blocker for any building or unit with upkeep costs that require operating resource access (e.g. supply chain buildings).

**Gap 8 — War weariness: per-team accumulators and derived city anger are fully opaque [MISSED in §14 H]**
CvTeam::m_aiWarWeariness, CvPlayer::m_iWarWearinessPercentAnger, and the CvCity::m_iWarWearinessTimer are all invisible from outside. War weariness is the largest per-turn anger source during prolonged wars. Any cascade model that attempts to predict or observe city disorder must account for war weariness but has no signal for it.

**Gap 9 — Deal state: no /deals endpoint, deal kills fire silently [MISSED in §14 H]**
Active deals, their contents (trade resources, open borders, peace treaties, defensive pacts), and their per-turn verification/expiry are completely opaque. A deal kill can silently revoke a bonus resource that a building depends on, triggering setDisabledBuilding, with no event visible externally. This is a cascading-state risk: deal verification → resource loss → building dormancy, none of which is logged.

**Gap 10 — AI war-plan state and AI diplomatic counters [MISSED in §14 H]**
The prior inventory did not assess the AI diplomatic memory subsystem at all. m_eWarPlan, the attitude counters (AtWarCounter, AtPeaceCounter, DefensivePactCounter etc.), and the per-player-pair memory counts (grievances, trust) are all invisible. War declarations are the highest-impact sudden state changes in the game; they mutate isAtWar (which is a requires.operate gate on war-gated buildings) with no pre-declaration observable signal.

**Gap 11 — Corporation system: presence, influence, isActive, bonus chain all opaque [IN §A, depth missed]**
The §A inventory listed corporations as opaque but did not enumerate what the cascade specifically needs to reconstruct: isHasCorporation per city, getCorporationInfluence per city per corp, isActiveCorporation (which gates yield/commerce contributions), and the updateCorporationBonus multi-iteration chain (which silently mutates effective resource counts via processBonus ±1). The bonus chain in particular is a hidden source of resource-count changes that will corrupt the B-ii resource dormancy shadow if not accounted for.

**Gap 12 — Victory countdown: not queryable, mastery/mercy-rule invisible [MISSED in §14 H]**
The prior inventory does not mention victory countdown at all. testVictory runs every turn and decrements per-team per-condition counters. A player approaching a space-race victory will have m_aiVictoryCountdown ticking toward zero with no external signal. The mercy-rule counter is similarly invisible. These affect whether requires atoms about game-state conditions (victory blocked by active countdown) can be externally verified.

**Gap 13 — [SPINE/DIAGNOSTIC] DIAGNOSTIC line format is unreadable [NEW — sweep finding]**
The per-mismatch lines emitted by the tally shadow-verify fall back to the raw format (eventId=10/11, type=N a=tallied b=truth c=player) without resolving the type index to an XML name. This makes mismatch lines practically unreadable without a separate lookup. This was not in any prior documentation and must be fixed before the DIAGNOSTIC stream can serve as a reliable replacement proof. The reference doc ai-logging-reference.md does not describe the [SPINE/DIAGNOSTIC] payload format.

---

## 4. Recommended Next Shadows

**Build B-ii dormancy shadow first.**
This is the single highest-risk gap blocking the §14 hard switch. The shadow needs to emit, per city per turn, the full active/dormant split for all present buildings — comparing cascade requires.operate dormancy verdict against isActiveBuilding. The minimum viable form is a new [DORMANCY] log line (at gPlayerLogLevel>=1) and a /diagnostic/dormancySweep endpoint analogous to placementSweep, covering all three mechanisms: resource disabling, religious disabling, and replacement suppression. Without this, the placementSweep shadow already running (B-i) is incomplete: a building reported as "cascade would not place" may in fact be present-but-dormant, a state that looks like a divergence from the B-i perspective but is actually correct B-ii behaviour.

**Add /cities religion and civic fields in the same pass.**
The B-ii religious dormancy shadow cannot compare cascade verdict against isActiveBuilding for religion-gated buildings without knowing which religions are present per city. Adding religions[] (array of {type, hasReligion}) and the player's activeCivics to /players (or a new /diagnostic/civicState endpoint) unblocks both B-ii and the requires.operate STATE_RELIGION and CIVIC_ atom verification in a single endpoint change.

**Wire the anger/timer cluster to the observability layer before touching doTurn ordering.**
The eight per-city anger timer fields and the WLTK flag are collectively the largest unmapped per-turn state cluster that directly feeds building dormancy preconditions (isDisorder(), angryPopulation() > 0 gate WLTK and several requires checks). Adding these to /cities (even as a diagnostics subobject at level 2) gives the cascade shadow a way to correlate dormancy decisions with city disorder state without touching any game logic.

**Fix [SPINE/DIAGNOSTIC] line format and add /tally snapshot endpoint in parallel.**
These are both pure observability additions with no game-logic risk. The DIAGNOSTIC line fix (resolving type index to XML name in the log consumer) is a one-liner in CvEventSpine.cpp. The /tally endpoint is the snapshot analog to /cities for count state and is required before any count-threshold or allowed-cap atom can be batch-verified. Both should ship together since they serve the same purpose: letting an agent confirm the tally reflects reality without replaying the entire delta stream.
