# Logging field catalog — the raw-field inventory of every gated `[TAG]` log line

> **Status:** reference · **Verified against:** the BBAI domain-helper call sites across `Sources/AI/`, `Sources/Engine/`, `Sources/Cascade/` (the file:line citations below) — re-confirm against the live source before relying on an exact line. Line numbers **drift**; the per-source-file reorg moved the `Cv*` files under `Sources/Engine/` and `Sources/AI/`, so confirm the named function/tag, not the integer.
> **Grounding:** each row's `file:line` cites the live emit site. The field lists are the *raw payload* each line carries (the values needed to reconstruct the formatted line without re-entering game state). Where a tag has multiple variants, the variant label (e.g. `ownership` / `plotInvalid`) disambiguates.
> This is the **field-level census** of S2S's gated `[TAG]` logging — every domain, every template, its gate, its level, and the exact fields it emits. It exists so a consumer (a `/events` parser, a shadow, or the event-spine migration) knows precisely what each line carries. After reading you will know which gate turns a domain on, what each line's fields are, and the type/width distribution across all ~196 templates.

**BLUF.** S2S has ~196 gated log templates across ~10 domains (WAI, CIT, UNT/COM/GRP, HAI, DAI/DIP/ESP, WAR, FND/INIT/ENG, CTB, PERF, Cascade). Each is gated by one of `gPlayerLogLevel` / `gCityLogLevel` / `gTeamLogLevel` / `gUnitLogLevel` / `gPerfLogLevel`, emits at a level (1 = headline … 3 = per-candidate), and carries a fixed raw-field payload. The widest are `[PERF/choose]` (26 fields) and `[STATE/city]` (22); the median is 5–6. This catalog is the **Stage-0 input** for migrating these helpers onto the cascade event spine — that migration's design (the `IEventConsumer` contract, the raw-payload-pure spine) is **not restated here**; see [the event spine](#relationship-to-the-event-spine).

> **Field-type legend.** `int` = plain int / coord / flag / count. `string` = narrow `const char*` (XML type keys, reason literals) or wide `wchar_t*` (city/unit names, descriptions). `typeIndex` = an enum int (`UnitAITypes`, `BuildingTypes`, …) the consumer resolves via `GC.get*Info()`. `float` = exclusively PERF. `other` = one `int64_t` (`[CIT/order] CONSTRUCT` score).

---

## 1. Per-domain template tables

### 1.1 WAI — Worker AI (`logBuildEvaluation` → `BuildEvaluation.log`)

