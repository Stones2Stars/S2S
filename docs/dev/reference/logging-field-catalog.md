# Logging Field Catalog — Stage-0 Raw-Payload Spec

**Purpose:** This catalog is the Stage-0 input for migrating the BBAI domain helpers
(`logBuildEvaluation`, `logCityAI`, `logUnitAI`, etc.) onto the cascade event spine
(R-1 ruling: spine stays RAW-payload-pure; formatting deferred to consumer side).
Each entry lists the exact fields that must travel as raw payload so a consumer can
reconstruct the log line without re-entering game state.

Related: `docs/dev/plans/event-spine-spec.md` (spine architecture),
`docs/dev/reference/ai-logging-reference.md` (current helper API).

---

## 1. Per-Domain Template Tables

### 1.1 WAI — Worker AI (`logBuildEvaluation` → `BuildEvaluation.log`)

Gate variable: `gPlayerLogLevel`. Helper declared in `BetterBTSAI.h:35`.
All call sites in `Sources/CvWorkerAI.cpp`.
The `[%s/mission]` / `[%s/end]` family emits under both `WAI` and `WAI/city` namespaces
depending on the `section` parameter in `pushBuildMission()`.

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

**Domain totals:** 43 templates. Widest: `[WAI/score]` at 16 fields. String fields: bonus names (`getType()`), build names (`getType()`), section tag, mission name, city name (wide `%S`). No floats. No typeIndex.

---

### 1.2 CIT — City Production (`logCityAI` → `CityAI.log`)

Gate: `gCityLogLevel`. Sources: `CvCityAI.cpp`, `CvCity.cpp`. City/unit/building names are
wide `%S` from `getDescription()` or `getName()`.

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

**Domain totals:** 22 templates. Widest: `[CIT/prop]` at 14 fields. Special: `score` in
`[CIT/order] CONSTRUCT` is `int64_t` (printed `%I64d`) — typed `other`; needs separate
treatment on the spine. All city/unit/project names are wide strings resolved at call site.

---

### 1.3 UNT/COM/GRP — Unit, Combat, Group

Gate: `gUnitLogLevel`. Sources: `CvUnitAI.cpp`, `CvUnit.cpp`, `CvSelectionGroupAI.cpp`, `CvArmy.cpp`.

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

**Domain totals:** 20 templates. Widest: `[COM/calib]` at 13 fields. Contains `typeIndex`
fields (UnitAITypes, MissionAITypes) and two string fields in `[COM/calib]` (unit/defender
descriptions). The `[UNT/merge]` and `[UNT/split]` templates at 11 fields each are the
widest pure-int-plus-typeIndex lines.

---

### 1.4 HAI — Hunter AI (`logHunterAI` → `HunterAI.log`)

Gate: `gUnitLogLevel`. Source: `CvHunterAI.cpp`.

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

**Domain totals:** 22 templates. Widest: `[HAI/begin]` hunterMove at 8 fields. Entirely
int/typeIndex — no strings, no floats. Easiest domain for spine migration.

---

### 1.5 DAI/DIP/ESP — Decision, Diplomacy, Espionage (`logDecisionAI`/`logDiploAI`/`logEspionageAI`)

Gate: `gPlayerLogLevel`. Sources: `CvDecisionAI.cpp`, `CvPlayerAI.cpp`, `CvCityAI.cpp`, `CvDeal.cpp`.

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

**Domain totals:** 21 templates. Widest: `[DAI/strategy]` at 12 fields. String-heavy: civ
names, tech/civic descriptions, flavor names resolved to narrow `c_str()` at call site. The
`hash` field in `[DAI/strategy]` is an int bitmask printed `%08x` — still int on the wire.
`[DAI/tech/best]` carries three string fields (civ, picks, start) that are wide-char
descriptions.

---

### 1.6 WAR — Team War (`logWarAI` → `WarAI.log`)