Gate: `gPlayerLogLevel`. Helper declared in `BetterBTSAI.h`. All call sites in `Sources/AI/CvWorkerAI.cpp`. The `[%s/mission]` / `[%s/end]` family emits under both `WAI` and `WAI/city` namespaces depending on the `section` parameter in `pushBuildMission()`.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[WAI/begin]` | 1 | owner:int unit:int at.x:int at.y:int allowedTurns:int searchRange:int canRoute:int | CvWorkerAI.cpp:477 |
| `[WAI/plotset-empty]` | 1 | unit:int | CvWorkerAI.cpp:491 |
| `[WAI/plot/skip]` (cached) | 3 | at.x:int at.y:int reason:string | CvWorkerAI.cpp:515 |
| `[WAI/plot/skip]` ownership | 2 | at.x:int at.y:int owner:int | CvWorkerAI.cpp:530 |
| `[WAI/plot/skip]` plotInvalid | 2 | at.x:int at.y:int | CvWorkerAI.cpp:538 |
| `[WAI/plot/skip]` areaMismatch | 2 | at.x:int at.y:int | CvWorkerAI.cpp:548 |
| `[WAI/plot/skip]` noBonus | 3 | at.x:int at.y:int | CvWorkerAI.cpp:561 |
| `[WAI/plot/close]` | 3 | at.x:int at.y:int bonus:string closeEnough:int | CvWorkerAI.cpp:590 |
| `[WAI/plot/skip]` notCloseEnough | 2 | at.x:int at.y:int | CvWorkerAI.cpp:599 |
| `[WAI/plot/skip]` inaccessible/visibleEnemy | 2 | at.x:int at.y:int reason:string | CvWorkerAI.cpp:626 |
| `[WAI/build/hit]` | 2 | at.x:int at.y:int bonus:string build:string qualified:int yield:int | CvWorkerAI.cpp:689 |
| `[WAI/build/winner]` (qualified>0) | 2 | at.x:int at.y:int bonus:string build:string qualified:int yield:int time:int | CvWorkerAI.cpp:756 |
| `[WAI/build/winner]` (NO_BUILD) | 3 | at.x:int at.y:int bonus:string | CvWorkerAI.cpp:764 |
| `[WAI/build/cand]` | 3 | build:string impr:string cultureSuffix:string yield:int time:int timeScore:int | CvWorkerAI.cpp:263 |
| `[WAI/plot/skip]` noPath | 3 | at.x:int at.y:int | CvWorkerAI.cpp:811 |
| `[WAI/score]` | 2 | at.x:int at.y:int bonus:string base:int yield:int def:int counter:int aiObj:int noTrade:int total:int path:int maxW:int others:int cityRad:int atPlot:int ok:int | CvWorkerAI.cpp:931 |
| `[WAI/dedup]` | 2 | at.x:int at.y:int reason:string others:int max:int | CvWorkerAI.cpp:944 |
| `[WAI/best]` (improve) | 1 | at.x:int at.y:int bonus:string build:string value:int | CvWorkerAI.cpp:967 |
| `[WAI/best]` (connect) | 1 | at.x:int at.y:int bonus:string value:int | CvWorkerAI.cpp:992 |
| `[WAI/end]` noTarget | 1 | unit:int | CvWorkerAI.cpp:1006 |
| `[WAI/mission]` connectPlot | 2 | at.x:int at.y:int value:int | CvWorkerAI.cpp:1043 |
| `[WAI/end]` route | 1 | unit:int at.x:int at.y:int value:int | CvWorkerAI.cpp:1049 |
| `[WAI/end]` fallthrough | 1 | unit:int | CvWorkerAI.cpp:1060 |
| `[%s/mission]` substituted | 2 | section:string at.x:int at.y:int chosen:string actual:string mission:string value:int | CvWorkerAI.cpp:335 |
| `[%s/mission]` not substituted | 2 | section:string at.x:int at.y:int build:string mission:string value:int | CvWorkerAI.cpp:344 |
| `[%s/end]` build atPlot substituted | 1 | section:string unit:int at.x:int at.y:int chosen:string actual:string value:int | CvWorkerAI.cpp:365 |
| `[%s/end]` build atPlot not substituted | 1 | section:string unit:int at.x:int at.y:int build:string value:int | CvWorkerAI.cpp:372 |
| `[%s/end]` noMoves | 1 | section:string unit:int at.x:int at.y:int target.x:int target.y:int mission:string | CvWorkerAI.cpp:386 |
| `[%s/end]` build moved substituted | 1 | section:string unit:int at.x:int at.y:int chosen:string actual:string value:int | CvWorkerAI.cpp:403 |
| `[%s/end]` build moved not substituted | 1 | section:string unit:int at.x:int at.y:int build:string value:int | CvWorkerAI.cpp:410 |
| `[%s/end]` pushFailed | 1 | section:string unit:int from.x:int from.y:int to.x:int to.y:int mission:string moves:int isBusy:int | CvWorkerAI.cpp:422 |
| `[WAI/city/begin]` | 1 | owner:int unit:int at.x:int at.y:int city:string cityId:int plots:int | CvWorkerAI.cpp:1116 |
| `[WAI/city/plot/skip]` safeAutomation | 3 | at.x:int at.y:int | CvWorkerAI.cpp:1144 |
| `[WAI/city/eval/hit]` | 2 | at.x:int at.y:int build:string value:int canBuild:int goldShort:int | CvWorkerAI.cpp:1168 |
| `[WAI/city/eval/new]` | 2 | at.x:int at.y:int build:string value:int canBuild:int goldShort:int | CvWorkerAI.cpp:1189 |
| `[WAI/city/plot/skip]` enemyUnit | 2 | at.x:int at.y:int build:string owner:int border:int dist:int | CvWorkerAI.cpp:1205 |
| `[WAI/city/plot/skip]` noPath | 2 | at.x:int at.y:int build:string owner:int own:int border:int dist:int enemyOnPlot:int adjOwn:int adjForeign:int adjWater:int adjPeak:int | CvWorkerAI.cpp:1243 |
| `[WAI/city/score]` | 2 | at.x:int at.y:int build:string base:int path:int atPlot:int maxW:int others:int scored:int ok:int | CvWorkerAI.cpp:1286 |
| `[WAI/city/dedup]` | 2 | at.x:int at.y:int others:int max:int | CvWorkerAI.cpp:1295 |
| `[WAI/city/best]` | 1 | at.x:int at.y:int build:string value:int | CvWorkerAI.cpp:1308 |
| `[WAI/city/frontier]` | 1 | owner:int city:string cityId:int at.x:int at.y:int radius:int foreignOwned:int notWorkedByUs:int considered:int enemyBlocked:int noPath:int noPathBorder:int found:int | CvWorkerAI.cpp:1317 |
| `[WAI/city/end]` noTarget | 1 | unit:int city:string | CvWorkerAI.cpp:1327 |

**Domain totals:** 43 templates. Widest: `[WAI/score]` at 16 fields. String fields: bonus/build names (`getType()`), section tag, mission name, city name (wide `%S`). No floats. No typeIndex.

### 1.2 CIT — City production (`logCityAI` → `CityAI.log`)

Gate: `gCityLogLevel`. Sources: `Sources/AI/CvCityAI.cpp`, `Sources/Engine/CvCity.cpp`. City/unit/building names are wide `%S` from `getDescription()` / `getName()`.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[CIT/garrcons]` | 1 | city:string owner:int merges:int strLeft:int need:int | CvCityAI.cpp:500 |
| `[CIT/begin]` | 1 | city:string owner:int pop:int danger:int dangerVal:int finTrouble:int critGold:int foodProd:int | CvCityAI.cpp:966 |
| `[CIT/stranded]` | 1 | city:string owner:int wHave:int wNeed:int areaHave:int areaNeed:int danger:int inhibit:int turtle:int bestBuildVal:int | CvCityAI.cpp:1108 |
| `[CIT/stranded/try]` | 1 | city:string owner:int wNeed:int areaHave:int areaNeed:int | CvCityAI.cpp:2021 |
| `[CIT/stranded/declined]` | 1 | city:string | CvCityAI.cpp:2031 |
| `[CIT/danger]` | 2 | city:string owner:int minAtk:int defShortfall:int sqrtCities:int need:int ownedAtk:int ownedAtkRaw:int fire:int | CvCityAI.cpp:2333 |
| `[CIT/order]` TRAIN | 1 | city:string unit:string unitAI:int reason:string | CvCityAI.cpp:8851 |
| `[CIT/order]` CONSTRUCT | 1 | city:string building:string score:other rank:int total:int focus:int | CvCityAI.cpp:9120 |
| `[CIT/order]` CREATE_PROJECT | 1 | city:string project:string | CvCityAI.cpp:9167 |
| `[CIT/order]` MAINTAIN_PROCESS | 1 | city:string process:string commerce:int | CvCityAI.cpp:9194 |
| `[CIT/prop]` | 2 | city:string owner:int prop:string val:int change:int pct:int eval:int check:int proj:int getting:int good:int maxed:int propPct:int fire:int | CvCityAI.cpp:14855 |
| `[CIT/proplevel]` | 1 | turn:int city:string owner:int prop:string val:int change:int | CvCity.cpp:1244 |
| `[CIT/push/reject]` UNIT | 2 | city:string owner:int unit:string alreadyQueued:int | CvCity.cpp:15554 |
| `[CIT/push/reject]` BUILDING | 2 | city:string owner:int building:string alreadyQueued:int | CvCity.cpp:15587 |
| `[CIT/push]` | 2 | city:string owner:int kind:string name:string append:int force:int | CvCity.cpp:15662 |
| `[CIT/cancel]` | 1 | city:string owner:int kind:string name:string progressLost:int willChoose:int | CvCity.cpp:15770 |
| `[CIT/produced]` UNIT | 1 | city:string owner:int unit:string unitAI:int ownerHas:int aiRoleHas:int overflow:int lost:int | CvCity.cpp:15836 |
| `[CIT/produced]` BUILDING | 1 | city:string owner:int building:string overflow:int lost:int | CvCity.cpp:15980 |
| `[CIT/produced]` PROJECT | 1 | city:string owner:int project:string | CvCity.cpp:16022 |
| `[CIT/spin]` produceLoopCap | 1 | city:string owner:int | CvCity.cpp:16571 |
| `[CIT/spin]` noProductionChosen | 1 | city:string owner:int | CvCity.cpp:16590 |
| `[CIT/waste]` | 1 | city:string owner:int lostProd:int gold:int | CvCity.cpp:16619 |

**Domain totals:** 22 templates. Widest: `[CIT/prop]` at 14 fields. Special: `score` in `[CIT/order] CONSTRUCT` is `int64_t` (printed `%I64d`) — typed `other`; needs a 64-bit or dual-slot treatment on the spine. All city/unit/project names are wide strings resolved at call site.

### 1.3 UNT/COM/GRP — Unit, combat, group

Gate: `gUnitLogLevel`. Sources: `Sources/AI/CvUnitAI.cpp`, `Sources/Engine/CvUnit.cpp`, `Sources/AI/CvSelectionGroupAI.cpp`, `Sources/Engine/CvArmy.cpp`.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[UNT/act]` | 2 | owner:int unit:int type:typeIndex decision:string reason:string targetX:int targetY:int | CvUnitAI.cpp:479 |
| `[UNT/move]` | 2 | owner:int unit:int type:typeIndex atX:int atY:int stack:int | CvUnitAI.cpp:504 |
| `[UNT/role]` | 1 | owner:int unit:int unitAIOld:typeIndex unitAINew:typeIndex | CvUnitAI.cpp:1547 |
| `[UNT/horde]` city | 2 | owner:int unit:int cityX:int cityY:int dist:int pack:int reach:int | CvUnitAI.cpp:2479 |
| `[UNT/horde]` fieldPack | 2 | owner:int unit:int atX:int atY:int | CvUnitAI.cpp:2507 |
| `[UNT/horde]` fieldMarch | 2 | owner:int unit:int atX:int atY:int | CvUnitAI.cpp:2513 |
| `[UNT/merge2breach]` | 1 | owner:int unit:int type:typeIndex targetX:int targetY:int singleStr:int defStr:int | CvUnitAI.cpp:3188 |
| `[UNT/garrison]` | 2 | owner:int unit:int type:typeIndex action:string city:int | CvUnitAI.cpp:28475 |
| `[UNT/merge]` | 1 | owner:int type:typeIndex ai:typeIndex atX:int atY:int id1:int id2:int id3:int idOut:int rank:int quality:int | CvUnit.cpp:27248 |
| `[UNT/split]` | 1 | owner:int type:typeIndex ai:typeIndex atX:int atY:int idIn:int id1:int id2:int id3:int rank:int quality:int | CvUnit.cpp:27439 |
| `[UNT/mission]` | 2 | owner:int unit:int unitAI:typeIndex missionAI:typeIndex targetX:int targetY:int stack:int | CvSelectionGroupAI.cpp:1179 |
| `[COM/calib]` | 3 | atk:string atkId:int ourStr:int def:string theirStr:int climit:int nrUs:int nrThem:int roundsDiff:int heurBase:int binom:int finalBiased:int mod:int | CvUnitAI.cpp:1340 |
| `[COM/decision]` cityAttack | 2 | owner:int unit:int targetX:int targetY:int odds:int base:int | CvUnitAI.cpp:18003 |
| `[COM/decision]` anyAttack | 2 | owner:int unit:int targetX:int targetY:int odds:int base:int | CvUnitAI.cpp:18206 |
| `[COM/decision]` leaveAttack | 2 | owner:int unit:int targetX:int targetY:int odds:int base:int | CvUnitAI.cpp:18342 |
| `[COM/threshold]` | 3 | owner:int unit:int targetX:int targetY:int base:int final:int | CvUnitAI.cpp:25258 |
| `[COM/odds]` | 3 | owner:int unit:int targetX:int targetY:int goodness:int leadWin:int win:int | CvSelectionGroupAI.cpp:648 |
| `[GRP/split]` | 2 | owner:int group:int separated:int | CvSelectionGroupAI.cpp:82 |
| `[GRP/army]` | 2 | owner:int army:int mission:int leaderUnit:int atX:int atY:int targetX:int targetY:int | CvArmy.cpp:231 |
| `[GRP/leader]` | 2 | owner:int army:int leaderGroup:int | CvArmy.cpp:496 |

**Domain totals:** 20 templates. Widest: `[COM/calib]` at 13 fields. Contains `typeIndex` fields (UnitAITypes, MissionAITypes) and two string fields in `[COM/calib]` (unit/defender descriptions). `[UNT/merge]` / `[UNT/split]` at 11 fields are the widest pure-int-plus-typeIndex lines.

### 1.4 HAI — Hunter AI (`logHunterAI` → `HunterAI.log`)

Gate: `gUnitLogLevel`. Source: `Sources/AI/CvHunterAI.cpp`.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[HAI/spin]` | 1 | unit:int x:int y:int | CvHunterAI.cpp:53 |
| `[HAI/begin]` hunterMove | 1 | owner:int unit:int aitype:typeIndex automate:int withCmd:int x:int y:int stack:int | CvHunterAI.cpp:102 |
| `[HAI/begin]` autoHuntMove | 1 | owner:int unit:int aitype:typeIndex automate:int x:int y:int stack:int | CvHunterAI.cpp:448 |
| `[HAI/heal]` safety | 2 | unit:int | CvHunterAI.cpp:151 |
| `[HAI/heal]` heal | 2 | unit:int | CvHunterAI.cpp:160 |
| `[HAI/heal]` safety3 | 2 | unit:int | CvHunterAI.cpp:169 |
| `[HAI/escort]` merge | 2 | unit:int | CvHunterAI.cpp:191 |
| `[HAI/engage]` adjacent kill | 1 | unit:int | CvHunterAI.cpp:240 |
| `[HAI/scrap]` revert (owned) | 2 | unit:int owned:int | CvHunterAI.cpp:256 |
| `[HAI/scrap]` revert (deficit) | 2 | unit:int deficit:int | CvHunterAI.cpp:267 |
| `[HAI/escort]` advertise | 2 | unit:int | CvHunterAI.cpp:299 |
| `[HAI/scrap]` financial | 2 | unit:int has:int needed:int | CvHunterAI.cpp:397 |
| `[HAI/spread]` refreshExplore | 2 | unit:int | CvHunterAI.cpp:405 |
| `[HAI/spread]` borders | 2 | unit:int | CvHunterAI.cpp:411 |
| `[HAI/spread]` patrol | 2 | unit:int | CvHunterAI.cpp:417 |
| `[HAI/end]` hunterMove skip | 1 | unit:int | CvHunterAI.cpp:432 |
| `[HAI/end]` autoHuntMove skip | 1 | unit:int | CvHunterAI.cpp:562 |
| `[HAI/engage]` seaAreaAttack | 1 | unit:int | CvHunterAI.cpp:515 |
| `[HAI/engage]` blockade | 1 | unit:int | CvHunterAI.cpp:520 |
| `[HAI/explore]` exploreGeneric | 2 | unit:int | CvHunterAI.cpp:533 |
| `[HAI/explore]` seaExploreKeep | 2 | unit:int x:int y:int | CvHunterAI.cpp:611 |
| `[HAI/explore]` seaExplore | 1 | unit:int x:int y:int | CvHunterAI.cpp:683 |