Gate: `gTeamLogLevel`. Source: `CvTeamAI.cpp`. All level 1.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[WAR/area]` | 1 | team:int area:int posture_old:typeIndex posture_new:typeIndex | CvTeamAI.cpp:243 |
| `[WAR/warplan]` | 1 | team:int vs_team:int plan_old:typeIndex plan_new:typeIndex atWar:int | CvTeamAI.cpp:3302 |
| `[WAR/begin]` | 1 | team:int turn:int enemyPowerPct:int fundedPct:int safePct:int atWar:int warPlans:int | CvTeamAI.cpp:3946 |

**Domain totals:** 3 templates. Widest: `[WAR/begin]` at 7 fields. Mostly int/typeIndex — no
strings, no floats. Second easiest domain after HAI.

---

### 1.7 FND/INIT/ENG — Settler/Init/Engine

Gate: `gPlayerLogLevel` (FND/ENG via `logFoundAI`/`logEngine`); INIT gated by any log level > 0.

| Tag | Lvl | Fields (name:cType) | Sample site |
|-----|-----|---------------------|-------------|
| `[FND/site]` | 1 | owner:int unit:int site_x:int site_y:int value:int candidateSites:int action:string | CvUnitAI.cpp:19291 |
| `[INIT/begin]` | 0 | gameState:string turn:int speed:string handicap:string startEra:string map_w:int map_h:int maxTurns:int civsAlive:int | CvGame.cpp:591 |
| `[INIT/option]` | 0 | optionType:string | CvGame.cpp:603 |
| `[INIT/victory]` | 0 | victoryType:string | CvGame.cpp:610 |
| `[INIT/player]` | 0 | id:int team:int human:int leader:string civ:string | CvGame.cpp:618 |
| `[ENG/viscap]` | 2 | team:int plot_x:int plot_y:int count:int change:int | CvPlot.cpp:9142 |

**Domain totals:** 6 templates. Widest: `[INIT/begin]` at 9 fields. String fields are all
XML type-string (`getType()`) not descriptions — narrow `const char*`, shorter-lived.
`[INIT/begin]` has 5 string fields; this is the most string-dense 9-field line in the catalog.

---

### 1.8 CTB — ContractBroker (`logContractBroker`)

Gate: `gPlayerLogLevel`. Source: primarily `CvContractBroker.cpp`; external call sites in
`CvUnitAI.cpp` and `CvHunterAI.cpp`. All CTB lines from `CvContractBroker.cpp` have an
implicit `owner` field appended by the internal wrapper; external call sites do not.

Selected key templates (full set is 50+ entries; representative lines shown):

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

**Domain totals:** ~50 templates. Widest: `[CTB/work]` request details and
`[CTB/tender/bid]` at 11 fields each. String fields appear in city names (wide `%S`),
unit descriptions, and the `criteria`/`missionInfo`/`joinInfo` pre-formatted strings.
The `criteria` and `joinInfo` fields in CvUnitAI.cpp call sites are pre-composed
`CvString` objects — these cannot be expressed as raw field values; they are the hardest
CTB lines to migrate because the "string" is itself composed at the call site.

---

### 1.9 PERF — Performance (`logPerf` → `Perf.log`)

Gate: `gPerfLogLevel`. Sources: `BetterBTSAI.cpp`, `CvGame.cpp`, `CvCityAI.cpp`,
`CvSelectionGroupAI.cpp`, `CvPlayer.cpp`, `CvGlobals.cpp`.

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

**Domain totals:** 15 templates. Widest: `[PERF/choose]` at 26 fields (mixed float+int
sub-timer pairs). `[PERF/cabv]` at 15 float fields. PERF is the float-heaviest domain.
`[PERF/spin]` has one wide-char string (unit name) plus 11 int/typeIndex fields.

---

### 1.10 Cascade — CvCascadeReadJson / CvEventSpine

Gate: `gPlayerLogLevel`. Sources: `Sources/Cascade/CvCascadeReadJson.cpp`,
`Sources/Cascade/CvEventSpine.cpp`.

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

**Domain totals:** 14 templates. Widest: `[CITY]` at 22 fields (all int). The `[DIP]` line
has variable field count (scales with civ count). `[SPINE/*]` lines are already on the
spine — these are existing raw-payload lines, not migration targets.

---

## 2. Field-Width and Type Distribution

### 2.1 Widest Lines Per Domain

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
| Cascade | `[CITY]` | 22 |

**Absolute widest:** `[PERF/choose]` at 26 fields, followed by `[CITY]` at 22, then
`[WAI/score]` at 16.

### 2.2 Field-Count Histogram (across all ~196 templates)

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
| 22–26 | 2 (`[CITY]`, `[PERF/choose]`) |

The **median** is 5–6 fields. Roughly 85% of templates fit in 9 fields or fewer.
Only 6 templates exceed 12 fields.

### 2.3 cType Distribution

| cType | Occurrence | Notes |
|-------|-----------|-------|
| `int` | Dominant — ~80% of all field slots | Plain ints, coords, flags, counts, bool-as-int |
| `string` | ~15% of field slots | Narrow `const char*` (type keys, reason literals) or wide `wchar_t*` (city/unit names, descriptions). Cannot travel raw on a fixed-int spine. |
| `typeIndex` | ~5% of field slots | Enum ints (UnitAITypes, BuildingTypes, etc.); travel as `int` on wire, consumer resolves via `GC.get*Info()`. |
| `float` | ~3% of field slots | Exclusively in PERF domain (timing, gold amounts). Not present elsewhere. |
| `other` | 1 field | `[CIT/order] CONSTRUCT` score is `int64_t`. Requires 64-bit slot or two 32-bit slots. |

**Pure-int lines** (zero string/float/other fields): approximately 60% of all templates.
**String-bearing lines**: approximately 35% — these require special handling (interned
index, pointer, or a side-channel string slot).
**Float-bearing lines**: exclusively PERF, approximately 5%.

---

## 3. Recommended Raw-Payload Shape for CvCascadeEvent

### 3.1 Current event struct (insufficient)

```cpp
struct CvCascadeEvent {
    EventKind eKind;  // KIND enum
    int iEventId;
    int iType;        // 1 generic type slot
    int iA;           // 3 generic int slots
    int iB;
    int iC;
};
```

This covers only the existing `[SPINE/DOMAIN]` lines (4 fields each). The catalog's
median of 5–6 fields and maximum of 26 already break this shape.

### 3.2 Design constraints (from spine-spec §9b)

- **Allocation-free hot path**: no `std::vector`, no `new`, no `std::string` on the spine.
- **32-bit process**: pointer size is 4 bytes; no `int64_t` unless explicitly accommodated.
- **C++03**: no `std::array`, no variadic templates; `union`, plain arrays, and structs only.
- **Consumer resolves names**: typeIndex fields travel as raw `int`; strings must be either
  interned (index into a stable table) or deferred entirely to a side channel.

### 3.3 Recommended shape: fixed typed-slot array + tag discriminator

The catalog shows a clear bimodal structure:

**Tier A — ~85% of templates (≤9 fields, pure int/typeIndex):** A fixed array of 10
`int` slots covers them completely with no overhead.

**Tier B — outliers:** `[PERF/choose]` (26 fields, 10 floats), `[CITY]` (22 ints),
`[WAI/score]` (16 ints), and `[CIT/prop]` (14 ints) exceed 10 slots; `[PERF/choose]`
also requires `float`. The CTB pre-composed string fields (`criteria`, `joinInfo`)
cannot be decomposed into raw fields at all without restructuring the call sites.

**Recommended struct (Stage-0 proposal — REFINED on build, see below):**

> ⚠ **As-built differs (2026-06-18):** the field model was refined to **per-domain field resolution** — there is NO
> global `SpineFieldTag` enum; each domain registers its own field-tag→(name,type) resolver, and the slot is
> `{int eTag; union{int i; float f;}}` (the eTag is a domain-local int). `SpineFieldType` carries many typeIndex kinds
> (`SFT_BUILDING`/`UNIT`/`BONUS`/…) that resolve to type names. The proposal below captured the SHAPE; the authoritative
> contract is `event-spine-spec.md §3` + `Sources/Cascade/CvEventSpine.h`.

```cpp
// Stage-0 PROPOSAL (refined to per-domain resolvers on build — see the note above):
enum SpineFieldTag { SF_INT = 0, SF_FLOAT = 1 };

struct CvCascadeEventField {
    SpineFieldTag eTag;
    union {
        int   iVal;
        float fVal;
    };
};

static const int SPINE_MAX_FIELDS = 16;

struct CvCascadeEvent {
    EventKind          eKind;
    int                iEventId;
    int                iDomainTag;    // domain-local line discriminator (enum per domain)
    int                nFields;
    CvCascadeEventField aFields[SPINE_MAX_FIELDS];
};
```

**Why 16 slots:** Covers `[WAI/score]` (16 fields, the widest non-PERF, non-state-dump
line), `[CIT/prop]` (14), `[COM/calib]` (13), `[DAI/strategy]` (12), and all HAI/WAR
lines. This covers every operational AI line (WAI, CIT, UNT, HAI, DAI, DIP, WAR, CTB,
FND) at the 97th percentile of width.

**PERF outliers argue for a per-domain struct.** `[PERF/choose]` at 26 fields (10
floats + 16 ints/floats mixed) and `[CITY]` at 22 ints cannot fit the 16-slot generic
array without either bloating the struct for every event (adding 26 float-capable slots
wastes ~96 bytes per event in the common case) or a per-domain override. Recommendation:
**PERF lines use a dedicated `CvPerfEvent` struct** with its own consumer path; the
cascade state-dump lines (`[CITY]`, `[DIP]`) are diagnostic-only and can use a
lightweight string-payload path (a pre-formatted `const char*` from a caller-owned
fixed buffer) since they are already assembled at low frequency.

**String fields:** All `string`-typed fields in the catalog are one of three kinds:
1. **XML type keys** (`getType()` — narrow `const char*`, stable for process lifetime):
   pass as a `const char*` in a separate `szLabel` slot on the event, or intern to an
   `int` index via a global string table built at load time.
2. **In-game names** (city names, unit descriptions — wide `wchar_t*`, session-stable):
   pass as an entity ID (`int`) in a field slot; consumer resolves via `GC` or entity
   pointer. **Never copy `wchar_t*` into the event struct on the hot path.**
3. **Composed strings** (CTB `criteria`, `joinInfo`; WAI `mission` literal): these are
   already composed by the caller and cannot be decomposed without call-site refactoring.
   Migrate these lines last; until then, emit them via the legacy helper in parallel with
   the spine (the shadow-logging model from spine-spec).

**typeIndex fields** travel as `int` with `SF_INT` tag; the `iDomainTag` discriminator
tells the consumer which `GC.get*Info()` to call for name resolution.

### 3.4 One-line summary of recommended shape

Use a fixed `CvCascadeEventField aFields[16]` typed-slot array (int or float union) for
all operational AI domains (WAI/CIT/UNT/HAI/DAI/DIP/WAR/CTB/FND); cap at 16 slots which
covers 97% of templates; treat PERF as a separate `CvPerfEvent` struct given its float
depth and 26-field outlier; defer CTB composed-string lines and cascade state-dump lines
to a legacy-parallel shadow path until call sites can be refactored.

---

## 4. Migration Notes — Domain Cut Order

### Tier 1 — Trivial (few int/typeIndex fields, no strings, no floats)

**WAR** (3 templates, max 7 fields, pure int/typeIndex): one afternoon's work.
All fields are raw ints or TypeIndex enums. No call-site refactoring needed.

**HAI** (22 templates, max 8 fields, pure int/typeIndex): slightly larger but equally
clean. No wide strings, no floats. The `aitype`/`automate` typeIndex fields travel
as raw int. **Recommended pilot domain.**

### Tier 2 — Moderate (int-dominant, occasional narrow string/typeIndex)

**UNT/COM/GRP** (20 templates, max 13 fields): mostly int/typeIndex. Two string fields
in `[COM/calib]` (attacker/defender descriptions) can be replaced with unit ID + player
ID and resolved by the consumer. The `[UNT/act]` `decision`/`reason` strings are
domain-internal literals — intern them to a per-domain `int` enum.

**DAI/DIP** (21 templates, max 12 fields): mostly int. String fields are civ/tech
descriptions (resolve via player/tech ID) and flavor-type names (intern at load time).
`[DAI/strategy]` at 12 fields fits within 16 slots.

**FND/INIT/ENG** (6 templates, max 9 fields): low volume, init-time. Strings are all
XML type keys — intern at startup. Low risk.

### Tier 3 — Harder (wider lines, pre-composed strings, or city/unit name strings)

**WAI** (43 templates, max 16 fields): widest operational domain. The bonus/build name
strings are all `getType()` — intern to `int`. City name in `[WAI/city/begin]` etc. is
wide `wchar_t*` — replace with city ID. `[WAI/score]` at 16 fields fits the 16-slot cap
exactly. The `section` string in the `[%s/mission]`/`[%s/end]` family is a binary flag
(WAI vs WAI/city) — encode as 1 bit in `iDomainTag`.

**CIT** (22 templates, max 14 fields): city/unit/building name strings are wide
descriptions — replace with entity IDs. The `int64_t` score in `[CIT/order] CONSTRUCT`
needs two `int` slots or a dedicated 64-bit field; simplest fix is to pack it as two
consecutive `SF_INT` slots (high word / low word) with a consumer contract. The `other`
typed `[CIT/prop]` `propPct` calculation (`iPropControlInArea * 100 / (iUnitsInArea + 1)`)
should be computed before emitting; result is an `int`.

**CTB** (~50 templates, max 11 fields): the `criteria`/`joinInfo` pre-composed string
fields in CvUnitAI.cpp are the hardest: they are the result of multiple formatting
calls before `logContractBroker`. Options: (a) decompose at the call site (requires
audit of all composing code), or (b) keep these specific lines on the legacy helper
in the shadow-logging phase indefinitely. All other CTB fields are int/typeIndex/city-ID.

### Tier 4 — Deferred (structural outliers)

**PERF** (15 templates, max 26 fields, float-heavy): `[PERF/choose]` with 26 mixed
float/int fields is a one-off instrumentation line. Use a dedicated `CvPerfEvent`
struct instead of the generic slot array. `[PERF/spin]` has a wide-char unit name —
replace with unit ID. All other PERF lines are manageable once the per-domain struct
exists.

**Cascade state-dump** (`[CITY]` at 22 fields, `[DIP]` variable width): these are
diagnostic/shadow lines emitted at low frequency outside the hot AI path. They can
remain on the legacy path or emit as a pre-formatted `const char*` diagnostic payload.

---

## 5. Cross-Cutting Migration Rules

1. **Never copy `wchar_t*` into event payload.** Always carry entity IDs (city ID, unit
   ID, player ID) and let the consumer call `getName()` / `getDescription()` if needed
   for display. This keeps the hot path allocation-free.

2. **typeIndex fields travel as `int`.** The `iDomainTag` discriminator (a per-domain
   enum baked into the event) tells the logging consumer which `GC.get*Info()` resolver
   to use. Do not resolve names on the emit path.

3. **Intern narrow string literals at load time.** Flavor names, XML type keys, reason
   codes (e.g., `"inaccessible"`, `"noPath"`) are stable for the game session. Build a
   per-domain string-to-int intern table at startup; carry the int on the spine.

4. **Binary/enum literals in format strings become `iDomainTag` sub-discriminators.**
   Lines like `[HAI/heal] action=safety` vs `action=heal` vs `action=safety3` are
   already distinct templates; the action is not a `%s` field but a discriminator.
   Encode as a per-domain sub-tag int, not a string field.

5. **Composed strings are the migration blocker.** Any field whose value is assembled
   by a `CvString::format()` chain before the log call cannot be decomposed without
   refactoring the composing code. Identify these early per domain and plan the
   call-site refactor or accept legacy parallel emission for that line.

6. **`int64_t` fields:** encode as two consecutive `SF_INT` slots (high:int, low:int)
   with consumer contract, or extend `CvCascadeEventField` with a `SF_INT64` tag and
   `int64_t` union member. The latter adds 4 bytes per field slot on a 32-bit process
   but is cleaner. Decide before the CIT domain migration.