**Domain totals:** 22 templates. Widest: `[HAI/begin]` hunterMove at 8 fields. Entirely int/typeIndex — no strings, no floats.

### 1.5 DAI/DIP/ESP — Decision, diplomacy, espionage (`logDecisionAI`/`logDiploAI`/`logEspionageAI`)

Gate: `gPlayerLogLevel`. Sources: `Sources/AI/CvDecisionAI.cpp`, `Sources/AI/CvPlayerAI.cpp`, `Sources/AI/CvCityAI.cpp`, `Sources/Engine/CvDeal.cpp`.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[DAI/begin]` | 1 | player:int name:string turn:int era:int | CvDecisionAI.cpp:37 |
| `[DAI/flavors]` | 1 | player:int flavor:string value:int | CvDecisionAI.cpp:42 |
| `[DAI/tech/best]` | 1 | player:int civ:string picks:string value/cost:int start:string | CvPlayerAI.cpp:4130 |
| `[DAI/tech/cand]` | 3 | player:int tech:string flavor:string contrib:int running:int | CvPlayerAI.cpp:5335 |
| `[DAI/civic/cand]` | 3 | player:int civic:string flavor:string contrib:int | CvPlayerAI.cpp:13799 |
| `[DAI/civic/best]` REVOLUTION | 2 | player:int civ:string curValue:int bestValue:int | CvPlayerAI.cpp:17290 |
| `[DAI/civic/best]` option | 2 | player:int option:int civic:string | CvPlayerAI.cpp:17296 |
| `[DAI/religion]` | 1 | player:int civ:string best:int state:int willConvert:int flRel:int | CvPlayerAI.cpp:17354 |
| `[DAI/strategy]` | 1 | player:int civ:string hash:int PRODUCTION:int MISSIONARY:int DAGGER:int CRUSH:int flMil:int flProd:int flRel:int flCul:int flGro:int | CvPlayerAI.cpp:22810 |
| `[DAI/city/unit]` | 3 | city:string unitAI:int unit:string value:int | CvCityAI.cpp:4375 |
| `[DAI/city/build]` flavor-contrib | 3 | city:string building:string flavor:string contrib:int | CvCityAI.cpp:4950 |
| `[DAI/city/build]` summary | 2 | city:string building:string flavorTotal:int finalValue:int | CvCityAI.cpp:4957 |
| `[DIP/cand]` | 3 | player:int from:int item:int data:int value:int | CvPlayerAI.cpp:7909 |
| `[DIP/dealval]` | 2 | player:int from:int items:int total:int atWar:int | CvPlayerAI.cpp:7912 |
| `[DIP/begin]` | 1 | player:int with:int give:int get:int iChange:int | CvPlayerAI.cpp:7949 |
| `[DIP/decision]` denial | 1 | player:int with:int item:int | CvPlayerAI.cpp:7964 |
| `[DIP/score]` | 2 | player:int with:int ourValue:int theirValue:int | CvPlayerAI.cpp:8022 |
| `[DIP/decision]` grant | 1 | player:int with:int verdict:string ourValue:int threshold:int | CvPlayerAI.cpp:8078 |
| `[DIP/decision]` value-compare | 1 | player:int with:int verdict:string ourValue:int theirValue:int | CvPlayerAI.cpp:8086 |
| `[DIP/trade]` | 2 | from:int to:int item:int data:int | CvDeal.cpp:783 |
| `[ESP/best]` | 1 | player:int spyAt.x:int spyAt.y:int mission:int target:int value:int | CvPlayerAI.cpp:15495 |

**Domain totals:** 21 templates. Widest: `[DAI/strategy]` at 12 fields. String-heavy: civ names, tech/civic descriptions, flavor names resolved to narrow `c_str()` at call site. The `hash` field in `[DAI/strategy]` is an int bitmask printed `%08x` — still int on the wire. `[DAI/tech/best]` carries three wide-char string fields (civ, picks, start).

### 1.6 WAR — Team war (`logWarAI` → `WarAI.log`)

Gate: `gTeamLogLevel`. Source: `Sources/AI/CvTeamAI.cpp`. All level 1.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[WAR/area]` | 1 | team:int area:int posture_old:typeIndex posture_new:typeIndex | CvTeamAI.cpp:243 |
| `[WAR/warplan]` | 1 | team:int vs_team:int plan_old:typeIndex plan_new:typeIndex atWar:int | CvTeamAI.cpp:3302 |
| `[WAR/begin]` | 1 | team:int turn:int enemyPowerPct:int fundedPct:int safePct:int atWar:int warPlans:int | CvTeamAI.cpp:3946 |

**Domain totals:** 3 templates. Widest: `[WAR/begin]` at 7 fields. Mostly int/typeIndex — no strings, no floats.

### 1.7 FND/INIT/ENG — Settler / init / engine

Gate: `gPlayerLogLevel` (FND/ENG via `logFoundAI`/`logEngine`); INIT gated by any log level > 0. Sources: `Sources/AI/CvUnitAI.cpp`, `Sources/Engine/CvGame.cpp`, `Sources/Engine/CvPlot.cpp`.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[FND/site]` | 1 | owner:int unit:int site_x:int site_y:int value:int candidateSites:int action:string | CvUnitAI.cpp:19291 |
| `[INIT/begin]` | 0 | gameState:string turn:int speed:string handicap:string startEra:string map_w:int map_h:int maxTurns:int civsAlive:int | CvGame.cpp:591 |
| `[INIT/option]` | 0 | optionType:string | CvGame.cpp:603 |
| `[INIT/victory]` | 0 | victoryType:string | CvGame.cpp:610 |
| `[INIT/player]` | 0 | id:int team:int human:int leader:string civ:string | CvGame.cpp:618 |
| `[ENG/viscap]` | 2 | team:int plot_x:int plot_y:int count:int change:int | CvPlot.cpp:9142 |

**Domain totals:** 6 templates. Widest: `[INIT/begin]` at 9 fields. String fields are all XML type-string (`getType()`), not descriptions — narrow `const char*`. `[INIT/begin]` has 5 string fields.

### 1.8 CTB — ContractBroker (`logContractBroker`)

Gate: `gPlayerLogLevel`. Sources: primarily `Sources/AI/CvContractBroker.cpp`; external call sites in `Sources/AI/CvUnitAI.cpp` and `Sources/AI/CvHunterAI.cpp`. All CTB lines from `CvContractBroker.cpp` have an implicit `owner` field appended by the internal wrapper; external call sites do not.

Selected key templates (full set is ~50; representative lines shown — `CTB.cpp` = `CvContractBroker.cpp`):

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[CTB/turn]` cleanup | 1 | contractedUnits:int advertisingTenders:int advertisingUnits:int owner:int | CTB.cpp:62 |
| `[CTB/avail]` asking for work | 1 | unit:string unitId:int atX:int atY:int owner:int | CTB.cpp:181 |
| `[CTB/avail]` unit details | 2 | unit:int worker:int healer:int offValue:int defValue:int minPriority:int owner:int | CTB.cpp:188 |
| `[CTB/work]` request details | 2 | priority:int atX:int atY:int aiType:typeIndex flags:int strength:int strengthX100:int maxPath:int join:int criteria:string owner:int | CTB.cpp:261 |
| `[CTB/work]` added | 1 | index:int priority:int atX:int atY:int aiType:typeIndex flags:int requiredStrx100:int maxPath:int join:int owner:int | CTB.cpp:335 |
| `[CTB/tender/bid]` full | 3 | workRequest:int city:string cityId:int unit:string aiType:typeIndex baseValue:int turns:int distance:int depreciatedValue:int prevBest:int owner:int | CTB.cpp:621 |
| `[CTB/tender/build]` | 1 | city:string cityId:int unit:string unitAI:typeIndex workRequest:int atX:int atY:int append:int danger:int owner:int | CTB.cpp:744 |
| `[CTB/contract]` dispatch | 1 | unit:int atX:int atY:int priority:int aiType:typeIndex joinUnit:int workRequest:int owner:int | CTB.cpp:896 |
| `[CTB/finalize]` | 1 | contractsSatisfied:int contractsTotal:int unitsEmployed:int unitsWithoutWork:int owner:int | CTB.cpp:768 |
| `[CTB] (CvUnitAI found work)` | 1 | unitName:string unitId:int player:int playerCiv:string atX:int atY:int missionInfo:string workAtX:int workAtY:int joinInfo:string | CvUnitAI.cpp:21595 |

**Domain totals:** ~50 templates. Widest: `[CTB/work]` request details and `[CTB/tender/bid]` at 11 fields each. String fields appear in city names (wide `%S`), unit descriptions, and the `criteria`/`missionInfo`/`joinInfo` pre-formatted strings. The `criteria` and `joinInfo` fields in the `CvUnitAI.cpp` call sites are pre-composed `CvString` objects — they cannot be expressed as raw field values (the "string" is composed at the call site), making them the hardest CTB lines to decompose.

### 1.9 PERF — Performance (`logPerf` → `Perf.log`)

Gate: `gPerfLogLevel`. Sources: `Sources/AI/BetterBTSAI.cpp`, `Sources/Engine/CvGame.cpp`, `Sources/AI/CvCityAI.cpp`, `Sources/AI/CvSelectionGroupAI.cpp`, `Sources/Engine/CvPlayer.cpp`, `Sources/Defines/CvGlobals.cpp`.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[PERF/phase]` RAII | 1 | turn:int owner:int phase:string ms:float | BetterBTSAI.cpp:91 |
| `[PERF/phase]` turn.wall | 1 | turn:int ms:float | CvGame.cpp:5859 |
| `[PERF/phase]` accumulator | 1 | turn:int ms:float | CvGame.cpp:5861 |
| `[PERF/phase]` pathGen | 1 | turn:int ms:float n:int | CvGame.cpp:5866 |
| `[PERF/phase]` reachable | 1 | turn:int ms:float n:int | CvGame.cpp:5867 |
| `[PERF/unitai]` | 1 | turn:int type:typeIndex ms:float n:int force:int awake:int exitReady:int | CvGame.cpp:5880 |
| `[PERF/spin]` | 2 | turn:int owner:int type:typeIndex unit:string id:int at.x:int at.y:int act:int missionQ:int missionAI:int busy:int moves:int | CvSelectionGroupAI.cpp:170 |
| `[PERF/choose]` | 1 | turn:int owner:int city:int head:int dirty:int total:float building:float building.n:int bestBldgs:float bestBldgs.n:int scoreBldgs:float scoreBldgs.n:int unit:float unit.n:int unitImm:float unitImm.n:int defender:float defender.n:int leastRep:float leastRep.n:int process:float process.n:int bestUnit:float bestUnit.n:int bestUnitAI:float bestUnitAI.n:int | CvCityAI.cpp:116 |
| `[PERF/cabvset]` | 1 | turn:int owner:int city:int numBuildings:int constructible:int enablers:int setSize:int | CvCityAI.cpp:12850 |
| `[PERF/cabv]` | 1 | owner:int flags:int preloop:float building:float defense:float happy:float health:float exp:float notdev:float sea:float maint:float spec:float commerceYields:float commerceVal:float food:float | CvCityAI.cpp:14185 |
| `[PERF/rescons]` MISMATCH | 2 | owner:int bonus:int legacy:int fast:int | CvPlayer.cpp:27308 |
| `[PERF/rescons]` summary | 2 | turn:int owner:int bonuses:int mismatches:int | CvPlayer.cpp:27314 |
| `[PERF/reqmodel]` MISMATCH bldg | 1 | building:string typed:int model:int | CvGlobals.cpp:3499 |
| `[PERF/reqmodel]` MISMATCH unit | 1 | unit:string | CvGlobals.cpp:3537 |
| `[PERF/reqmodel]` summary | 1 | buildings:int units:int mismatches:int | CvGlobals.cpp:3542 |

**Domain totals:** 15 templates. Widest: `[PERF/choose]` at 26 fields (mixed float+int sub-timer pairs). `[PERF/cabv]` at 15 float fields. PERF is the float-heaviest domain. `[PERF/spin]` has one wide-char string (unit name) plus 11 int/typeIndex fields.

### 1.10 Cascade — readJson / event spine

Gate: `gPlayerLogLevel`. Sources: `Sources/Cascade/CvCascadeReadJson.cpp`, `Sources/Cascade/CvEventSpine.cpp`.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[READJSON]` type-not-loaded | 1 | typeKey:string | CvCascadeReadJson.cpp:796 |
| `[READJSON]` no-json/parse-error | 1 | typeKey:string notes:string | CvCascadeReadJson.cpp:802 |
| `[READJSON]` main result | 1 | typeKey:string p:int city:int notes:string szCap:string cascade:int legacy:int agree:string | CvCascadeReadJson.cpp:837 |
| `[PLACEMENT] DIVERGE` | 2 | p:int city:int type:string kind:int cascade:int legacy:int reason:string | CvCascadeReadJson.cpp:957 |
| `[PLACEMENT]` summary | 1 | p:int roster:int cities:int cells:int diverge:int | CvCascadeReadJson.cpp:964 |
| `[DORMANCY] DIVERGE` | 2 | p:int city:int type:string cascadeActive:int legacyActive:int cascade:string legacy:string | CvCascadeReadJson.cpp:1031 |
| `[DORMANCY]` summary | 1 | p:int cities:int builtCells:int diverge:int | CvCascadeReadJson.cpp:1038 |
| `[STATE/game]` | 1 | turn:int state:int era:int winnerTeam:int victory:int maxTurns:int | CvCascadeReadJson.cpp:1051 |
| `[STATE/fin]` | 1 | p:int gold:float rate:int maint:int civic:int units:float strike:int finTrouble:int | CvCascadeReadJson.cpp:1061 |
| `[STATE/dip]` per-player | 2 | p:int (+ variable per-peer q:int att:int pairs) | CvCascadeReadJson.cpp:1074 |
| `[STATE/city]` | 2 | p:int id:int pop:int happy:int unhappy:int angry:int disorder:int occ:int occT:int hurryT:int conscT:int defyT:int happyT:int wltkd:int good:int bad:int food:int foodDiff:int grow:int gpp:int cultRate:int rels:int | CvCascadeReadJson.cpp:1086 |
| `[SPINE/DOMAIN]` buildingCount | 1 | type:typeIndex player:int count:int delta:int | CvEventSpine.cpp:87 |
| `[SPINE/DOMAIN]` unitCount | 1 | type:typeIndex player:int count:int delta:int | CvEventSpine.cpp:93 |
| `[SPINE/%s]` generic fallback | 1 | kind:string eventId:int type:int a:int b:int c:int | CvEventSpine.cpp:98 |

**Domain totals:** 14 templates. Widest: `[STATE/city]` at 22 fields (all int). `[STATE/dip]` has a variable field count (scales with civ count). `[SPINE/*]` lines are *already* raw-payload lines on the spine — they are existing emitters, not migration targets.

---

## 2. Field-width and type distribution

### 2.1 Widest line per domain

| Domain | Widest tag | Field count |
|--------|-----------|-------------|
| WAI | `[WAI/score]` | 16 |
| CIT | `[CIT/prop]` | 14 |
| UNT/COM/GRP | `[COM/calib]` | 13 |
| HAI | `[HAI/begin]` hunterMove | 8 |
| DAI/DIP/ESP | `[DAI/strategy]` | 12 |
| WAR | `[WAR/begin]` | 7 |
| FND/INIT/ENG | `[INIT/begin]` | 9 |
| CTB | `[CTB/work]` / `[CTB/tender/bid]` | 11 |
| PERF | `[PERF/choose]` | 26 |
| Cascade | `[STATE/city]` | 22 |

**Absolute widest:** `[PERF/choose]` (26), then `[STATE/city]` (22), then `[WAI/score]` (16).

### 2.2 Field-count histogram (~196 templates)

| Field count | Template count (approx) |
|-------------|------------------------|
| 1 | ~28 |
| 2–3 | ~32 |
| 4–5 | ~45 |
| 6–7 | ~38 |
| 8–9 | ~22 |
| 10–11 | ~16 |
| 12–13 | ~8 |
| 14–16 | ~4 |
| 22–26 | 2 (`[STATE/city]`, `[PERF/choose]`) |

The **median** is 5–6 fields; ~85% fit in ≤9 fields; only 6 templates exceed 12.

### 2.3 cType distribution

| cType | Occurrence | Notes |
|-------|-----------|-------|
| `int` | ~80% of field slots | Ints, coords, flags, counts, bool-as-int |
| `string` | ~15% | Narrow `const char*` (type keys, reason literals) or wide `wchar_t*` (city/unit names, descriptions). Cannot travel raw on a fixed-int spine |
| `typeIndex` | ~5% | Enum ints (UnitAITypes, BuildingTypes, …); travel as `int`, consumer resolves via `GC.get*Info()` |
| `float` | ~3% | Exclusively PERF (timing, gold amounts) |
| `other` | 1 field | `[CIT/order] CONSTRUCT` score is `int64_t` — needs a 64-bit or dual-32 slot |

**Pure-int lines** (no string/float/other): ~60% of templates. **String-bearing:** ~35%. **Float-bearing:** exclusively PERF, ~5%.

---

## Relationship to the event spine

This catalog is the **Stage-0 raw-field census** for migrating the BBAI domain helpers (`logBuildEvaluation`, `logCityAI`, `logUnitAI`, …) off direct `gDLL->logMsg` formatting and onto the cascade **event spine** — where the spine stays raw-payload-pure and formatting is deferred to a consumer behind the `IEventConsumer` contract. That architecture (the contract, the substrate, the consumers, the per-domain field resolution) is **not restated here** — it lives in:

- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) §5 — the event spine & the `IEventConsumer` contract (why logging is one pluggable consumer among tally/grants).
- [`../../plans/cascade-migration.md`](../../json-migration/cascade-migration.md) — the migration roadmap and what is built (the substrate `CvEventSpine` + logging/tally consumers) vs pending.

The raw-payload shape itself is the live `Sources/Cascade/CvEventSpine.{h,cpp}` — the per-domain field-tag→(name,type) resolvers and the `{int eTag; union{int i; float f;}}` slot model. Confirm field handling there, not from a prose proposal. The cross-cutting rules this catalog implies (never copy `wchar_t*` into the payload — carry entity IDs and let the consumer resolve names; typeIndex travels as `int`; intern narrow literals; binary/enum format-string variants become discriminators, not `%s` fields; `int64_t` needs a dual-slot or extended tag) are migration-design and tracked there.

## See also
- [`plotsnapshot.md`](plotsnapshot.md) — the per-plot CSV snapshot whose `(x,y)`/`plotIdx` keys the `[WAI/*]` and `[HAI/*]` lines here join against, to recover full plot context for a logged decision.
- [`README.md`](README.md) — the observability scale + the three canonical hook shapes; these gated `[TAG]` lines are hook shape #2 (gated `[TAG]` log, teed to `/events`).
- [`http-server.md`](http-server.md) — the live surface; how these lines reach `/events` (the `streamLogTee` gate, separate from the `gPlayerLogLevel` generation gate) and why you read the endpoint, not the open log file.
