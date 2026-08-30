# The tooltip LOOK reference — every legacy tooltip, in free text

**What each tooltip SHOWED, written as plain text with standardized icon placeholders.** This is the reference
for [mimic how tooltips LOOKED; never compromise how they are RENDERED](../architecture/patterns/04-the-info-data-out-contract-what-an/03-the-coherent-surface-grouped.md#the-coherent-surface--grouped-storage-parameterized-getters-clarity-and-predictability-is-king):
the legacy LOOK is the target, the legacy MECHANISM is not.

⛔ **IT IS A LOOK RECORD AND A DESIGN SURFACE — NEVER A CONTENT MANDATE.** Legacy PARITY does not come back: the
tooltip SET stays demand-driven, and completeness is measured against what the ENTITY CARRIES, never against
what the legacy composer used to print (same page). A line here is evidence of how something READ, not an
obligation to print it again.

## How to use it

**To design a tooltip, write it here in free text.** That is the whole point of the file: the hard part of a
composer is the block structure — which sources compose a block, what heading it sits under, and when it shows —
and that is a design decision best made in prose, not in C++. Sketch the wanted tooltip as lines and
placeholders, then implement it through the shared entry renderer.

⛔ **The implementation NEVER follows from this file's mechanism, only its shape.** A line here may have come
from a hand-assembled legacy composer reading a legacy getter; reproducing THAT is the thing the spec bans. The
composer consumes rendered entry lines
([the division of labour](../architecture/patterns/04-the-info-data-out-contract-what-an/03-the-coherent-surface-grouped.md#the-coherent-surface--grouped-storage-parameterized-getters-clarity-and-predictability-is-king));
where the wanted look does not fall out of the renderer, the RENDERER gains the capability, centrally — never
the composer a special case.

## The placeholder vocabulary

Icons are written as `<name>` so a line says what it looks like without depending on a font symbol:

| placeholder | | placeholder | |
|---|---|---|---|
| `<food>` | food | `<happy>` / `<unhappy>` | the happiness pair |
| `<hammer>` | production | `<health>` / `<unhealth>` | the health pair |
| `<commerce>` | commerce | `<power>` | power |
| `<gold>` | gold | `<greatperson>` | great people |
| `<beaker>` | research | `<strength>` · `<moves>` | unit stats |
| `<culture>` | culture | `<religion icon>` · `<corporation icon>` | resolved at runtime |
| `<spy>` | espionage | `<name of the thing>` | the entity's own description |

`%d1` / `%s2` and friends are the legacy value placeholders, kept verbatim — they show WHERE a number or a name
landed in the sentence, which is part of the look. `*` is a bullet. `→ helper` marks a delegation to another
composer rather than text of its own.

## Provenance and maintenance

Seeded mechanically from the legacy `Sources/CvGameTextMgr.cpp` at the initial commit (`bb5cb972e`), resolving
each `TXT_KEY` against the current `Assets/XML/GameText` and normalising icons to the vocabulary above. **It is
hand-maintained from here** — there is no regenerator, deliberately: a regenerator would overwrite the design
work this file exists to hold.

⚠ Entries are the composer's emissions **in source order, de-duplicated** — a composer's branches are all listed,
so a single run showed some subset of its lines, never all of them at once. Read it as the vocabulary a tooltip
drew from, not as one screen.

⚑ The companion is `python Tools/verify-tooltip-composers.py`, which censuses the LIVE composers and says which
still hand-build. That tool answers the MECHANISM; this file answers the LOOK. Neither alone says a tooltip is
finished.

---


## `buildAdjustString`

- *Can Adjust <commerce icon> Rate  <!-- TXT_KEY_MISC_ADJUST_COMMERCE_RATE -->

## `buildBonusRevealString`

- <name of the thing>
- → `setListHelp`
- *Reveals  <!-- TXT_KEY_MISC_REVEALS -->

## `buildBridgeString`

- *Enables Bridge Building  <!-- TXT_KEY_MISC_ENABLES_BRIDGE_BUILDING -->

## `buildBuildingRequiresString`

- <name of the thing>
- → `setListHelp`
- Requires  <!-- TXT_KEY_REQUIRES -->
- or  <!-- TXT_KEY_OR -->
- Requires %s2_tech (any player).  <!-- TXT_KEY_REQUIRES_TECH_ANYONE -->
- *Requires %s2  <!-- TXT_KEY_REQUIRES_LINK -->
- Requires %s2 (%d3 Total) in any city  <!-- TXT_KEY_HELPTEXT_REQUIRES_NUM_BUILDINGS_0 -->
- Requires %s2 (%d3/%d4 Total) in any city  <!-- TXT_KEY_HELPTEXT_REQUIRES_NUM_BUILDINGS_1 -->
- Requires at least %d1_Num Cities  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_NUM_CITIES -->
- Requires a Unit of Level %d1_Num Experience  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_UNIT_LEVEL -->
- Can build at a minimum latitude of %d1 degrees  <!-- TXT_KEY_MIN_LATITUDE -->
- Can build at a maximum latitude of %d1 degrees  <!-- TXT_KEY_MAX_LATITUDE -->
- You must have a State <religion> and it must be in the city.  <!-- TXT_KEY_REQUIRES_STATE_RELIGION -->
- Can only be Constructed in the <religion icon> Holy City.  <!-- TXT_KEY_ACTION_ONLY_HOLY_CONSTRUCT -->
- *This building requires <religion icon> in the city.  <!-- TXT_KEY_REQUIRES_PREREQUISITE_RELIGION -->
- *Some features only function if <religion icon> is your State <religion>.  <!-- TXT_KEY_BUILDINGHELP_RELIGION_DECLARED -->
- *This building requires <religion icon> to be your State <religion>.  <!-- TXT_KEY_BUILDINGHELP_STATE_RELIGION_PREREQ -->
- *Requires  <!-- TXT_KEY_REQUIRES_2 -->
-   <!-- TXT_KEY_SET_WARNING_COLOR -->
- and  <!-- TXT_KEY_AND -->
- Requires Civics to be Active  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_ACTIVE_CIVICS_MET -->
- Requires %s1_Text in city and in city vicinity, or manufactured locally.  <!-- TXT_KEY_REQUIRES_BONUS_VICINITY -->
- Requires %s1_Text in City Vicinity, or manufactured locally (No need for improvement)  <!-- TXT_KEY_REQUIRES_BONUS_RAWVICINITY -->
- Bonus required in city and in city vicinity, or manufactured locally:  <!-- TXT_KEY_REQUIRES_BONUS_VICINITY_ONEOF -->
- %s2  <!-- TXT_KEY_LINK -->
- Bonus required in city vicinity, or manufactured locally (No need for improvement):  <!-- TXT_KEY_REQUIRES_BONUS_RAWVICINITY_ONEOF -->
- Requires Access to Power  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_POWER -->
- Can only be built and operated during Wartime  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_WARTIME -->
- Requires City Size of at least %d1  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_POPULATION -->
- Requires River or Coastal Access  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_RIVER_OR_COAST -->
- Requires Coast  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_COAST -->
- Requires River Access  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_RIVER -->
- Requires Fresh water  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_FRESH_WATER -->
- Requires a Culture Level of %s1 or better  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_CULTURE -->
- Requires Anyone to have constructed %s1  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_ANY_BUILDING -->
- Can Not be Built in the Same City as  <!-- TXT_KEY_BUILDINGHELP_NOT_REQUIRED_TO_BUILD -->
- In City Vicinity  <!-- TXT_KEY_IN_CITY_VICINITY -->
- Nuclear Weapons are Banned  <!-- TXT_KEY_PROJECTHELP_NO_NUKES -->
- → `buildDisplayString`
- *%s1_Name Victory must be Enabled  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_VICTORY -->
- *Can only be Built on %s1_Name and Earlier Starts  <!-- TXT_KEY_BUILDINGHELP_MAX_START_ERA -->
- *Requires at least %d1_Num Teams  <!-- TXT_KEY_BUILDINGHELP_REQUIRES_NUM_TEAMS -->
- Requires %s1_Text  <!-- TXT_KEY_UNITHELP_REQUIRES_STRING -->

## `buildBuildingTechHappinessChangesString`

- with  <!-- TXT_KEY_WITH -->
- <name of the thing>
- → `setListHelp`

## `buildBuildingTechHealthChangesString`

- with  <!-- TXT_KEY_WITH -->
- <name of the thing>
- → `setListHelp`

## `buildBuildingTechSpecialistChangeString`

- <name of the thing>
- Can turn 1 Citizen into %s2_SpecName  <!-- TXT_KEY_BUILDINGHELP_TURN_CITIZEN_INTO_NO_BULLET -->
- Can turn %d1_Num Citizens into %s3_SpecName  <!-- TXT_KEY_BUILDINGHELP_TURN_CITIZENS_INTO_NO_BULLET -->

## `buildCanFoundOnPeaksString`

- *Players can found cities on Peaks  <!-- TXT_KEY_CAN_FOUND_ON_PEAKS -->

## `buildCanPassPeaksString`

- *Units can move onto Peaks  <!-- TXT_KEY_CAN_PASS_PEAKS -->

## `buildCanRebaseAnywhereString`

- *Removes Airlift Range Restriction  <!-- TXT_KEY_CAN_REBASE_ANYWHERE -->

## `buildCivicRevealString`

- <name of the thing>
- → `setListHelp`
- *Enables  <!-- TXT_KEY_MISC_ENABLES -->

## `buildDefensivePactString`

- *Enables Defensive Pacts  <!-- TXT_KEY_MISC_ENABLES_DEFENSIVE_PACTS -->

## `buildDomainExtraMovesString`

- *%D1_Change Extra Operation Range For Aircraft  <!-- TXT_KEY_MISC_EXTRA_RANGE -->
- *%D1_Change Extra Moves for %s2_Name  <!-- TXT_KEY_MISC_EXTRA_MOVES -->

## `buildDomesticTradeString`

- → `buildTradeString`

## `buildEmbassyString`

- *Allows for the establishment of an Embassy  <!-- TXT_KEY_MISC_ENABLES_EMBASSIES -->

## `buildFeatureProductionString`

- *Workers Produce %D1_Change%% <hammer> from Chopping  <!-- TXT_KEY_FEATUREHELP_PRODUCTION_MODIFIER -->

## `buildFinanceAwaySupplyString`

- %d1<gold>: Handicap cost  <!-- TXT_KEY_FINANCE_ADVISOR_HANDICAP_COST -->
- %d3<gold>: Away supply cost for %d1 units (free supply for %d2)%s4------------------------TOTAL SUPPLY COST: %d5<gold>  <!-- TXT_KEY_FINANCE_ADVISOR_SUPPLY_COST -->

## `buildFinanceCityMaintString`

- %s1_MaintNum <gold>: Base Maintenance  <!-- TXT_KEY_MISC_BASE_MAINTENANCE -->
- ------------------------ %s1<gold>: Distance to Capital  <!-- TXT_KEY_FINANCE_ADVISOR_CITY_MAINT_COST_DISTANCE -->
- %s1_MaintNum <gold>: Number of Cities  <!-- TXT_KEY_MISC_NUM_CITIES_FLOAT -->
- %s1_MaintNum <gold>: Colonial expenses  <!-- TXT_KEY_MISC_COLONY_MAINT_FLOAT -->
- %s1_MaintNum <gold>: Corporation payments  <!-- TXT_KEY_MISC_CORPORATION_MAINT_FLOAT -->
- %s1_MaintNum <gold>: Building maintenance  <!-- TXT_KEY_MISC_BUILDING_MAINT_FLOAT -->
- ------------------------ TOTAL NATIONWIDE CITY MAINTENANCE COST: %d1<gold>  <!-- TXT_KEY_FINANCE_ADVISOR_CITY_MAINT_COST_TOTAL -->

## `buildFinanceCivicUpkeepString`

- <name of the thing>
- %s1------------------------TOTAL CIVIC UPKEEP: %d2<gold>  <!-- TXT_KEY_FINANCE_ADVISOR_CIVIC_UPKEEP_COST -->

## `buildFinanceForeignIncomeString`

- %s1------------------------TOTAL FOREIGN INCOME: %d2<gold>  <!-- TXT_KEY_FINANCE_ADVISOR_FOREIGN_INCOME -->

## `buildFinanceInflationString`

- Total costs before inflation = %d1<gold>  <!-- TXT_KEY_FINANCE_ADVISOR_INFLATION_0 -->
- Inflation rate = %d1.%d2%%Cost due to Inflation = %d3<gold>  <!-- TXT_KEY_FINANCE_ADVISOR_INFLATION_1 -->
- Sources of Inflation *Inflation from Civics = %d1% *Inflation from Technologies = %d2% *Inflation from Buildings = %d3% *Inflation from Projects = %d4% *Inflation from Hurrying Production = %d5%  <!-- TXT_KEY_FINANCE_ADVISOR_SOURCES_OF_INFLATION -->

## `buildFinanceSpecialistGoldString`

- %s1<gold>: %s3 (%d2)  <!-- TXT_KEY_BUG_FINANCIAL_ADVISOR_SPECIALIST_GOLD -->
- <name of the thing>
- ------------------------TOTAL SPECIALISTS: %d1<gold>  <!-- TXT_KEY_BUG_FINANCIAL_ADVISOR_SPECIALIST_TOTAL_GOLD -->

## `buildFinanceUnitUpkeepString`

- Civilian Unit upkeep: * Upkeep: %s1 <gold>  <!-- TXT_KEY_FINANCE_ADVISOR_UNIT_UPKEEP_CIVILIAN -->
- * Civic change: %s1 <gold>  <!-- TXT_KEY_FINANCE_ADVISOR_UNIT_UPKEEP_MOD_CIVIC -->
- * Trait change: %s1 <gold>  <!-- TXT_KEY_FINANCE_ADVISOR_UNIT_UPKEEP_MOD_TRAIT -->
- * Free: %d1 <gold>  <!-- TXT_KEY_FINANCE_ADVISOR_UNIT_UPKEEP_FREE -->
- * Total: %d1 <gold>  <!-- TXT_KEY_FINANCE_ADVISOR_UNIT_UPKEEP_TOTAL_1 -->
- Military Unit upkeep: * Upkeep: %s1 <gold>  <!-- TXT_KEY_FINANCE_ADVISOR_UNIT_UPKEEP_MILITARY -->
- *Handicap adjustment: %d1 <gold>  <!-- TXT_KEY_FINANCE_ADVISOR_UNIT_UPKEEP_HANDICAP_ADJUSTMENT -->
- -------------------------------- Total unit upkeep: %d1 <gold>  <!-- TXT_KEY_FINANCE_ADVISOR_UNIT_UPKEEP_TOTAL_2 -->

## `buildForeignTradeString`

- → `buildTradeString`

## `buildFoundCorporationString`

- <name of the thing>
- → `setListHelp`
- *First to Discover Incorporates  <!-- TXT_KEY_MISC_FIRST_DISCOVER_INCORPORATES -->

## `buildFoundReligionString`

- a Religion  <!-- TXT_KEY_RELIGION_UNKNOWN -->
- <name of the thing>
- → `setListHelp`
- *First to Discover Founds  <!-- TXT_KEY_MISC_FIRST_DISCOVER_FOUNDS -->

## `buildFreeTechString`

- *First to Discover Receives a Free Technology  <!-- TXT_KEY_TECHHELP_FIRST_FREE_TECH -->
- *First to Discover Receives %d1_Num Free Technologies  <!-- TXT_KEY_TECHHELP_FIRST_FREE_TECHS -->

## `buildFreeUnitString`

- *First to Discover receives a %s2_UnitName.  <!-- TXT_KEY_TECHHELP_FIRST_RECEIVES -->

## `buildGameObjectRelationString`

- Trade Cities  <!-- TXT_KEY_RELATION_TRADE -->
- %s1 in range %D2  <!-- TXT_KEY_RELATION_NEAR -->
- Associated City  <!-- TXT_KEY_RELATION_WORKING_CITY -->
- Plots in city area  <!-- TXT_KEY_RELATION_WORKING_PLOT -->

## `buildGoldTradeString`

- *Enables Gold Trading via Diplomacy  <!-- TXT_KEY_MISC_ENABLES_GOLD_TRADING -->

## `buildHappinessRateString`

- *%D1_Change%F2_HappyOrUn in All Cities  <!-- TXT_KEY_MISC_HAPPINESS_ALL_CITIES -->

## `buildHealthRateString`

- *%D1_Change%F2_HealthOrUn in All Cities  <!-- TXT_KEY_MISC_HEALTH_ALL_CITIES -->

## `buildIgnoreIrrigationString`

- *Can Build Farms without Irrigation  <!-- TXT_KEY_MISC_IRRIGATION_ANYWHERE -->

## `buildImprovementString`

- Obsoletes %s2_TechName  <!-- TXT_KEY_TECHHELP_OBSOLETES -->
- *Can %s1_ImpName  <!-- TXT_KEY_MISC_CAN_BUILD_IMPROVEMENT -->
- *May only exist on a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_PREREQUISITE -->

## `buildIrrigationString`

- *Farms Spread Irrigation  <!-- TXT_KEY_MISC_SPREAD_IRRIGATION -->

## `buildLOSString`

- *+1 Sight Across Water  <!-- TXT_KEY_UNITHELP_EXTRA_SIGHT -->

## `buildMaintenanceModifiersString`

- *%D1_MOD%% Maintenance Costs in All Cities  <!-- TXT_KEY_TECHHELP_MAINT_MOD -->
- *%D1_Mod%% Maintenance Costs from Distance to Palace  <!-- TXT_KEY_TECHHELP_DISTANCE_MAINT_MOD -->
- *%D1_Mod%% Maintenance Costs from Number of Cities  <!-- TXT_KEY_TECHHELP_NUM_CITIES_MAINT_MOD -->
- *%D1_Mod%% Maintenance Costs from Distance to Palace for Coastal Cities  <!-- TXT_KEY_COASTAL_DISTANCE_MAINT_MOD -->

## `buildMapCenterString`

- *Centers World Map  <!-- TXT_KEY_MISC_CENTERS_MAP -->

## `buildMapRevealString`

- *Reveals World Map  <!-- TXT_KEY_MISC_REVEALS_MAP -->

## `buildMapTradeString`

- *Enables Map Trading  <!-- TXT_KEY_MISC_ENABLES_MAP_TRADING -->

## `buildMoveFastPeaksString`

- *Units suffer no movement penalty when passing through peaks  <!-- TXT_KEY_MOVE_FAST_PEAKS -->

## `buildMoveString`

- *%D1_Change %s2_MoveType Movement  <!-- TXT_KEY_UNITHELP_MOVEMENT -->

## `buildObsoleteBonusString`

- Obsoletes %s2_TechName  <!-- TXT_KEY_TECHHELP_OBSOLETES -->

## `buildObsoleteSpecialString`

- Obsoletes %s1_Name  <!-- TXT_KEY_TECHHELP_OBSOLETES_NO_LINK -->

## `buildObsoleteString`

- Obsoletes %s2_TechName  <!-- TXT_KEY_TECHHELP_OBSOLETES -->

## `buildOpenBordersString`

- *Enables Open Borders  <!-- TXT_KEY_MISC_ENABLES_OPEN_BORDERS -->

## `buildPermanentAllianceString`

- *Enables Permanent Alliances  <!-- TXT_KEY_MISC_ENABLES_PERM_ALLIANCES -->

## `buildProcessInfoString`

- <name of the thing>
- → `setListHelp`
- *Can Build  <!-- TXT_KEY_MISC_CAN_BUILD -->

## `buildPromotionString`

- <name of the thing>
- → `setListHelp`
- *Enables  <!-- TXT_KEY_MISC_ENABLES -->

## `buildRiverTradeString`

- *Enables <icon> on %s2_TerrainName  <!-- TXT_KEY_MISC_ENABLES_ON_TERRAIN -->
- Rivers  <!-- TXT_KEY_MISC_RIVERS -->

## `buildSingleLineTechTreeString`

- <name of the thing>
- → `setListHelp`
- *Leads to  <!-- TXT_KEY_MISC_LEADS_TO -->

## `buildSpecialBuildingString`

- *Can Construct %s1_BldgName  <!-- TXT_KEY_MISC_CAN_CONSTRUCT_BUILDING -->
- *Any player can Construct %s2_BldgName  <!-- TXT_KEY_MISC_CAN_CONSTRUCT_BUILDING_ANYONE -->

## `buildSpecialistHappinessString`

- *%s1_Specialist gain %D2_Change%F3_HappOrUnhapp.  <!-- TXT_KEY_TECHHELP_SPECIALIST_TECH_HAPPINESS_TYPE -->

## `buildSpecialistHealthString`

- *%s1_Specialist gain %D2_Change%F3_HealthOrUnhealth.  <!-- TXT_KEY_TECHHELP_SPECIALIST_TECH_HEALTH_TYPE -->

## `buildTechTradeString`

- *Enables Technology Trading  <!-- TXT_KEY_MISC_ENABLES_TECH_TRADING -->

## `buildTechTreeString`

- <name of the thing>
- → `setListHelp`
- or  <!-- TXT_KEY_OR -->
- with  <!-- TXT_KEY_WITH -->
- and  <!-- TXT_KEY_AND_SPACE -->
- *%s1_TechName alternatively derived from %s2_AltTech  <!-- TXT_KEY_MISC_ALTERNATIVELY_DERIVED -->

## `buildTerrainTradeString`

- *Enables <icon> on %s2_TerrainName  <!-- TXT_KEY_MISC_ENABLES_ON_TERRAIN -->

## `buildTradeRouteString`

- *%D1_Change Trade [NUM1:Routes:Routes] per City  <!-- TXT_KEY_MISC_TRADE_ROUTES -->

## `buildTradeString`

- Domestic Trade  <!-- TXT_KEY_BUG_DOMESTIC_TRADE_HEADING -->
- Trade with %s1_PlyrName of %s2_CivName  <!-- TXT_KEY_BUG_FOREIGN_TRADE_HEADING -->
- No Trade with %s1_PlyrName of %s2_CivName  <!-- TXT_KEY_BUG_CANNOT_TRADE_HEADING -->
- Trade  <!-- TXT_KEY_BUG_TRADE_HEADING -->
- *They are dead  <!-- TXT_KEY_BUG_CANNOT_TRADE_DEAD -->
- *Your Trade Networks are not connected  <!-- TXT_KEY_BUG_CANNOT_TRADE_NETWORK_NOT_CONNECTED -->
- *You have not signed an Open Borders agreement  <!-- TXT_KEY_BUG_CANNOT_TRADE_CLOSED_BORDERS -->
- *Your Civics don't allow Foreign Trade  <!-- TXT_KEY_BUG_CANNOT_TRADE_FOREIGN_YOU -->
- *Their Civics don't allow Foreign Trade  <!-- TXT_KEY_BUG_CANNOT_TRADE_FOREIGN_THEM -->
- Total Yield: %s1<commerce>  <!-- TXT_KEY_BUG_TOTAL_TRADE_YIELD -->
- Number of Routes: %d1  <!-- TXT_KEY_BUG_TOTAL_TRADE_ROUTES -->
- Average Yield: %s1<commerce>  <!-- TXT_KEY_BUG_AVERAGE_TRADE_YIELD -->

## `buildVassalStateString`

- *Enables Vassal States  <!-- TXT_KEY_MISC_ENABLES_VASSAL_STATES -->

## `buildWaterWorkString`

- *Can Work Water Tiles  <!-- TXT_KEY_MISC_WATER_WORK -->

## `buildWorkerRateString`

- *Workers Build Improvements %D1_Change%% Faster  <!-- TXT_KEY_UNITHELP_WORKERS_FASTER -->

## `buildYieldChangeString`

- <name of the thing>
- → `setYieldChangeHelp`

## `eventGoldHelp`

- *Receive %d1<gold> from %s2_playerName  <!-- TXT_KEY_EVENT_GOLD_FROM_PLAYER -->
- *Give %d1<gold> to %s2_playerName  <!-- TXT_KEY_EVENT_GOLD_TO_PLAYER -->
- *Receive %d1<gold>  <!-- TXT_KEY_EVENT_GOLD_GAINED -->
- *Subtract %d1<gold> from your treasury  <!-- TXT_KEY_EVENT_GOLD_LOST -->
- *Receive between %d1<gold> and %d2<gold> from %s3_playerName  <!-- TXT_KEY_EVENT_GOLD_RANGE_FROM_PLAYER -->
- *Give between %d1<gold> and %d2<gold> to %s2_playerName  <!-- TXT_KEY_EVENT_GOLD_RANGE_TO_PLAYER -->
- *Receive between %d1<gold> and %d2<gold>  <!-- TXT_KEY_EVENT_GOLD_RANGE_GAINED -->
- *Subtract between %d1<gold> and %d2<gold> from your treasury  <!-- TXT_KEY_EVENT_GOLD_RANGE_LOST -->

## `eventTechHelp`

- *Gain knowledge of %s1 from %s2_player  <!-- TXT_KEY_EVENT_TECH_GAINED_FROM_PLAYER -->
- *Gain knowledge of %s1  <!-- TXT_KEY_EVENT_TECH_GAINED -->
- *%D1<beaker> towards %s2 from %s3_player  <!-- TXT_KEY_EVENT_TECH_GAINED_FROM_PLAYER_PERCENT -->
- *%D1_num<beaker> towards %s2  <!-- TXT_KEY_EVENT_TECH_GAINED_PERCENT -->

## `getActiveTeamRelationsString`

- *You are at war!  <!-- TXT_KEY_AT_WAR_WITH_YOU -->
- *You have a Peace Treaty  <!-- TXT_KEY_PEACE_TREATY_WITH_YOU -->
- *You are Worst Enemy  <!-- TXT_KEY_WORST_ENEMY_IS_YOU -->
- *You have a Defensive Pact  <!-- TXT_KEY_DEFENSIVE_PACT_WITH_YOU -->
- *You are preparing for War  <!-- TXT_KEY_WARPLAN_TARGET_OF_YOU -->
- *You are War Target  <!-- TXT_KEY_WARPLAN_TARGET_IS_YOU -->
- *You are Limited War Target  <!-- TXT_KEY_WARPLAN_LIMITED_TARGET_IS_YOU -->

## `getAttitudeString`

- %s1_attitude towards %s2_leader  <!-- TXT_KEY_ATTITUDE_TOWARDS -->
- Vassal of %s1_CivDesc  <!-- TXT_KEY_ATTITUDE_VASSAL_OF -->
- → `setVassalRevoltHelp`
- Master of %s1_player  <!-- TXT_KEY_ATTITUDE_MASTER_OF -->
- %D1: "I like the cut of your jib."  <!-- TXT_KEY_MISC_ATTITUDE_TRAIT_GOOD -->
- %D1: "Your personality leaves something to be desired."  <!-- TXT_KEY_MISC_ATTITUDE_TRAIT_BAD -->
- %D1: "Our close borders spark tensions."  <!-- TXT_KEY_MISC_ATTITUDE_LAND_TARGET -->
- %D1: "This war spoils our relationship."  <!-- TXT_KEY_MISC_ATTITUDE_WAR -->
- %D1: "Years of peace have strengthened our relations."  <!-- TXT_KEY_MISC_ATTITUDE_PEACE -->
- %D1: "We care for our brothers and sisters of the faith."  <!-- TXT_KEY_MISC_ATTITUDE_SAME_RELIGION -->
- %D1: "We are upset that you have fallen under the sway of a heathen religion."  <!-- TXT_KEY_MISC_ATTITUDE_DIFFERENT_RELIGION -->
- %D1: "We appreciate the years you have supplied us with resources."  <!-- TXT_KEY_MISC_ATTITUDE_BONUS_TRADE -->
- %D1: "Our Open Borders have brought our people close together."  <!-- TXT_KEY_MISC_ATTITUDE_OPEN_BORDERS -->
- %D1: "Our Defensive Pact proves that we are close friends."  <!-- TXT_KEY_MISC_ATTITUDE_DEFENSIVE_PACT -->
- %D1: "We are upset that you have signed Defensive Pacts with our rivals."  <!-- TXT_KEY_MISC_ATTITUDE_RIVAL_DEFENSIVE_PACT -->
- %D1: "We are worried about our rivals being vassals to your empire!"  <!-- TXT_KEY_MISC_ATTITUDE_RIVAL_VASSAL -->
- %D1: "Our mutual military struggle brings us closer together."  <!-- TXT_KEY_MISC_ATTITUDE_SHARE_WAR -->
- %D1: "You have wisely chosen your Civics."  <!-- TXT_KEY_MISC_ATTITUDE_FAVORITE_CIVIC -->
- %D1: "Our trade relations have been fair and forthright."  <!-- TXT_KEY_MISC_ATTITUDE_TRADE -->
- %D1: "You have traded with our worst enemies!"  <!-- TXT_KEY_MISC_ATTITUDE_RIVAL_TRADE -->
- %D1: "You have granted us our independence!"  <!-- TXT_KEY_MISC_ATTITUDE_FREEDOM -->
- %D1: "Past events have brought our people together."  <!-- TXT_KEY_MISC_ATTITUDE_EXTRA_GOOD -->
- %D1: "Past events have drawn our people apart."  <!-- TXT_KEY_MISC_ATTITUDE_EXTRA_BAD -->
- %D1: "We share the same philosophies"  <!-- TXT_KEY_MISC_ATTITUDE_CIVIC_SHARE_GOOD -->
- %D1: "Our philosophies clash"  <!-- TXT_KEY_MISC_ATTITUDE_CIVIC_SHARE_BAD -->
- %D1: "You let our ambassadors in"  <!-- TXT_KEY_EMBASSY_DIPLOMACY_BONUS -->
- %D1: "You kicked our ambassadors out!"  <!-- TXT_KEY_EMBASSY_DIPLOMACY_MALUS -->
- %D1: "Your small civilization is no threat to us."  <!-- TXT_KEY_MISC_ATTITUDE_BETTER_RANK -->
- %D1: "We feel threatened by your large civilization."  <!-- TXT_KEY_MISC_ATTITUDE_WORSE_RANK -->
- %D1: "Developing nations should work together to catch up."  <!-- TXT_KEY_MISC_ATTITUDE_LOW_RANK -->
- %D1: "The war is going badly for us."  <!-- TXT_KEY_MISC_ATTITUDE_LOST_WAR -->
- %D1: "Your team is too big."  <!-- TXT_KEY_MISC_ATTITUDE_TEAM_SIZE -->
- %D1: "A first impression is a lasting one."  <!-- TXT_KEY_MISC_ATTITUDE_FIRST_IMPRESSION -->
- %D1: %s2  <!-- TXT_KEY_MISC_ATTITUDE_MEMORY -->
- <name of the thing>
- War Weariness: %d1  <!-- TXT_KEY_WAR_WEAR_HELP -->

## `getDealString`

- → `setListHelp`
- %s1 to %s2 for %s3  <!-- TXT_KEY_MISC_OUR_DEAL -->
- %s1 gives %s2 to %s3 for %s4  <!-- TXT_KEY_MISC_DEAL -->
- %s1 to %s2  <!-- TXT_KEY_MISC_DEAL_ONESIDED_OURS -->
- %s1 from %s2  <!-- TXT_KEY_MISC_DEAL_ONESIDED_THEIRS -->
- %s1 gives %s2 to %s3  <!-- TXT_KEY_MISC_DEAL_ONESIDED -->

## `getDefenseHelp`

- %d1%<defense> from Buildings  <!-- TXT_KEY_MISC_BUILDING_DEFENSE_HOVER -->
- %d1%<defense> from Wonders  <!-- TXT_KEY_MISC_WONDER_DEFENSE_HOVER -->
- %d1%<defense> from Resources  <!-- TXT_KEY_MISC_RESOURCE_DEFENSE_HOVER -->
- %d1%<defense> from Civics  <!-- TXT_KEY_MISC_CIVIC_DEFENSE_HOVER -->
- %d1%<defense> from Traits  <!-- TXT_KEY_MISC_TRAIT_DEFENSE_HOVER -->
- %d1%<defense> from Culture  <!-- TXT_KEY_MISC_CULTURE_DEFENSE_HOVER -->
- Base Defense: %d1%<defense>  <!-- TXT_KEY_MISC_TOTAL_DEFENSE_HOVER -->
- %d1%<defense> from Terrain  <!-- TXT_KEY_MISC_TERRAIN_DEFENSE_HOVER -->
- Percent Siege Damaged: %d1%  <!-- TXT_KEY_MISC_DEFENSE_DAMAGE_PERCENT_HOVER -->
- Siege Damage: %d1%<defense>  <!-- TXT_KEY_MISC_DEFENSE_DAMAGE_HOVER -->
- Current Defense: %d1%<defense>  <!-- TXT_KEY_MISC_CURRENT_DEFENSE_HOVER -->
- Minimum Defense: %d1%%  <!-- TXT_KEY_MISC_MIN_DEFENSE_HOVER -->
- Enemy units may not attack city until Defense is reduced below %d1%%  <!-- TXT_KEY_MISC_MINIMUM_DEFENSE_LEVEL_HOVER -->
- City Defense recovers at %D1%% its usual rate when reduced under %d2%%  <!-- TXT_KEY_MISC_BUILDING_RECOVERY_HOVER -->
- City Defense recovers at %D1%% its usual rate.  <!-- TXT_KEY_MISC_CITY_RECOVERY_HOVER -->
- Bombard Resistance: %d1%<defense>  <!-- TXT_KEY_MISC_BUILDING_BOMBARD_DEFENSE_HOVER -->
- Combat Modifier for Defenders: %D1%% VS %s2_UnitCombat  <!-- TXT_KEY_MISC_DEFENSE_AGAINST_UNIT_COMBAT_HOVER -->
- Combat Modifier for %s1_UnitCombat Defenders: %D2%%  <!-- TXT_KEY_MISC_DEFENSE_UNIT_COMBAT_HOVER -->
- Dynamic Defense: %d1%%  <!-- TXT_KEY_MISC_BUILDING_DYNAMIC_DEFENSE_HOVER -->
- Attackers crossing Rivers gain a %D1%% Modifier  <!-- TXT_KEY_MISC_BUILDING_RIVER_DEFENSE_HOVER -->
- Espionage Defense: %d1%  <!-- TXT_KEY_MISC_BUILDING_ESPIONAGE_DEFENSE_HOVER -->
- Insidiousness to local Criminals: %s1%%  <!-- TXT_KEY_MISC_BUILDING_INSIDIOUSNESS_HOVER -->
- Current Investigation Total: %s1%%  <!-- TXT_KEY_MISC_BUILDING_INVESTIGATION_HOVER -->
- Amount of Lurking Criminals: %d1  <!-- TXT_KEY_MISC_CRIMINAL_COUNT_DEFENSE_HOVER -->
- Repel Value for Defenders: %D1%%  <!-- TXT_KEY_MISC_LOCAL_REPEL_HOVER -->
- Repel Value for Defenders: %D1%% VS %s2_UnitCombat  <!-- TXT_KEY_MISC_REPEL_AGAINST_UNIT_COMBAT_HOVER -->
- Repel Value for %s1_UnitCombat Defenders: %D2%%  <!-- TXT_KEY_MISC_REPEL_UNIT_COMBAT_HOVER -->
- City has an established Zone of Control. (Enemy units may not move directly from one adjacent space to another adjacent space.)  <!-- TXT_KEY_MISC_ZOC_HOVER -->
- Adjacent Damage to Attacking Units: %D1%%  <!-- TXT_KEY_MISC_ADJ_DMG_HOVER -->
- *May damage any unit as it attacks the city. %d1_Chance%% Chance to deal %d2_Damage%% Damage. (Chance modified by attacker's Dodge. Damage NOT modified by attacker's Armor.)  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ALL_ATTACKER_ARMOR_EXEMPT -->
- *May damage any unit as it attacks the city. %d1_Chance%% Chance to deal %d2_Damage%% Damage. (Chance modified by attacker's Dodge. Damage modified by attacker's Armor.)  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ALL_ATTACKER -->
- *May damage %s1_UnitCombat  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ATTACKER_START -->
- , %s1_UnitCombat  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ATTACKER_MIDDLE -->
- type units as they attack the city. %d1_Chance%% Chance to deal %d2_Damage%% Damage. (Chance modified by attacker's Dodge. Damage NOT modified by attacker's Armor.)  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ATTACKER_END_ARMOR_EXEMPT -->
- type units as they attack the city. %d1_Chance%% Chance to deal %d2_Damage%% Damage. (Chance modified by attacker's Dodge. Damage modified by attacker's Armor.)  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ATTACKER_END -->
- → `setBuildingAdditionalDefenseHelp`

## `getEspionageString`

- <spy> Ratio with %s1_player: %d2/%d3  <!-- TXT_KEY_ESPIONAGE_AGAINST_PLAYER -->

## `getGlobeLayerName`

- View  <!-- TXT_KEY_GLOBELAYER_STRATEGY_VIEW -->
- New line  <!-- TXT_KEY_GLOBELAYER_STRATEGY_NEW_LINE -->
- New sign  <!-- TXT_KEY_GLOBELAYER_STRATEGY_NEW_SIGN -->
- Delete  <!-- TXT_KEY_GLOBELAYER_STRATEGY_DELETE -->
- Delete All Lines  <!-- TXT_KEY_GLOBELAYER_STRATEGY_DELETE_LINES -->
- All Military  <!-- TXT_KEY_GLOBELAYER_UNITS_ALLMILITARY -->
- Team Military  <!-- TXT_KEY_GLOBELAYER_UNITS_TEAMMILITARY -->
- Enemies in Territory  <!-- TXT_KEY_GLOBELAYER_UNITS_ENEMY_TERRITORY_MILITARY -->
- Enemy Military  <!-- TXT_KEY_GLOBELAYER_UNITS_ENEMYMILITARY -->
- Domestics  <!-- TXT_KEY_GLOBELAYER_UNITS_DOMESTICS -->
- All Resources  <!-- TXT_KEY_ALL_RESOURCES -->
- Strategic  <!-- TXT_KEY_STRATEGIC -->
- Luxury  <!-- TXT_KEY_LUXURY -->
- Production  <!-- TXT_KEY_PRODUCTION -->
- Growth  <!-- TXT_KEY_GROWTH -->
- Misc  <!-- TXT_KEY_MISC -->
- Unclaimed  <!-- TXT_KEY_UNCLAIMED -->
- Can claim  <!-- TXT_KEY_CANCLAIM -->
- <name of the thing>

## `getInterfaceCenterText`

- %s1_PlyrName Wins a %s2_VctryName Victory!!!!  <!-- TXT_KEY_MISC_WINS_VICTORY -->
- You have been defeated!!!  <!-- TXT_KEY_MISC_DEFEAT -->

## `getOtherRelationsString`

- → `setListHelp`
- *War with %s1_player  <!-- TXT_KEY_AT_WAR_WITH -->
- *%s1_player is Worst Enemy  <!-- TXT_KEY_WORST_ENEMY_IS -->
- *Worst Enemy of %s1_player  <!-- TXT_KEY_WORST_ENEMY_OF -->
- *Peace Treaty with %s1_player  <!-- TXT_KEY_PEACE_TREATY_WITH -->
- *Defensive Pact with %s1_player  <!-- TXT_KEY_DEFENSIVE_PACT_WITH -->
- *War Plan against %s1_player  <!-- TXT_KEY_WARPLAN_TARGET_IS -->
- *Limited War Plan against %s1_player  <!-- TXT_KEY_WARPLAN_LIMITED_TARGET_IS -->

## `getPlotHelp`

- → `setCityBarHelp`
- → `setPlotListHelp`
- → `setCombatPlotHelp`
- → `setPlotHelp`
- Your units cannot enter %s1_cityName because it has the protection of its %s2_buildingName. Either destroy the %s3_buildingName, or reduce the city defenses to %d4% to attack the city.  <!-- TXT_KEY_PLOT_HOVER_MAX_CITY_DEFENSES -->
- <name of the thing>
- Your units cannot enter this tile because it is inside of a nearby improvement's Zone of Control. Destroy the improvement, or it's defenders to restore normal movement.  <!-- TXT_KEY_PLOT_HOVER_FORT_ZOC -->
- Your units cannot enter this tile because it is inside of a nearby city's Zone of Control. Capture the city to restore normal movement.  <!-- TXT_KEY_PLOT_HOVER_CITY_ZOC -->
- Your units cannot enter this tile because it is inside of a nearby unit's Zone of Control. Destroy the unit to restore normal movement.  <!-- TXT_KEY_PLOT_HOVER_UNIT_ZOC -->
- Cannot Nuke Friendly Territory or Units  <!-- TXT_KEY_CANT_NUKE_FRIENDS -->
- The %s1 is faster than your %s2. Only units with %d3 or more moves can protect a %s4.  <!-- TXT_KEY_CAN_NOT_AUTOMATE_PROTECT_NOT_FAST_ENOUGH -->
- The %s1 is not on your team!  <!-- TXT_KEY_CAN_NOT_AUTOMATE_PROTECT_DIFFERENT_TEAM -->
- You cannot protect yourself!  <!-- TXT_KEY_CAN_NOT_AUTOMATE_PROTECT_YOURSELF -->

## `getRebasePlotHelp`

- Air Unit Capacity: %d1/%d2  <!-- TXT_KEY_CITY_BAR_AIR_UNIT_CAPACITY -->

## `getTradeScreenHeader`

- <name of the thing>

## `getTradeString`

- Gold (%d1_Num)  <!-- TXT_KEY_MISC_GOLD -->
- Gold Per Turn (%d1_Num)  <!-- TXT_KEY_MISC_GOLD_PER_TURN -->
- World Map  <!-- TXT_KEY_MISC_WORLD_MAP -->
- Capitulation  <!-- TXT_KEY_MISC_CAPITULATE -->
- Vassal State  <!-- TXT_KEY_MISC_VASSAL -->
- Open Borders  <!-- TXT_KEY_MISC_OPEN_BORDERS -->
- Defensive Pact  <!-- TXT_KEY_MISC_DEFENSIVE_PACT -->
- Permanent Alliance  <!-- TXT_KEY_MISC_PERMANENT_ALLIANCE -->
- Peace Treaty (%d1_Num Turns)  <!-- TXT_KEY_MISC_PEACE_TREATY -->
- <name of the thing>
- Embassy  <!-- TXT_KEY_MISC_EMBASSY -->
- Right of Passage  <!-- TXT_KEY_MISC_LIMITED_BORDERS -->
- Free Trade Agreement  <!-- TXT_KEY_MISC_FREE_TRADE_ZONE -->

## `getTurnTimerText`

- %d1 [NUM1:turn:turns] of Universal Peace Remaining  <!-- TXT_KEY_MISC_ADVANCED_START_PEACE_REMAINING -->
- %d1 [NUM1:Turn:Turns] to Victory!  <!-- TXT_KEY_MISC_TURNS_LEFT_TO_VICTORY -->
- %d1 [NUM1:Turn:Turns] Left!  <!-- TXT_KEY_MISC_TURNS_LEFT -->

## `parseBuildUp`

- <name of the thing>

## `parseCivInfos`

- UNKNOWN CIV  <!-- TXT_KEY_CIV_UNKNOWN -->
- <name of the thing>
- Starting Technologies  <!-- TXT_KEY_FREE_TECHS -->
- No Starting Technologies  <!-- TXT_KEY_FREE_TECHS_NO -->

## `parseCivicInfo`

- <name of the thing>
- *Requires %s2  <!-- TXT_KEY_REQUIRES_LINK -->
- *Can Build special units without %s1_Building  <!-- TXT_KEY_CIVICHELP_BUILD_MISSIONARIES -->
- * Causes your Civilization to have Fixed Borders  <!-- TXT_KEY_FIXED_BORDERS_CIVIC -->
- *Unlimited  <!-- TXT_KEY_CIVICHELP_UNLIMTED -->
- → `setListHelp`
- *%D1_Mod%%<greatperson> Birth Rate  <!-- TXT_KEY_CIVICHELP_GREAT_PEOPLE_MOD -->
- *%D1_Mod%% Great General Emergence  <!-- TXT_KEY_CIVICHELP_GREAT_GENERAL_MOD -->
- *%D1_Mod%% Great General Emergence inside Cultural Borders  <!-- TXT_KEY_DOMESTIC_GREAT_GENERAL_MODIFIER -->
- *%D1_Mod%%<greatperson> Birth Rate in Cities with %F2_Religion  <!-- TXT_KEY_CIVICHELP_GREAT_PEOPLE_MOD_RELIGION -->
- *%D1_Mod%%<greatperson> Birth Rate in Cities with State %F2_Religion  <!-- TXT_KEY_CIVICHELP_GREAT_PEOPLE_MOD_STATE_RELIGION -->
- *%D1_Mod%% Maintenance Costs from Distance to Palace  <!-- TXT_KEY_CIVICHELP_DISTANCE_MAINT_MOD -->
- *%D1_Mod%% Maintenance Costs from Number of Cities  <!-- TXT_KEY_CIVICHELP_NO_MAINT_NUM_CITIES_MOD -->
- *%D1_Mod%% Maintenance Costs from Corporations  <!-- TXT_KEY_CIVICHELP_NO_MAINT_CORPORATION_MOD -->
- *No Maintenance Costs for All Cities on Home Continent.  <!-- TXT_KEY_CIVICHELP_HOME_AREA_MAINT -->
- *%D1_Mod%% Maintenance Costs for All Cities on Home Continent.  <!-- TXT_KEY_CIVICHELP_HOME_AREA_MAINT_MOD -->
- *No Maintenance Costs for Overseas Cities  <!-- TXT_KEY_OVERSEAS_CITY_MAINT -->
- *%D1_Mod%% Maintenance Costs for Overseas Cities  <!-- TXT_KEY_OVERSEAS_CITY_MAINT_MOD -->
- *%D1_Change%F2_HealthOrUn in All Cities  <!-- TXT_KEY_CIVICHELP_EXTRA_HEALTH -->
- *New Units Receive %D1_Change Experience Points  <!-- TXT_KEY_CIVICHELP_FREE_XP -->
- *Workers Build Improvements %D1_Mod%% Faster  <!-- TXT_KEY_CIVICHELP_WORKER_SPEED -->
- *%D1_Mod%% Growth Speed for Improvements  <!-- TXT_KEY_CIVICHELP_IMPROVEMENT_UPGRADE -->
- *%D1_Mod%% Military Unit Production  <!-- TXT_KEY_CIVICHELP_MILITARY_PRODUCTION -->
- *Free Civilian Unit Upkeep: %d1 <gold>  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_CIVILIAN -->
- *%d1%% Free Civilian Unit Upkeep per Population.  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_CIVILIAN_PER_POP -->
- (%s1 <gold>)  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_PER_POP -->
- *Free Military Unit Upkeep: %d1 <gold>  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_MILITARY -->
- *%d1%% Free Military Unit Upkeep per Population.  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_MILITARY_PER_POP -->
- *%D1_PerUnit%F2_HappyOrSad per Military Unit Stationed in a City  <!-- TXT_KEY_CIVICHELP_UNIT_HAPPINESS -->
- *Military Units Produced with <food>  <!-- TXT_KEY_CIVICHELP_MILITARY_FOOD -->
- Conscription  <!-- TXT_KEY_CIVIC_CONSCRIPTION -->
- *No <unhealth> from City Population  <!-- TXT_KEY_CIVICHELP_NO_POP_UNHEALTHY -->
- *No <unhealth> from Buildings  <!-- TXT_KEY_CIVICHELP_NO_BUILDING_UNHEALTHY -->
- *%D1%% Experience gained from Combat within own Borders  <!-- TXT_KEY_CIVICHELP_EXPERIENCE_IN_BORDERS -->
- *Can upgrade units outside of national borders  <!-- TXT_KEY_CAN_UPGRADE_ANYWHERE -->
- *Inquisitor Units may purge Non-State <religion> from cities where State <religion> is present.  <!-- TXT_KEY_CIVICHELP_ALLOW_INQUISITONS -->
- *Even if otherwise possible, Civic religious freedoms disables Inquisitions.  <!-- TXT_KEY_CIVICHELP_DISALLOW_INQUISITONS -->
- *Enables full function of all religious buildings regardless of State Religion.  <!-- TXT_KEY_ALL_RELIGIONS_ACTIVE -->
- *Disables all Non-State religious buildings.  <!-- TXT_KEY_BANS_NON_STATE_RELIGIONS -->
- *Gives cause for your citizens to rise up and resist as Freedom Fighters when your cities are captured.  <!-- TXT_KEY_FREEDOM_FIGHTER -->
- *Units have a %D1%% chance to Capture.  <!-- TXT_KEY_NATIONAL_CAPTURE_PROBABILITY_MODIFIER -->
- *Units have a %D1%% chance to avoid Capture.  <!-- TXT_KEY_NATIONAL_CAPTURE_RESISTANCE_MODIFIER -->
- *%d1 local instability penalty per turn.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_LOCAL_PENALTY -->
- *%d1 local stability bonus per turn in each city.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_LOCAL_BONUS -->
- *%d1 national instability penalty per turn.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_NATIONAL_PENALTY -->
- *%d1 national stability bonus per turn.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_NATIONAL_BONUS -->
- *%d1 stability bonus per turn for owning State Religion Holy City.  <!-- TXT_KEY_CIVICHELP_REV_GOOD_HOLY_CITY -->
- *%d1 instability penalty per turn if State Religion Holy City is owned by heathens.  <!-- TXT_KEY_CIVICHELP_REV_BAD_HOLY_CITY -->
- *%D1 one-time stability bonus for adopting Civic.  <!-- TXT_KEY_CIVICHELP_REV_SWITCH_TO_BONUS -->
- *%D1 one-time instability penalty for adopting Civic.  <!-- TXT_KEY_CIVICHELP_REV_SWITCH_TO_PENALTY -->
- *%s1% Revolutionary sentiment (patriotism).  <!-- TXT_KEY_CIVICHELP_REV_NATIONALITY_REDUCTION_MOD -->
- *+%s1% Revolutionary sentiment (alienation).  <!-- TXT_KEY_CIVICHELP_REV_NATIONALITY_INCREASE_MOD -->
- *+%s1% increase in instability penalties from Non-State Religions.  <!-- TXT_KEY_CIVICHELP_REV_BAD_RELIGION_MOD -->
- *+%s1% increase in stability bonuses from State Religion.  <!-- TXT_KEY_CIVICHELP_REV_GOOD_RELIGION_MOD -->
- *%d1% to City Distance Instability penalty.  <!-- TXT_KEY_CIVICHELP_CITY_DISTANCE_GOOD_MOD -->
- *%D1% National Stability (religious oppression).  <!-- TXT_KEY_CIVICHELP_REV_RELIGION_OPRESSION -->
- *%D1% National Stability (religious freedom).  <!-- TXT_KEY_CIVICHELP_REV_RELIGION_FREEDOM -->
- *%D1% National Stability (labor).  <!-- TXT_KEY_CIVICHELP_REV_LABOR -->
- *%D1% National Stability (Health and Safety).  <!-- TXT_KEY_CIVICHELP_REV_ENVIRONMENT -->
- *%D1% National Stability (Suffrage).  <!-- TXT_KEY_CIVICHELP_REV_DEMOCRACY -->
- *Increases local rebelliousness  <!-- TXT_KEY_INCREASE_LOCAL_REBELS -->
- *Decreases local rebelliousness  <!-- TXT_KEY_DECREASE_LOCAL_REBELS -->
- *Increases national rebelliousness  <!-- TXT_KEY_INCREASE_NATIONAL_REBELS -->
- *Decreases national rebelliousness  <!-- TXT_KEY_DECREASE_NATIONAL_REBELS -->
- *State <religion> with Holy City decreases rebelliousness  <!-- TXT_KEY_STATE_RELIGION_WITH_HOLY_CITY -->
- *State <religion> without Holy City increases rebelliousness  <!-- TXT_KEY_STATE_RELIGION_WITHOUT_HOLY_CITY -->
- *No War <unhappy>  <!-- TXT_KEY_CIVICHELP_NO_WAR_WEARINESS -->
- *%D1_WarWearMod%% War <unhappy>  <!-- TXT_KEY_CIVICHELP_EXTRA_WAR_WEARINESS -->
- *%D1_Change Free [NUM1:Specialist:Specialists] per City  <!-- TXT_KEY_CIVICHELP_FREE_SPECIALISTS -->
- *%D1_Change Trade Routes (<trade>) per City  <!-- TXT_KEY_CIVICHELP_TRADE_ROUTES -->
- *%D1_Mod%% Foreign Trade Route Yield in All Cities.  <!-- TXT_KEY_CIVICHELP_FOREIGN_TRADE_ROUTE_MOD -->
- *%d1%% Increased Supply Cost for Distant Units.  <!-- TXT_KEY_CIVICHELP_DISTANT_UNIT_SUPPLY_COST_MOD -->
- *%d1%% Decreased Supply Cost for Distant Units.  <!-- TXT_KEY_CIVICHELP_DISTANT_UNIT_SUPPLY_COST_MOD_DECREASE -->
- *%D1%% City Defense in all Cities  <!-- TXT_KEY_CIVICHELP_CITY_DEFENSE_MOD -->
- *%D_Change Freedom Fighters when one of your cities are captured.  <!-- TXT_KEY_FREEDOM_FIGHTER_CHANGE -->
- *No Foreign Trade Routes (<trade>)  <!-- TXT_KEY_CIVICHELP_NO_FOREIGN_TRADE -->
- *Corporations have no effect  <!-- TXT_KEY_CIVICHELP_NO_CORPORATIONS -->
- *Foreign Corporations have no effect  <!-- TXT_KEY_CIVICHELP_NO_FOREIGN_CORPORATIONS -->
- *Can train Units to Spread Corporations.  <!-- TXT_KEY_CIVICHELP_ALLOWS_USE_OF_EXECUTIVES -->
- *Increases <unhappy> equal to %d1%% population.  <!-- TXT_KEY_CIVICHELP_FREEDOM_ANGER -->
- *Decreases <unhappy> equal to %d1%% population.  <!-- TXT_KEY_CIVICHELP_FREEDOM_HAPPINESS -->
- *No State <religion>  <!-- TXT_KEY_CIVICHELP_NO_STATE_RELIGION -->
- *%D1_Change%F2_HappyOrUn in Cities with <religion icon>  <!-- TXT_KEY_CIVICHELP_STATE_RELIGION_HAPPINESS -->
- *%D1_Change%F2_HappyOrUn in Cities with State <religion>  <!-- TXT_KEY_CIVICHELP_RELIGION_HAPPINESS -->
- *%D1_Change%F2_HappyOrUn per <religion> in a City  <!-- TXT_KEY_CIVICHELP_NON_STATE_REL_HAPPINESS_NO_STATE -->
- *%D1_Change%F2_HappyOrUn per Non-State <religion> in a City  <!-- TXT_KEY_CIVICHELP_NON_STATE_REL_HAPPINESS_WITH_STATE -->
- *Cities with <religion icon> Train Units %D2_Mod%% as fast.  <!-- TXT_KEY_CIVICHELP_REL_TRAIN -->
- *Cities with State <religion> Train Units %D1_Mod%% as fast.  <!-- TXT_KEY_CIVICHELP_STATE_REL_TRAIN -->
- *Cities with <religion icon> Construct Buildings %D2_Mod%% as fast.  <!-- TXT_KEY_CIVICHELP_REL_BUILDING -->
- *Cities with State <religion> Construct Buildings %D1_Mod%% as fast.  <!-- TXT_KEY_CIVICHELP_STATE_REL_BUILDING -->
- *%D1_XP Experience Points in Cities with <religion icon>  <!-- TXT_KEY_CIVICHELP_REL_FREE_XP -->
- *%D1_XP Experience Points in Cities with State <religion>  <!-- TXT_KEY_CIVICHELP_STATE_REL_FREE_XP -->
- *No Non-State <religion> Spread  <!-- TXT_KEY_CIVICHELP_NO_NON_STATE_SPREAD -->
- *Religions Spread %d1% Faster.  <!-- TXT_KEY_CIVICHELP_RELIGION_SPREAD_RATE_FAST -->
- *Religions spread at %d1% of normal rate.  <!-- TXT_KEY_CIVICHELP_RELIGION_SPREAD_RATE_SLOW -->
- *No Religions will Spread.  <!-- TXT_KEY_CIVICHELP_RELIGION_NO_SPREAD -->
- *City Require %d1%% More <food> to Grow  <!-- TXT_KEY_BUILDINGHELP_CITY_SLOW_GROWTH_SPEED -->
- *City Require %d1%% Less <food> to Grow  <!-- TXT_KEY_BUILDINGHELP_CITY_FAST_GROWTH_SPEED -->
- *No <unhappy> In the Capital City.  <!-- TXT_KEY_CIVICHELP_NO_CAPITAL_ANGER -->
- *Extra <unhappy> In All Cities Per %d1% Tax Rate.  <!-- TXT_KEY_CIVICHELP_TAXATION_ANGER -->
- *+%d1_Change<happy> In all cities.  <!-- TXT_KEY_CIVICHELP_HAPPINESS -->
- *+%d1_Change<unhappy> In all cities.  <!-- TXT_KEY_CIVICHELP_UNHAPPINESS -->
- *Settling more than %d1 cities will cause %d2 Unhappiness in every city for each one over that limit.  <!-- TXT_KEY_CIVICHELP_SOFT_CITY_LIMIT -->
- *Cannot settle more than %d1 cities.  <!-- TXT_KEY_CIVICHELP_CITY_LIMIT -->
- *+%d1<unhappy> per %d2% foreign <culture>  <!-- TXT_KEY_CIVICHELP_FOREIGNER_ANGER -->
- *No Inflation  <!-- TXT_KEY_NO_INFLATION -->
- *%d1%% Inflation  <!-- TXT_KEY_ADJUSTS_INFLATION -->
- *%D1_Mod%% Hurry Production Cost  <!-- TXT_KEY_BUILDINGHELP_HURRY_MOD -->
- *Hurrying production causes %d1 more inflation  <!-- TXT_KEY_HURRY_INFLATION_MOD_MORE -->
- *Hurrying production causes %d1 less inflation  <!-- TXT_KEY_HURRY_INFLATION_MOD_LESS -->
- *%d1 trade income from nations with the same civic  <!-- TXT_KEY_SHARED_CIVIC_TRADE_MOD -->
- *Landmarks give +%d1 <happy> to nearby cities.  <!-- TXT_KEY_CIVICHELP_LANDMARK_HAPPINESS -->
- *Landmarks give +%d1 <unhappy> to nearby cities.  <!-- TXT_KEY_CIVICHELP_LANDMARK_UNHAPPINESS -->
- *Damaging or Removing Landmarks causes no <unhappy>.  <!-- TXT_KEY_CIVICHELP_NO_LANDMARK_ANGER -->
- *%s1<gold> per city with access to  <!-- TXT_KEY_CIVICHELP_BONUS_MINTED -->
- *Consumes all %s1  <!-- TXT_KEY_CONSUMES_BONUSES_CIVIC -->
- *%D1_Change%% %F2 from  <!-- TXT_KEY_CIVICHELP_BUILDING_COMMERCE_MODIFIER -->
- *%D1_Change %F2 from  <!-- TXT_KEY_CIVICHELP_BUILDING_COMMERCE_CHANGE -->
- *%s1_Change %F2 from  <!-- TXT_KEY_CIVICHELP_SPECIALIST_COMMERCE_CHANGE -->
- *+%s1_Change %F2 from  <!-- TXT_KEY_CIVICHELP_IMPROVEMENT_HEALTHPERCENT -->
- *+%D1_Change% %F2 from  <!-- TXT_KEY_CIVICHELP_IMPROVEMENT_HAPPINESS -->
- from  <!-- TXT_KEY_MISC_FROM -->
- with %s2_Bonus  <!-- TXT_KEY_BUILDINGHELP_WITH_BONUS -->
- → `setYieldChangeHelp`
- from Landmarks  <!-- TXT_KEY_CIVICHELP_FROM_LANDMARK -->
- in All Cities  <!-- TXT_KEY_CIVICHELP_IN_ALL_CITIES -->
- in Capital  <!-- TXT_KEY_CIVICHELP_IN_CAPITAL -->
- from Trade Routes (<trade>)  <!-- TXT_KEY_CIVICHELP_FROM_TRADE_ROUTES -->
- per Specialist  <!-- TXT_KEY_CIVICHELP_PER_SPECIALIST -->
- *%D1_Change%F2_HappyOrUn in %d3_NumCities Largest Cities  <!-- TXT_KEY_CIVICHELP_LARGEST_CITIES_HAPPINESS -->
- *%D1_Change<icon> from  <!-- TXT_KEY_CIVICHELP_IMPROVEMENT_YIELD_CHANGE -->
- *%D1_Change%F2_HappOrUnhapp from  <!-- TXT_KEY_CIVICHELP_BUILDING_HAPPINESS_PREFIX -->
- *%D1_Change%F2_HappOrUnhapp from %s4_Building  <!-- TXT_KEY_CIVICHELP_BUILDING_HAPPINESS -->
- *%D1% chance to create a slave from combat  <!-- TXT_KEY_UNITHELP_ENSLAVEMENT_CHANCE -->
- Trained %D1%% Faster.  <!-- TXT_KEY_CIVICHELP_UNIT_COMBAT_PRODUCTION_MOD -->
- %Faster Training of  <!-- TXT_KEY_UNITHELP_CLASS_PRODUCTION_FAST_MOD -->
- %Slower Training of  <!-- TXT_KEY_UNITHELP_CLASS_PRODUCTION_SLOW_MOD -->
- % Faster Construction of  <!-- TXT_KEY_CIVICHELP_BUILDING_PRODUCTION_MOD -->
- % Slower Construction of  <!-- TXT_KEY_CIVICHELP_BUILDING_PRODUCTION_SLOW -->
- Improves Relations With:  <!-- TXT_KEY_CIVICHELP_BOOSTS_DIPLOMACY -->
- Damages Relations With:  <!-- TXT_KEY_CIVICHELP_HURTS_DIPLOMACY -->
- Damages Relations With Players Using:  <!-- TXT_KEY_CIVICHELP_HURTS_DIPLOMACY_WITH_PLAYERS -->
- Improves Relations With Players Using:  <!-- TXT_KEY_CIVICHELP_BOOSTS_DIPLOMACY_WITH_PLAYERS -->
- *%D1_Change Free %s3_SpclstName  <!-- TXT_KEY_BUILDINGHELP_FREE_SPECIALIST -->
- in all cities  <!-- TXT_KEY_BUILDINGHELP_GLOBAL -->
- *%d1%% Civilian Unit Upkeep cost  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_MOD_CIVILIAN -->
- *%d1%% Military Unit Upkeep cost  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_MOD_MILITARY -->
- *Allows construction of  <!-- TXT_KEY_CIVICHELP_UNLOCKS_BUILDING -->
- *Disables construction of  <!-- TXT_KEY_CIVICHELP_BLOCKS_BUILDING -->
- → `buildDisplayString`

## `parseFreeSpecialistHelp`

- <name of the thing>
- → `setYieldChangeHelp`
- %D1 XP  <!-- TXT_KEY_SPECIALISTHELP_EXPERIENCE_SHORT -->
- *%D1_Change XP to %s2_UnitCombat units  <!-- TXT_KEY_SPECIALISTHELP_UNIT_COMBAT_EXPERIENCE_SHORT -->
- *%D1_ChangeRate<greatperson> Birth Rate  <!-- TXT_KEY_SPECIALISTHELP_BIRTH_RATE -->
- *%D1_Change%F2_HealthOrUnhealth from knowledge of %s3_Tech Tech.  <!-- TXT_KEY_SPECIALISTHELP_TECH_HEALTH_TYPE_KNOWN -->
- *%D1_Change%F2_HealthOrUnhealth if the %s3_Tech Tech is known.  <!-- TXT_KEY_SPECIALISTHELP_TECH_HEALTH_TYPE -->
- *+%s1_Change %F2  <!-- TXT_KEY_SPECIALISTHELP_PERCENT -->
- *%D1_Change%F2_HappOrUnhapp if the %s3_Tech Tech is known.  <!-- TXT_KEY_SPECIALISTHELP_TECH_HAPPINESS_TYPE -->

## `parseGreatGeneralHelp`

- Great Military Person: %d1_Curr/%d2_Need Experience (%s3_GMP)  <!-- TXT_KEY_MISC_GREAT_MILITARY_PERSON -->
- %s1_GMP: %d2_Curr  <!-- TXT_KEY_MISC_GREAT_MILITARY_PERSON_BREAKDOWN -->

## `parseGreatPeopleHelp`

- Great Person: %d1_Curr/%d2_Need <greatperson>  <!-- TXT_KEY_MISC_GREAT_PERSON -->
- Probabilities:  <!-- TXT_KEY_MISC_PROB -->
- <name of the thing>
- Specialists  <!-- TXT_KEY_CONCEPT_SPECIALISTS -->
- Buildings  <!-- TXT_KEY_WB_BUILDINGS -->
- Base Great Person Rate = %D1<greatperson>  <!-- TXT_KEY_MISC_HELP_GREATPEOPLE_BASE_RATE -->
- *%D1%%<greatperson> from Buildings  <!-- TXT_KEY_MISC_HELP_GREATPEOPLE_BUILDINGS -->
- *%D1%%<greatperson> from Civics  <!-- TXT_KEY_MISC_HELP_GREATPEOPLE_CIVICS -->
- *%D1%%<greatperson> for %s2_trait leader  <!-- TXT_KEY_MISC_HELP_GREATPEOPLE_TRAIT -->
- *%D1%%<greatperson> from Golden Age  <!-- TXT_KEY_MISC_HELP_GREATPEOPLE_GOLDEN_AGE -->
- Final Great Person Rate = %D1<greatperson>  <!-- TXT_KEY_MISC_HELP_GREATPEOPLE_FINAL -->
- → `setBuildingAdditionalGreatPeopleHelp`

## `parseHappinessHelp`

- → `setAngerHelp`
- → `setHappyHelp`
- → `setBuildingAdditionalHappinessHelp`

## `parseLeaderTraits`

- <name of the thing>
- Unknown  <!-- TXT_KEY_TRAITHELP_PLAYER_UNKNOWN -->

## `parsePlayerHasFixedBorders`

- Has fixed borders!  <!-- TXT_KEY_PLAYER_HAS_FIXED_BORDERS -->
- Doesn't have fixed borders!  <!-- TXT_KEY_PLAYER_HAS_NOT_FIXED_BORDERS -->

## `parsePlayerTraits`

- <name of the thing>
- Leadership Level: %d1_Level  <!-- TXT_KEY_LEADER_LEVEL -->
- National Culture*%s1<culture>  <!-- TXT_KEY_LEADER_LEVEL_PROGRESS_1 -->
- *%s1<culture> (Level %d2)  <!-- TXT_KEY_LEADER_LEVEL_PROGRESS_2 -->
- *%s1<culture> (Remaining)  <!-- TXT_KEY_LEADER_LEVEL_PROGRESS_3 -->

## `parsePromotionHelpInternal`

- *On the %s1_PromotionLine  <!-- TXT_KEY_PROMOTIONHELP_LINE -->
- *Promotion Line Rank: %d1_PromotionLine  <!-- TXT_KEY_PROMOTIONHELP_LINE_PRIORITY -->
- *Can Attack Multiple Times per Turn  <!-- TXT_KEY_PROMOTIONHELP_BLITZ -->
- *Creates fallout on sabotaged Improvements.  <!-- TXT_KEY_PROMOTIONHELP_RADIATION_SPY -->
- *No Combat Penalty for Attacking from Sea  <!-- TXT_KEY_PROMOTIONHELP_AMPHIB -->
- *No Combat Penalty for Crossing River  <!-- TXT_KEY_PROMOTIONHELP_RIVER_ATTACK -->
- *Can Use Enemy Roads  <!-- TXT_KEY_PROMOTIONHELP_ENEMY_ROADS -->
- *Status Promotion  <!-- TXT_KEY_PROMOTIONHELP_STATUS -->
- *This Unit Cannot Heal Without Assistance  <!-- TXT_KEY_UNITHELP_SELF_HEAL_NONE -->
- *Never Reveals Nationality  <!-- TXT_KEY_PROMOTIONHELP_LOYALTY_SPY -->
- *Can Heal while Moving  <!-- TXT_KEY_PROMOTIONHELP_ALWAYS_HEAL -->
- *Double Movement in Hills  <!-- TXT_KEY_PROMOTIONHELP_HILLS_MOVE -->
- *Can Pass through Peaks.  <!-- TXT_KEY_PROMOTIONHELP_CAN_MOVE_PEAKS -->
- *Can lead units through Peaks.  <!-- TXT_KEY_PROMOTIONHELP_CAN_LEAD_THROUGH_PEAKS -->
- *Adds one cause for the unit to only be able to attack cities.  <!-- TXT_KEY_PROMOTIONHELP_ATTACK_ONLY_CITIES_ADD -->
- *Removes one cause for the unit to only be able to attack cities.  <!-- TXT_KEY_PROMOTIONHELP_ATTACK_ONLY_CITIES_SUBTRACT -->
- *Adds one cause for the unit to ignore Minimum Defense to Attack City requirements.  <!-- TXT_KEY_PROMOTIONHELP_IGNORE_NO_ENTRY_LEVEL_ADD -->
- *Removes one cause for the unit to ignore Minimum Defense to Attack City requirements.  <!-- TXT_KEY_PROMOTIONHELP_IGNORE_NO_ENTRY_LEVEL_SUBTRACT -->
- *Adds one cause for the unit to ignore Zones of Control.  <!-- TXT_KEY_PROMOTIONHELP_IGNORE_ZONE_OF_CONTROL_ADD -->
- *Removes one cause for the unit to ignore Zones of Control.  <!-- TXT_KEY_PROMOTIONHELP_IGNORE_ZONE_OF_CONTROL_SUBTRACT -->
- *Exerts a Zone Of Control on all adjacent tiles  <!-- TXT_KEY_BUILDINGHELP_ZONE_OF_CONTROL -->
- *Adds one cause for the unit to fly when moving.  <!-- TXT_KEY_PROMOTIONHELP_FLIES_TO_MOVE_ADD -->
- *Removes one cause for the unit to fly when moving.  <!-- TXT_KEY_PROMOTIONHELP_FLIES_TO_MOVE_SUBTRACT -->
- *Causes unit to fight until it or all defenders are dead.  <!-- TXT_KEY_PROMOTIONHELP_STAMPEDE -->
- *Eliminates one source of Stampede Ability.  <!-- TXT_KEY_PROMOTIONHELP_REMOVE_STAMPEDE -->
- *Continues attack while at full <strength>  <!-- TXT_KEY_PROMOTIONHELP_ONSLAUGHT -->
- *Unit is Paralyzed until Overcome  <!-- TXT_KEY_PROMOTIONHELP_PARALYZE -->
- *This Disease cannot be contracted from battle.  <!-- TXT_KEY_PROMOTIONHELP_NO_SPREAD_ON_BATTLE -->
- *This Disease cannot be contracted from other Units on the same plot.  <!-- TXT_KEY_PROMOTIONHELP_NO_SPREAD_UNIT_PROXIMITY -->
- *This Disease cannot cause an outbreak in a city from units on the same plot.  <!-- TXT_KEY_PROMOTIONHELP_NO_SPREAD_UNIT_TO_CITY -->
- *This Disease cannot be caught by a unit in a city suffering from an outbreak.  <!-- TXT_KEY_PROMOTIONHELP_NO_SPREAD_CITY_TO_UNIT -->
- *Makes the Damage this Unit deals Cold Damage.  <!-- TXT_KEY_PROMOTIONHELP_MAKES_DAMAGE_COLD -->
- *Removes a cause this unit would have to be dealing Cold Damage.  <!-- TXT_KEY_PROMOTIONHELP_MAKES_DAMAGE_NOT_COLD -->
- *Causes unit to be immune to Cold Damage penalties.  <!-- TXT_KEY_PROMOTIONHELP_ADDS_COLD_IMMUNITY -->
- *Removes a cause this unit would have to be immune to Cold Damage penalties.  <!-- TXT_KEY_PROMOTIONHELP_REMOVES_COLD_IMMUNITY -->
- *Critical Injury Promotion  <!-- TXT_KEY_PROMOTIONHELP_CRITICAL -->
- *+1 Move on Defensive Combat Victories  <!-- TXT_KEY_PROMOTIONHELP_DV_MOVE -->
- *No Movement Cost to Paradrop and Can Attack After Paradrop  <!-- TXT_KEY_PROMOTIONHELP_FREE_DROP -->
- *+1 Move on Offensive Combat Victories  <!-- TXT_KEY_PROMOTIONHELP_OV_MOVE -->
- *This Promotion gives the Unit an Extra Life.  <!-- TXT_KEY_PROMOTIONHELP_ONEUP -->
- *Pillages <spy> in addition to <gold>  <!-- TXT_KEY_PROMOTIONHELP_ESPIONAGE_PILLAGE -->
- *Pillages twice the <gold> and potentially twice the Improvement.  <!-- TXT_KEY_PROMOTIONHELP_MARAUDER_PILLAGE -->
- *Automatically pillages Improvements on move.  <!-- TXT_KEY_PROMOTIONHELP_MOVING_PILLAGE -->
- *Reaps the Profits of Pillaging on a Combat Victory (no loss of Improvement though).  <!-- TXT_KEY_PROMOTIONHELP_VICTORY_PILLAGE -->
- *Pillages <beaker> in addition to <gold>  <!-- TXT_KEY_PROMOTIONHELP_RESEARCH_PILLAGE -->
- *Requires some invisibility to use  <!-- TXT_KEY_PROMOTIONHELP_PREREQ_NORM_INVISIBLE -->
- (key not in current GameText)  <!-- TXT_KEY_PROMOTIONHELP_EQUIPMENT -->
- *Affliction Promotion  <!-- TXT_KEY_PROMOTIONHELP_AFFLICTION -->
- *+%d1_Num Control Points  <!-- TXT_KEY_PROMOTIONHELP_CONTROL_POINTS -->
- *+%d1_Num Command Range  <!-- TXT_KEY_PROMOTIONHELP_COMMAND_RANGE -->
- *Adds a reason for the unit to be considered an Exile.  <!-- TXT_KEY_PROMOTIONHELP_EXCILE_ADD -->
- *Removes a reason for the unit to be considered an Exile.  <!-- TXT_KEY_PROMOTIONHELP_EXCILE_REMOVE -->
- *Adds a reason for the unit to be considered qualified to pass through territories with a Right of Passage or Open Borders agreement.  <!-- TXT_KEY_PROMOTIONHELP_PASSAGE_ADD -->
- *Removes a reason for the unit to be considered qualified to pass through territories with a Right of Passage or Open Borders agreement.  <!-- TXT_KEY_PROMOTIONHELP_PASSAGE_REMOVE -->
- *Adds a reason for the unit to be restricted from entering a city that is not your own without attacking it.  <!-- TXT_KEY_PROMOTIONHELP_NONONOWNED_ADD -->
- *Removes a reason for the unit to be restricted from entering a city that is not your own without attacking it.  <!-- TXT_KEY_PROMOTIONHELP_NONONOWNED_REMOVE -->
- *Adds a reason for the unit to treat Non-Animal Barbarians as friendly units and Barbarian cities as if they are your own.  <!-- TXT_KEY_PROMOTIONHELP_BARBCOEXIST_ADD -->
- *Removes a reason for the unit to treat Non-Animal Barbarians as friendly units and Barbarian cities as if they are your own.  <!-- TXT_KEY_PROMOTIONHELP_BARBCOEXIST_REMOVE -->
- *Adds a reason for the unit to peacefully enter all cities, even those you are at war with.  <!-- TXT_KEY_PROMOTIONHELP_BLENDCITY_ADD -->
- *Removes a reason for the unit to peacefully enter all cities, even those you are at war with.  <!-- TXT_KEY_PROMOTIONHELP_BLENDCITY_REMOVE -->
- *Adds a reason for the unit to be capable of upgrading anywhere.  <!-- TXT_KEY_PROMOTIONHELP_UPGRADEANYWHERE_ADD -->
- *Removes a reason for the unit to be capable of upgrading anywhere.  <!-- TXT_KEY_PROMOTIONHELP_UPGRADEANYWHERE_REMOVE -->
- *%D1_Change Movement Range  <!-- TXT_KEY_PROMOTIONHELP_MOVE -->
- *%D1_Discount Terrain Movement Cost  <!-- TXT_KEY_PROMOTIONHELP_MOVE_DISCOUNT -->
- *%D1_Change Operational Range  <!-- TXT_KEY_PROMOTIONHELP_AIR_RANGE -->
- *%d1_percent%% Enemy Spy Detection Bonus  <!-- TXT_KEY_PROMOTIONHELP_INTERCEPT_SPY -->
- *%d1_percent%% Bonus To Counter Espionage Missions  <!-- TXT_KEY_PROMOTIONHELP_INTERCEPT_SPY_COUNTER -->
- *%D1_Change%% Interception Chance  <!-- TXT_KEY_PROMOTIONHELP_INTERCEPT -->
- *%d1_percent%% Detection Evasion Bonus  <!-- TXT_KEY_PROMOTIONHELP_EVASION_SPY -->
- *%D1_Change%% Evasion Chance  <!-- TXT_KEY_PROMOTIONHELP_EVASION -->
- *%d1_percent%% Bonus Escape Chance  <!-- TXT_KEY_PROMOTIONHELP_ESCAPE_SPY -->
- *%D1_Change%% Withdrawal Chance  <!-- TXT_KEY_PROMOTIONHELP_WITHDRAWAL -->
- *%D1_Change%% modifier to Attack  <!-- TXT_KEY_PROMOTIONHELP_ATTACK_MODIFIER -->
- *%D1_Change%% modifier to Defense  <!-- TXT_KEY_PROMOTIONHELP_DEFENSE_MODIFIER -->
- *%d1_Amount%% VS Each Size Rank Larger  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_MOD_PER_SIZE_MORE -->
- *%d1_Amount%% VS Each Size Rank Smaller  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_MOD_PER_SIZE_LESS -->
- *%d1_Amount%% VS Each Group Rank Larger  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_MOD_PER_VOLUME_MORE -->
- *%d1_Amount%% VS Each Group Rank Smaller  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_MOD_PER_VOLUME_LESS -->
- *Pursuit Chance: %D1_Change%%  <!-- TXT_KEY_PROMOTIONHELP_PURSUIT -->
- *Starts Withdrawal at %D1_Change%% HP  <!-- TXT_KEY_PROMOTIONHELP_EARLY_WITHDRAW -->
- *+%d1_Change%% vs Non-Animal Barbarians  <!-- TXT_KEY_PROMOTIONHELP_VSBARBS -->
- *%D1_Amount%% Religious Combat Modifier  <!-- TXT_KEY_UNITHELP_RELIGIOUS_COMBAT_MODIFIER_SHORT -->
- *%D1_Change%% Armor Value  <!-- TXT_KEY_PROMOTIONHELP_ARMOR -->
- *%D1_Change Puncture Value  <!-- TXT_KEY_PROMOTIONHELP_PUNCTURE -->
- *Damage Modifier: %D1_Change%  <!-- TXT_KEY_PROMOTIONHELP_DAMAGE_MODIFIER -->
- *Base Unit Upkeep change: %d1%%  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_MODIFIER_BASE -->
- *Extra Unit Upkeep: %s1 <gold>  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_EXTRA -->
- *%D1_Change%% Overrun vs fortify bonuses.  <!-- TXT_KEY_PROMOTIONHELP_OVERRUN -->
- *%D1_Change%% Repel vs Attackers.  <!-- TXT_KEY_PROMOTIONHELP_REPEL -->
- *%D1_Change%% Repel value per turn fortified.  <!-- TXT_KEY_PROMOTIONHELP_FORT_REPEL -->
- *%D1_Change Repel attempts  <!-- TXT_KEY_PROMOTIONHELP_REPEL_RETRIES -->
- *%D1_Change%% Unyielding value vs Knockback/Repel.  <!-- TXT_KEY_PROMOTIONHELP_UNYIELDING -->
- *%D1_Change%% chance per round to Knockback defenders.  <!-- TXT_KEY_PROMOTIONHELP_KNOCKBACK -->
- *%D1_Change Knockback attempts  <!-- TXT_KEY_PROMOTIONHELP_KNOCKBACK_RETRIES -->
- *%D1_Change%% Round Stun Probability  <!-- TXT_KEY_PROMOTIONHELP_ROUND_STUN_PROB -->
- *%D1_Change%% Poison Probability Modifier  <!-- TXT_KEY_PROMOTIONHELP_POISON_PROB -->
- *%D1_Change%% Reflexes (additional withdrawal likelihood per Attack in a given turn.)  <!-- TXT_KEY_PROMOTIONHELP_REFLEXES -->
- *%D1_Change%% Frays (less withdrawal likelihood per Attack in a given turn.)  <!-- TXT_KEY_PROMOTIONHELP_FRAYS -->
- *%D1_Change%% Unnerve Value  <!-- TXT_KEY_PROMOTIONHELP_UNNERVE -->
- *%D1_Change%% Enclose Value  <!-- TXT_KEY_PROMOTIONHELP_ENCLOSE -->
- *%D1_Change%% Lunge Value  <!-- TXT_KEY_PROMOTIONHELP_LUNGE -->
- *%D1_Change%% Dynamic Defense Value  <!-- TXT_KEY_PROMOTIONHELP_DYNAMIC_DEFENSE -->
- *Strengthen: %D1_Change Strength  <!-- TXT_KEY_PROMOTIONHELP_STRENGTHEN -->
- *Weaken: %D1_Change Strength  <!-- TXT_KEY_PROMOTIONHELP_WEAKEN -->
- *Base chance to overcome: %D1_Change%%  <!-- TXT_KEY_PROMOTIONHELP_OVERCOME_PROBABILITY -->
- *Wears off: Adds cumulative %D1_Change%% to Overcome each turn.  <!-- TXT_KEY_PROMOTIONHELP_WEAROFF -->
- *Intensifies: Chance to Overcome is reduced by cumulative %D1_Change%% each turn.  <!-- TXT_KEY_PROMOTIONHELP_INTENSIFIES -->
- *%D1_Change%% Fortitude  <!-- TXT_KEY_PROMOTIONHELP_FORTITUDE_CHANGE -->
- *Deals %D1_Change Damage to unit each turn  <!-- TXT_KEY_PROMOTIONHELP_DAMAGE_PER_TURN -->
- *Drops Unit's Base Strength by %D1_Change each turn - recovered when Overcome.  <!-- TXT_KEY_PROMOTIONHELP_STR_ADJ_PER_TURN_NEGATIVE -->
- *Enhances Unit's Base Strength by %D1_Change each turn - rescinded when Overcome.  <!-- TXT_KEY_PROMOTIONHELP_STR_ADJ_PER_TURN_POSITIVE -->
- *Drops Unit's Combat % by %D1_Change each turn - recovered when Overcome.  <!-- TXT_KEY_PROMOTIONHELP_WEAKEN_PER_TURN_POSITIVE -->
- *Enhances Unit's Combat % by %D1_Change each turn - rescinded when Overcome.  <!-- TXT_KEY_PROMOTIONHELP_WEAKEN_PER_TURN_NEGATIVE -->
- *Affliction can spread to other units: %D1_Change%% base chance  <!-- TXT_KEY_PROMOTIONHELP_COMMUNICABILITY -->
- *Chance of Worsening Modifier: %D1_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_PROBABILITY_OF_WORSENING_MODIFIER -->
- *Each time a unit overcomes this Affliction it gains %D1_Change%% tolerance against it.  <!-- TXT_KEY_PROMOTIONHELP_TOLERANCE_BUILDUP -->
- *Each time a unit overcomes this Affliction it loses %D1_Change%% tolerance against it.  <!-- TXT_KEY_PROMOTIONHELP_TOLERANCE_EROSION -->
- *Each round a Unit or City experiences without this Affliction, it loses %d1_Change tolerance against it until all Tolerance has Decayed away entirely.  <!-- TXT_KEY_PROMOTIONHELP_TOLERANCE_DECAY -->
- *Each round a Unit or City experiences without this Affliction, it GAINS %d1_Change tolerance against it.  <!-- TXT_KEY_PROMOTIONHELP_TOLERANCE_DECAY_ADD -->
- *+%d1_Amount%% to Front Support Value  <!-- TXT_KEY_PROMOTIONHELP_FRONT_SUPPORT_PERCENT_CHANGE -->
- *+%d1_Amount%% to Short Range Support Value  <!-- TXT_KEY_PROMOTIONHELP_SHORT_RANGE_SUPPORT_PERCENT_CHANGE -->
- *+%d1_Amount%% to Medium Range Support Value  <!-- TXT_KEY_PROMOTIONHELP_MEDIUM_RANGE_SUPPORT_PERCENT_CHANGE -->
- *+%d1_Amount%% to Long Range Support Value  <!-- TXT_KEY_PROMOTIONHELP_LONG_RANGE_SUPPORT_PERCENT_CHANGE -->
- *+%d1_Amount%% to Flank Support Value  <!-- TXT_KEY_PROMOTIONHELP_FLANK_SUPPORT_PERCENT_CHANGE -->
- *Additional %D1_Change%% Dodge  <!-- TXT_KEY_PROMOTIONHELP_DODGE_MODIFIER -->
- *Additional %D1_Change%% Precision  <!-- TXT_KEY_PROMOTIONHELP_PRECISION_MODIFIER -->
- *Additional %D1_Change Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOTS -->
- *Additional %D1_Change%% Combat during Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOT_COMBAT_MODIFIER -->
- *Additional %D1_Change Puncture during Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOT_PUNCTURE_MODIFIER -->
- *Additional %D1_Change Precision during Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOT_PRECISION_MODIFIER -->
- *Additional %D1_Change%% Critical Hit Chance per round of battle during Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOT_CRITICAL_MODIFIER -->
- *Additional %D1_Change%% Critical Hit Chance per round of battle  <!-- TXT_KEY_PROMOTIONHELP_CRITICAL_MODIFIER -->
- *%D1_Change Endurance  <!-- TXT_KEY_PROMOTIONHELP_ENDURANCE -->
- *%s1_Change%% Insidiousness  <!-- TXT_KEY_PROMOTIONHELP_INSIDIOUSNESS -->
- *%s1_Change%% Investigation  <!-- TXT_KEY_PROMOTIONHELP_INVESTIGATION -->
- *Trait - Assassination: %D1_Change  <!-- TXT_KEY_PROMOTIONHELP_ASSASSIN -->
- *%D1_Change Stealth Strikes  <!-- TXT_KEY_PROMOTIONHELP_STEALTH_STRIKES -->
- *%D1_Change%% Stealth Combat Modifier  <!-- TXT_KEY_PROMOTIONHELP_STEALTH_COMBAT_MODIFIER -->
- *Trait - Stealth Defend: %D1_Change  <!-- TXT_KEY_PROMOTIONHELP_STEALTH_DEFENSE_CHANGE -->
- *Trait - Defense only: %D1_Change  <!-- TXT_KEY_PROMOTIONHELP_DEFENSE_ONLY_CHANGE -->
- *Trait - Never Invisible: %D1_Change  <!-- TXT_KEY_PROMOTIONHELP_NO_INVISIBILITY_CHANGE -->
- *%D1_Change Min Damage from Traps.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_MIN_DAMAGE -->
- *%D1_Change Max Damage from Traps.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_MAX_DAMAGE -->
- *%D1_Change Disarm Complexity.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_COMPLEXITY -->
- *%D1_Change Charges  <!-- TXT_KEY_PROMOTIONHELP_TRAP_NUM_TRIGGERS -->
- *Trait - Trigger Before Combat: %D1_Change  <!-- TXT_KEY_PROMOTIONHELP_TRAP_TRIGGER_BEFORE_ATTACK -->
- *%D1_Change Vision Range  <!-- TXT_KEY_PROMOTIONHELP_VISIBILITY -->
- *%D1%% chance to Capture.  <!-- TXT_KEY_UNITHELP_CAPTURE_PROBABILITY_MODIFIER -->
- *%D1%% chance to avoid Capture.  <!-- TXT_KEY_UNITHELP_CAPTURE_RESISTANCE_MODIFIER -->
- *Breakdown Chance: %D1%%  <!-- TXT_KEY_UNITHELP_BREAKDOWN_CHANCE -->
- *Breakdown Amount: %D1%%  <!-- TXT_KEY_UNITHELP_BREAKDOWN_DAMAGE -->
- *Taunt Chance: %D1%%  <!-- TXT_KEY_UNITHELP_TAUNT -->
- *Max HP: %D1%% (usually 100.)  <!-- TXT_KEY_PROMOTIONHELP_MAX_HP -->
- *Strength Modifier: %D1%% (Subject to adjustment by Diminishing Return)  <!-- TXT_KEY_PROMOTIONHELP_STRENGTH_MODIFIER -->
- *%d1%% Change to Air Combat Limit  <!-- TXT_KEY_PROMOTIONHELP_AIR_LIMIT_CHANGE -->
- *%D1_Happy <happy> to any city the unit is in.  <!-- TXT_KEY_PROMOTIONHELP_CELEBRITY -->
- *%d1%% Change to Collateral Damage Limit  <!-- TXT_KEY_PROMOTIONHELP_COLLATERAL_LIMIT_CHANGE -->
- *+%d1 Increase to Maximum Number of Units affected by Collateral Damage.  <!-- TXT_KEY_PROMOTIONHELP_MAX_UNITS_CHANGE -->
- *%d1%% change to Maximum Damage Limit  <!-- TXT_KEY_PROMOTIONHELP_COMBAT_LIMIT -->
- *+%d1 Paradrop Range  <!-- TXT_KEY_PROMOTIONHELP_EXTRA_DROP_RANGE -->
- *+%d1%% Chance to Survive a Combat Loss  <!-- TXT_KEY_PROMOTIONHELP_SURVIVOR -->
- *Self heal: %d1%%  <!-- TXT_KEY_UNITHELP_SELF_HEAL -->
- *Can heal %D1_Amount unit(s)/turn  <!-- TXT_KEY_PROMOTIONHELP_HEAL_SUPPORT -->
- *+%d1%% Chance to Heal Friendly Units within one tile on Combat Victory  <!-- TXT_KEY_PROMOTIONHELP_VICTORY_ADJACENT -->
- *+%d1%% Chance to Heal on Combat Victory  <!-- TXT_KEY_PROMOTIONHELP_VICTORY_HEAL -->
- *+%d1%% Chance to Heal Friendly Units sharing the tile on Combat Victory  <!-- TXT_KEY_PROMOTIONHELP_VICTORY_STACK -->
- *%D1_Change Cargo Space  <!-- TXT_KEY_PROMOTIONHELP_CARGO -->
- *%D1_DmgChange%% Collateral Damage  <!-- TXT_KEY_PROMOTIONHELP_COLLATERAL_DAMAGE -->
- *%D1_DmgChange%% City Bombard Damage  <!-- TXT_KEY_PROMOTIONHELP_BOMBARD -->
- *Ranged Assault Distance: %D1_Value  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_RANGE -->
- *Ranged Assault Accuracy: %D1_Value%%  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_ACCURACY -->
- *Ranged Assault Damage: %D1_Value%%  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_DAMAGE -->
- *Ranged Assault Damage Limit: %D1_Value%%  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_DAMAGE_LIMIT -->
- *Ranged Assault Max Targets: %D1_Value  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_DAMAGE_MAX_UNITS -->
- *%d1_Num Extra First Strike  <!-- TXT_KEY_PROMOTIONHELP_FIRST_STRIKE -->
- *%D1_Change First Strikes  <!-- TXT_KEY_PROMOTIONHELP_FIRST_STRIKES -->
- *%d1_Num Extra First Strike Chance  <!-- TXT_KEY_PROMOTIONHELP_FIRST_STRIKE_CHANCE -->
- *%D1_Change First Strike Chances  <!-- TXT_KEY_PROMOTIONHELP_FIRST_STRIKES_CHANCE -->
- *%d1_percent%% Bonus Unrest From Missions  <!-- TXT_KEY_PROMOTIONHELP_INSTIGATE_SPY -->
- *Heals Extra %d1_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_EXTRA -->
- Damage/Turn in Enemy Lands  <!-- TXT_KEY_PROMOTIONHELP_ENEMY_LANDS -->
- *%d1_percent%% Bonus City Revolt From Missions  <!-- TXT_KEY_PROMOTIONHELP_INSTIGATE2_SPY -->
- Damage/Turn in Neutral Lands  <!-- TXT_KEY_PROMOTIONHELP_NEUTRAL_LANDS -->
- *%d1_percent%% Bonus Unhealthiness From Missions  <!-- TXT_KEY_PROMOTIONHELP_POISON_SPY -->
- Damage/Turn in Friendly Lands  <!-- TXT_KEY_PROMOTIONHELP_FRIENDLY_LANDS -->
- *Heals Units in Same Tile Extra %d1_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_SAME -->
- Damage/Turn  <!-- TXT_KEY_PROMOTIONHELP_DAMAGE_TURN -->
- *Heals Units in Adjacent Tiles Extra %d1_Heals%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_ADJACENT -->
- *%D1_Bonus%% Strength  <!-- TXT_KEY_PROMOTIONHELP_STRENGTH -->
- *%D1_Bonus%% City Attack  <!-- TXT_KEY_PROMOTIONHELP_CITY_ATTACK -->
- *%D1_Bonus%% City Defense  <!-- TXT_KEY_PROMOTIONHELP_CITY_DEFENSE -->
- *%D1_Change%% Hills Attack  <!-- TXT_KEY_UNITHELP_HILLS_ATTACK -->
- *%D1_Bonus%% Hills Defense  <!-- TXT_KEY_PROMOTIONHELP_HILLS_DEFENSE -->
- *%D1_Change%% Work Speed  <!-- TXT_KEY_PROMOTIONHELP_WORK_RATE -->
- *%D1_Change%% Work Speed on Hills  <!-- TXT_KEY_PROMOTIONHELP_HILLS_WORK -->
- *%D1_Change%% Work Speed on Peaks  <!-- TXT_KEY_PROMOTIONHELP_PEAKS_WORK -->
- *%d1_Change Cultural Revolt Protection  <!-- TXT_KEY_TEMP_REVOLT_PROTECTION -->
- *Suffers %d1_percent%% less Collateral Damage  <!-- TXT_KEY_PROMOTIONHELP_COLLATERAL_PROTECTION -->
- *Yields %D1_percent%% <gold> from Pillaging  <!-- TXT_KEY_PROMOTIONHELP_PILLAGE_CHANGE -->
- *%d1_percent%% Free Preparation Bonus  <!-- TXT_KEY_PROMOTIONHELP_UPGRADE_DISCOUNT_SPY -->
- *Free upgrades  <!-- TXT_KEY_PROMOTIONHELP_UPGRADE_DISCOUNT_FREE -->
- *%d1_percent%% less <gold> to Upgrade  <!-- TXT_KEY_PROMOTIONHELP_UPGRADE_DISCOUNT -->
- *Gains %D1_percent%% Experience from Combat  <!-- TXT_KEY_PROMOTIONHELP_FASTER_EXPERIENCE -->
- *Sacrificed in Combat in Exchange for %D1_percent%% <strength>  <!-- TXT_KEY_PROMOTIONHELP_KAMIKAZE -->
- *Hides the unit's Nationality  <!-- TXT_KEY_PROMOTIONHELP_HIDDEN_NATIONALITY_ADDS -->
- *Reveals the unit's Nationality  <!-- TXT_KEY_PROMOTIONHELP_HIDDEN_NATIONALITY_REMOVES -->
- *Gives this Animal %D1_Reasons Causes to Ignore Border Restrictions.  <!-- TXT_KEY_PROMOTIONHELP_ANIMAL_IGNORES_BORDERS -->
- *Gives %D1_Reasons Cause(s) to be unable to benefit from Defense Combat bonuses.  <!-- TXT_KEY_PROMOTIONHELP_NO_DEFENSIVE_BONUS_CHANGE_POSITIVE -->
- *Removes %D1_Reasons Cause(s) to be unable to benefit from Defense Combat bonuses.  <!-- TXT_KEY_PROMOTIONHELP_NO_DEFENSIVE_BONUS_CHANGE_NEGATIVE -->
- *Immune to First Strikes  <!-- TXT_KEY_PROMOTIONHELP_IMMUNE_FIRST_STRIKES -->
- *Outfits unit to transport %s1_Change Units (And then allows NO OTHER domains!)  <!-- TXT_KEY_PROMOTIONHELP_DOMAIN_CARGO_CHANGE -->
- *Outfits unit to transport %s1_Change Units (And then allows NO OTHER type!)  <!-- TXT_KEY_PROMOTIONHELP_SPECIAL_CARGO_CHANGE -->
- *Does not allow transport of %s1 Units.  <!-- TXT_KEY_PROMOHELP_CHANGE_NOT_SPECIAL_CARGO -->
- *Changes the Unit's Special type to %s1  <!-- TXT_KEY_PROMOHELP_CHANGE_SPECIAL_UNIT -->
- <name of the thing>
- *Gives %D1_chance% chance to immediately inflict %s2_PromotionName to struck enemy.  <!-- TXT_KEY_PROMOTIONHELP_AFFLICT_ON_ATTACK_IMMEDIATE -->
- *Gives %D1_chance% chance to inflict %s2_PromotionName to struck enemy, delayed until AFTER battle.  <!-- TXT_KEY_PROMOTIONHELP_AFFLICT_ON_ATTACK_NOT_IMMEDIATE -->
- *Gives %D1_chance% chance to inflict %s2_PromotionName to struck enemy.  <!-- TXT_KEY_PROMOTIONHELP_AFFLICT_ON_ATTACK -->
- (Applies to Both Distance AND Melee Attacks)  <!-- TXT_KEY_PROMOTIONHELP_AFFLICT_ON_ATTACK_BOTH -->
- (Applies to Melee Attacks)  <!-- TXT_KEY_PROMOTIONHELP_AFFLICT_ON_ATTACK_MELEE -->
- (Applies to Distance Attacks)  <!-- TXT_KEY_PROMOTIONHELP_AFFLICT_ON_ATTACK_DISTANCE -->
- *Empowers the ability to remove %s1_PromotionName from those afflicted.  <!-- TXT_KEY_PROMOTIONHELP_CURE_AFFLICTION -->
- *%D1_Change%% resistance and overcome bonus against the %s2_PromotionName affliction.  <!-- TXT_KEY_PROMOTIONHELP_AFFLICTION_FORTITUDE_CHANGE_MODIFIER -->
- *Adds %s1_SubCombatClass to the list of the unit's combat classes.  <!-- TXT_KEY_PROMOTIONHELP_SUB_COMBAT -->
- *Removes %s1_SubCombatClass from the list of the unit's combat classes.  <!-- TXT_KEY_PROMOTIONHELP_REMOVES_COMBAT -->
- *Adds %s1_promo to valid Traps.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_PROMOTION_SET -->
- *Immune to %s1_promo traps.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_IMMUNITY -->
- *Makes unit attack %s1_promo units first in a stack. (Also enables assassinations against them if the unit has that ability.)  <!-- TXT_KEY_PROMOTIONHELP_TARGET_UNITCOMBAT -->
- *Assists in Healing %s1_UNITCOMBAT Units in Same Tile %D2_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_UNITCOMBAT_SAME -->
- *Assists in Healing %s1_UNITCOMBAT Units in Adjacent Tiles %D2_Heals%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_UNITCOMBAT_ADJACENT -->
- *Adds %s1_Build to the unit's build capabilities.  <!-- TXT_KEY_PROMOTIONHELP_ADDS_BUILD_TYPE -->
- *Negates %F1_Invisible Veil.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_NEGATES_VEIL -->
- *May only have Promotion while on plots with %s1_Terrain terrain (or other specific terrain(s) if also is listed).  <!-- TXT_KEY_PROMOTIONHELP_TERRAIN_PREREQ -->
- *May only have promotion while on plots with %s1_Feature feature (or other specific feature(s) if also is listed).  <!-- TXT_KEY_PROMOTIONHELP_FEATURE_PREREQ -->
- *May only have promotion while on plots with %s1_Feature Improvement (or other specific Improvement(s) or specific buildings in a city if also listed).  <!-- TXT_KEY_PROMOTIONHELP_IMPROVEMENT_PREREQ -->
- *May only have promotion while in a city with a %s1_Building constructed (or plots with other specific Improvement(s) or specific buildings in a city if also listed).  <!-- TXT_KEY_PROMOTIONHELP_BUILDING_PREREQ -->
- *May only have promotion while on plots with a %s1_Bonus resource (or other specific resource(s) if also is listed).  <!-- TXT_KEY_PROMOTIONHELP_BONUS_PREREQ -->
- *Double Movement in %s1_TerrFeatType  <!-- TXT_KEY_PROMOTIONHELP_DOUBLE_MOVE -->
- *%D1_Bonus%% %s2_TerrFeat Attack  <!-- TXT_KEY_PROMOTIONHELP_ATTACK -->
- *%D1_Bonus%% %s2_TerrFeat Defense  <!-- TXT_KEY_PROMOTIONHELP_DEFENSE -->
- *%D1_Change%% Work Speed on %s2_Gameobject  <!-- TXT_KEY_PROMOTIONHELP_WORK -->
- *%D1_Mod%% Withdraw when battle is on %s3_TypeName  <!-- TXT_KEY_PROMOTIONHELP_WITHDRAW_ON -->
- *%D1_Mod%% vs. %s3_Against  <!-- TXT_KEY_PROMOTIONHELP_VERSUS -->
- *This Affliction is %D1_Change%% more likely to infect a %s2_UnitCombat unit.  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_COMMUNICABILITY_CHANGE_POSITIVE -->
- *This Affliction is %D1_Change%% less likely to infect a %s2_UnitCombat unit.  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_COMMUNICABILITY_CHANGE_NEGATIVE -->
- *This Affliction is %D1_Change%% more likely to be overcome by a %s2_UnitCombat unit.  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_OVERCOME_CHANGE_POSITIVE -->
- *This Affliction is %D1_Change%% less likely to be overcome by a %s2_UnitCombat unit.  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_OVERCOME_CHANGE_NEGATIVE -->
- *%D1_Change more Strength, and possibly the Ability to Flank attack VS %s2_UnitCombat units.  <!-- TXT_KEY_PROMOTIONHELP_FLANKING_STRENGTH_BY_UNIT_COMBAT_POSITIVE -->
- *%D1_Change less Strength, and if totals negative, removes the Ability to Flank attack VS %s2_UnitCombat units.  <!-- TXT_KEY_PROMOTIONHELP_FLANKING_STRENGTH_BY_UNIT_COMBAT_NEGATIVE -->
- *%D1_Value to the ability to Disable %s2_trap traps.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_DISABLE -->
- *%D1_Value%% chance to Avoid Triggering %s2_trap traps.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_AVOID -->
- *%D1_Value%% chance to Trigger against %s2_trap units.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_TRIGGER -->
- *%D1_Change Aid (%s2_Type)  <!-- TXT_KEY_PROMOTIONHELP_AID_CHANGE -->
- *Withdraw vs. %s3_AgainstName: %D1_Mod%%  <!-- TXT_KEY_PROMOTIONHELP_WITHDRAW_VERSUS -->
- *Pursuit vs. %s3_AgainstName: %D1_Mod%%  <!-- TXT_KEY_PROMOTIONHELP_PURSUIT_VERSUS -->
- *%D1_Mod%% Repel vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_REPEL_VERSUS -->
- *%D1_Mod%% Knockback vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_KNOCKBACK_VERSUS -->
- *%D1_Mod Puncture vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_PUNCTURE_VERSUS -->
- *%D1_Mod Armor vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_ARMOR_VERSUS -->
- *%D1_Mod Dodge vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_DODGE_VERSUS -->
- *%D1_Mod Precision vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_PRECISION_VERSUS -->
- *%D1_Mod%% Critical Chance vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_CRITICAL_VERSUS -->
- *%D1_Mod%% Round Stun Chance vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_ROUND_STUN_VERSUS -->
- *This Affliction is %D1_Change%% more likely to infect when %s2_Tech is known.  <!-- TXT_KEY_PROMOTIONHELP_TECH_COMMUNICABILITY_CHANGE_POSITIVE -->
- *This Affliction is %D1_Change%% less likely to infect when %s2_Tech is known.  <!-- TXT_KEY_PROMOTIONHELP_TECH_COMMUNICABILITY_CHANGE_NEGATIVE -->
- *This Affliction is %D1_Change%% more likely to be overcome when %s2_Tech is known.  <!-- TXT_KEY_PROMOTIONHELP_TECH_OVERCOME_CHANGE_POSITIVE -->
- *This Affliction is %D1_Change%% less likely to be overcome when %s2_Tech is known.  <!-- TXT_KEY_PROMOTIONHELP_TECH_OVERCOME_CHANGE_NEGATIVE -->
- *%D1_Mod <icon> Spot.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_SPOT_CHANGE -->
- *%D1_Mod <icon> Spot Range.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_SPOT_RANGE_CHANGE -->
- *%D1_Mod <icon> Veil.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_VEIL_INTENSITY_CHANGE -->
- *%D1_Mod <icon> Veil on %s3_Type.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_VEIL_PLOT_CHANGE -->
- *%D1_Mod <icon> Spot on %s3_TypeName.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_SPOT_PLOT_CHANGE -->
- *%D1_Mod <icon> Spot Range on %s3_TypeName.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_SPOT_PLOT_RANGE_CHANGE -->
- *%D1_Mod%% to %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_OUTCOME -->
- → `buildDisplayString`
- *Level Prerequisite: %d1_Change  <!-- TXT_KEY_PROMOTIONHELP_LEVEL_PREREQ -->
- If the unit with this promotion cannot gain Defensive Bonuses, they will not gain inapplicable promotion benefits.  <!-- TXT_KEY_PROMOTIONHELP_DEF_WARN -->

## `parseSpecialistHelpActual`

- <name of the thing>
- → `setYieldChangeHelp`
- *New Military Units Receive %D1_Change Experience Points  <!-- TXT_KEY_SPECIALISTHELP_EXPERIENCE -->
- *%D1_Change Experience to %s2_UnitCombat type units trained in this city.  <!-- TXT_KEY_SPECIALISTHELP_UNIT_COMBAT_EXPERIENCE -->
- *%D1_ChangeRate<greatperson> Birth Rate  <!-- TXT_KEY_SPECIALISTHELP_BIRTH_RATE -->
- *%s1_Change%% Insidiousness for Local Criminals  <!-- TXT_KEY_SPECIALISTHELP_INSIDIOUSNESS -->
- *%s1_Change%% Investigation for Local Law Enforcement  <!-- TXT_KEY_SPECIALISTHELP_INVESTIGATION -->
- *%D1_Change%F2_HealthOrUnhealth from knowledge of %s3_Tech Tech.  <!-- TXT_KEY_SPECIALISTHELP_TECH_HEALTH_TYPE_KNOWN -->
- *%D1_Change%F2_HealthOrUnhealth if the %s3_Tech Tech is known.  <!-- TXT_KEY_SPECIALISTHELP_TECH_HEALTH_TYPE -->
- *+%s1_Change %F2  <!-- TXT_KEY_SPECIALISTHELP_PERCENT -->
- *%D1_Change%F2_HappOrUnhapp if the %s3_Tech Tech is known.  <!-- TXT_KEY_SPECIALISTHELP_TECH_HAPPINESS_TYPE -->
- Actual  <!-- TXT_KEY_ACTUAL_EFFECTS -->
- → `setResumableYieldChangeHelp`
- → `setResumableCommerceTimes100ChangeHelp`
- → `setResumableValueChangeHelp`
- → `buildDisplayString`

## `parseTraits`

- <name of the thing>
- *This is a Negative Trait  <!-- TXT_KEY_TRAITHELP_NEGATIVE -->
- *Civilizations can Earn this Trait - Not available for Leader selections.  <!-- TXT_KEY_TRAITHELP_CIVILIZATION -->
- *Only active when the %s1_Option Game Option is in use.  <!-- TXT_KEY_TRAITHELP_ON_GAME_OPTION -->
- *Deactivated when the %s2_Option Game Option is in use.  <!-- TXT_KEY_TRAITHELP_NOT_ON_GAME_OPTION -->
- *On the %s1_PromotionLine  <!-- TXT_KEY_PROMOTIONHELP_LINE -->
- *Promotion Line Rank: %d1_PromotionLine  <!-- TXT_KEY_PROMOTIONHELP_LINE_PRIORITY -->
- *You currently require %s1_trait  <!-- TXT_KEY_TRAITHELP_PREREQ_BEGIN -->
- and %s1_trait  <!-- TXT_KEY_TRAITHELP_PREREQ_AND_ENTRY -->
- (%s1_Trait may substitute for %s2_Trait)  <!-- TXT_KEY_TRAITHELP_PREREQ_OR_ENTRY -->
- *This trait is only for leaders when not playing the Developing Leader Option.  <!-- TXT_KEY_TRAITHELP_DISABLED_ON -->
- *This trait is only for leaders when playing the Developing Leader Option.  <!-- TXT_KEY_TRAITHELP_DISABLED_OFF -->
- *Leaders with this trait may not select %s1_Trait Trait  <!-- TXT_KEY_TRAITHELP_DISALLOWED -->
- *Requires knowledge of %s1_TechName  <!-- TXT_KEY_TRAITHELP_TECH_PREREQ -->
- *  <!-- TXT_KEY_BULLET -->
- /City  <!-- TXT_KEY_PER_CITY -->
- *%D1_Change%F2_HappyOrUn in %d3_NumCities Largest Cities  <!-- TXT_KEY_CIVICHELP_LARGEST_CITIES_HAPPINESS -->
- *%D1_Change%F2_HappyOrUn in Cities with State <religion>  <!-- TXT_KEY_CIVICHELP_RELIGION_HAPPINESS -->
- *%D1_Change %F2_HappyOrUn per Non-State <religion> in a City.  <!-- TXT_KEY_CIVICHELP_NON_STATE_REL_HAPPINESS_AMBIGUOUS -->
- *%D1_PerUnit%F2_HappyOrSad per Military Unit Stationed in a City  <!-- TXT_KEY_CIVICHELP_UNIT_HAPPINESS -->
- *%D1_Change%F2_HappyOrHealth from access to %s4_Bonus  <!-- TXT_KEY_TRAITHELP_BONUS_HAPPINESS_CHANGE_FIRST -->
- , %s2_Bonus  <!-- TXT_KEY_TRAITHELP_BONUS_HAPPINESS_CHANGE_ADDITIONAL -->
- Resources  <!-- TXT_KEY_TRAITHELP_BONUS_HAPPINESS_CHANGE_END -->
- *%D1_WarWearMod%% War <unhappy>  <!-- TXT_KEY_CIVICHELP_EXTRA_WAR_WEARINESS -->
- *Enemies suffer %D1_percent%% War <unhappy>  <!-- TXT_KEY_BUILDINGHELP_ENEMY_WAR_WEAR -->
- → `setYieldChangeHelp`
- in All Cities  <!-- TXT_KEY_CIVICHELP_IN_ALL_CITIES -->
- *+1%F1_yield on Plots with %d2%F3_yield  <!-- TXT_KEY_TRAITHELP_EXTRA_YIELD_THRESHOLDS -->
- *-1%F1_yield on Plots with %d2%F3_yield  <!-- TXT_KEY_TRAITHELP_LESS_YIELD_THRESHOLDS -->
- *%D1%F2_yeild/City  <!-- TXT_KEY_TRAITHELP_YIELD_CHANGES -->
- *%D1%% %F2_yield from <trade>  <!-- TXT_KEY_TRAITHELP_TRADE_YIELD_MODIFIERS -->
- All Cities Water Tiles  <!-- TXT_KEY_BUILDINGHELP_WATER_PLOTS_ALL_CITIES -->
- During Golden Ages All Cities  <!-- TXT_KEY_GOLDEN_AGE_YIELD -->
- per Specialist  <!-- TXT_KEY_CIVICHELP_PER_SPECIALIST -->
- from %s2_SpclstName in All Cities  <!-- TXT_KEY_BUILDINGHELP_FROM_IN_ALL_CITIES -->
- in Capital  <!-- TXT_KEY_CIVICHELP_IN_CAPITAL -->
- *%D1_Change<icon> from  <!-- TXT_KEY_CIVICHELP_IMPROVEMENT_YIELD_CHANGE -->
- → `setListHelp`
- *%D1%F2_commerce/City  <!-- TXT_KEY_TRAITHELP_COMMERCE_CHANGES -->
- *%D1%% %F2_commerce  <!-- TXT_KEY_TRAITHELP_COMMERCE_MODIFIERS -->
- *Non-State <religion> Still produces base <culture>, <gold>, <beaker> amounts.  <!-- TXT_KEY_TRAITHELP_NONSTATE_RELIGIOUS_COMMERCE -->
- *%D1_Change Free [NUM1:Specialist:Specialists] per City  <!-- TXT_KEY_CIVICHELP_FREE_SPECIALISTS -->
- *Gains a free %s1_Specialist Specialist in all cities with each advance in Era.  <!-- TXT_KEY_CIVICHELP_ERA_ADVANCE_FREE_SPECIALIST -->
- *%D1_Mod%%<greatperson> Birth Rate in Cities with State %F2_Religion  <!-- TXT_KEY_CIVICHELP_GREAT_PEOPLE_MOD_STATE_RELIGION -->
- *%D1%% <greatperson> Birth Rate  <!-- TXT_KEY_TRAITHELP_GREAT_PEOPLE_MODIFIER -->
- *%D1%% Great Military People Emergence  <!-- TXT_KEY_TRAITHELP_GREAT_GENERAL_MODIFIER -->
- *%D1%% Great Military People Emergence inside Cultural Borders.  <!-- TXT_KEY_DOMESTIC_GREAT_GENERAL_MODIFIER_C2C -->
- *Gets a Free Specialist for each National Wonder where built.  <!-- TXT_KEY_TRAITHELP_FREE_SPEC_NW -->
- *Gets a Free Specialist for each World Wonder where built.  <!-- TXT_KEY_TRAITHELP_FREE_SPEC_WW -->
- *Gets a Free Specialist for each Team Project where built.  <!-- TXT_KEY_TRAITHELP_FREE_SPEC_TP -->
- *%D1_Mod%% Hurry Production Cost  <!-- TXT_KEY_BUILDINGHELP_HURRY_MOD -->
- *%D1_percent%% Anger Duration from Sacrificing Population  <!-- TXT_KEY_BUILDINGHELP_HURRY_ANGER_MOD -->
- *Cities with State <religion> Construct Buildings %D1_Mod%% as fast.  <!-- TXT_KEY_CIVICHELP_STATE_REL_BUILDING -->
- *%D1%% Wonder Production  <!-- TXT_KEY_TRAITHELP_WONDER_PRODUCTION_MODIFIER -->
- *%D1%% World Wonder Production  <!-- TXT_KEY_TRAITHELP_WORLD_WONDER_PRODUCTION_MODIFIER -->
- *%D1%% Team Project Production  <!-- TXT_KEY_TRAITHELP_TEAM_WONDER_PRODUCTION_MODIFIER -->
- *%D1%% National Wonder Production  <!-- TXT_KEY_TRAITHELP_NATIONAL_WONDER_PRODUCTION_MODIFIER -->
- *Free Civilian Unit Upkeep: %d1 <gold>  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_CIVILIAN -->
- *%d1%% Free Civilian Unit Upkeep per Population.  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_CIVILIAN_PER_POP -->
- (%s1 <gold>)  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_PER_POP -->
- *Free Military Unit Upkeep: %d1 <gold>  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_MILITARY -->
- *%d1%% Free Military Unit Upkeep per Population.  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_FREE_MILITARY_PER_POP -->
- *Free military unit upkeep per 100 population: %D1  <!-- TXT_KEY_TRAITHELP_FREE_UNIT_UPKEEP_MILITARY_PER_100_POP -->
- *%d1%% Civilian Unit Upkeep cost  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_MOD_CIVILIAN -->
- *%d1%% Military Unit Upkeep cost  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_MOD_MILITARY -->
- *%D1_Mod%% <gold> to Upgrade Units  <!-- TXT_KEY_BUILDINGHELP_UNIT_UPGRADE_COST_MOD -->
- *You can Draft %D1_Num [NUM1:Unit:Units]  <!-- TXT_KEY_CIVICHELP_CONSCRIPTION_CHANGE -->
- *Military Units Produced with <food>  <!-- TXT_KEY_CIVICHELP_MILITARY_FOOD -->
- *Can upgrade units outside of national borders  <!-- TXT_KEY_CAN_UPGRADE_ANYWHERE -->
- *Inquisitor Units may purge Non-State <religion> from cities where State <religion> is present.  <!-- TXT_KEY_CIVICHELP_ALLOW_INQUISITONS -->
- *%D1_Amount Air Unit Capacity in all cities.  <!-- TXT_KEY_TRAITHELP_AIR_UNIT_CAPACITY -->
- *%D1_Amount Flight Operation Range for Air Units.  <!-- TXT_KEY_TRAITHELP_FLIGHT_RANGE -->
- *%D1_Amount Cargo Space for all Missile Carrying Units.  <!-- TXT_KEY_TRAITHELP_MISSILE_CARGO_SPACE -->
- *%D1_Amount Mission Operation Range for Missile Units.  <!-- TXT_KEY_TRAITHELP_MISSILE_RANGE -->
- *%D1_Amount Cargo Space for all transport-capable Naval Units.  <!-- TXT_KEY_TRAITHELP_NAVAL_CARGO_SPACE -->
- *Units have a %D1%% chance to Capture.  <!-- TXT_KEY_NATIONAL_CAPTURE_PROBABILITY_MODIFIER -->
- *Units have a %D1%% chance to avoid Capture.  <!-- TXT_KEY_NATIONAL_CAPTURE_RESISTANCE_MODIFIER -->
- *Automatically Drafts new units in newly conquered cities.  <!-- TXT_KEY_TRAITHELP_DRAFT_ON_CAPTURE -->
- *Gains a second result from Goody Huts and Islands.  <!-- TXT_KEY_TRAITHELP_EXTRA_GOODY -->
- *Free %s2_PromotionName Promotion for  <!-- TXT_KEY_TRAITHELP_FREE_PROMOTION_UNITCOMBAT -->
- *%D1%% XP Needed for Unit Promotions.  <!-- TXT_KEY_TRAITHELP_CIVIC_LEVEL_MODIFIER -->
- *New Units Receive %D1_Change Experience Points  <!-- TXT_KEY_CIVICHELP_FREE_XP -->
- *%D1 Experience to all newly trained %s3_CombatClass  <!-- TXT_KEY_TRAITHELP_UNIT_COMBAT_FREE_XP_FIRST -->
- , %s2_CombatClass  <!-- TXT_KEY_TRAITHELP_UNIT_COMBAT_FREE_XP_ADDITIONAL -->
- Unit Types  <!-- TXT_KEY_TRAITHELP_UNIT_COMBAT_FREE_XP_END -->
- *%D1_XP Experience Points in Cities with State <religion>  <!-- TXT_KEY_CIVICHELP_STATE_REL_FREE_XP -->
- *%D1%% Experience gained from Combat within own Borders  <!-- TXT_KEY_CIVICHELP_EXPERIENCE_IN_BORDERS -->
- *New %s1_UnitName Receive %D2_Change Experience Points  <!-- TXT_KEY_BUILDINGHELP_FREE_XP -->
- *%D1%% Free Experience to all units trained in a Capital or Government Center.  <!-- TXT_KEY_TRAITHELP_CAPITAL_XP_MODIFIER -->
- *%D1%% Free Experience to all units trained in the Holy City of your State Religion.  <!-- TXT_KEY_TRAITHELP_HOLY_CITY_STATE_REL_XP_MODIFIER -->
- *%D1%% Free Experience to all units trained in Holy Cities of Non-State Religions.  <!-- TXT_KEY_TRAITHELP_HOLY_CITY_NON_STATE_REL_XP_MODIFIER -->
- *%D1%% Production of  <!-- TXT_KEY_TRAITHELP_PRODUCTION_MODIFIER -->
- *%D1_Mod%% Military Unit Production  <!-- TXT_KEY_CIVICHELP_MILITARY_PRODUCTION -->
- *%D1%% Production speed when training %s3_CombatClass  <!-- TXT_KEY_TRAITHELP_UNIT_PRODUCTION_FIRST -->
- *Builds %s1_UnitName %D2_Mod%% Faster  <!-- TXT_KEY_BUILDINGHELP_BUILDS_FASTER_DOMAIN -->
- *Cities with State <religion> Train Units %D1_Mod%% as fast.  <!-- TXT_KEY_CIVICHELP_STATE_REL_TRAIN -->
- *No Anarchy  <!-- TXT_KEY_TRAITHELP_NO_ANARCHY -->
- *Max %d1 [NUM1:Turn:Turns] of Anarchy  <!-- TXT_KEY_TRAITHELP_MAX_ANARCHY -->
- *Min %d1 [NUM1:Turn:Turns] of Anarchy  <!-- TXT_KEY_TRAITHELP_MIN_ANARCHY -->
- *%D1_Modifier%% Anarchy time from Civic changes.  <!-- TXT_KEY_CIVICHELP_ANARCHY_CHANGE -->
- *%D1_Modifier%% Anarchy time from State Religion changes  <!-- TXT_KEY_RELIGIOUS_ANARCHY_CHANGE -->
- *%D1_Change Trade Routes (<trade>) per City  <!-- TXT_KEY_CIVICHELP_TRADE_ROUTES -->
- *%D1_Change Trade Routes in All Coastal Cities  <!-- TXT_KEY_BUILDINGHELP_COASTAL_TRADE_ROUTES -->
- *%D1_Change to Maximum possible Trade Routes (<trade>) per City  <!-- TXT_KEY_TRAITHELP_MAX_TRADE_ROUTES -->
- *%D1_Mod%% Foreign Trade Route Yield in All Cities.  <!-- TXT_KEY_CIVICHELP_FOREIGN_TRADE_ROUTE_MOD -->
- *%D1%% City Defense in all Cities  <!-- TXT_KEY_CIVICHELP_CITY_DEFENSE_MOD -->
- *%D1% to Espionage Defense in all cities  <!-- TXT_KEY_MISC_TRAIT_ESPIONAGE_DEFENSE -->
- *%D1_Mod%% Damage to Defenses and Units from Bombardment and Ranged Assault in all cities.  <!-- TXT_KEY_TRAITHELP_BOMBARD_DEFENSE_MOD -->
- *Gives cause for your citizens to rise up and resist as Freedom Fighters when your cities are captured.  <!-- TXT_KEY_FREEDOM_FIGHTER -->
- *%D_Change Freedom Fighters when one of your cities are captured.  <!-- TXT_KEY_FREEDOM_FIGHTER_CHANGE -->
- *%D1_Num Diplomacy Bonus with other Leaders  <!-- TXT_KEY_TRAITHELP_ATTITUDE_MODIFIER_POSITIVE -->
- *%D1_Num Diplomacy Penalty (inverts to a bonus with Leaders with same trait).  <!-- TXT_KEY_TRAITHELP_ATTITUDE_MODIFIER_NEGATIVE -->
- *%D1%% chance to Spread State Religion with your Missionaries.  <!-- TXT_KEY_TRAITHELP_STATE_REL_SPREAD_MODIFIER -->
- *%D1%% chance to Spread Non-State Religion with your Missionaries.  <!-- TXT_KEY_TRAITHELP_NON_STATE_REL_SPREAD_MODIFIER -->
- *Enables full function of all religious buildings regardless of State Religion.  <!-- TXT_KEY_ALL_RELIGIONS_ACTIVE -->
- *Disables all Non-State religious buildings.  <!-- TXT_KEY_BANS_NON_STATE_RELIGIONS -->
- *%D1_Mod%% Golden Age Length  <!-- TXT_KEY_BUILDINGHELP_GOLDENAGE_MOD -->
- *Starts a Golden Age when a %s1_GP is born in an owned city.  <!-- TXT_KEY_TRAITHELP_GOLDEN_AGE_ON_BIRTH_OF_GP_TYPE -->
- *City Require %d1%% More <food> to Grow  <!-- TXT_KEY_BUILDINGHELP_CITY_SLOW_GROWTH_SPEED -->
- *City Require %d1%% Less <food> to Grow  <!-- TXT_KEY_BUILDINGHELP_CITY_FAST_GROWTH_SPEED -->
- *New cities begin with %D1 population.  <!-- TXT_KEY_TRAITHELP_CITY_START_POPULATION -->
- *New cities begin with %D1 Culture.  <!-- TXT_KEY_TRAITHELP_CITY_START_CULTURE -->
- *New cities begin with the State Religion.  <!-- TXT_KEY_TRAITHELP_CITY_START_STATE_RELIGION -->
- *%d1 local instability penalty per turn.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_LOCAL_PENALTY -->
- *%d1 local stability bonus per turn in each city.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_LOCAL_BONUS -->
- *%d1 national instability penalty per turn.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_NATIONAL_PENALTY -->
- *%d1 national stability bonus per turn.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_NATIONAL_BONUS -->
- *%d1 stability bonus per turn for owning State Religion Holy City.  <!-- TXT_KEY_CIVICHELP_REV_GOOD_HOLY_CITY -->
- *%d1 instability penalty per turn if State Religion Holy City is owned by heathens.  <!-- TXT_KEY_CIVICHELP_REV_BAD_HOLY_CITY -->
- *%s1% Revolutionary sentiment (patriotism).  <!-- TXT_KEY_CIVICHELP_REV_NATIONALITY_REDUCTION_MOD -->
- *+%s1% Revolutionary sentiment (alienation).  <!-- TXT_KEY_CIVICHELP_REV_NATIONALITY_INCREASE_MOD -->
- *+%s1% increase in instability penalties from Non-State Religions.  <!-- TXT_KEY_CIVICHELP_REV_BAD_RELIGION_MOD -->
- *+%s1% increase in stability bonuses from State Religion.  <!-- TXT_KEY_CIVICHELP_REV_GOOD_RELIGION_MOD -->
- *%d1% to City Distance Instability penalty.  <!-- TXT_KEY_CIVICHELP_CITY_DISTANCE_GOOD_MOD -->
- *Increases local rebelliousness  <!-- TXT_KEY_INCREASE_LOCAL_REBELS -->
- *Decreases local rebelliousness  <!-- TXT_KEY_DECREASE_LOCAL_REBELS -->
- *Increases national rebelliousness  <!-- TXT_KEY_INCREASE_NATIONAL_REBELS -->
- *Decreases national rebelliousness  <!-- TXT_KEY_DECREASE_NATIONAL_REBELS -->
- *State <religion> with Holy City decreases rebelliousness  <!-- TXT_KEY_STATE_RELIGION_WITH_HOLY_CITY -->
- *State <religion> without Holy City increases rebelliousness  <!-- TXT_KEY_STATE_RELIGION_WITHOUT_HOLY_CITY -->
- *%D1%% Civic Upkeep  <!-- TXT_KEY_TRAITHELP_CIVIC_UPKEEP_MODIFIER -->
- *No Upkeep Costs for %s1 Civics  <!-- TXT_KEY_TRAITHELP_NO_UPKEEP -->
- *%D1_Mod%% Maintenance Costs from Distance to Palace  <!-- TXT_KEY_CIVICHELP_DISTANCE_MAINT_MOD -->
- *%D1_Mod%% Maintenance Costs from Number of Cities  <!-- TXT_KEY_CIVICHELP_NO_MAINT_NUM_CITIES_MOD -->
- *%D1_Mod%% Maintenance Costs from Corporations  <!-- TXT_KEY_CIVICHELP_NO_MAINT_CORPORATION_MOD -->
- *Workers build Improvements %D1_Modifier%% faster.  <!-- TXT_KEY_TRAITHELP_WORKER_SPEED_POSITIVE -->
- *Workers build Improvements with a %D1_Modifier%% speed penalty.  <!-- TXT_KEY_TRAITHELP_WORKER_SPEED_NEGATIVE -->
- *%D1_Modifier%% modifier to upgrade rate on all improvements  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_RATE_MODIFIER -->
- *%D1_Modifier%% modifier to upgrade rate for %s3_Improvement  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_RATE_MODIFIER_SPECIFIC -->
- , %s2_Improvement  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_RATE_MODIFIER_ADDITIONAL -->
- Improvements  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_RATE_MODIFIER_END -->
- *%D1_Modifier%% modifier to worker speeds when they %s3_Build  <!-- TXT_KEY_BUILDHELP_WORKER_SPEED_MODIFIER_SPECIFIC -->
- , %s2_Build  <!-- TXT_KEY_BUILDHELP_WORKER_SPEED_MODIFIER_ADDITIONAL -->
- *Research %D1_Change%% as fast as normal when researching %s3_Tech  <!-- TXT_KEY_TRAITHELP_TECH_RESEARCH_MODIFIER_FIRST -->
- , %s2_Tech  <!-- TXT_KEY_TRAITHELP_TECH_RESEARCH_MODIFIER_ADDITIONAL -->
- Technologies  <!-- TXT_KEY_TRAITHELP_TECH_RESEARCH_MODIFIER_END -->
- *%D1_Change%F2_HappOrUnhapp from  <!-- TXT_KEY_TRAITHELP_BUILDING_HAPPINESS -->
- → `buildDisplayString`

## `setAngerHelp`

- "Viva la Resistance!"  <!-- TXT_KEY_ANGER_RESISTANCE -->
- "Our Civilization is in Anarchy!"  <!-- TXT_KEY_ANGER_ANARCHY -->
- +%d1_Change<unhappy>: "It's too crowded!"  <!-- TXT_KEY_ANGER_OVERCROWDING -->
- +%d1_Change<unhappy>: "We fear for our safety. We demand military protection!"  <!-- TXT_KEY_ANGER_MILITARY_PROTECTION -->
- +%d1_Change<unhappy>: "We yearn to join our Motherland!"  <!-- TXT_KEY_ANGER_OCCUPIED -->
- +%d1_Change<unhappy>: "We will not fight with our brothers and sisters in the Faith!"  <!-- TXT_KEY_ANGER_RELIGION_FIGHT -->
- +%d1_Change<unhappy>: "We cannot forget your cruel oppression!"  <!-- TXT_KEY_ANGER_OPPRESSION -->
- +%d1_Change<unhappy>: "You refused to grant our request!"  <!-- TXT_KEY_REV_REQUEST_ANGER -->
- +%d1_Change<unhappy>: "We're feeling rebellious!"  <!-- TXT_KEY_REV_INDEX_ANGER -->
- +%d1_Change<unhappy>: "Hell NO, we won't GO!"  <!-- TXT_KEY_ANGER_DRAFT -->
- +%d1_Change<unhappy>: "The world considers you a villain!"  <!-- TXT_KEY_ANGER_DEFY_RESOLUTION -->
- +%d1_Change<unhappy>: "War... What is it good for? Absolutely nothing... Unh!"  <!-- TXT_KEY_ANGER_WAR_WEAR -->
- +%d1_Change<unhappy>: "Down with foreign influence!"  <!-- TXT_KEY_UNHAPPY_VASSAL -->
- +%d1_Change<unhappy>: "People are telling us of your villainy!"  <!-- TXT_KEY_ANGER_ESPIONAGE -->
- +%d1_Change<unhappy>: %s2_Name  <!-- TXT_KEY_ANGER_CIVIC_PERCENT_BAD -->
- %d1_Change<unhappy>: %s2_Name  <!-- TXT_KEY_ANGER_CIVIC_PERCENT_GOOD -->
- +%d1_Change<unhappy>: "Large Cities Are Oppressive!"  <!-- TXT_KEY_ANGER_BIG_CITY -->
- +%d1_Change<unhappy>: "We hate our government!"  <!-- TXT_KEY_UNHAPPY_CIVIC -->
- +%d1_Change<unhappy>: "Other nations' technological breakthroughs have harmed the world!"  <!-- TXT_KEY_UNHAPPY_WORLD_PROJECT -->
- +%d1_Change<unhappy>: "Our Nation's Technological Breakthroughs have harmed Ourselves!"  <!-- TXT_KEY_UNHAPPY_PROJECT -->
- +%d1_Change<unhappy>: "No Taxation Without Representation!"  <!-- TXT_KEY_CITY_TAXATION_ANGER -->
- +%d1_Change<unhappy>: "Corporations are Harming our City!"  <!-- TXT_KEY_UNHAPPY_CORPORATIONS -->
- +%d1_Change<unhappy>: "We Hate Nature!"  <!-- TXT_KEY_CITY_LANDMARK_ANGER -->
- +%d1_Change<unhappy>: "You're Destroying the Environment!"  <!-- TXT_KEY_CITY_LANDMARK_DESTRUCTION_ANGER -->
- +%d1_Change<unhappy>: "The military presence makes us nervous!"  <!-- TXT_KEY_ANGER_MILITARY_PRESENCE -->
- +%d1_Change<unhappy>: "We demand a new State Religion!"  <!-- TXT_KEY_ANGER_STATE_RELIGION -->
- +%d1_Change<unhappy>: Our inept government's attempts to over-expand are making us unhappy  <!-- TXT_KEY_ANGER_TOO_MANY_CITIES -->
- +%d1<unhappy> "Buildings and Effects in this city are directly irritating us!"  <!-- TXT_KEY_UNHAPPY_CITY_BUILDINGS -->
- +%d1_Change<unhappy>: "Some improvements are making us unhappy!"  <!-- TXT_KEY_ANGER_FEATURES -->
- +%d1_Change<unhappy>: "Some goods are making us unhappy!"  <!-- TXT_KEY_ANGER_BONUS -->
- +%d1_Change<unhappy>: "We are angered by some citizens' occupations!"  <!-- TXT_KEY_UNHAPPY_SPECIALISTS -->
- +%d1_Change<unhappy>: "We desire religious freedom!"  <!-- TXT_KEY_ANGER_RELIGIOUS_FREEDOM -->
- +%d1_Change<unhappy>: "We do not appreciate this type of entertainment!"  <!-- TXT_KEY_ANGER_BAD_ENTERTAINMENT -->
- +%d1<unhappy> "Buildings and Effects that influence the region we are in are driving us crazy!"  <!-- TXT_KEY_UNHAPPY_AREA_BUILDINGS -->
- +%d1<unhappy> "Buildings and Effects that impact all of our cities are annoying us all!"  <!-- TXT_KEY_UNHAPPY_PLAYER_BUILDINGS -->
- +%d1_Change<unhappy>: "AAAARRRGHHHH!"  <!-- TXT_KEY_ANGER_ARGH -->
- +%d1<unhappy> "Our advanced society has made the efforts of some professions completely intolerable!"  <!-- TXT_KEY_UNHAPPY_TECH_SPECIALIST -->
- +%d1_Change<unhappy>: "We're just being difficult!"  <!-- TXT_KEY_ANGER_HANDICAP -->
- +%d1_Change<unhappy>: "Misc."  <!-- TXT_KEY_ANGER_MISC -->
- Total Unhappiness: %d1_Num<unhappy>  <!-- TXT_KEY_ANGER_TOTAL_UNHAPPY -->

## `setBadHealthHelp`

- %D1_Change<unhealth> from %s2_FeatName  <!-- TXT_KEY_MISC_FEAT_HEALTH -->
- Features  <!-- TXT_KEY_MISC_FEATURES -->
- %D1_Change<unhealth> from %s2_ImpName  <!-- TXT_KEY_MISC_IMPR_HEALTH -->
- Improvements  <!-- TXT_KEY_MISC_IMPROVEMENTS -->
- %D1_Change<unhealth> from Specialists  <!-- TXT_KEY_BAD_HEALTH_FROM_SPECIALISTS -->
- %D1_Change<unhealth> from Poisoned Water  <!-- TXT_KEY_MISC_HEALTH_FROM_ESPIONAGE -->
- %D1_Change<unhealth> from Bonuses  <!-- TXT_KEY_MISC_HEALTH_FROM_BONUSES -->
- %D1_Change<unhealth> from Buildings  <!-- TXT_KEY_MISC_HEALTH_FROM_BUILDINGS -->
- %D1_Change<unhealth> from Civics  <!-- TXT_KEY_MISC_BAD_HEALTH_FROM_CIVICS -->
- %D1_Change<unhealth> from Civilization  <!-- TXT_KEY_MISC_HEALTH_FROM_CIV -->
- %D1_Change<unhealth> from Events  <!-- TXT_KEY_MISC_BAD_HEALTH_FROM_EVENTS -->
- %D1_Change<unhealth> penalty  <!-- TXT_KEY_MISC_UNHEALTH_EXTRA -->
- %D1_Change<unhealth> from Difficulty Level  <!-- TXT_KEY_MISC_HEALTH_FROM_HANDICAP -->
- %D1_Change<unhealth> from Population  <!-- TXT_KEY_MISC_HEALTH_FROM_POP -->
- %D1_Change<unhealth> From World Projects  <!-- TXT_KEY_MISC_HEALTH_FROM_WORLD_PROJECT -->
- %D1_Change<unhealth> From National Projects  <!-- TXT_KEY_MISC_HEALTH_FROM_PROJECT -->
- %D1_Change<unhealth> From Corporations  <!-- TXT_KEY_MISC_HEALTH_FROM_CORPORATION -->
- +%d1<unhealth> "Our advanced society has made the efforts of some professions produce filthy byproducts!"  <!-- TXT_KEY_UNHEALTHY_TECH_SPECIALIST -->
- %d1_Num<unhealth> Total Unhealthiness  <!-- TXT_KEY_MISC_TOTAL_UNHEALTHY -->

## `setBasicUnitHelpWithCity`

- *Tech: %s1_Tech Grid X: %d2_Value  <!-- TXT_KEY_UNITHELP_GRID_X -->
- %d1_Num Range  <!-- TXT_KEY_UNITHELP_AIRRANGE -->
- → `setUnitExperienceHelp`
- *Fortified Repel: Adds %d1_Amount Repel Value per turn fortified.  <!-- TXT_KEY_UNITHELP_FORT_REPEL -->
- *Overrun: +%d1_Amount%% vs Fortification bonuses.  <!-- TXT_KEY_UNITHELP_OVERRUN -->
- *Doesn't Receive Defensive Bonuses  <!-- TXT_KEY_UNITHELP_NO_DEFENSE_BONUSES -->
- *%D1_Amount%% Attack Strength  <!-- TXT_KEY_UNITHELP_ATTACK_MODIFIER -->
- *%D1_Amount%% Defence Strength  <!-- TXT_KEY_UNITHELP_DEFENSE_MODIFIER -->
- *%D1_Change%% City Strength  <!-- TXT_KEY_UNITHELP_CITY_STRENGTH_MOD -->
- *%D1_Change%% City Attack  <!-- TXT_KEY_UNITHELP_CITY_ATTACK_MOD -->
- *%D1_Change%% City Defense  <!-- TXT_KEY_UNITHELP_CITY_DEFENSE_MOD -->
- *%D1_Change%% vs. %s2_Type  <!-- TXT_KEY_UNITHELP_MOD_VS_TYPE_NO_LINK -->
- *%D1_Change%% Hills Strength  <!-- TXT_KEY_UNITHELP_HILLS_STRENGTH -->
- *%D1_Change%% Hills Attack  <!-- TXT_KEY_UNITHELP_HILLS_ATTACK -->
- *%D1_Change%% Hills Defense  <!-- TXT_KEY_UNITHELP_HILLS_DEFENSE -->
- ,  <!-- TXT_KEY_COMMA -->
- *%D1_Change%% %s2_TerrOrFeat Defense  <!-- TXT_KEY_UNITHELP_DEFENSE -->
- *%D1_Change%% %s2_TerrOrFeat Attack  <!-- TXT_KEY_UNITHELP_ATTACK -->
- *%D1_Change%% vs. %s3_Type  <!-- TXT_KEY_UNITHELP_MOD_VS_TYPE -->
- *%D1_Change%% Attack vs. %s3_Class.  <!-- TXT_KEY_UNITHELP_ATTACK_MOD_VS_CLASS -->
- *%D1_Change%% Defense vs. %s3_Class  <!-- TXT_KEY_UNITHELP_DEFENSE_MOD_VS_CLASS -->
- *%D1_Amount%% Religious Combat Modifier - applies against units that differ in religion to your unit - inverses to a penalty when foe is of same religion.  <!-- TXT_KEY_UNITHELP_RELIGIOUS_COMBAT_MODIFIER -->
- *%d1_Amount%% VS Non-Animal Barbarians  <!-- TXT_KEY_UNITHELP_VSBARBS -->
- *%D1_Change%% vs. Wild Animals  <!-- TXT_KEY_UNITHELP_ANIMAL_COMBAT_MOD -->
- *Reduces combat damaged suffered by %d1_Amount%% per round.  <!-- TXT_KEY_UNITHELP_ARMOR_BASE -->
- *%D1_Change Armor vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_ARMOR_VS_TYPE -->
- *Reduces opponent's Armor value by %D1_Amount.  <!-- TXT_KEY_UNITHELP_PUNCTURE -->
- *%D1_Change Puncture vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_PUNCTURE_VS_TYPE -->
- *Damage dealt is modified by %D1_Amount%  <!-- TXT_KEY_UNITHELP_DAMAGE_MODIFIER -->
- *Modifies the base 50% Dodge by %d1_Amount%%  <!-- TXT_KEY_UNITHELP_DODGE_MODIFIER -->
- *%D1_Change Dodge vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_DODGE_VS_TYPE -->
- *Modifies the base 50% Precision by %d1_Amount  <!-- TXT_KEY_UNITHELP_PRECISION_MODIFIER -->
- *%D1_Change Precision vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_PRECISION_VS_TYPE -->
- *%D1_Amount%% VS Each Size Rank Larger  <!-- TXT_KEY_UNITHELP_COMBAT_MOD_PER_SIZE_MORE -->
- *%D1_Amount%% VS Each Size Rank Smaller  <!-- TXT_KEY_UNITHELP_COMBAT_MOD_PER_SIZE_LESS -->
- *%D1_Amount%% VS Each Group Rank Larger  <!-- TXT_KEY_UNITHELP_COMBAT_MOD_PER_VOLUME_MORE -->
- *%D1_Amount%% VS Each Group Rank Smaller  <!-- TXT_KEY_UNITHELP_COMBAT_MOD_PER_VOLUME_LESS -->
- *Maximum %d1%% damage to enemy on attack  <!-- TXT_KEY_UNITHELP_COMBAT_LIMIT -->
- *Can Escape Capture (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_ESCAPE_SPY -->
- *Can Withdraw from Combat (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_WITHDRAWL_PROBABILITY -->
- *%D1_Change%% Withdraw vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_WITHDRAW_VS_TYPE -->
- *Starts Withdrawal at %d1_Amount%% HP  <!-- TXT_KEY_UNITHELP_EARLY_WITHDRAW -->
- *Reflexes: Gains +%d1_Amount%% additional withdrawal chance per attack in a given round.  <!-- TXT_KEY_UNITHELP_REFLEXES -->
- *Frays: Loses %d1_Amount%% withdrawal chance per attack in a given round.  <!-- TXT_KEY_UNITHELP_FRAYS -->
- *Pursuit Chance: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_PURSUIT -->
- *%D1_Change%% Pursuit vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_PURSUIT_VS_TYPE -->
- *Repel: When defending, forces attackers to attempt withdraw at a %d1_Amount%% chance each round.  <!-- TXT_KEY_UNITHELP_REPEL -->
- *Beyond the first, unit attempts an additional %d1_Amount Repel efforts when defending.  <!-- TXT_KEY_UNITHELP_REPEL_RETRIES -->
- *%D1_Change%% Repel vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_REPEL_VS_TYPE -->
- *Knockback: %d1_Amount%% per round chance to force defenders to withdraw.  <!-- TXT_KEY_UNITHELP_KNOCKBACK -->
- *Beyond the first, unit attempts an additional %d1_Amount Knockback efforts when defending.  <!-- TXT_KEY_UNITHELP_KNOCKBACK_RETRIES -->
- *%D1_Change%% Knockback vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_KNOCKBACK_VS_TYPE -->
- *Unyielding: Subtracts %d1_Amount from enemy Knockback and Repel values.  <!-- TXT_KEY_UNITHELP_UNYIELDING -->
- *Stampede: Will continue attacking until all defenders in the attacked plot are dead.  <!-- TXT_KEY_UNITHELP_STAMPEDE -->
- *This unit attacks repeatedly until harmed.  <!-- TXT_KEY_UNITHELP_ONSLAUGHT -->
- *First Strikes: %d1  <!-- TXT_KEY_UNITHELP_FIRST_STRIKES -->
- *First Strikes: %d1-%d2  <!-- TXT_KEY_UNITHELP_FIRST_STRIKE_CHANCES -->
- *Immune to First Strikes  <!-- TXT_KEY_UNITHELP_FIRST_STRIKES_IMMUNE -->
- *Breakdown Chance: %D1%%  <!-- TXT_KEY_UNITHELP_BREAKDOWN_CHANCE -->
- *Breakdown Amount: %D1%%  <!-- TXT_KEY_UNITHELP_BREAKDOWN_DAMAGE -->
- *Can Only Attack Cities  <!-- TXT_KEY_UNITHELP_CAN_ONLY_ATTACK_CITIES -->
- *Can Always Attack Cities  <!-- TXT_KEY_UNITHELP_CAN_ALWAYS_ATTACK_CITIES -->
- *Can perform Ranged Attacks  <!-- TXT_KEY_IS_DCM_BOMBARD -->
- *Ranged Assault Distance: %D1_Value  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_RANGE -->
- *Ranged Assault Accuracy: %D1_Value%%  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_ACCURACY -->
- *Ranged Assault Damage: %D1_Value%%  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_DAMAGE -->
- *Ranged Assault Damage Limit: %D1_Value%%  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_DAMAGE_LIMIT -->
- *Ranged Assault Max Targets: %D1_Value  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_DAMAGE_MAX_UNITS -->
- *Can Bombard City Defenses (-%d1_Mod%%/Turn).  <!-- TXT_KEY_UNITHELP_BOMBARD_RATE -->
- *Can Destroy Tile Improvements and Bomb City Defenses (-%d1_Mod%%/Turn).  <!-- TXT_KEY_UNITHELP_BOMB_RATE -->
- *Up to %d1% Collateral to %d2 Unit(s)  <!-- TXT_KEY_UNITHELP_COLLATERAL_DAMAGE_REVDCM -->
- *%d1_Amount Strength Flank attack against %s2_unitlist  <!-- TXT_KEY_UNITHELP_COMBAT_FLANKING_STRIKES -->
- <name of the thing>
- *Can Intercept Enemy Spies (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_INTERCEPT_AIRCRAFT_SPY -->
- *Improved Counter Espionage Missions (%d1_Amount%%)  <!-- TXT_KEY_UNITHELP_INTERCEPT_AIRCRAFT_SPY_COUNTER -->
- *Can Intercept Aircraft (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_INTERCEPT_AIRCRAFT -->
- *Can Evade Enemy Detection (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_EVADE_INTERCEPTION_SPY -->
- *Can Evade Interception (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_EVADE_INTERCEPTION -->
- Can perform Fighter Engagement mission.  <!-- TXT_KEY_IS_FIGHTER_ENGAGE -->
- *Unnerve: Unit counts as +%d1_Amount%% strength when surrounding.  <!-- TXT_KEY_UNITHELP_UNNERVE -->
- *Enclose: Adds +%d1_Amount%% to the maximum possible Surround and Destroy bonus when surrounding.  <!-- TXT_KEY_UNITHELP_ENCLOSE -->
- *Lunge: Gets +%d1_Amount%% Surround and Destroy Bonus when attacking.  <!-- TXT_KEY_UNITHELP_LUNGE -->
- *Dynamic Defense: Denies %d1_Amount%% Surround and Destroy Penalty when defending.  <!-- TXT_KEY_UNITHELP_DYNAMIC_DEFENSE -->
- *Power Shots: Unit starts with %d1_Amount Power Shots - enhanced first rounds of battle  <!-- TXT_KEY_UNITHELP_POWER_SHOTS -->
- *Power Shot Combat Modifier: Unit gets a %d1_Amount%% modifier to Combat during its Power Shots  <!-- TXT_KEY_UNITHELP_POWER_SHOT_COMBAT_MODIFIER -->
- *Power Shot Puncture Modifier: Unit gets a %d1_Amount modifier to Puncture during its Power Shots  <!-- TXT_KEY_UNITHELP_POWER_SHOT_PUNCTURE_MODIFIER -->
- *Power Shot Precision Modifier: Unit gets a %d1_Amount modifier to Precision during its Power Shots  <!-- TXT_KEY_UNITHELP_POWER_SHOT_PRECISION_MODIFIER -->
- *Power Shot Critical Modifier: Unit gets a %d1_Amount%% modifier to Critical Hit chances during its Power Shots  <!-- TXT_KEY_UNITHELP_POWER_SHOT_CRITICAL_MODIFIER -->
- *Critical Modifier: Unit gets a %d1_Amount%% modifier to Critical Hit chances each round of combat  <!-- TXT_KEY_UNITHELP_CRITICAL_MODIFIER -->
- *%D1_Change%% Critical Chance vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_CRITICAL_VS_TYPE -->
- *Round Stun: %d1_Amount%% of damage dealt is base chance to stun an opponent each round.  <!-- TXT_KEY_UNITHELP_ROUND_STUN_PROB -->
- *%D1_Change%% Round Stun Chance vs. %s3_TypeName  <!-- TXT_KEY_UNITHELP_ROUND_STUN_VS_TYPE -->
- *%d1_Change%% Disable Traps vs %s3_TypeName  <!-- TXT_KEY_UNITHELP_TRAP_DISABLE_TYPE -->
- *%d1_Change%% chance to Avoid Traps of %s3_TypeName  <!-- TXT_KEY_UNITHELP_TRAP_AVOID_TYPE -->
- *%d1_Change%% chance of Triggering when %s3_TypeName moves into armed tile.  <!-- TXT_KEY_UNITHELP_TRAP_TRIGGER_TYPE -->
- *Gives a free %s1_Promotion promotion to any traps this unit sets. (As long as promotion is valid for that trap)  <!-- TXT_KEY_UNITHELP_TRAP_PROMOTION_TYPE -->
- *Immune to %s1_UnitCombat traps.  <!-- TXT_KEY_UNITHELP_TRAP_IMMUNE_TYPE -->
- TBD  <!-- TXT_KEY_UNITHELP_TRAP_DAMAGE -->
- *May be triggered %d1_value times before final destruction.  <!-- TXT_KEY_UNITHELP_TRAP_NUM_TRIGGERS -->
- *If this trap can trigger against an attacking foe, it will do so before combat begins, weakening the attacker before combat.  <!-- TXT_KEY_UNITHELP_TRAP_TRIGGER_BEFORE_ATTACK -->
- *Disabling Complexity: %d1_value  <!-- TXT_KEY_UNITHELP_TRAP_COMPLEXITY -->
- *Afflict (Immediate): Instant %d1_Probability chance of inflicting %s2_PromotionName on striking your enemy.  <!-- TXT_KEY_AFFLICT_ON_ATTACK_IMMEDIATE -->
- *Afflict: At end of battle, %d1_Probability chance of inflicting %s2_PromotionName to an enemy you injured.  <!-- TXT_KEY_AFFLICT_ON_ATTACK -->
- *Poison Mastery: %D1_Amount%% Modifier to all Afflict Attempts.  <!-- TXT_KEY_UNITHELP_POISON_PROB -->
- *Weak Poisoner: %D1_Amount%% Modifier to all Afflict Attempts.  <!-- TXT_KEY_UNITHELP_POISON_PROB_NEG -->
- *Deals Cold Damage with its attacks.  <!-- TXT_KEY_UNITHELP_DEALS_COLD_DAMAGE -->
- *Rage: Gains +%d1_Amount%% additional strength per round of battle during combat.  <!-- TXT_KEY_UNITHELP_RAGE -->
- *Fatigue: Loses %d1_Amount%% strength per round of battle during combat.  <!-- TXT_KEY_UNITHELP_FATIGUE -->
- (key not in current GameText)  <!-- TXT_KEY_UNITHELP_RAMPAGE -->
- *Tires: Loses %d1_Amount%% strength per attack in a given round.  <!-- TXT_KEY_UNITHELP_TIRES -->
- *Determination: Gains +%d1_Amount%% additional strength per defense in a given round.  <!-- TXT_KEY_UNITHELP_DETERMINATION -->
- *Base Cargo Space (estimated # of units): %d1  <!-- TXT_KEY_UNITHELP_CARGO_SPACE_BASE_SM -->
- (Carries %s1)  <!-- TXT_KEY_UNITHELP_CARRIES -->
- *Does not allow transport of %s1 Units.  <!-- TXT_KEY_PROMOHELP_CHANGE_NOT_SPECIAL_CARGO -->
- *Cargo Space: %d1  <!-- TXT_KEY_UNITHELP_CARGO_SPACE_FOREIGN -->
- *Aid (%s1_Type): Adds a bonus to units and cities on the same tile to Overcoming %s1_Type Afflictions by %d2_Amount  <!-- TXT_KEY_UNITHELP_AID -->
- *Cure: May remove or improve %s1_PromotionName on those afflicted.  <!-- TXT_KEY_CURE_AFFLICTION -->
- *Inoculated: %D1_Change%% less likely to contract %s2_Affliction.  <!-- TXT_KEY_UNITHELP_AFFLICTION_FORTITUDE_MODIFIER_POSITIVE -->
- *Affliction Synergy: %D1_Change%% more likely to contract %s2_Affliction.  <!-- TXT_KEY_UNITHELP_AFFLICTION_FORTITUDE_MODIFIER_NEGATIVE -->
- *Fortitude: Resistance to Disease and Increased likelihood of Overcoming any Affliction by %d1_Amount%%  <!-- TXT_KEY_UNITHELP_FORTITUDE -->
- *Immune to Cold Damage Penalties.  <!-- TXT_KEY_UNITHELP_COLD_IMMUNE -->
- *Immune to collateral damage from %s2_units  <!-- TXT_KEY_UNITHELP_COLLATERAL_IMMUNE -->
- *Cannot Capture Enemy Cities or Units  <!-- TXT_KEY_UNITHELP_CANNOT_CAPTURE -->
- *%D1%% chance to Capture.  <!-- TXT_KEY_UNITHELP_CAPTURE_PROBABILITY_MODIFIER -->
- *%D1%% chance to avoid Capture.  <!-- TXT_KEY_UNITHELP_CAPTURE_RESISTANCE_MODIFIER -->
- *Taunt Chance: %D1%%  <!-- TXT_KEY_UNITHELP_TAUNT -->
- *%d1_Change Cultural Revolt Protection  <!-- TXT_KEY_TEMP_REVOLT_PROTECTION -->
- *Targets any %s1_unit_list first in combat. If Assassination is possible, these units are also the list of units that may be targeted for assassination.  <!-- TXT_KEY_UNITHELP_TARGETS_UNIT_FIRST -->
- *Defends first against %s1_unit_list  <!-- TXT_KEY_UNITHELP_DEFENDS_UNIT_FIRST -->
- *Can Improve Tiles  <!-- TXT_KEY_UNITHELP_IMPROVE_PLOTS -->
- *Can  <!-- TXT_KEY_UNITHELP_CAN -->
- → `setListHelp`
- *%D1_Change%% Work Speed on %s2_Gameobject  <!-- TXT_KEY_PROMOTIONHELP_WORK -->
- *%D1_Change%% Work Speed on Hills  <!-- TXT_KEY_PROMOTIONHELP_HILLS_WORK -->
- *%D1_Change%% Work Speed on Peaks  <!-- TXT_KEY_PROMOTIONHELP_PEAKS_WORK -->
- *Front Support: +%d1_Amount%%  <!-- TXT_KEY_UNITHELP_FRONT_SUPPORT_PERCENT -->
- *Short Range Support: +%d1_Amount%%  <!-- TXT_KEY_UNITHELP_SHORT_RANGE_SUPPORT_PERCENT -->
- *Medium Range Support: +%d1_Amount%%  <!-- TXT_KEY_UNITHELP_MEDIUM_RANGE_SUPPORT_PERCENT -->
- *Long Range Support: +%d1_Amount%%  <!-- TXT_KEY_UNITHELP_LONG_RANGE_SUPPORT_PERCENT -->
- *Flank Support: +%d1_Amount%%  <!-- TXT_KEY_UNITHELP_FLANK_SUPPORT_PERCENT -->
- → `buildDisplayString`
- *Can Start a Golden Age  <!-- TXT_KEY_UNITHELP_GOLDEN_AGE -->
- *Can Discover a Technology  <!-- TXT_KEY_UNITHELP_DISCOVER_TECH -->
- *Can Hurry Production  <!-- TXT_KEY_UNITHELP_HURRY_PRODUCTION -->
- *Can Hurry Food (Adds Base: %d1)  <!-- TXT_KEY_UNITHELP_HURRY_FOOD -->
- *Can Conduct a Trade Mission (Base: %d1, Mult: x%d2)  <!-- TXT_KEY_UNITHELP_TRADE_MISSION -->
- *Can Create a Great Work (%D1_Change<culture>)  <!-- TXT_KEY_UNITHELP_GREAT_WORK -->
- *Can Infiltrate another player's City (%D1_Change <spy>)  <!-- TXT_KEY_UNITHELP_ESPIONAGE_MISSION -->
- *May conduct Inquisition Mission  <!-- TXT_KEY_UNITHELP_IS_INQUISITOR -->
- *Can Spread  <!-- TXT_KEY_UNITHELP_CAN_SPREAD -->
- *Can Expand  <!-- TXT_KEY_UNITHELP_CAN_EXPAND -->
- *Can Join City as  <!-- TXT_KEY_UNITHELP_CAN_JOIN -->
- *Can Construct  <!-- TXT_KEY_UNITHELP_CAN_CONSTRUCT -->
- *Better Results from Tribal Villages  <!-- TXT_KEY_UNITHELP_NO_BAD_GOODIES -->
- *This Unit is Tradable  <!-- TXT_KEY_TRADABLE_UNIT -->
- *Can Found a New City  <!-- TXT_KEY_UNITHELP_FOUND_CITY -->
- *Max HP: %d1 (usually 100.)  <!-- TXT_KEY_UNITHELP_MAX_HP -->
- *Military Branch  <!-- TXT_KEY_UNITHELP_BRANCH_MILITARY -->
- *Civilian Branch  <!-- TXT_KEY_UNITHELP_BRANCH_CIVILIAN -->
- *Upkeep: %s1 <gold>  <!-- TXT_KEY_UNITHELP_UPKEEP -->
- *Upkeep change: %d1 <gold>  <!-- TXT_KEY_UNITHELP_UPKEEP_CHANGE -->
- *Helps Thwart Rival Spies  <!-- TXT_KEY_UNITHELP_EXPOSE_SPIES -->
- *Can Nuke Enemy Lands  <!-- TXT_KEY_UNITHELP_CAN_NUKE -->
- *Can Attack without Declaring War  <!-- TXT_KEY_UNITHELP_ALWAYS_HOSTILE -->
- *Can Explore Rival Territory  <!-- TXT_KEY_UNITHELP_EXPLORE_RIVAL -->
- *Can upgrade almost anywhere.  <!-- TXT_KEY_UPGRADE_ANYWHERE -->
- *Hidden Nationality  <!-- TXT_KEY_UNITHELP_HIDDEN_NATIONALITY -->
- *Exiled from its own cultural border.  <!-- TXT_KEY_EXCILE -->
- *This unit can enter territories you have a Right of Passage or Open Borders agreement with.  <!-- TXT_KEY_PASSAGE -->
- *This unit cannot enter a city that is not your own without attacking it.  <!-- TXT_KEY_NO_NON_OWNED_CITY -->
- *At peace with humanoid NPC's.  <!-- TXT_KEY_UNITHELP_BARB_COEXIST -->
- *Enters all cities peacefully.  <!-- TXT_KEY_UNITHELP_BLEND_INTO_CITY -->
- *This unit can initiate Assassinations against units on the same tile.  <!-- TXT_KEY_UNITHELP_ASSASSIN -->
- *Invisible to All Units  <!-- TXT_KEY_UNITHELP_INVISIBLE_ALL -->
- *Invisible to Most Units  <!-- TXT_KEY_UNITHELP_INVISIBLE_MOST -->
- *Can See %F1_Name  <!-- TXT_KEY_UNITHELP_SEE_INVISIBLE -->
- *%D1_Change %F2_Type Spot Range  <!-- TXT_KEY_UNITHELP_INVISIBILITY_SPOT_RANGE_VALUE -->
- *%d1_Change %F2_Type Veil  <!-- TXT_KEY_UNITHELP_INVISIBILITY_VEIL_VALUE -->
- *%D1_Change %F2_Type Veil on %s3_Plot  <!-- TXT_KEY_UNITHELP_INVISIBILITY_VEIL_PLOT_VALUE -->
- *%D1_Change %F2_Type Spot on %s3_Plot  <!-- TXT_KEY_UNITHELP_INVISIBILITY_SPOT_PLOT_VALUE -->
- *%D1_Change %F2_Type Spot Range on %s3_Plot  <!-- TXT_KEY_UNITHELP_INVISIBILITY_SPOT_PLOT_RANGE_VALUE -->
- *Flat Movement Costs  <!-- TXT_KEY_UNITHELP_FLAT_MOVEMENT -->
- *Ignores Terrain Movement Costs  <!-- TXT_KEY_UNITHELP_IGNORE_TERRAIN -->
- *Ignores Zones of Control  <!-- TXT_KEY_UNITHELP_IGNORE_ZONE_OF_CONTROL -->
- *Flies To Move  <!-- TXT_KEY_UNITHELP_FLIES_TO_MOVE -->
- *This Animal has %d1_Causes Causes to Ignore border Restrictions.  <!-- TXT_KEY_UNITHELP_ANIMAL_IGNORES_BORDERS -->
- *Cannot Enter  <!-- TXT_KEY_UNITHELP_CANNOT_ENTER -->
- %s2_terrain (until %s3_technology)  <!-- TXT_KEY_TERRAINHELP_UNTIL_TECH -->
- Can Not Traverse  <!-- TXT_KEY_UNITHELP_CAN_ONLY_TRAVERSE -->
- *Can Move through Impassable Terrain  <!-- TXT_KEY_UNITHELP_CAN_MOVE_IMPASSABLE -->
- *Can Only Defend  <!-- TXT_KEY_UNITHELP_ONLY_DEFENSIVE -->
- *Can Provide %d1_Num Experience to Units in the Same Tile  <!-- TXT_KEY_UNITHELP_LEADER -->
- *Can Provide up to %d1 XP to Each Unit in the Same Tile  <!-- TXT_KEY_UNITHELP_LEADER_EXPERIENCE -->
- *Can perform paradrops (Range=%d1_range)  <!-- TXT_KEY_UNITHELP_PARADROP_RANGE -->
- When attached to a unit:  <!-- TXT_KEY_PROMOTIONHELP_WHEN_LEADING -->
- Requires your state religion to be present in this city  <!-- TXT_KEY_REQUIRES_STATE_RELIGION_IN_CITY -->
- *State Religion must be present in city to train  <!-- TXT_KEY_UNITHELP_REQUIRES_STATE_RELIGION -->
- *Can only be Built on %s1_Name and Earlier Starts  <!-- TXT_KEY_UNITHELP_MAX_START_ERA -->
- Free Starting Experience:  <!-- TXT_KEY_UNITHELP_WILL_RECEIVE_FREE_EXPERIENCE -->
- *%D1 from City  <!-- TXT_KEY_CITY_FREE_EXPERIENCE -->
- *%D1 from Wonders, Civics and Traits  <!-- TXT_KEY_PLAYER_FREE_EXPERIENCE -->
- *%D1 from Specialists that Add to All Units  <!-- TXT_KEY_SPECIALISTHELP_FREE_EXPERIENCE -->
- *%D1 for being a %s2_UNITCOMBAT type unit  <!-- TXT_KEY_UNITCOMBATHELP_FREE_EXPERIENCE -->
- *%D1 for being a %s2_DOMAIN type unit  <!-- TXT_KEY_DOMAIN_FREE_EXPERIENCE -->
- *%D1 from State Religion  <!-- TXT_KEY_STATE_RELIGION_FREE_EXPERIENCE -->
- *%D1 with State Religion  <!-- TXT_KEY_NO_STATE_RELIGION_FREE_EXPERIENCE -->
- *%D1 with %s2_building  <!-- TXT_KEY_NO_BUILDING_FREE_EXPERIENCE -->
- *%D1%% for being trained in a Capital or Government Center  <!-- TXT_KEY_UNITHELP_XP_MOD_CAPITAL -->
- *%D1%% for being trained in your State Religion's Holy City  <!-- TXT_KEY_UNITHELP_XP_MOD_HOLY_CITY_STATE_RELIGION -->
- *%D1%% for being trained in a Holy City to a Non-State Religion  <!-- TXT_KEY_UNITHELP_XP_MOD_HOLY_CITY_NONSTATE_RELIGION -->
- *%D1%% Total XP modifier  <!-- TXT_KEY_UNITHELP_XP_MOD_TOTAL -->
- *%d1 Total XP.  <!-- TXT_KEY_UNITHELP_XP_TOTAL -->
- *Starts with  <!-- TXT_KEY_BULLET_STARTS_WITH -->
- *This Unit Cannot Heal Without Assistance  <!-- TXT_KEY_UNITHELP_SELF_HEAL_NONE -->
- *Self heal: %d1%%  <!-- TXT_KEY_UNITHELP_SELF_HEAL -->
- *Can heal %D1_Amount unit(s)/turn  <!-- TXT_KEY_PROMOTIONHELP_HEAL_SUPPORT -->
- *Assists in Healing %s1_UNITCOMBAT Units in Same Tile %D2_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_UNITCOMBAT_SAME -->
- Damage/Turn  <!-- TXT_KEY_PROMOTIONHELP_DAMAGE_TURN -->
- *Assists in Healing %s1_UNITCOMBAT Units in Adjacent Tiles %D2_Heals%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_UNITCOMBAT_ADJACENT -->
- *Double production speed for %s1_trait leaders  <!-- TXT_KEY_DOUBLE_SPEED_TRAIT -->
- *%D1_change%% production speed for %s2_trait leaders  <!-- TXT_KEY_PRODUCTION_MODIFIER_TRAIT -->
- *Does not get unit production modifers while training, except for those specifically for this unit type.  <!-- TXT_KEY_NO_NON_TYPE_PROD_MODS -->
- *%s1_Change%% Insidiousness  <!-- TXT_KEY_PROMOTIONHELP_INSIDIOUSNESS -->
- *%s1_Change%% Investigation  <!-- TXT_KEY_PROMOTIONHELP_INVESTIGATION -->
- *%D1_Change Stealth Strikes  <!-- TXT_KEY_PROMOTIONHELP_STEALTH_STRIKES -->
- *%D1_Change%% Stealth Combat Modifier  <!-- TXT_KEY_PROMOTIONHELP_STEALTH_COMBAT_MODIFIER -->
- *This Unit will Stealth Defend  <!-- TXT_KEY_UNITHELP_STEALTH_DEFEND -->
- *May only exist on a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_PREREQUISITE -->
- → `setUnitCombatHelp`

## `setBonusHelp`

- → `setBonusTradeHelp`

## `setBonusTradeHelp`

- <name of the thing>
- (%s2_player has %d1_num_bonus)  <!-- TXT_KEY_BONUSHELP_AVAILABLE_PLAYER -->
- (%s2_player is importing %d1_num_bonus)  <!-- TXT_KEY_BONUSHELP_IMPORTS_PLAYER -->
- (%s2_player is exporting %d1_num_bonus)  <!-- TXT_KEY_BONUSHELP_EXPORTS_PLAYER -->
- → `setYieldChangeHelp`
- *Revealed by %s1_TechName  <!-- TXT_KEY_BONUSHELP_REVEALED_BY -->
- Healthy: +%d1_Change<health>  <!-- TXT_KEY_BONUSHELP_HEALTHY -->
- Unhealthy: +%d1_Change<unhealth>  <!-- TXT_KEY_BONUSHELP_UNHEALTHY -->
- (with %s1_ImpName)  <!-- TXT_KEY_BONUSHELP_WITH_IMPROVEMENT -->
- Happy: +%d1_Change<happy>  <!-- TXT_KEY_BONUSHELP_HAPPY -->
- Unhappy: +%d1_Change<unhappy>  <!-- TXT_KEY_BONUSHELP_UNHAPPY -->
- Qualifies %s1_ImpName to exist on plot  <!-- TXT_KEY_BONUSHELP_OBSOLETED_VALIDATES_IMPROVEMENT -->
- *May only exist on a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_PREREQUISITE -->

## `setBuildingActualEffects`

- → `setResumableValueChangeHelp`
- → `setResumableGoodBadChangeHelp`
- → `setResumableYieldChangeHelp`
- → `setResumableCommerceTimes100ChangeHelp`

## `setBuildingAdditionalCommerceHelp`

- <name of the thing>
- → `setResumableValueTimes100ChangeHelp`

## `setBuildingAdditionalDefenseHelp`

- <name of the thing>
- → `setResumableValueChangeHelp`

## `setBuildingAdditionalGreatPeopleHelp`

- <name of the thing>
- → `setResumableValueChangeHelp`

## `setBuildingAdditionalHappinessHelp`

- <name of the thing>
- → `setResumableGoodBadChangeHelp`
- → `setResumableValueChangeHelp`

## `setBuildingAdditionalHealthHelp`

- <name of the thing>
- → `setResumableGoodBadChangeHelp`
- → `setResumableValueChangeHelp`

## `setBuildingAdditionalYieldHelp`

- <name of the thing>
- → `setResumableValueChangeHelp`

## `setBuildingHelp`

- <name of the thing>
- Disabled  <!-- TXT_KEY_HELPTEXT_BUILDING_DISABLED -->
- Replaced By  <!-- TXT_KEY_PEDIA_REPLACED_BY -->
- Religiously disabled  <!-- TXT_KEY_HELPTEXT_BUILDING_DISABLED_RELIGIOUSLY -->
- *This building is partialy disabled due to your State Religion and may be missing details here that would be available if your State Religion re-enables it. The Pedia will show those details.  <!-- TXT_KEY_REL_DISABLED -->
- This Building is Orbital, and so gives different benefits than normal buildings  <!-- TXT_KEY_ORBITAL -->
- This Building is Orbital Infrastructure  <!-- TXT_KEY_ORBITAL_INFRASTRUCTURE -->
- Cannot be built in a Holy City  <!-- TXT_KEY_NO_HOLY_CITY -->
- → `setYieldChangeHelp`
- → `setYieldPerPopChangeHelp`
- *-%s1 <gold> for building maintenance  <!-- TXT_KEY_BUILDINGHELP_MAINTENANCE -->
- Actual  <!-- TXT_KEY_ACTUAL_EFFECTS -->
- *World Wonder (%d1_Num Allowed)  <!-- TXT_KEY_BUILDINGHELP_WORLD_WONDER_ALLOWED -->
- (World Wonder: %d1_Num Left)  <!-- TXT_KEY_BUILDINGHELP_WORLD_WONDER_LEFT -->
- *Team Wonder (%d1_Num Allowed)  <!-- TXT_KEY_BUILDINGHELP_TEAM_WONDER_ALLOWED -->
- (Team Wonder: %d1_Num Left)  <!-- TXT_KEY_BUILDINGHELP_TEAM_WONDER_LEFT -->
- *National Wonder (%d1_Num Allowed)  <!-- TXT_KEY_BUILDINGHELP_NATIONAL_WONDER_ALLOWED -->
- (National Wonder: %d1_Num Left)  <!-- TXT_KEY_BUILDINGHELP_NATIONAL_WONDER_LEFT -->
- *Group Wonder of %s1_Special (%d2_Num Allowed)  <!-- TXT_KEY_BUILDINGHELP_GROUP_WONDER_ALLOWED -->
- (Group Wonder of %s1_Special: %d2_Num Left)  <!-- TXT_KEY_BUILDINGHELP_GROUP_WONDER_LEFT -->
- This building is automatically built by your citizens when its requirements are met  <!-- TXT_KEY_BUILDINGHELP_AUTO_BUILD -->
- *%D1_Change %F2 from  <!-- TXT_KEY_CIVICHELP_BUILDING_COMMERCE_CHANGE -->
- → `setListHelp`
- Per population  <!-- TXT_KEY_PER_POPULATION -->
- per City with <religion icon>  <!-- TXT_KEY_BUILDINGHELP_PER_CITY_WITH -->
- *Incorporates %s2_corporation  <!-- TXT_KEY_FOUNDS_CORPORATION -->
- *Provides %d1_Num %s3_BonusName (<icon>)  <!-- TXT_KEY_BUILDINGHELP_PROVIDES -->
- (Have %d1)  <!-- TXT_KEY_BONUSHELP_AVAILABLE_PLAYER_1 -->
- *Free %s2_Name in Every City  <!-- TXT_KEY_BUILDINGHELP_FREE_IN_CITY -->
- *Building Given for Free By  <!-- TXT_KEY_BUILDINGHELP_GIVEN_FREE -->
- Provides a free %s2_BuildingName in every city on the same continent.  <!-- TXT_KEY_BUILDINGHELP_FREE_IN_AREA -->
- *Building Given for Free in same continent By  <!-- TXT_KEY_BUILDINGHELP_GIVEN_FREE_AREA -->
- *Provides Fresh Water  <!-- TXT_KEY_BUILDINGHELP_PROVIDES_WATER -->
- *Expands the Workable Radius of the City to %d1  <!-- TXT_KEY_BUILDINGHELP_EXPANDS_WORKABLE_RADIUS -->
- *Adjacent Enemy units will receive %d1% damage per turn.  <!-- TXT_KEY_BUILDINGHELP_DAMAGES_ENEMY_UNITS -->
- *Nearby Battles will not affect Culture  <!-- TXT_KEY_BUILDINGHELP_PROTECTS_CULTURE -->
- *Citizens Will Rebel against enemy captors %d1% longer  <!-- TXT_KEY_BUILDINGHELP_OCCUPATION_TIME -->
- *Enemy Units will not be able to invade the city until the defenses have fallen to %d1%  <!-- TXT_KEY_BUILDINGHELP_NO_ENTRY -->
- *Each turn, %d1 unit(s) will be healed to full health.  <!-- TXT_KEY_BUILDINGHELP_FULL_HEAL_UNITS -->
- *Exerts a Zone Of Control on all adjacent tiles  <!-- TXT_KEY_BUILDINGHELP_ZONE_OF_CONTROL -->
- *May damage any unit as it attacks the city. %d1_Chance%% Chance to deal %d2_Damage%% Damage. (Chance modified by attacker's Dodge. Damage NOT modified by attacker's Armor.)  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ALL_ATTACKER_ARMOR_EXEMPT -->
- *May damage any unit as it attacks the city. %d1_Chance%% Chance to deal %d2_Damage%% Damage. (Chance modified by attacker's Dodge. Damage modified by attacker's Armor.)  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ALL_ATTACKER -->
- *May damage %s1_UnitCombat  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ATTACKER_START -->
- , %s1_UnitCombat  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ATTACKER_MIDDLE -->
- type units as they attack the city. %d1_Chance%% Chance to deal %d2_Damage%% Damage. (Chance modified by attacker's Dodge. Damage NOT modified by attacker's Armor.)  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ATTACKER_END_ARMOR_EXEMPT -->
- type units as they attack the city. %d1_Chance%% Chance to deal %d2_Damage%% Damage. (Chance modified by attacker's Dodge. Damage modified by attacker's Armor.)  <!-- TXT_KEY_BUILDINGHELP_DAMAGE_ATTACKER_END -->
- *Sets base Maximum Population at %d1_amount if the city doesn't already possess a greater base value.  <!-- TXT_KEY_BUILDINGHELP_MAX_POPULATION_ALLOWED -->
- *Adjusts Maximum Population by %D1_amount.  <!-- TXT_KEY_BUILDINGHELP_MAX_POPULATION_CHANGE -->
- *Can spawn a barbarian %s1_Unit.  <!-- TXT_KEY_BUILDINGHELP_PROPERTY_SPAWN_BARB -->
- *Can spawn a friendly %s1_Unit.  <!-- TXT_KEY_BUILDINGHELP_PROPERTY_SPAWN_FRIENDLY -->
- *Increases local Insidiousness by %s1_amount%% (Helps Criminals avoid discovery)  <!-- TXT_KEY_BUILDINGHELP_INSIDIOUSNESS -->
- *Increases local Investigation by %s1_amount%% (Helps Law Enforcement catch Criminals)  <!-- TXT_KEY_BUILDINGHELP_INVESTIGATION -->
- *When the city gets this building, its population adjusts by %d1_amount.  <!-- TXT_KEY_BUILDINGHELP_POPULATION_CHANGE -->
- *Employs %d1 Citizens.  <!-- TXT_KEY_BUILDINGHELP_POPULATION_EMPLOYED -->
- Per Citizen  <!-- TXT_KEY_MISC_PER_CITIZEN -->
- *%d1 local instability penalty per turn.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_LOCAL_PENALTY -->
- *%d1 local stability bonus per turn in each city.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_LOCAL_BONUS -->
- *%d1 national instability penalty per turn.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_NATIONAL_PENALTY -->
- *%d1 national stability bonus per turn.  <!-- TXT_KEY_CIVICHELP_REV_INDEX_NATIONAL_BONUS -->
- *%d1% to City Distance Instability penalty.  <!-- TXT_KEY_CIVICHELP_CITY_DISTANCE_GOOD_MOD -->
- *Enables all %s1_Name Civics  <!-- TXT_KEY_BUILDINGHELP_ENABLES_CIVICS -->
- *Free %s2_TechName  <!-- TXT_KEY_BUILDINGHELP_FREE_SPECIAL_TECH -->
- *Provides Power (<power>)  <!-- TXT_KEY_BUILDINGHELP_PROVIDES_POWER -->
- *Prevents Barbarians from Entering Borders on Continent  <!-- TXT_KEY_BUILDINGHELP_BORDER_OBSTACLE -->
- *Triggers %s1 Global Elections  <!-- TXT_KEY_BUILDINGHELP_DIPLO_VOTE -->
- *Guarantees Eligibility for Diplomatic Votes  <!-- TXT_KEY_BUILDINGHELP_ELECTION_ELIGIBILITY -->
- *Makes this City the Capital  <!-- TXT_KEY_BUILDINGHELP_CAPITAL -->
- *Reduces Maintenance in Nearby Cities  <!-- TXT_KEY_BUILDINGHELP_REDUCES_MAINTENANCE -->
- *Starts a Golden Age  <!-- TXT_KEY_BUILDINGHELP_GOLDEN_AGE -->
- Enables Nuclear Weapons for the Player that builds this  <!-- TXT_KEY_PROJECTHELP_ENABLES_NUKES -->
- *Centers World Map  <!-- TXT_KEY_BUILDINGHELP_CENTERS_MAP -->
- *No <unhappy> in this City  <!-- TXT_KEY_BUILDINGHELP_NO_UNHAPPY -->
- *No <unhealth> from Population  <!-- TXT_KEY_BUILDINGHELP_NO_UNHEALTHY_POP -->
- *No <unhealth> from Buildings  <!-- TXT_KEY_BUILDINGHELP_NO_UNHEALTHY_BUILDINGS -->
- *%D1_Mod%% <greatperson> Birth Rate  <!-- TXT_KEY_BUILDINGHELP_BIRTH_RATE_MOD -->
- *%D1_Mod%% Great General Emergence  <!-- TXT_KEY_BUILDINGHELP_GENERAL_RATE_MOD -->
- *%D1_Mod%% Great General Emergence inside Cultural Borders  <!-- TXT_KEY_DOMESTIC_GREAT_GENERAL_MODIFIER -->
- *%D1_Mod%% <greatperson> Birth Rate in All Cities  <!-- TXT_KEY_BUILDINGHELP_BIRTH_RATE_MOD_ALL_CITIES -->
- *%D1_Mod%% Anarchy Length  <!-- TXT_KEY_BUILDINGHELP_ANARCHY_MOD -->
- *%D1%% Wait Between Civics or State Religion changes  <!-- TXT_KEY_BUILDINGHELP_ANARCHY_TIMER_MOD -->
- *%D1_Mod%% Golden Age Length  <!-- TXT_KEY_BUILDINGHELP_GOLDENAGE_MOD -->
- *%D1_Mod%% Hurry Production Cost  <!-- TXT_KEY_BUILDINGHELP_HURRY_MOD -->
- *New Units Receive %D1_Change Experience Points  <!-- TXT_KEY_BUILDINGHELP_FREE_XP_UNITS -->
- *New Units Receive %D1_Change Experience Points in All Cities  <!-- TXT_KEY_BUILDINGHELP_FREE_XP_ALL_CITIES -->
- *Stores %d1_Mod%% of <food> after Growth  <!-- TXT_KEY_BUILDINGHELP_STORES_FOOD -->
- *Can Airlift %d1_Num Land [NUM1:Unit:Units] per Turn  <!-- TXT_KEY_BUILDINGHELP_AIRLIFT -->
- *%D1_Mod%% Damage from Air Units  <!-- TXT_KEY_BUILDINGHELP_AIR_DAMAGE_MOD -->
- *%D1 Air Unit Capacity  <!-- TXT_KEY_BUILDINGHELP_AIR_UNIT_CAPACITY -->
- *%D1_Mod%% Damage from Nukes  <!-- TXT_KEY_BUILDINGHELP_NUKE_DAMAGE_MOD -->
- *Chance of Nuclear Meltdown: %d1_chance in 10000 each round  <!-- TXT_KEY_BUILDINGHELP_NUKE_EXPLOSION_CHANCE -->
- *%D1_Change Free Specialists  <!-- TXT_KEY_BUILDINGHELP_FREE_SPECIALISTS -->
- *%D1_Change Free Specialists in All Cities on this Continent  <!-- TXT_KEY_BUILDINGHELP_FREE_SPECIALISTS_CONT -->
- *%D1_Change Free Specialists in All Cities  <!-- TXT_KEY_BUILDINGHELP_FREE_SPECIALISTS_ALL_CITIES -->
- *%D1_Mod%% Maintenance  <!-- TXT_KEY_BUILDINGHELP_MAINT_MOD -->
- *No maintenance costs in all cities  <!-- TXT_KEY_BUILDINGHELP_GLOBAL_MAINT -->
- *%D1_MOD%% Maintenance Costs in All Cities  <!-- TXT_KEY_BUILDINGHELP_GLOBAL_MAINT_MOD -->
- *%D1_Mod%% Maintenance Costs from Distance to Palace  <!-- TXT_KEY_BUILDINGHELP_DISTANCE_MAINT_MOD -->
- *%D1_Mod%% Maintenance Costs from Number of Cities  <!-- TXT_KEY_BUILDINGHELP_NO_MAINT_NUM_CITIES_MOD -->
- *%D1_Mod%% Maintenance Costs from Distance to Palace for Coastal Cities  <!-- TXT_KEY_COASTAL_DISTANCE_MAINT_MOD -->
- *No Maintenance Costs for Cities Connected to Capital  <!-- TXT_KEY_CONNECTED_CITY_MAINT -->
- *%D1_Mod%% Maintenance Costs for Cities Connected to Capital  <!-- TXT_KEY_CONNECTED_CITY_MAINT_MOD -->
- *No Maintenance Costs for All Cities on Continent  <!-- TXT_KEY_CONTINENTAL_CITY_MAINT -->
- *%D1_Mod%% Maintenance Costs for All Cities on Continent  <!-- TXT_KEY_CONTINENTAL_CITY_MAINT_MOD -->
- *No Maintenance Costs for Overseas Cities  <!-- TXT_KEY_OVERSEAS_CITY_MAINT -->
- *%D1_Mod%% Maintenance Costs for Overseas Cities  <!-- TXT_KEY_OVERSEAS_CITY_MAINT_MOD -->
- *%D1_percent%% Anger Duration from Sacrificing Population  <!-- TXT_KEY_BUILDINGHELP_HURRY_ANGER_MOD -->
- *Increases City Line-Of-Sight  <!-- TXT_KEY_BUILDINGHELP_INCREASES_LINE_OF_SIGHT -->
- *Decreases City Line-Of-Sight  <!-- TXT_KEY_BUILDINGHELP_DECREASES_LINE_OF_SIGHT -->
- *%d1%% Inflation  <!-- TXT_KEY_ADJUSTS_INFLATION -->
- *%D1_Mod%% War <unhappy>  <!-- TXT_KEY_BUILDINGHELP_WAR_WEAR_MOD -->
- *%D1_Mod%% War <unhappy> in All Cities  <!-- TXT_KEY_BUILDINGHELP_WAR_WEAR_MOD_ALL_CITIES -->
- *Enemies suffer %D1_percent%% War <unhappy>  <!-- TXT_KEY_BUILDINGHELP_ENEMY_WAR_WEAR -->
- *Heals Units Extra %d1_Mod%% Damage/Turn  <!-- TXT_KEY_BUILDINGHELP_HEAL_MOD -->
- *%s1_UNITCOMBAT %D2_Mod%%/Turn  <!-- TXT_KEY_BUILDINGHELP_HEAL_UNITCOMBAT_MOD -->
- *%D1_Change%F2_HealthOrUn in All Cities on this Continent  <!-- TXT_KEY_BUILDINGHELP_HEALTH_CHANGE_CONT -->
- *%D1_Change%F2_HealthOrUn in All Cities  <!-- TXT_KEY_BUILDINGHELP_HEALTH_CHANGE_ALL_CITIES -->
- *%D1_Change%F2_HappyOrUn in All Cities on this Continent  <!-- TXT_KEY_BUILDINGHELP_HAPPY_CHANGE_CONT -->
- *%D1_Change%F2_HappyOrUn in All Cities  <!-- TXT_KEY_BUILDINGHELP_HAPPY_CHANGE_ALL_CITIES -->
- *%D1_Change%F2_HappyOrUn if <religion icon> is State <religion>  <!-- TXT_KEY_BUILDINGHELP_RELIGION_HAPPINESS -->
- *Workers Build Improvements %D1_Mod%% Faster  <!-- TXT_KEY_BUILDINGHELP_WORKER_MOD -->
- *%D1_Mod%% Military Unit Production  <!-- TXT_KEY_BUILDINGHELP_MILITARY_MOD -->
- *%D1_Mod%% Spaceship Production  <!-- TXT_KEY_BUILDINGHELP_SPACESHIP_MOD -->
- *%D1_Mod%% Spaceship Production in All Cities  <!-- TXT_KEY_BUILDINGHELP_SPACESHIP_MOD_ALL_CITIES -->
- *%D1_Change Trade Routes  <!-- TXT_KEY_BUILDINGHELP_TRADE_ROUTES -->
- *%D1_Change Trade Routes in All Coastal Cities  <!-- TXT_KEY_BUILDINGHELP_COASTAL_TRADE_ROUTES -->
- *%D1_Change Trade Routes in All Cities  <!-- TXT_KEY_BUILDINGHELP_TRADE_ROUTES_ALL_CITIES -->
- *%D1_Mod%% Trade Route Yield  <!-- TXT_KEY_BUILDINGHELP_TRADE_ROUTE_MOD -->
- *%D1_Mod%% Foreign Trade Route Yield  <!-- TXT_KEY_BUILDINGHELP_FOREIGN_TRADE_ROUTE_MOD -->
- *%D1_Change Population in All Cities  <!-- TXT_KEY_BUILDINGHELP_GLOBAL_POP -->
- *1 Free Technology  <!-- TXT_KEY_BUILDINGHELP_FREE_TECH -->
- *%d1_Num Free Technologies  <!-- TXT_KEY_BUILDINGHELP_FREE_TECHS -->
- *%d1_Mod%% Defense (Not vs. high explosives)  <!-- TXT_KEY_BUILDINGHELP_DEFENSE_MOD -->
- *%D1_Mod%% Less Damage to Defenses (Not vs. high explosives)  <!-- TXT_KEY_BUILDINGHELP_BOMBARD_DEFENSE_MOD -->
- *%d1_Mod%% Defense in All Cities  <!-- TXT_KEY_BUILDINGHELP_DEFENSE_MOD_ALL_CITIES -->
- *%D1_Mod%% Defense against Espionage  <!-- TXT_KEY_BUILDINGHELP_ESPIONAGE_DEFENSE_MOD -->
- *Helps Thwart Rival Spies  <!-- TXT_KEY_UNITHELP_EXPOSE_SPIES -->
- *%D1_Amount Aid in Overcoming %s2_Type Afflictions within the City  <!-- TXT_KEY_BUILDINGHELP_AID_RATE -->
- *Cities trading with this one are +%D1_Amount%% more likely to catch this disease.  <!-- TXT_KEY_BUILDINGHELP_TRADE_COMMUNICABILITY_ADDED -->
- *Cities trading with this one are %D1_Amount%% less likely to catch this disease.  <!-- TXT_KEY_BUILDINGHELP_TRADE_COMMUNICABILITY_REDUCED -->
- *This is a disease which equates to the %s1_Affliction Unit Affliction.  <!-- TXT_KEY_BUILDINGHELP_DISEASE_TYPE -->
- *Access to %s1_Bonus offers a +%d2 Aid Bonus to help Overcome %s2_Property Afflictions from this building.  <!-- TXT_KEY_BUILDINGHELP_BONUS_AID_MODIFIER -->
- *Adjusts the base outbreak threshold of %s1_Affliction by %d2_Amount.  <!-- TXT_KEY_BUILDINGHELP_AFFLICTION_OUTBREAK_LEVEL_CHANGE -->
- *Discovering %s1_Tech reduces the outbreak threshold and increases recovery threshold by %d2_Amount.  <!-- TXT_KEY_BUILDINGHELP_TECH_OUTBREAK_LEVEL_CHANGE -->
- *As long as this building is active, its owning player gains the %s1_Trait Trait.  <!-- TXT_KEY_BUILDINGHELP_FREE_TRAIT -->
- *Free %s2 promotion for  <!-- TXT_KEY_BUILDINGHELP_FREE_PROMO_CONDITION -->
- new units that are  <!-- TXT_KEY_BUILDINGHELP_FREE_PROMO_CONDITION_ADDON -->
- → `buildDisplayString`
- *Builds %s1_Unit_Combat %d2_Amount%% Faster  <!-- TXT_KEY_BUILDINGHELP_UNIT_COMBAT_PROD_POSITIVE_MODIFIER -->
- *Builds %s1_Unit_Combat %d2_Amount%% Slower  <!-- TXT_KEY_BUILDINGHELP_UNIT_COMBAT_PROD_NEGATIVE_MODIFIER -->
- *%d1_Amount%% additional Repel value to local defending %s2_Unit_Combat.  <!-- TXT_KEY_BUILDINGHELP_UNIT_COMBAT_REPEL_MODIFIER -->
- *Units defending the city gain %D1_Amount%% additional Repel value against attacking %s2_Unit_Combat.  <!-- TXT_KEY_BUILDINGHELP_UNIT_COMBAT_REPEL_AGAINST_MODIFIER -->
- *Units defending the city gain a %D1_Amount%% Combat Modifier against attacking %s2_Unit_Combat.  <!-- TXT_KEY_BUILDINGHELP_UNIT_COMBAT_DEFENSE_AGAINST_MODIFIER -->
- *+%d1_Amount%% Support Strength from any Front Support Unit aiding in city defense.  <!-- TXT_KEY_BUILDINGHELP_FRONT_SUPPORT_PERCENT_MODIFIER -->
- *+%d1_Amount%% Support Strength from any Short Range Support Unit aiding in city defense.  <!-- TXT_KEY_BUILDINGHELP_SHORT_RANGE_SUPPORT_PERCENT_MODIFIER -->
- *+%d1_Amount%% Support Strength from any Medium Range Support Unit aiding in city defense.  <!-- TXT_KEY_BUILDINGHELP_MEDIUM_RANGE_SUPPORT_PERCENT_MODIFIER -->
- *+%d1_Amount%% Support Strength from any Long Range Support Unit aiding in city defense.  <!-- TXT_KEY_BUILDINGHELP_LONG_RANGE_SUPPORT_PERCENT_MODIFIER -->
- *+%d1_Amount%% Support Strength from any Flank Support Unit aiding in city defense.  <!-- TXT_KEY_BUILDINGHELP_FLANK_SUPPORT_PERCENT_MODIFIER -->
- River Tiles  <!-- TXT_KEY_BUILDINGHELP_RIVER_PLOTS -->
- All Cities Water Tiles  <!-- TXT_KEY_BUILDINGHELP_WATER_PLOTS_ALL_CITIES -->
- with Power (<power>)  <!-- TXT_KEY_BUILDINGHELP_WITH_POWER -->
- All Cities This Continent  <!-- TXT_KEY_BUILDINGHELP_ALL_CITIES_THIS_CONTINENT -->
- All Cities  <!-- TXT_KEY_BUILDINGHELP_ALL_CITIES -->
- per Specialist in All Cities  <!-- TXT_KEY_BUILDINGHELP_PER_SPECIALIST_ALL_CITIES -->
- from All <religion icon> Buildings  <!-- TXT_KEY_BUILDINGHELP_FROM_ALL_REL_BUILDINGS -->
- from All State <religion> Buildings  <!-- TXT_KEY_BUILDINGHELP_STATE_REL_BUILDINGS -->
- *+1%F1_HappyOrUn per %d2_Percent%% <icon> Rate  <!-- TXT_KEY_BUILDINGHELP_PER_LEVEL -->
- *Can Adjust <commerce icon> Rate  <!-- TXT_KEY_BUILDINGHELP_ADJUST_COMM_RATE -->
- ,  <!-- TXT_KEY_COMMA -->
- from %s2_SpclstName in All Cities  <!-- TXT_KEY_BUILDINGHELP_FROM_IN_ALL_CITIES -->
- from Local %s2_SpclstName  <!-- TXT_KEY_BUILDINGHELP_FROM_SPECIALIST -->
- with %s2_Bonus  <!-- TXT_KEY_BUILDINGHELP_WITH_BONUS -->
- In City Vicinity  <!-- TXT_KEY_IN_CITY_VICINITY -->
- Peak Plots  <!-- TXT_KEY_PLOTS_PEAK -->
- Hill Plots  <!-- TXT_KEY_PLOTS_HILL -->
- Flatland Plots  <!-- TXT_KEY_PLOTS_FLATLAND -->
- Water Plots  <!-- TXT_KEY_PLOTS_WATER -->
- *%F1 spread change: %D2  <!-- TXT_KEY_BUILDINGHELP_SPREADS_RELIGION -->
- *Can turn 1 Citizen into %s2_SpecName  <!-- TXT_KEY_BUILDINGHELP_TURN_CITIZEN_INTO -->
- *Can turn %d1_Num Citizens into %s3_SpclstName  <!-- TXT_KEY_BUILDINGHELP_TURN_CITIZENS_INTO -->
- *%D1_Change Free %s3_SpclstName  <!-- TXT_KEY_BUILDINGHELP_FREE_SPECIALIST -->
- *%D1_Change Free [NUM1:Specialist:Specialists] per  <!-- TXT_KEY_BUILDINGHELP_IMPROVEMENT_FREE_SPECIALISTS -->
- *%D1_Change%F2_HappyOrHealth from  <!-- TXT_KEY_BUILDINGHELP_HEALTH_HAPPINESS_CHANGE -->
- with  <!-- TXT_KEY_WITH -->
- *%D1_Change%F2_HappyOrHealth with  <!-- TXT_KEY_BUILDINGHELP_CIVIC_HEALTH_HAPPINESS_CHANGE -->
- *New %s1_UnitName Receive %D2_Change Experience Points  <!-- TXT_KEY_BUILDINGHELP_FREE_XP -->
- *Builds %s1_UnitName %D2_Mod%% Faster  <!-- TXT_KEY_BUILDINGHELP_BUILDS_FASTER_DOMAIN -->
- * Opens trade routes to all other players  <!-- TXT_KEY_BUILDINGHELP_ALL_TRADE -->
- * +%d1 Trade Routes in the World  <!-- TXT_KEY_BUILDINGHELP_MORE_WORLD_TRADE -->
- * -%d1 Trade Routes in the World  <!-- TXT_KEY_BUILDINGHELP_LESS_WORLD_TRADE -->
- %Faster Training of  <!-- TXT_KEY_UNITHELP_CLASS_PRODUCTION_FAST_MOD -->
- %Slower Training of  <!-- TXT_KEY_UNITHELP_CLASS_PRODUCTION_SLOW_MOD -->
- % Faster Construction of  <!-- TXT_KEY_CIVICHELP_BUILDING_PRODUCTION_MOD -->
- % Slower Construction of  <!-- TXT_KEY_CIVICHELP_BUILDING_PRODUCTION_SLOW -->
- *National %d1%% cost modifier:  <!-- TXT_KEY_BUILDINGHELP_GLOBAL_BUILDINGCOST_MOD -->
- *  <!-- TXT_KEY_HELP_LIST -->
- in all cities  <!-- TXT_KEY_BUILDINGHELP_GLOBAL -->
- *City Require %d1%% More <food> to Grow  <!-- TXT_KEY_BUILDINGHELP_CITY_SLOW_GROWTH_SPEED -->
- *City Require %d1%% Less <food> to Grow  <!-- TXT_KEY_BUILDINGHELP_CITY_FAST_GROWTH_SPEED -->
- → `buildChangesString`
- → `buildChangesAllCitiesString`
- → `buildRequiresMinString`
- → `buildRequiresMaxString`
- Replaces  <!-- TXT_KEY_BUILDINGHELP_REPLACED_BY_BUILDING -->
- % City Defense with access to  <!-- TXT_KEY_BUILDINGHELP_BONUS_DEFENSE_CHANGE -->
- % Strength to defending  <!-- TXT_KEY_BUILDINGHELP_UNITCOMBAT_EXTRA_STRENGTH -->
- in the city.  <!-- TXT_KEY_BUILDINGHELP_IN_CITY -->
- *%D1_Change<icon> from  <!-- TXT_KEY_CIVICHELP_IMPROVEMENT_YIELD_CHANGE -->
- *Required to Train  <!-- TXT_KEY_BUILDINGHELP_REQUIRED_TO_TRAIN -->
- *May be needed to Train  <!-- TXT_KEY_BUILDINGHELP_NEEDED_TO_TRAIN -->
- *Can be built by  <!-- TXT_KEY_UNITHELP_REQUIRED_TO_BUILD -->
- *%D1_Change%F2_HappOrUn from all  <!-- TXT_KEY_BUILDINGHELP_HAPPINESS_CHANGE -->
- *Provides Power with %s2_BonusName  <!-- TXT_KEY_BUILDINGHELP_PROVIDES_POWER_WITH -->
- *Required to Build  <!-- TXT_KEY_BUILDINGHELP_REQUIRED_TO_BUILD -->
- *May be needed to Build  <!-- TXT_KEY_BUILDINGHELP_NEEDED_TO_BUILD -->
- *Needed to Build anywhere in empire:  <!-- TXT_KEY_BUILDINGHELP_NEEDED_TO_BUILD_ANYWHERE -->
- *%D1_Mod%% <gold> to Upgrade Units  <!-- TXT_KEY_BUILDINGHELP_UNIT_UPGRADE_COST_MOD -->
- *Units have a %D1%% chance to Capture.  <!-- TXT_KEY_NATIONAL_CAPTURE_PROBABILITY_MODIFIER -->
- *Units have a %D1%% chance to avoid Capture.  <!-- TXT_KEY_NATIONAL_CAPTURE_RESISTANCE_MODIFIER -->
- *Units in the city gain a %D1%% chance to Capture.  <!-- TXT_KEY_LOCAL_CAPTURE_PROBABILITY_MODIFIER -->
- *Units in the city gain a %D1%% chance to avoid Capture.  <!-- TXT_KEY_LOCAL_CAPTURE_RESISTANCE_MODIFIER -->
- *Units in the city gain %D1%% vs Surround and Destroy.  <!-- TXT_KEY_LOCAL_DYNAMIC_DEFENSE -->
- *Units attacking the city from across a river are %D1%% less penalized than normal.  <!-- TXT_KEY_RIVER_DEFENSE_PENALTY_POSITIVE -->
- *Units attacking the city from a river are %D1%% more penalized than normal.  <!-- TXT_KEY_RIVER_DEFENSE_PENALTY_NEGATIVE -->
- *Units gain %D1%% Repel when defending the city.  <!-- TXT_KEY_CITY_REPEL -->
- *%D1%% to the Minimum Defensive value of the city.  <!-- TXT_KEY_MIN_DEFENSE -->
- *Building recovers its own defensive values %d1%% faster.  <!-- TXT_KEY_BUILDINGHELP_DEFENSE_RECOVERY_SPEED_MODIFIER_POSITIVE -->
- *Building recovers its own defensive values %d1%% slower.  <!-- TXT_KEY_BUILDINGHELP_DEFENSE_RECOVERY_SPEED_MODIFIER_NEGATIVE -->
- *City recovers its defensive values %d1%% faster.  <!-- TXT_KEY_CITY_DEFENSE_RECOVERY_SPEED_MODIFIER_POSITIVE -->
- *City recovers its defensive values %d1%% slower.  <!-- TXT_KEY_CITY_DEFENSE_RECOVERY_SPEED_MODIFIER_NEGATIVE -->
- *Double production speed for %s1_trait leaders  <!-- TXT_KEY_DOUBLE_SPEED_TRAIT -->
- *%D1_change%% production speed for %s2_trait leaders  <!-- TXT_KEY_PRODUCTION_MODIFIER_TRAIT -->
- *%D1_change<happy> for %s2_trait leaders  <!-- TXT_KEY_BUILDINGHELP_HAPPINESS_TRAIT -->
- *City more likely to generate %s2_GPName  <!-- TXT_KEY_BUILDINGHELP_LIKELY_TO_GENERATE -->
- *Available for free on %s1_EraName and later starts  <!-- TXT_KEY_BUILDINGHELP_FREE_START_ERA -->
- → `buildBuildingRequiresString`
- *Exists below %D1  <!-- TXT_KEY_PROPERTY_BUILDING_DISPLAY_UNDER -->
- *Exists above %D1  <!-- TXT_KEY_PROPERTY_BUILDING_DISPLAY_OVER -->
- *Exists between %D1 and %D2  <!-- TXT_KEY_PROPERTY_BUILDING_DISPLAY -->
- %d1_Date BC  <!-- TXT_KEY_TIME_BC -->
- %d1_Date AD  <!-- TXT_KEY_TIME_AD -->
- Built in %s1_year  <!-- TXT_KEY_BUG_YEAR_BUILT -->
- *Double %s1_commerce_type next year  <!-- TXT_KEY_BUG_DOUBLE_COMMERCE_NEXT_YEAR -->
- *Double %s1_commerce_type in %d2 years  <!-- TXT_KEY_BUG_DOUBLE_COMMERCE_YEARS -->
- *%d1 years of history have doubled the %s2_commerce_type  <!-- TXT_KEY_BUG_DOUBLE_COMMERCE_COMPLETE -->
- This city's current culture level can only support %d1_Num World Wonders.  <!-- TXT_KEY_BUILDINGHELP_WORLD_WONDERS_PER_CITY -->
- This city's current culture level can only support %d1_Num Team Wonders.  <!-- TXT_KEY_BUILDINGHELP_TEAM_WONDERS_PER_CITY -->
- This city's current culture level can only support %d1_Num National Wonders.  <!-- TXT_KEY_BUILDINGHELP_NATIONAL_WONDERS_PER_CITY -->
- %d1_Num Turns  <!-- TXT_KEY_BUILDINGHELP_NUM_TURNS -->
- Will lose %d1<hammer> if not produced this turn  <!-- TXT_KEY_PRODUCTION_DECAY -->
- Will lose %d1<hammer> if delayed for %d2 [NUM2:turn:turns]  <!-- TXT_KEY_PRODUCTION_DECAY_TURNS -->
-   <!-- TXT_KEY_COLOR_POSITIVE -->
- Double production speed with %s2_Bonus  <!-- TXT_KEY_BUILDINGHELP_DOUBLE_SPEED_WITH -->
- Builds %d1_Mod%% faster with %s3_Bonus  <!-- TXT_KEY_BUILDINGHELP_BUILDS_FASTER_WITH -->
- Obsolete with %s2_TechName  <!-- TXT_KEY_BUILDINGHELP_OBSOLETE_WITH -->
- (Turns into %s2)  <!-- TXT_KEY_BUILDINGHELP_OBSOLETE_WITH_TO -->
- Sid's Tips:  <!-- TXT_KEY_SIDS_TIPS -->
- *May only exist on a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_PREREQUISITE -->

## `setBuildingSavedMaintenanceHelp`

- <name of the thing>
- → `setResumableValueTimes100ChangeHelp`

## `setCityBarHelp`

- %d1 <food>/TurnGrowing: %d2/%d3 <food> (%d4 [NUM4:Turn:Turns])  <!-- TXT_KEY_CITY_BAR_FOOD_GROW -->
- Stagnant: %d1/%d2 <food>  <!-- TXT_KEY_CITY_BAR_FOOD_STAGNATE -->
- %d1 <bad_food>/TurnShrinking: %d2/%d3 <food> (%d4 [NUM4:Turn:Turns])  <!-- TXT_KEY_CITY_BAR_FOOD_SHRINK -->
- %d1 <bad_food>/TurnSTARVATION: %d2/%d3 <food>  <!-- TXT_KEY_CITY_BAR_FOOD_STARVE -->
- Growth: %d1/%d2 <food>  <!-- TXT_KEY_CITY_BAR_GROWTH -->
- %d1 <food>/TurnGrowth: %d2/%d3 <food> (%d4 [NUM4:Turn:Turns])  <!-- TXT_KEY_CITY_BAR_FOOD_GROWTH -->
- %d1 <food> + %d2 <hammer>/Turn (%d7 <hammer>/Turn)%s3: %d4/%d5 <hammer> (%d6 [NUM6:Turn:Turns])  <!-- TXT_KEY_CITY_BAR_FOOD_HAMMER_PRODUCTION_WITH_BASE -->
- %d1 <food> + %d2 <hammer>/Turn%s3: %d4/%d5 <hammer> (%d6 [NUM6:Turn:Turns])  <!-- TXT_KEY_CITY_BAR_FOOD_HAMMER_PRODUCTION -->
- %d1 <hammer>/Turn (%d6 <hammer>/Turn)%s2: %d3/%d4 <hammer> (%d5 [NUM5:Turn:Turns])  <!-- TXT_KEY_CITY_BAR_HAMMER_PRODUCTION_WITH_BASE -->
- %d1 <hammer>/Turn%s2: %d3/%d4 <hammer> (%d5 [NUM5:Turn:Turns])  <!-- TXT_KEY_CITY_BAR_HAMMER_PRODUCTION -->
- %s1: %d2/%d3 <hammer>  <!-- TXT_KEY_CITY_BAR_PRODUCTION -->
- %d1 <hammer> (%d2 <hammer>/Turn)  <!-- TXT_KEY_CITY_BAR_BASE_PRODUCTION_WITH_OVERFLOW -->
- %d1 <hammer>/Turn  <!-- TXT_KEY_CITY_BAR_BASE_PRODUCTION -->
- → `setListHelp`
- /Turn  <!-- TXT_KEY_PER_TURN -->
- <name of the thing>
- <culture>: %s1/%d2 (%s3: Lvl %d4)  <!-- TXT_KEY_CITY_BAR_CULTURE -->
- <culture>: %s1 (%s2: Lvl %d3)  <!-- TXT_KEY_CITY_BAR_CULTURE_MAX -->
- <greatperson>: %d1/%d2  <!-- TXT_KEY_CITY_BAR_GREAT_PEOPLE -->
- ,  <!-- TXT_KEY_COMMA -->
- (%d1)  <!-- TXT_KEY_INTERFACE_CITY_BAR_SPECIALIST_ADDENDUM -->
- Air Unit Capacity: %d1/%d2  <!-- TXT_KEY_CITY_BAR_AIR_UNIT_CAPACITY -->
- Revolt %%/Turn: %s1_Chance (%s2_CityStrength base: x%s3_SpeedAdjustment gamespeed, x%s4_Garrison units)  <!-- TXT_KEY_MISC_CHANCE_OF_REVOLT -->
- → `buildDisplayString`
- Hold &lt;CTRL&gt; to see base values.  <!-- TXT_KEY_CITY_BAR_CTRL_BASE_VALUES -->
- Left-Click to select %s1  <!-- TXT_KEY_CITY_BAR_SELECT -->
- (&lt;CTRL&gt; for All Cities on this Continent)  <!-- TXT_KEY_CITY_BAR_SELECT_CTRL -->
- (&lt;ALT&gt; for All Cities in the World)  <!-- TXT_KEY_CITY_BAR_SELECT_ALT -->

## `setCombatPlotHelp`

- Breakdown: %d1_chance%% chance per round of reducing city defenses by %d2_damage%%. Opponent Repel values modify this Chance. Damage modified by City Bombard Defense.  <!-- TXT_KEY_COMBAT_BREAKDOWN_EFFECTS -->
- Combat Odds: %s1%%  <!-- TXT_KEY_COMBAT_PLOT_ODDS -->
- Retreat odds: %s1%%  <!-- TXT_KEY_COMBAT_PLOT_ODDS_RETREAT -->
- Defender Retreat odds: %s1%%  <!-- TXT_KEY_COMBAT_PLOT_ODDS_DEFENDER_RETREAT -->
- Attacker Repelled odds: %s1%%  <!-- TXT_KEY_COMBAT_PLOT_ODDS_REPEL -->
- Defender Knocked Back odds: %s1%%  <!-- TXT_KEY_COMBAT_PLOT_ODDS_KNOCKBACK -->
- Attacker Precision:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_ATTACKER_PRECISION -->
- Defender Dodge:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_DEFENDER_DODGE -->
- Attacker to Hit Modifier/Round:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_ATTACKER_HIT_MODIFIER -->
- Defender Precision:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_DEFENDER_PRECISION -->
- Attacker Dodge:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_ATTACKER_DODGE -->
- Defender to Hit Modifier/Round:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_DEFENDER_HIT_MODIFIER -->
- Attacker Armor:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_ATTACKER_ARMOR -->
- Defender Puncture:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_DEFENDER_PUNCTURE -->
- Total Attacker Armor:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_TOTAL_ATTACKER_ARMOR -->
- Defender Armor:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_DEFENDER_ARMOR -->
- Attacker Puncture:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_ATTACKER_PUNCTURE -->
- Total Defender Armor:  <!-- TXT_KEY_COMBAT_ARMOR_COMPARE_TOTAL_DEFENDER_ARMOR -->
- *Attacker Damage Modifier: %D1_Bonus%%  <!-- TXT_KEY_COMBAT_ATTACKER_DAMAGE_MODIFIER -->
- *Defender Damage Modifier: %D1_Bonus%%  <!-- TXT_KEY_COMBAT_DEFENDER_DAMAGE_MODIFIER -->
- Attacker Withdrawal:  <!-- TXT_KEY_COMBAT_WITHDRAW_ODDS_ATTACKER_WITHDRAWAL -->
- Reflexes:  <!-- TXT_KEY_COMBAT_WITHDRAW_ODDS_ATTACKER_REFLEXES -->
- Frayed:  <!-- TXT_KEY_COMBAT_WITHDRAW_ODDS_ATTACKER_FRAYED -->
- Defender Pursuit:  <!-- TXT_KEY_COMBAT_WITHDRAW_ODDS_DEFENDER_PURSUIT -->
- Withdraw Odds/rnd Attempted:  <!-- TXT_KEY_COMBAT_WITHDRAW_ODDS_PER_ROUND_ATTEMPTED -->
- Attacker Early Withdraw:  <!-- TXT_KEY_COMBAT_WITHDRAW_ODDS_ATTACKER_EARLY_WITHDRAW -->
- Defender Withdrawal:  <!-- TXT_KEY_COMBAT_WITHDRAW_ODDS_DEFENDER_WITHDRAWAL -->
- Attacker Pursuit:  <!-- TXT_KEY_COMBAT_WITHDRAW_ODDS_ATTACKER_PURSUIT -->
- Defender Early Withdraw:  <!-- TXT_KEY_COMBAT_WITHDRAW_ODDS_DEFENDER_EARLY_WITHDRAW -->
- Defender Fortification:  <!-- TXT_KEY_COMBAT_FORTIFICATION_COMPARE_DEFENDER_FORTIFICATION -->
- and Fortified Repel:  <!-- TXT_KEY_COMBAT_FORTIFICATION_COMPARE_FORTIFIED_REPEL -->
- vs Attacker Overrun:  <!-- TXT_KEY_COMBAT_FORTIFICATION_COMPARE_ATTACKER_OVERRUN -->
- Defender Repel:  <!-- TXT_KEY_COMBAT_FORTIFICATION_COMPARE_DEFENDER_REPEL -->
- vs Unyielding:  <!-- TXT_KEY_COMBAT_FORTIFICATION_COMPARE_ATTACKER_UNYIELDING -->
- Total Repel:  <!-- TXT_KEY_COMBAT_FORTIFICATION_COMPARE_TOTAL_REPEL -->
- Total Fortification:  <!-- TXT_KEY_COMBAT_FORTIFICATION_COMPARE_TOTAL_FORTIFICATION -->
- Attacker Knockback:  <!-- TXT_KEY_COMBAT_KNOCKBACK_ATTACKER_KNOCKBACK -->
- vs Defender Unyielding:  <!-- TXT_KEY_COMBAT_KNOCKBACK_DEFENDER_UNYIELDING -->
- Total Knockback:  <!-- TXT_KEY_COMBAT_KNOCKBACK_TOTAL_KNOCKBACK -->
- Chance to Stun is % of damage dealt each round  <!-- TXT_KEY_COMBAT_ROUND_STUN_START -->
- Attacker Round Stun Chance:  <!-- TXT_KEY_COMBAT_ROUND_STUN_TOTAL_ATTACKER_ROUND_STUN -->
- Modified by Defender Combat Class:  <!-- TXT_KEY_COMBAT_ROUND_STUN_VS_DEFENDER_COMBAT_CLASS -->
- Plus Critical Modifier:  <!-- TXT_KEY_COMBAT_ROUND_STUN_PLUS_CRITICAL -->
- Plus Electrical Damage Modifier:  <!-- TXT_KEY_COMBAT_ROUND_STUN_PLUS_ELECTRICAL -->
- Minus Defender Endurance Modifier:  <!-- TXT_KEY_COMBAT_ROUND_STUN_ATTACKER_MINUS_DEFENDER_ENDURANCE -->
- Total Attacker Chance for Round Stun:  <!-- TXT_KEY_COMBAT_ROUND_STUN_ATTACKER_GRAND_TOTAL -->
- Defender Round Stun Chance:  <!-- TXT_KEY_COMBAT_ROUND_STUN_TOTAL_DEFENDER_ROUND_STUN -->
- Modified by Attacker Combat Class:  <!-- TXT_KEY_COMBAT_ROUND_STUN_VS_ATTACKER_COMBAT_CLASS -->
- TBD  <!-- TXT_KEY_COMBAT_ROUND_STUN_DEFENDER_MINUS_ATTACKER_ENDURANCE -->
- Total Defender Chance for Round Stun:  <!-- TXT_KEY_COMBAT_ROUND_STUN_DEFENDER_GRAND_TOTAL -->
- (key not in current GameText)  <!-- TXT_KEY_COMBAT_ADJUST_RAGE_PER_ROUND -->
- Fights to Death  <!-- TXT_KEY_COMBAT_FIGHT_TO_DEATH -->
- Repeatedly attacks until damaged  <!-- TXT_KEY_COMBAT_ATTACK_UNTIL_DAMAGED -->
- Your Attacks inflict Cold Damage  <!-- TXT_KEY_COMBAT_ATTACKER_COLD_DAMAGE -->
- Defender Attacks inflict Cold Damage  <!-- TXT_KEY_COMBAT_DEFENDER_COLD_DAMAGE -->
- Attacker May Afflict:  <!-- TXT_KEY_COMBAT_ATTACKER_MAY_AFFLICT -->
- <name of the thing>
- Chance  <!-- TXT_KEY_COMBAT_MAY_AFFLICT2 -->
- Tolerance:  <!-- TXT_KEY_COMBAT_TOLERANCE -->
- Opponent Fortitude = Total Chance:  <!-- TXT_KEY_COMBAT_MAY_AFFLICT3 -->
- Will check affliction the first round enemy is damaged.  <!-- TXT_KEY_COMBAT_MAY_AFFLICT_IMMEDIATE -->
- Will check affliction after battle if the enemy is damaged.  <!-- TXT_KEY_COMBAT_MAY_AFFLICT_DELAYED -->
- Defender May Afflict:  <!-- TXT_KEY_COMBAT_DEFENDER_MAY_AFFLICT -->
- Base Chance:  <!-- TXT_KEY_COMBAT_BASE_CHANCE -->
- Fortitude:  <!-- TXT_KEY_COMBAT_FORTITUDE -->
- Aid present:  <!-- TXT_KEY_COMBAT_AID_PRESENT -->
- Misc Mods:  <!-- TXT_KEY_COMBAT_MISC_MODS -->
- Attacker Critical Modifier:  <!-- TXT_KEY_COMBAT_ATTACKER_CRITICAL_MODIFIER -->
- Attacker Chance to Inflict Critical per Hit:  <!-- TXT_KEY_COMBAT_ATTACKER_CHANCE_CRITICAL -->
- Defender Critical Modifier:  <!-- TXT_KEY_COMBAT_DEFENDER_CRITICAL_MODIFIER -->
- Defender Chance to Inflict Critical per Hit:  <!-- TXT_KEY_COMBAT_DEFENDER_CHANCE_CRITICAL -->
- Attacker Power Shots:  <!-- TXT_KEY_COMBAT_ATTACKER_POWER_SHOTS -->
- Combat Modifier during Power Shots:  <!-- TXT_KEY_COMBAT_ATTACKER_POWER_SHOTS_COMBAT_MODIFIER -->
- Puncture Modifier during Power Shots:  <!-- TXT_KEY_COMBAT_ATTACKER_POWER_SHOTS_PUNCTURE_MODIFIER -->
- Precision Modifier during Power Shots:  <!-- TXT_KEY_COMBAT_ATTACKER_POWER_SHOTS_PRECISION_MODIFIER -->
- Critical Modifier during Power Shots:  <!-- TXT_KEY_COMBAT_ATTACKER_POWER_SHOTS_CRITICAL_MODIFIER -->
- Defender Power Shots:  <!-- TXT_KEY_COMBAT_DEFENDER_POWER_SHOTS -->
- Total Attacker Support Strength:  <!-- TXT_KEY_COMBAT_TOTAL_ATTACKER_SUPPORT -->
- Total Defender Support Strength:  <!-- TXT_KEY_COMBAT_TOTAL_DEFENDER_SUPPORT -->
-   <!-- TXT_KEY_COLOR_POSITIVE -->
- → `setUnitHelp`
- *First Strikes: %d1  <!-- TXT_KEY_UNITHELP_FIRST_STRIKES -->
- *First Strikes: %d1-%d2  <!-- TXT_KEY_UNITHELP_FIRST_STRIKE_CHANCES -->
- *STEALTH ATTACK!: %d1_Num Stealth Strikes  <!-- TXT_KEY_UNITHELP_NUM_STEALTH_STRIKES_ATTACK -->
- *STEALTH ATTACK!: %D1_Num%% Stealth Combat Modifier Factored in.  <!-- TXT_KEY_UNITHELP_STEALTH_COMBAT_ATTACK -->
- *%D1_Bonus%% Strength  <!-- TXT_KEY_COMBAT_PLOT_EXTRA_STRENGTH -->
- *STEALTH DEFENSE!: %d1_Num Stealth Strikes  <!-- TXT_KEY_UNITHELP_NUM_STEALTH_STRIKES_DEFENSE -->
- *STEALTH DEFENSE!: %D1_Num%% Stealth Combat Modifier Factored in.  <!-- TXT_KEY_UNITHELP_STEALTH_COMBAT_DEFENSE -->
- *%D1_Bonus%% from Crossing River  <!-- TXT_KEY_COMBAT_PLOT_RIVER_MOD -->
- *%D1_Bonus%% from Amphibious Landing  <!-- TXT_KEY_COMBAT_PLOT_AMPHIB_MOD -->
- *%D1_Bonus%% from Defense Modifier  <!-- TXT_KEY_COMBAT_DEFENSE_MODIFIER -->
- *%d1_Bonus%% VS Non-Animal Barbarians  <!-- TXT_KEY_COMBAT_VSBARBS -->
- *%D1_Bonus%% vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_MOD_VS_TYPE -->
- *Withdrawal VS %s2_Type: %D1_Bonus%%  <!-- TXT_KEY_COMBAT_PLOT_WITHDRAW_VS_TYPE -->
- *%D1_Bonus%% Pursuit vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_PURSUIT_VS_TYPE -->
- *%D1_Bonus%% Repel vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_REPEL_VS_TYPE -->
- *%D1_Bonus%% Knockback vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_KNOCKBACK_VS_TYPE -->
- *%D1_Bonus Puncture vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_PUNCTURE_VS_TYPE -->
- *%D1_Bonus Armor vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_ARMOR_VS_TYPE -->
- *%D1_Bonus Dodge vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_DODGE_VS_TYPE -->
- *%D1_Bonus Precision vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_PRECISION_VS_TYPE -->
- *%D1_Bonus%% Critical vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_CRITICAL_VS_TYPE -->
- *%D1_Bonus%% Round Stun vs. %s2_Type  <!-- TXT_KEY_COMBAT_PLOT_ROUND_STUN_VS_TYPE -->
- *%D1_Bonus%% from Tile Defense  <!-- TXT_KEY_COMBAT_PLOT_TILE_MOD -->
- *%D1_Bonus%% from Unit Fortify  <!-- TXT_KEY_COMBAT_PLOT_FORTIFY_MOD -->
- *%D1_Bonus%% from City (Unit Ability)  <!-- TXT_KEY_COMBAT_PLOT_CITY_MOD -->
- *%D1_Bonus%% from Hills (Unit Ability)  <!-- TXT_KEY_COMBAT_PLOT_HILLS_MOD -->
- *%D1_Bonus%% from %s2_TerrOrFeat (Unit Ability)  <!-- TXT_KEY_COMBAT_PLOT_UNIT_MOD -->
- *%D1_Bonus%% from Attack Modifier  <!-- TXT_KEY_COMBAT_ATTACK_MODIFIER -->
- *%D1_Bonus%% from Size Differential  <!-- TXT_KEY_COMBAT_ATTACK_FROM_SIZE_MODIFIER -->
- *%D1_Bonus%% from Group Volume Differential  <!-- TXT_KEY_COMBAT_ATTACK_FROM_VOLUME_MODIFIER -->
- *%D1_percent%% for Self-Destruction  <!-- TXT_KEY_COMBAT_KAMIKAZE_MOD -->
- *%D1_Change%% for surrounding the enemy  <!-- TXT_KEY_COMBAT_SURROUNDED_DEFENSE_MOD -->
- *%D1_Change%% vs. Wild Animals  <!-- TXT_KEY_UNITHELP_ANIMAL_COMBAT_MOD -->
- *%D1_Change%% vs. Barbarians  <!-- TXT_KEY_UNITHELP_BARBARIAN_COMBAT_MOD -->
- %s1 vs. %s2  <!-- TXT_KEY_COMBAT_PLOT_ODDS_VS -->
- %[d1]/%[d2]HP  <!-- TXT_KEY_COMBAT_PLOT_HP -->
- *Right Front: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_FIRST_FRONT_SUPPORT -->
- *Left Front: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_SECOND_FRONT_SUPPORT -->
- *Right Short Range: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_FIRST_SHORT_RANGE_SUPPORT -->
- *Left Short Range: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_SECOND_SHORT_RANGE_SUPPORT -->
- *Right Medium Range: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_FIRST_MEDIUM_RANGE_SUPPORT -->
- *Left Medium Range: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_SECOND_MEDIUM_RANGE_SUPPORT -->
- *Right Long Range: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_FIRST_LONG_RANGE_SUPPORT -->
- *Left Long Range: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_SECOND_LONG_RANGE_SUPPORT -->
- *Right Flank: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_FIRST_FLANK_SUPPORT -->
- *Left Flank: %s1_Unit% adding +%d2_Amount Str  <!-- TXT_KEY_SECOND_FLANK_SUPPORT -->

## `setCommerceHelp`

- → `setYieldHelp`
- %d1%% of %d2<commerce> = %s3%F4  <!-- TXT_KEY_MISC_HELP_SLIDER_PERCENT_FLOAT -->
- Specialists  <!-- TXT_KEY_CONCEPT_SPECIALISTS -->
- *%D1%F2 from Religion  <!-- TXT_KEY_MISC_HELP_RELIGION_COMMERCE -->
- *%D1%F2 from Corporations  <!-- TXT_KEY_MISC_HELP_CORPORATION_COMMERCE -->
- Buildings  <!-- TXT_KEY_WB_BUILDINGS -->
- Traits/Civics/Heritage.  <!-- TXT_KEY_NATIONAL_SOURCES -->
- *%s1%F2 from Minted Coins  <!-- TXT_KEY_MISC_HELP_MINTED_COMMERCE -->
- *%D1%F2 from golden age  <!-- TXT_KEY_MISC_HELP_GOLDEN_AGE_PLAYER_COMMERCE -->
- Base %s1: %s2%F3  <!-- TXT_KEY_BASE_S1_IS_S2_F3 -->
- *%D1%%%F2 from Bonus  <!-- TXT_KEY_MISC_HELP_BONUS_COMMERCE -->
- *%D1%%%F2 from Buildings  <!-- TXT_KEY_MISC_HELP_YIELD_BUILDINGS -->
- *%D1%%%F2 from Projects and Wonders  <!-- TXT_KEY_MISC_HELP_YIELD_PROJECTS -->
- *%D1%% %F2 from Events  <!-- TXT_KEY_MISC_HELP_COMMERCE_EVENTS -->
- *%D1%%%F2 for %s3_trait leader  <!-- TXT_KEY_MISC_HELP_COMMERCE_TRAIT -->
- *%D1%%%F2 from Civics  <!-- TXT_KEY_MISC_HELP_YIELD_CIVICS -->
- TBD  <!-- TXT_KEY_MISC_HELP_COMMERCE_TECH -->
- *%D1%%%F2 for Capital  <!-- TXT_KEY_MISC_HELP_YIELD_CAPITAL -->
- *%D1%F2 from <hammer>  <!-- TXT_KEY_MISC_HELP_PRODUCTION_TO_COMMERCE -->
- *%s1 %F2 from <hammer>  <!-- TXT_KEY_MISC_HELP_PRODUCTION_TO_COMMERCE_FLOAT -->
- Total %s1_commerce_type: %s2%F3  <!-- TXT_KEY_MISC_HELP_COMMERCE_FINAL_YIELD_FLOAT -->
- → `setBuildingAdditionalCommerceHelp`

## `setCommerceTimes100ChangeHelp`

- → `setResumableCommerceTimes100ChangeHelp`

## `setConvertHelp`

- No State Religion  <!-- TXT_KEY_MISC_NO_STATE_RELIGION -->
- Cannot Convert to %s1_RelName  <!-- TXT_KEY_MISC_CANNOT_CONVERT_TO -->
- While in Anarchy  <!-- TXT_KEY_MISC_WHILE_IN_ANARCHY -->
- It is your current State Religion  <!-- TXT_KEY_MISC_ALREADY_STATE_REL -->
- because of recent Revolution  <!-- TXT_KEY_MISC_ANOTHER_REVOLUTION_RECENTLY -->
- Wait %d1_Num more [NUM1:Turn:Turns]  <!-- TXT_KEY_MISC_WAIT_MORE_TURNS -->

## `setCorporationHelp`

- <name of the thing>
- *All Cities: %s1_yield_symbols per resource consumed  <!-- TXT_KEY_CORPORATION_ALL_CITIES -->
- *Incorporated by First to Discover %s2_TechName  <!-- TXT_KEY_CORPORATION_FOUNDED_FIRST -->
- *Consumes:  <!-- TXT_KEY_CORPORATION_BONUS_REQUIRED -->
- %s2_BonusName (<icon>)  <!-- TXT_KEY_CORPORATION_BONUS_CONSUMES -->
- *Generates: %F1_bonus  <!-- TXT_KEY_CORPORATION_BONUS_PRODUCED -->
- *Founder Receives a %s1_UnitName  <!-- TXT_KEY_RELIGION_FOUNDER_RECEIVES -->
- → `setListHelp`
- *Competes with  <!-- TXT_KEY_CORPORATION_COMPETES -->
- *Cities Receive +%d1 <health>  <!-- TXT_KEY_CORPORATION_HEALTHINESS -->
- *Cities Receive +%d1 <unhealth>  <!-- TXT_KEY_CORPORATION_UNHEALTHINESS -->
- *Cities Receive +%d1 <happy>  <!-- TXT_KEY_CORPORATION_HAPPINESS -->
- *Cities Receive +%d1 <unhappy>  <!-- TXT_KEY_CORPORATION_UNHAPPINESS -->
- *%D1_Mod%% Military Unit Production  <!-- TXT_KEY_CIVICHELP_MILITARY_PRODUCTION -->
- *New Units Receive %D1_Change Experience Points  <!-- TXT_KEY_CIVICHELP_FREE_XP -->
- All Cities  <!-- TXT_KEY_BUILDINGHELP_ALL_CITIES -->
- → `setYieldChangeHelp`
- Requires %s2 (%d3 Total) in any city  <!-- TXT_KEY_HELPTEXT_REQUIRES_NUM_BUILDINGS_0 -->
- Obsolete with %s2_TechName  <!-- TXT_KEY_BUILDINGHELP_OBSOLETE_WITH -->

## `setCorporationHelpCity`

- <name of the thing>
- *Incorporated by First to Discover %s2_TechName  <!-- TXT_KEY_CORPORATION_FOUNDED_FIRST -->
- *Consumes:  <!-- TXT_KEY_CORPORATION_BONUS_REQUIRED -->
- *Generates: %F1_bonus  <!-- TXT_KEY_CORPORATION_BONUS_PRODUCED -->
- *Generates:  <!-- TXT_KEY_CORPORATION_BONUS_GENERATES -->

## `setDateStr`

- <name of the thing>
- %s2 %d3, %s1  <!-- TXT_KEY_TIME_DATE -->
- Turn: %d1_TurnNum  <!-- TXT_KEY_TIME_TURN -->
- Week %d1_WeekNum  <!-- TXT_KEY_TIME_WEEK -->

## `setEmploymentHelp`

- <name of the thing>
- Total Population Employed: %d1  <!-- TXT_KEY_MISC_TOTAL_EMPLOYED -->

## `setEspionageCostHelp`

- <name of the thing>
- Sabotage %s1_improvement  <!-- TXT_KEY_ESPIONAGE_HELP_DESTROY_IMPROVEMENT -->
- Sabotage current production (%d1<hammer>)  <!-- TXT_KEY_ESPIONAGE_HELP_DESTROY_PRODUCTION -->
- Bribe the loyalty of %s1_name  <!-- TXT_KEY_ESPIONAGE_HELP_BRIBE -->
- Spread %d2<culture> to %s1_city (%d3%%)  <!-- TXT_KEY_ESPIONAGE_HELP_INSERT_CULTURE -->
- Cause %d1%F2 in %s3_city (fully recovers after %d4 [NUM4:turn:turns])  <!-- TXT_KEY_ESPIONAGE_HELP_POISON -->
- Cause %s1_city to revolt for %d2 [NUM2:turn:turns]  <!-- TXT_KEY_ESPIONAGE_HELP_REVOLT -->
- Causes a nuclear detonation in the city of %s1. (May cause war and injure friendly units)  <!-- TXT_KEY_ESPIONAGE_HELP_NUKE -->
- Causes Rebel Factions in %s1 to gain popular support.  <!-- TXT_KEY_ESPIONAGE_HELP_REVOLTUTION -->
- Will disable the power grid in %s1 for %d2 [NUM2:turn:turns].  <!-- TXT_KEY_ESPIONAGE_HELP_POWER -->
- Will temporarily increase the War Weariness in %s1 by %d2%.  <!-- TXT_KEY_ESPIONAGE_HELP_WAR_WEARINESS -->
- Will sabotage all of %s1's research in %s2  <!-- TXT_KEY_ESPIONAGE_HELP_SABATOGE_RESEARCH -->
- Will remove %s1 from %s2.  <!-- TXT_KEY_ESPIONAGE_HELP_REMOVE_RELIGIONS -->
- Assassinate %s1_unit_name  <!-- TXT_KEY_ESPIONAGE_HELP_DESTROY_UNIT -->
- Induce %s1_player to switch to %s2_civic  <!-- TXT_KEY_ESPIONAGE_HELP_SWITCH_CIVIC -->
- Cause %s1_player to enter Anarchy for %d2 [NUM2:turn:turns]  <!-- TXT_KEY_ESPIONAGE_HELP_ANARCHY -->
- Add %d1%% to the cost of %s2_civ_adjective espionage missions against you (lasts for %d3 [NUM3:turn:turns])  <!-- TXT_KEY_ESPIONAGE_HELP_COUNTERESPIONAGE -->
- Steal %d1<gold> from the %s2_civ_adjective treasury  <!-- TXT_KEY_ESPIONAGE_HELP_STEAL_TREASURY -->
- Gain knowledge of %s1_tech  <!-- TXT_KEY_ESPIONAGE_HELP_STEAL_TECH -->
- %d1 <spy>: Base Cost  <!-- TXT_KEY_ESPIONAGE_BASE_COST -->
- %D1%%: for City Size  <!-- TXT_KEY_ESPIONAGE_POPULATION_MOD -->
- %D1%%: for Trade Routes  <!-- TXT_KEY_ESPIONAGE_TRADE_ROUTE_MOD -->
- %D1%%: for our State Religion  <!-- TXT_KEY_ESPIONAGE_RELIGION_MOD -->
- %D1%%: for City Culture  <!-- TXT_KEY_ESPIONAGE_CULTURE_MOD -->
- %D1%%: for City Espionage Defense  <!-- TXT_KEY_ESPIONAGE_DEFENSE_MOD -->
- %D1%%: for Distance  <!-- TXT_KEY_ESPIONAGE_DISTANCE_MOD -->
- %D1%%: for Stationary Spy  <!-- TXT_KEY_ESPIONAGE_SPY_STATIONARY_MOD -->
- %D1%%: for Espionage Point Spending  <!-- TXT_KEY_ESPIONAGE_EP_RATIO_MOD -->
- %D1%%: for Counterespionage  <!-- TXT_KEY_ESPIONAGE_COUNTERESPIONAGE_MOD -->
- %d1%: For Having an Embassy in their Nation.  <!-- TXT_KEY_ESPIONAGE_EMBASSY_MOD -->
- %d1%: From Free Trade Agreement  <!-- TXT_KEY_FREE_TRADE_AGREEMENT_MOD -->
- %d1 <spy>: Total Cost  <!-- TXT_KEY_ESPIONAGE_COST_TOTAL -->
- Chance of Success: %d1%%  <!-- TXT_KEY_ESPIONAGE_CHANCE_OF_SUCCESS -->

## `setEspionageMissionHelp`

- Cannot Perform Espionage  <!-- TXT_KEY_UNITHELP_NO_ESPIONAGE -->
- (Moved)  <!-- TXT_KEY_UNITHELP_NO_ESPIONAGE_REASON_MOVED -->
- (Visible by %s1_player)  <!-- TXT_KEY_UNITHELP_NO_ESPIONAGE_REASON_VISIBLE -->
- %d1%% <spy> Cost  <!-- TXT_KEY_ESPIONAGE_COST -->

## `setEventHelp`

- the city  <!-- TXT_KEY_EVENT_THE_CITY -->
- unit  <!-- TXT_KEY_EVENT_THE_UNIT -->
- the religion  <!-- TXT_KEY_EVENT_THE_RELIGION -->
- *Gift %s1_resource to %s2_player  <!-- TXT_KEY_EVENT_GIFT_BONUS_TO_PLAYER -->
- *%D1<happy> in all cities. %s3_playerName gets %D2<unhappy>  <!-- TXT_KEY_EVENT_HAPPY_FROM_PLAYER -->
- *%D1<unhappy> in all cities. %s3_playerName gets %D2<happy>  <!-- TXT_KEY_EVENT_HAPPY_TO_PLAYER -->
- *%D1<happy> in %s2_city  <!-- TXT_KEY_EVENT_HAPPY_CITY -->
- *%D1<happy> in all cities  <!-- TXT_KEY_EVENT_HAPPY -->
- *%D1<unhappy> in %s2_city  <!-- TXT_KEY_EVENT_UNHAPPY_CITY -->
- *%D1<unhappy> in all cities  <!-- TXT_KEY_EVENT_UNHAPPY -->
- *%D1<health> in all cities. %s3_playerName gets %D2<unhealth>  <!-- TXT_KEY_EVENT_HEALTH_FROM_PLAYER -->
- *%D1<unhealth> in all cities. %s3_playerName gets %D2<happy>  <!-- TXT_KEY_EVENT_HEALTH_TO_PLAYER -->
- *%D1<health> in %s2_city  <!-- TXT_KEY_EVENT_HEALTH_CITY -->
- *%D1<health> in all cities  <!-- TXT_KEY_EVENT_HEALTH -->
- *%D1<unhealth> in all cities  <!-- TXT_KEY_EVENT_UNHEALTH -->
- *%D1<unhealth> in %s2_city  <!-- TXT_KEY_EVENT_UNHEALTH_CITY -->
- *%d1<unhappy> temporarily in %s2_city  <!-- TXT_KEY_EVENT_HURRY_ANGER_CITY -->
- *%d1<unhappy> temporarily in all cities  <!-- TXT_KEY_EVENT_HURRY_ANGER -->
- *%D1<happy> in %s3_city for %d2 [NUM2:turn:turns]  <!-- TXT_KEY_EVENT_TEMP_HAPPY_CITY -->
- *%D1<happy> in all cities for %d2 [NUM2:turn:turns]  <!-- TXT_KEY_EVENT_TEMP_HAPPY -->
- *%D1 stored <food> in %s2_city  <!-- TXT_KEY_EVENT_FOOD_CITY -->
- *Food stores in all cities: %D1<food>  <!-- TXT_KEY_EVENT_FOOD -->
- *Food stores in %s2_city: %d1%%<food>  <!-- TXT_KEY_EVENT_FOOD_PERCENT_CITY -->
- *%d1%% stored <food> in all cities  <!-- TXT_KEY_EVENT_FOOD_PERCENT -->
- *%d1 [NUM1:turn:turns] of disorder in %s2_city  <!-- TXT_KEY_EVENT_REVOLT_TURNS -->
- **%D1_Mod%% Spaceship Production in %s2_city  <!-- TXT_KEY_EVENT_SPACE_PRODUCTION_CITY -->
- *%D1_Mod%% Spaceship Production in All Cities  <!-- TXT_KEY_BUILDINGHELP_SPACESHIP_MOD_ALL_CITIES -->
- *%d1 terrain [NUM1:improvement:improvements] destroyed around %s2_city  <!-- TXT_KEY_EVENT_PILLAGE_CITY -->
- *Between %d1 and %d2 terrain improvements destroyed around %s3_city  <!-- TXT_KEY_EVENT_PILLAGE_RANGE_CITY -->
- *%d1 terrain [NUM1:improvement:improvements] destroyed  <!-- TXT_KEY_EVENT_PILLAGE -->
- *Between %d1 and %d2 terrain improvements destroyed  <!-- TXT_KEY_EVENT_PILLAGE_RANGE -->
- *%D1 free %s2_specialist in %s3_city  <!-- TXT_KEY_EVENT_FREE_SPECIALIST -->
- *%D1 population in %s2_city  <!-- TXT_KEY_EVENT_POPULATION_CHANGE_CITY -->
- *%D1 population in all cities  <!-- TXT_KEY_EVENT_POPULATION_CHANGE -->
- *%D1 <culture> in %s2_city  <!-- TXT_KEY_EVENT_CULTURE_CITY -->
- *%D1 <culture> in all cities  <!-- TXT_KEY_EVENT_CULTURE -->
- *Receive %d1 %s2_units  <!-- TXT_KEY_EVENT_BONUS_UNIT -->
- *Receive a %s1_building  <!-- TXT_KEY_EVENT_BONUS_BUILDING -->
- *Lose the %s1_building  <!-- TXT_KEY_EVENT_REMOVE_BUILDING -->
- → `setYieldChangeHelp`
- *%s1_building: %s2  <!-- TXT_KEY_EVENT_YIELD_CHANGE_BUILDING -->
- <name of the thing>
- *%s1_building: %D2%F3  <!-- TXT_KEY_EVENT_HAPPY_BUILDING -->
- Lowers risk of Revolution in the City  <!-- TXT_KEY_EVENT_REVOLUTION_INDEX_CITY -->
- Lowers risk of Revolution in all cities  <!-- TXT_KEY_EVENT_REVOLUTION_INDEX -->
- *A %s1_feature grows  <!-- TXT_KEY_EVENT_FEATURE_GROWTH -->
- *The %s1_feature is cleared  <!-- TXT_KEY_EVENT_FEATURE_REMOVE -->
- *A %s1_improvement is built  <!-- TXT_KEY_EVENT_IMPROVEMENT_GROWTH -->
- *The %s1_improvement is destroyed  <!-- TXT_KEY_EVENT_IMPROVEMENT_REMOVE -->
- *A source of %s1_bonus is discovered  <!-- TXT_KEY_EVENT_BONUS_GROWTH -->
- *The %s1_resource is lost  <!-- TXT_KEY_EVENT_BONUS_REMOVE -->
- *A new %s1_route is built  <!-- TXT_KEY_EVENT_ROUTE_GROWTH -->
- *The %s1_route is destroyed  <!-- TXT_KEY_EVENT_ROUTE_REMOVE -->
- *%s1 in the plot  <!-- TXT_KEY_EVENT_YIELD_CHANGE_PLOT -->
- *Reveal all %s1_bonus  <!-- TXT_KEY_EVENT_BONUS_REVEALED -->
- *The %s2_unit receives %d1 XP  <!-- TXT_KEY_EVENT_UNIT_EXPERIENCE -->
- *The %s1_unit is lost  <!-- TXT_KEY_EVENT_UNIT_DISBAND -->
- *All %s1_units are promoted to %s2_promotion  <!-- TXT_KEY_EVENT_UNIT_COMBAT_PROMOTION -->
- *Every %s1_unit is promoted to %s2_promotion  <!-- TXT_KEY_EVENT_UNIT_CLASS_PROMOTION -->
- *Convert up to %d1 of your own cities to %s2_religion  <!-- TXT_KEY_EVENT_CONVERT_OWN_CITIES -->
- *Convert up to %d1 of their cities to %s2_religion  <!-- TXT_KEY_EVENT_CONVERT_OTHER_CITIES -->
- *Gain a %D1 relations boost with %s2_player  <!-- TXT_KEY_EVENT_ATTITUDE_GOOD -->
- *Suffer a %D1 relations hit with %s2_player  <!-- TXT_KEY_EVENT_ATTITUDE_BAD -->
- *%D1<spy> against %s2_player  <!-- TXT_KEY_EVENT_ESPIONAGE_POINTS -->
- *Cost: %d1<spy>  <!-- TXT_KEY_EVENT_ESPIONAGE_COST -->
- *Start a Golden Age  <!-- TXT_KEY_EVENT_GOLDEN_AGE -->
- *Free support for %d1 [NUM1:unit:units]  <!-- TXT_KEY_EVENT_FREE_UNIT_SUPPORT -->
- *%D1%% inflation  <!-- TXT_KEY_EVENT_INFLATION_MODIFIER -->
- *Take the %s1_civ_adjective actions as a war declaration upon you  <!-- TXT_KEY_EVENT_DECLARE_WAR -->
- *The %s2_unit will be busy for %d1 [NUM1:turn:turns], unavailable for any other duty except self-defense.  <!-- TXT_KEY_EVENT_IMMOBILE_UNIT -->
- → `buildChangesString`
- → `buildChangesAllCitiesString`
- → `setEventHelp`
- *In addition%s2_txt_key_event_delay_below, %d1%% chance for:  <!-- TXT_KEY_EVENT_ADDITIONAL_CHANCE -->
- , in %d1 [NUM1:turn:turns]  <!-- TXT_KEY_EVENT_DELAY_TURNS -->
- *In addition %s1_txt_key_event_delay_below:  <!-- TXT_KEY_EVENT_DELAY -->
- *Requires %s2  <!-- TXT_KEY_REQUIRES_LINK -->

## `setFeatureHelp`

- <name of the thing>
- → `setYieldChangeHelp`
- adjacent to river  <!-- TXT_KEY_TERRAINHELP_NEXT_TO_RIVER -->
- *Movement Cost: %d1<moves>  <!-- TXT_KEY_TERRAINHELP_MOVEMENT_COST -->
- *+%s1<health> in nearby cities  <!-- TXT_KEY_FEATUREHRLP_GOOD_HEALTH -->
- *+%s1<unhealth> in nearby cities  <!-- TXT_KEY_FEATUREHELP_BAD_HEALTH -->
- *Defending units get %D1%% strength  <!-- TXT_KEY_TERRAINHELP_DEFENSE_MODIFIER -->
- *Acts as a source of fresh water  <!-- TXT_KEY_FEATUREHELP_ADDS_FRESH_WATER -->
- *Impassable terrain  <!-- TXT_KEY_TERRAINHELP_IMPASSABLE -->
- *Cannot build cities here  <!-- TXT_KEY_TERRAINHELP_NO_CITIES -->
- *Cannot build any Improvements on this terrain.  <!-- TXT_KEY_FEATUREHELP_NO_IMPROVEMENT -->
- *No bonus on plots with this feature.  <!-- TXT_KEY_FEATUREHELP_NO_BONUS -->
- *This feature is never destroyed by a city.  <!-- TXT_KEY_FEATUREHELP_POP_NEVER_DESTROYED -->
- *This feature is immediately destroyed when a city is founded on the same plot.  <!-- TXT_KEY_FEATUREHELP_POP_ALWAYS_DESTROYED -->
- *This feature is destroyed by a city founded on the same plot as soon as the city achieves %d1_pop Population.  <!-- TXT_KEY_FEATUREHELP_POP_DESTROYS -->
- (key not in current GameText)  <!-- TXT_KEY_TERRAINHELP_TURN_DAMAGE -->
- → `buildDisplayString`
- *May only exist on a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_PREREQUISITE -->

## `setFlagHelp`

- Player difficulty: %s1  <!-- TXT_KEY_SETTINGS_DIFFICULTY_PLAYER -->
- Game difficulty: %s1  <!-- TXT_KEY_SETTINGS_DIFFICULTY_GAME -->
- → `buildDisplayString`

## `setFoodHelp`

- → `setYieldHelp`
- *%D1<eatenfood> for Population  <!-- TXT_KEY_MISC_HELP_EATEN_FOOD -->
- *%D1<eatenfood> Wasted  <!-- TXT_KEY_MISC_HELP_WASTED_FOOD -->
- *%D1<eatenfood> for Health  <!-- TXT_KEY_MISC_HELP_SPOILED_FOOD -->
- -----------------------Total Food Consumed: %d1<eatenfood>  <!-- TXT_KEY_MISC_HELP_TOTAL_FOOD_CONSUMED -->
- Net Food: %D1<food> for %s2_production_type  <!-- TXT_KEY_MISC_HELP_NET_FOOD_PRODUCTION -->
- Net Food: %D1<food>  <!-- TXT_KEY_MISC_HELP_NET_FOOD_GROW -->
- Net Food: %D1<bad_food>  <!-- TXT_KEY_MISC_HELP_NET_FOOD_SHRINK -->
- Net Food: +0<food>  <!-- TXT_KEY_MISC_HELP_NET_FOOD_STAGNATE -->
- → `setBuildingAdditionalYieldHelp`

## `setGoodHealthHelp`

- %D1_Change<health> from Fresh Water  <!-- TXT_KEY_MISC_HEALTH_FROM_FRESH_WATER -->
- %D1_Change<health> from %s2_FeatName  <!-- TXT_KEY_MISC_FEAT_GOOD_HEALTH -->
- Features  <!-- TXT_KEY_MISC_FEATURES -->
- %D1_Change<health> from %s2_ImpName  <!-- TXT_KEY_MISC_IMPR_GOOD_HEALTH -->
- Improvements  <!-- TXT_KEY_MISC_IMPROVEMENTS -->
- %D1_Change<health> from Specialists  <!-- TXT_KEY_GOOD_HEALTH_FROM_SPECIALISTS -->
- %D1_Change<health> from Bonuses  <!-- TXT_KEY_MISC_GOOD_HEALTH_FROM_BONUSES -->
- %D1_Change<health> from Buildings  <!-- TXT_KEY_MISC_GOOD_HEALTH_FROM_BUILDINGS -->
- %D1_Change<health> from Civics  <!-- TXT_KEY_MISC_GOOD_HEALTH_FROM_CIVICS -->
- %D1_Change<health> from Civilization  <!-- TXT_KEY_MISC_GOOD_HEALTH_FROM_CIV -->
- %D1_Change<health> from Events  <!-- TXT_KEY_MISC_GOOD_HEALTH_FROM_EVENTS -->
- %D1_Change<health> from Extra Health  <!-- TXT_KEY_MISC_HEALTH_EXTRA -->
- %D1_Change<health> from Difficulty Level  <!-- TXT_KEY_MISC_GOOD_HEALTH_FROM_HANDICAP -->
- %D1_Change<health> From World Projects  <!-- TXT_KEY_MISC_GOOD_HEALTH_FROM_WORLD_PROJECT -->
- %D1_Change<health> From National Projects  <!-- TXT_KEY_MISC_GOOD_HEALTH_FROM_PROJECT -->
- %D1_Change<health> From Corporations  <!-- TXT_KEY_MISC_GOOD_HEALTH_FROM_CORPORATION -->
- +%d1<health> "Our advanced society has made the efforts of some professions a cleaner affair!"  <!-- TXT_KEY_HEALTHY_TECH_SPECIALIST -->
- %d1_Num<health> Total Health  <!-- TXT_KEY_MISC_TOTAL_HEALTHY -->

## `setHappyHelp`

- +%d1_Change<happy>: "Our rebellion has been successful"  <!-- TXT_KEY_REV_SUCCESS_HAPPINESS -->
- +%d1_Change<happy>: "We love this great city of ours!"  <!-- TXT_KEY_HAPPY_BIG_CITY -->
- +%d1_Change<happy>: "We love our government!"  <!-- TXT_KEY_HAPPY_CIVIC -->
- +%d1_Change<happy>: "Other nations' technological breakthroughs have benefited the world!"  <!-- TXT_KEY_HAPPY_WORLD_PROJECT -->
- +%d1_Change<happy>: "Corporations are Beneficial!"  <!-- TXT_KEY_HAPPY_CORPORATIONS -->
- +%d1_Change<happy>: "We Enjoy our National Landmarks!"  <!-- TXT_KEY_HAPPY_LANDMARKS -->
- +%d1_Change<happy>: "The military presence impresses us!"  <!-- TXT_KEY_HAPPY_MILITARY_PRESENCE -->
- +%d1_Change<happy>: "We influence other civilizations!"  <!-- TXT_KEY_HAPPY_VASSAL -->
- +%d1_Change<happy>: "Our State Religion is the best!"  <!-- TXT_KEY_HAPPY_STATE_RELIGION -->
- +%d1_Change<happy>: "Some buildings are making us happy!"  <!-- TXT_KEY_HAPPY_BUILDINGS -->
- +%d1_Change<happy>: "We love our National Parks!"  <!-- TXT_KEY_HAPPY_FEATURES -->
- +%d1_Change<happy>: "We enjoy our luxurious resources!"  <!-- TXT_KEY_HAPPY_BONUS -->
- +%d1_Change<happy>: "Some citizens' jobs please us!"  <!-- TXT_KEY_HAPPY_SPECIALISTS -->
- +%d1_Change<happy>: "In our Religion we trust!"  <!-- TXT_KEY_HAPPY_RELIGIOUS_FREEDOM -->
- +%d1_Change<happy>: "We appreciate our entertainment!"  <!-- TXT_KEY_HAPPY_ENTERTAINMENT -->
- +%d1_Change<happy>: "OH YEAH!"  <!-- TXT_KEY_HAPPY_YEAH -->
- %D1_Change<happy>: "Past events went well!" (%d2 [NUM2:turn:turns])  <!-- TXT_KEY_HAPPY_TEMP -->
- +%d1_Change<happy>: "We just enjoy life!"  <!-- TXT_KEY_HAPPY_HANDICAP -->
- +%d1<happy> "We've a celebrity in our midst!"  <!-- TXT_KEY_HAPPY_CELEBRITY -->
- +%d1<happy> "Our advanced society has made the efforts of some professions more enjoyable for all!"  <!-- TXT_KEY_HAPPY_TECH_SPECIALIST -->
- Total Happiness: %d1_Num<happy>  <!-- TXT_KEY_HAPPY_TOTAL_HAPPY -->

## `setHeritageHelp`

- *Requires %s2  <!-- TXT_KEY_REQUIRES_LINK -->
- <name of the thing>
- → `setListHelp`
- *Requires  <!-- TXT_KEY_REQUIRES_2 -->
- or  <!-- TXT_KEY_OR -->
- *Can be built by  <!-- TXT_KEY_UNITHELP_REQUIRED_TO_BUILD -->
- → `buildDisplayString`
- Sid's Tips:  <!-- TXT_KEY_SIDS_TIPS -->

## `setImprovementHelp`

- <name of the thing>
- → `setYieldChangeHelp`
- with Irrigation  <!-- TXT_KEY_MISC_WITH_IRRIGATION -->
- along River  <!-- TXT_KEY_MISC_ALONG_RIVER -->
- *%D1_Change<icon> with %s3_TechName  <!-- TXT_KEY_IMPROVEMENTHELP_WITH_TECH -->
- *%D1_Change<icon> from  <!-- TXT_KEY_CIVICHELP_IMPROVEMENT_YIELD_CHANGE -->
- *Requires River  <!-- TXT_KEY_IMPROVEMENTHELP_REQUIRES_RIVER -->
- *Requires peak  <!-- TXT_KEY_IMPROVEMENTHELP_REQUIRES_PEAK -->
- *Carries Irrigation  <!-- TXT_KEY_IMPROVEMENTHELP_CARRIES_IRRIGATION -->
- *Cannot be built near Fresh Water  <!-- TXT_KEY_IMPROVEMENTHELP_NO_BUILD_FRESH_WATER -->
- *Built only in Water  <!-- TXT_KEY_IMPROVEMENTHELP_BUILD_ONLY_WATER -->
- *Can only be built on Flatlands  <!-- TXT_KEY_IMPROVEMENTHELP_ONLY_BUILD_FLATLANDS -->
- *To place with %s1_Build requires %s2_Bonus  <!-- TXT_KEY_BUILDHELP_REQUIRES_BONUS -->
- *Upgrade time: %d1 [NUM1:Turn:Turns]  <!-- TXT_KEY_UPGRADE_TIME -->
- *Upgrades:  <!-- TXT_KEY_UPGRADES -->
- %s2  <!-- TXT_KEY_LINK -->
- *A unit must guard it to upgrade  <!-- TXT_KEY_IMPROVEMENTHELP_FORTIFY_TO_UPGRADE -->
- *Small chance of discovering  <!-- TXT_KEY_IMPROVEMENTHELP_CHANCE_DISCOVER -->
- → `setListHelp`
- *%D1%% Tile Defense  <!-- TXT_KEY_IMPROVEMENTHELP_DEFENSE_MODIFIER -->
- in nearby cities  <!-- TXT_KEY_MISC_ICON_CHANGE_NEARBY_CITIES_1 -->
- *%d1%F2 in nearby cities  <!-- TXT_KEY_MISC_ICON_CHANGE_NEARBY_CITIES -->
- *Acts as a city for combat purposes  <!-- TXT_KEY_IMPROVEMENTHELP_DEFENSE_MODIFIER_EXTRA -->
- *Gives terrain a higher chance to spread  <!-- TXT_KEY_IMPROVEMENTHELP_MORE_GROWTH -->
- *Gives terrain a lower chance to spread  <!-- TXT_KEY_IMPROVEMENTHELP_LESS_GROWTH -->
- *Can be bombarded  <!-- TXT_KEY_IMPROVEMENTHELP_BOMBARD -->
- *Cannot build another within a distance of %d1_Num  <!-- TXT_KEY_IMPROVEMENTHELP_UNIQUE_RANGE -->
- *Provides a zone of control  <!-- TXT_KEY_IMPROVEMENTHELP_IS_ZOC_SOURCE -->
- *When added to a plot, this improvement also changes the feature on the plot to %s1_Feature_Name (if feature is valid).  <!-- TXT_KEY_IMPROVEMENTHELP_FEATURE_CHANGE -->
- *When added to a plot, this improvement also adds a source of %s1_Bonus_Name (if the resource is valid).  <!-- TXT_KEY_IMPROVEMENTHELP_BONUS_CHANGE -->
- *May not be added to a plot that already has a resource, whether visible or not.  <!-- TXT_KEY_IMPROVEMENTHELP_NOT_ON_ANY_BONUS -->
- *May only be placed once by a given player.  <!-- TXT_KEY_IMPROVEMENTHELP_NATIONAL -->
- *May only be placed once in a game.  <!-- TXT_KEY_IMPROVEMENTHELP_GLOBAL -->
- *When added to a plot, this improvement immediately removes itself from the plot. (Useful for placing features and bonuses with upgrades, builds and razing.)  <!-- TXT_KEY_IMPROVEMENTHELP_CHANGE_REMOVE -->
- *Allows Naval Units on the plot  <!-- TXT_KEY_IMPROVEMENTHELP_IS_CAN_MOVE_SEA_UNITS -->
- *Provides any tradable resource from the plot on which it is built  <!-- TXT_KEY_IMPROVEMENTHELP_IS_UNIVERSAL_BONUS_PROVIDER -->
- *%D1_Change<culture> for owner in plot  <!-- TXT_KEY_IMPROVEMENTHELP_PLOT_CULTURE -->
- *Influences plots within a distance of %d1_Num  <!-- TXT_KEY_IMPROVEMENTHELP_CULTURE_RANGE -->
- *%D1_Change Vision Range  <!-- TXT_KEY_IMPROVEMENTHELP_VISIBILITY_RANGE -->
- *%D1_Change Vision Height  <!-- TXT_KEY_IMPROVEMENTHELP_SEE_FROM -->
- *Pillage yields %d1_Num<gold> on average  <!-- TXT_KEY_IMPROVEMENTHELP_PILLAGE_YIELDS -->
- Will Destroy the %s1_Feature giving %D2_Amount production to %s3_CityName this round.  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_DESTROYS_FEATURE -->
- Will Safely Retain the %s1_Feature on this plot.  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_RETAINS_FEATURE -->
- → `buildDisplayString`
- *May only exist on a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_PREREQUISITE -->

## `setMinimizePopupHelp`

- You can now convert to %s1_Religion as a State Religion...  <!-- TXT_KEY_MINIMIZED_CHANGE_RELIGION -->
- Choose a free technology...  <!-- TXT_KEY_MINIMIZED_CHOOSE_TECH_FREE -->
- Choose a new technology to research...  <!-- TXT_KEY_MINIMIZED_CHOOSE_TECH -->
- You can now adopt the %s1_Civic Civic...  <!-- TXT_KEY_MINIMIZED_CHANGE_CIVIC -->

## `setNetStats`

- [%d1_Num ms]  <!-- TXT_KEY_MISC_NUM_MS -->
- [Disconnected]  <!-- TXT_KEY_MISC_DISCONNECTED -->
-   <!-- TXT_KEY_MISC_AI -->

## `setOOSSeeds`

- OOS Values: Sync=%d1_Num; Options=%d2_Num  <!-- TXT_KEY_PLAYER_OOS -->

## `setPlotHelp`

- <name of the thing>
- Owner  <!-- TXT_KEY_MISC_OWNER -->
- *  <!-- TXT_KEY_BULLET -->
- Not in %s1_CivAdj city influence.  <!-- TXT_KEY_MISC_NO_CITY_INFLUENCE -->
- %s1:  <!-- TXT_KEY_S1_COLON_SPACE -->
- Defense Bonus: %D1_Mod%%  <!-- TXT_KEY_PLOT_BONUS -->
- Fresh Water  <!-- TXT_KEY_PLOT_FRESH_WATER -->
- IMPASSABLE  <!-- TXT_KEY_PLOT_IMPASSABLE -->
- Movement Cost: %d1<moves>  <!-- TXT_KEY_PLOT_MOVEMENT_COST -->
- Route Movement: %s1(%s2)  <!-- TXT_KEY_PLOT_ROUTE_MOVEMENT_COST -->
- , Research: %s1_Name  <!-- TXT_KEY_PLOT_RESEARCH -->
- , Requires: %s1_Name  <!-- TXT_KEY_PLOT_REQUIRES -->
- , or %s1_Name  <!-- TXT_KEY_PLOT_REQUIRES_OR -->
- → `setListHelp`
- (with %s1_ImpName)  <!-- TXT_KEY_BONUSHELP_WITH_IMPROVEMENT -->
- Obsolete with %s2_TechName  <!-- TXT_KEY_BUILDINGHELP_OBSOLETE_WITH -->
- , Requires Route  <!-- TXT_KEY_PLOT_REQUIRES_ROUTE -->
- → `buildDisplayString`
- (Irrigated)  <!-- TXT_KEY_PLOT_IRRIGATED -->
- (Not Irrigated)  <!-- TXT_KEY_PLOT_NOT_IRRIGATED -->
- Improvement upgrade frozen.Unfreeze by toggling city work here.  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_FROZEN -->
- No upgrade available  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_BLOCKED -->
- (Upgrade: %d1_Num [NUM1:Turn:Turns]  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_TURNS -->
- (Need Garrison)  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_NEED_GARRISON -->
- (Need Work)  <!-- TXT_KEY_IMPROVEMENTHELP_UPGRADE_NEED_WORK -->
- %d1°%d2'  <!-- TXT_KEY_LATLONG -->
- W  <!-- TXT_KEY_LATLONG_WEST -->
- E  <!-- TXT_KEY_LATLONG_EAST -->
- S  <!-- TXT_KEY_LATLONG_SOUTH -->
- N  <!-- TXT_KEY_LATLONG_NORTH -->
- %d1_Num [NUM1:Turn:Turns]  <!-- TXT_KEY_ACTION_NUM_TURNS -->
- Trade Blocked by Enemy Ship  <!-- TXT_KEY_PLOT_BLOCKADED -->
- (key not in current GameText)  <!-- TXT_KEY_PLOT_DAMAGE -->
- This is a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_TERRAIN -->

## `setPlotListHelp`

- → `setUnitHelp`
- <name of the thing>

## `setProcessHelp`

- <name of the thing>
- *Converts %d1_Mod%% of %F2_Type1 to %F3_Type2  <!-- TXT_KEY_PROCESS_CONVERTS -->

## `setProductionHelp`

- → `setYieldHelp`
- → `setBuildingAdditionalYieldHelp`

## `setProjectHelp`

- <name of the thing>
- *World Project (%d1_Num Allowed)  <!-- TXT_KEY_PROJECTHELP_WORLD_NUM_ALLOWED -->
- (World Project: %d1_Num Left)  <!-- TXT_KEY_PROJECTHELP_WORLD_NUM_LEFT -->
- *Team Project (%d1_Num Allowed)  <!-- TXT_KEY_PROJECTHELP_TEAM_NUM_ALLOWED -->
- (Team Project: %d1_Num Left)  <!-- TXT_KEY_PROJECTHELP_TEAM_NUM_LEFT -->
- *%D1_Mod%% Chance of Intercepting Nukes  <!-- TXT_KEY_PROJECTHELP_CHANCE_INTERCEPT_NUKES -->
- *Grants all Technologies acquired by any %d1_Num Known [NUM1:Civilization:Civilizations]  <!-- TXT_KEY_PROJECTHELP_TECH_SHARE -->
- *%D1_MOD%% Maintenance Costs in All Cities  <!-- TXT_KEY_PROJECTHELP_GLOBAL_MAINT_MOD -->
- *%D1_Mod%% Maintenance Costs from Distance to Palace  <!-- TXT_KEY_PROJECTHELP_DISTANCE_MAINT_MOD -->
- *%D1_Mod%% Maintenance Costs from Number of Cities  <!-- TXT_KEY_PROJECTHELP_NUM_CITIES_MAINT_MOD -->
- Enables Nuclear Weapons for the Player that builds this  <!-- TXT_KEY_PROJECTHELP_ENABLES_NUKES -->
- *Enables %s1_Name (For all Players)  <!-- TXT_KEY_PROJECTHELP_ENABLES_SPECIAL -->
- *All cities in the world receive +%d1 <happy>  <!-- TXT_KEY_PROJECTHELP_WORLD_HAPPINESS -->
- *All cities in the world receive +%d1 <unhappy>  <!-- TXT_KEY_PROJECTHELP_WORLD_UNHAPPINESS -->
- *All cities in the world receive +%d1 <health>  <!-- TXT_KEY_PROJECTHELP_WORLD_HEALTH -->
- *All cities in the world receive +%d1 <unhealth>  <!-- TXT_KEY_PROJECTHELP_WORLD_UNHEALTH -->
- *All of the player's cities receive +%d1 <happy>  <!-- TXT_KEY_PROJECTHELP_HAPPINESS -->
- *All of the player's cities receive +%d1 <unhappy>  <!-- TXT_KEY_PROJECTHELP_UNHAPPINESS -->
- *All of the player's cities receive +%d1 <health>  <!-- TXT_KEY_PROJECTHELP_HEALTH -->
- *All of the player's cities receive +%d1 <unhealth>  <!-- TXT_KEY_PROJECTHELP_UNHEALTH -->
- * +%d1 Trade Routes in the World  <!-- TXT_KEY_BUILDINGHELP_MORE_WORLD_TRADE -->
- * -%d1 Trade Routes in the World  <!-- TXT_KEY_BUILDINGHELP_LESS_WORLD_TRADE -->
- *%d1%% Inflation  <!-- TXT_KEY_ADJUSTS_INFLATION -->
- All Cities  <!-- TXT_KEY_BUILDINGHELP_ALL_CITIES -->
- *%s1 Required for %s2_victory Victory  <!-- TXT_KEY_PROJECTHELP_REQUIRED_FOR_VICTORY -->
- Required for Anyone to Create  <!-- TXT_KEY_PROJECTHELP_REQUIRED_TO_CREATE_ANYONE -->
- → `setListHelp`
- Required to Create  <!-- TXT_KEY_PROJECTHELP_REQUIRED_TO_CREATE -->
- Nuclear Weapons are Banned  <!-- TXT_KEY_PROJECTHELP_NO_NUKES -->
- Requires someone to create %s2_Name  <!-- TXT_KEY_PROJECTHELP_REQUIRES_ANYONE -->
- Requires %s2_Name (%d3/%d4)  <!-- TXT_KEY_PROJECTHELP_REQUIRES -->
- Requires %s2_Name (%d3)  <!-- TXT_KEY_PROJECTHELP_REQUIRES_NO_CITY -->
- *%s1_Name Victory must be Enabled  <!-- TXT_KEY_PROJECTHELP_REQUIRES_STRING_VICTORY -->
- *May only exist on a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_PREREQUISITE -->
- %d1_Num Turns  <!-- TXT_KEY_PROJECTHELP_NUM_TURNS -->
-   <!-- TXT_KEY_COLOR_POSITIVE -->
- Double Production speed with %s2_Bonus  <!-- TXT_KEY_PROJECTHELP_DOUBLE_SPEED_WITH -->
- Builds %d1_Mod%% Faster with %s3_Bonus  <!-- TXT_KEY_PROJECTHELP_BUILDS_FASTER_WITH -->

## `setPromotionHelp`

- <name of the thing>

## `setReligionHelp`

- <name of the thing>
- Holy City (if State <religion>)  <!-- TXT_KEY_RELIGION_HOLY_CITY -->
- All Cities (if State <religion>)  <!-- TXT_KEY_RELIGION_ALL_CITIES -->
- *Founded by First to Discover %s2  <!-- TXT_KEY_RELIGION_FOUNDED_FIRST -->
- *Founder Receives %s1_UnitName (%d2_num)  <!-- TXT_KEY_RELIGION_FOUNDER_RECEIVES_NUM -->
- *Founder Receives a %s1_UnitName  <!-- TXT_KEY_RELIGION_FOUNDER_RECEIVES -->

## `setReligionHelpCity`

- <name of the thing>
- *Founded by First to Discover %s2  <!-- TXT_KEY_RELIGION_FOUNDED_FIRST -->
- %D1%%<hammer> for Buildings  <!-- TXT_KEY_RELIGION_BUILDING_PROD_MOD -->
- %D1%%<hammer> for Units  <!-- TXT_KEY_RELIGION_UNIT_PROD_MOD -->
- %D1 XP  <!-- TXT_KEY_RELIGION_FREE_XP -->
- %D1%% <greatperson> Birth Rate  <!-- TXT_KEY_RELIGION_BIRTH_RATE_MOD -->

## `setResumableGoodBadChangeHelp`

- → `setResumableValueChangeHelp`

## `setResumableYieldChangeHelp`

- (Per Population)  <!-- TXT_KEY_PER_POP -->

## `setRevolutionHelp`

- Cannot change Civics  <!-- TXT_KEY_MISC_CANNOT_CHANGE_CIVICS -->
- While in Anarchy  <!-- TXT_KEY_MISC_WHILE_IN_ANARCHY -->
- because of recent Revolution  <!-- TXT_KEY_MISC_ANOTHER_REVOLUTION_RECENTLY -->
- Wait %d1_Num more [NUM1:Turn:Turns]  <!-- TXT_KEY_MISC_WAIT_MORE_TURNS -->

## `setRouteHelp`

- <name of the thing>
- → `setYieldChangeHelp`
- *Route is a Sea Tunnel  <!-- TXT_KEY_ROUTE_SEA_TUNNEL -->
- *Movement Cost: %s1  <!-- TXT_KEY_ROUTE_MOVEMENT_COST -->
- *Flat Movement Cost: All units may move %d1_Change tiles along this route  <!-- TXT_KEY_ROUTE_FLAT_MOVEMENT_COST -->
- *Requires %s1_Bonus  <!-- TXT_KEY_ROUTE_REQUIRES_BONUS -->
- OR %s1_Bonus  <!-- TXT_KEY_ROUTE_REQUIRES_BONUS_OR -->
- *%s1_Change if the %s2_Tech Tech is known.  <!-- TXT_KEY_MOVEMENT_ROUTE_WITH_TECH -->
- → `buildDisplayString`

## `setScoreHelp`

- %d1 from Population (%d2_pop/%d3_pop_max)%d4 from Land (%d5_land/%d6_land_max)%d7 from Technology (%d8_tech/%d9_tech_max)%d10 from Wonders (%d11_wonder/%d12_wonder_max)------------------------Total Score = %d13Score by winning this turn = %d14  <!-- TXT_KEY_SCORE_BREAKDOWN -->

## `setTechHelp`

- <name of the thing>
- → `buildTechTreeString`
- → `buildObsoleteString`
- → `buildObsoleteBonusString`
- → `buildObsoleteSpecialString`
- → `buildMoveString`
- → `buildFreeUnitString`
- → `buildFeatureProductionString`
- → `buildWorkerRateString`
- → `buildMaintenanceModifiersString`
- → `buildTradeRouteString`
- → `buildHealthRateString`
- → `buildSpecialistHealthString`
- → `buildHappinessRateString`
- → `buildSpecialistHappinessString`
- → `buildFreeTechString`
- → `buildLOSString`
- → `buildMapCenterString`
- → `buildMapRevealString`
- → `buildMapTradeString`
- → `buildTechTradeString`
- → `buildGoldTradeString`
- → `buildOpenBordersString`
- → `buildDefensivePactString`
- → `buildPermanentAllianceString`
- → `buildEmbassyString`
- → `buildCanPassPeaksString`
- → `buildMoveFastPeaksString`
- → `buildCanFoundOnPeaksString`
- → `buildCanRebaseAnywhereString`
- *%d1%% Inflation  <!-- TXT_KEY_ADJUSTS_INFLATION -->
- *%d1%% Income from trade routes  <!-- TXT_KEY_TRADE_INCOME -->
- *%d1%% Income from foreign trade routes  <!-- TXT_KEY_FOREIGN_TRADE_INCOME -->
- *%d1%% Income from trade missions  <!-- TXT_KEY_TRADE_MISSION_INCOME -->
- *%d1%% Income from Corporations  <!-- TXT_KEY_CORPORATIONS_REVENUE -->
- *%d1%% Corporation Maintenance  <!-- TXT_KEY_TECHHELP_CORPORATION_MAINTENANCE -->
- *Can Farm Desert Tiles  <!-- TXT_KEY_TECHHELP_ENABLES_DESERT_FARMING -->
- *Enables folklore heritage.  <!-- TXT_KEY_TECHHELP_LANGUAGE -->
- *Can only be researched once in a game.  <!-- TXT_KEY_TECHHELP_GLOBAL -->
- *%D1_Change Free %s3_SpclstName  <!-- TXT_KEY_BUILDINGHELP_FREE_SPECIALIST -->
- in all cities  <!-- TXT_KEY_BUILDINGHELP_GLOBAL -->
- *Allows %s1 to upgrade to %s2.  <!-- TXT_KEY_TECHHELP_ALLOWS_IMPROVEMENT_UPGRADE -->
- → `buildBridgeString`
- → `buildIrrigationString`
- → `buildIgnoreIrrigationString`
- → `buildWaterWorkString`
- → `buildVassalStateString`
- → `buildImprovementString`
- → `buildDomainExtraMovesString`
- → `buildAdjustString`
- → `buildTerrainTradeString`
- → `buildRiverTradeString`
- → `buildSpecialBuildingString`
- → `buildBuildingTechSpecialistChangeString`
- → `buildBuildingTechHappinessChangesString`
- → `buildBuildingTechHealthChangesString`
- → `buildYieldChangeString`
- → `buildBonusRevealString`
- → `buildCivicRevealString`
- *Can Train  <!-- TXT_KEY_TECHHELP_CAN_TRAIN -->
- → `setListHelp`
- *Can Construct  <!-- TXT_KEY_TECHHELP_CAN_CONSTRUCT -->
- *Can Create  <!-- TXT_KEY_TECHHELP_CAN_CREATE -->
- → `buildProcessInfoString`
- → `buildFoundReligionString`
- *First to Discover Receives a Free Great Prophet  <!-- TXT_KEY_TECHHELP_FIRST_FREE_PROPHET -->
- → `buildFoundCorporationString`
- → `buildPromotionString`
- Obsoletes %s2_TechName  <!-- TXT_KEY_TECHHELP_OBSOLETES -->
- → `buildSingleLineTechTreeString`
- Requires %s2 (%d3 Total) in any city  <!-- TXT_KEY_HELPTEXT_REQUIRES_NUM_BUILDINGS_0 -->
- Requires %s2 (%d3/%d4 Total) in any city  <!-- TXT_KEY_HELPTEXT_REQUIRES_NUM_BUILDINGS_1 -->
- Requires  <!-- TXT_KEY_REQUIRES -->
- or  <!-- TXT_KEY_OR -->
- *%d1_Num Turns  <!-- TXT_KEY_TECHHELP_NUM_TURNS -->
- *Can be researched by a %s1_great_person  <!-- TXT_KEY_TECHHELP_GREAT_PERSON_DISCOVER -->
- *Advances to the %s1_era Era  <!-- TXT_KEY_TECHHELP_ERA_ADVANCE -->
- Sid's Tips:  <!-- TXT_KEY_SIDS_TIPS -->

## `setTerrainHelp`

- <name of the thing>
- → `setYieldChangeHelp`
- *Movement Cost: %d1<moves>  <!-- TXT_KEY_TERRAINHELP_MOVEMENT_COST -->
- *Improvements take %D1%% time to build  <!-- TXT_KEY_TERRAINHELP_BUILD_MODIFIER -->
- *Defending units get %D1%% strength  <!-- TXT_KEY_TERRAINHELP_DEFENSE_MODIFIER -->
- *Impassable terrain  <!-- TXT_KEY_TERRAINHELP_IMPASSABLE -->
- *Cannot build cities here  <!-- TXT_KEY_TERRAINHELP_NO_CITIES -->
- unless they are coastal  <!-- TXT_KEY_TERRAINHELP_COASTAL_CITIES -->
- or  <!-- TXT_KEY_OR -->
- unless they are near fresh water  <!-- TXT_KEY_TERRAINHELP_FRESH_WATER_CITIES -->
- (key not in current GameText)  <!-- TXT_KEY_TERRAINHELP_TURN_DAMAGE -->
- → `buildDisplayString`
- This is a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_TERRAIN -->

## `setTradeRouteHelp`

- %s1 <commerce>: Base profit  <!-- TXT_KEY_TRADE_ROUTE_HELP_BASE -->
- %D2%%: for %s1_building  <!-- TXT_KEY_TRADE_ROUTE_MOD_BUILDING -->
- %D1%%: for Population  <!-- TXT_KEY_TRADE_ROUTE_MOD_POPULATION -->
- %D1%%: for connection to Capital  <!-- TXT_KEY_TRADE_ROUTE_MOD_CAPITAL -->
- %D1%%: from technologies  <!-- TXT_KEY_TRADE_ROUTE_TECH -->
- %D1%%: for Overseas Trade  <!-- TXT_KEY_TRADE_ROUTE_MOD_OVERSEAS -->
- %D1%%: for Foreign Trade  <!-- TXT_KEY_TRADE_ROUTE_MOD_FOREIGN -->
- %D1%%: for sustained Peace  <!-- TXT_KEY_TRADE_ROUTE_MOD_PEACE -->
- %s1 <commerce>: Total  <!-- TXT_KEY_TRADE_ROUTE_TOTAL_FRACTIONAL -->
- %d1 <commerce>: Total  <!-- TXT_KEY_TRADE_ROUTE_TOTAL -->

## `setUnitCombatHelp`

- <name of the thing>
- *Adds a reason for the unit to be considered an Exile.  <!-- TXT_KEY_PROMOTIONHELP_EXCILE_ADD -->
- *Removes a reason for the unit to be considered an Exile.  <!-- TXT_KEY_PROMOTIONHELP_EXCILE_REMOVE -->
- *Adds a reason for the unit to be considered qualified to pass through territories with a Right of Passage or Open Borders agreement.  <!-- TXT_KEY_PROMOTIONHELP_PASSAGE_ADD -->
- *Removes a reason for the unit to be considered qualified to pass through territories with a Right of Passage or Open Borders agreement.  <!-- TXT_KEY_PROMOTIONHELP_PASSAGE_REMOVE -->
- *Adds a reason for the unit to be restricted from entering a city that is not your own without attacking it.  <!-- TXT_KEY_PROMOTIONHELP_NONONOWNED_ADD -->
- *Removes a reason for the unit to be restricted from entering a city that is not your own without attacking it.  <!-- TXT_KEY_PROMOTIONHELP_NONONOWNED_REMOVE -->
- *Adds a reason for the unit to treat Non-Animal Barbarians as friendly units and Barbarian cities as if they are your own.  <!-- TXT_KEY_PROMOTIONHELP_BARBCOEXIST_ADD -->
- *Removes a reason for the unit to treat Non-Animal Barbarians as friendly units and Barbarian cities as if they are your own.  <!-- TXT_KEY_PROMOTIONHELP_BARBCOEXIST_REMOVE -->
- *Adds a reason for the unit to peacefully enter all cities, even those you are at war with.  <!-- TXT_KEY_PROMOTIONHELP_BLENDCITY_ADD -->
- *Removes a reason for the unit to peacefully enter all cities, even those you are at war with.  <!-- TXT_KEY_PROMOTIONHELP_BLENDCITY_REMOVE -->
- *Invisible to All Units  <!-- TXT_KEY_UNITHELP_INVISIBLE_ALL -->
- *%D1_Change Vision Range  <!-- TXT_KEY_IMPROVEMENTHELP_VISIBILITY_RANGE -->
- *%D1_Mod <icon> Spot Range.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_SPOT_RANGE_CHANGE -->
- *%D1_Mod <icon> Spot on Same Tile.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_SPOT_SAME_TILE_CHANGE -->
- *%D1_Mod <icon> Veil.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_VEIL_INTENSITY_CHANGE -->
- *%D1_Mod <icon> Veil on %s3_Type.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_VEIL_PLOT_CHANGE -->
- *%D1_Mod <icon> Spot on %s3_TypeName.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_SPOT_PLOT_CHANGE -->
- *%D1_Mod <icon> Spot Range on %s3_TypeName.  <!-- TXT_KEY_PROMOHELP_INVISIBILITY_SPOT_PLOT_RANGE_CHANGE -->
- *%D1_Change Aid (%s2_Type)  <!-- TXT_KEY_PROMOTIONHELP_AID_CHANGE -->
- *%D1_Change Movement Range  <!-- TXT_KEY_PROMOTIONHELP_MOVE -->
- *%D1_Discount Terrain Movement Cost  <!-- TXT_KEY_PROMOTIONHELP_MOVE_DISCOUNT -->
- *%D1_Change Operational Range  <!-- TXT_KEY_PROMOTIONHELP_AIR_RANGE -->
- *%d1_percent%% Enemy Spy Detection Bonus  <!-- TXT_KEY_PROMOTIONHELP_INTERCEPT_SPY -->
- *%d1_percent%% Bonus To Counter Espionage Missions  <!-- TXT_KEY_PROMOTIONHELP_INTERCEPT_SPY_COUNTER -->
- *%D1_Change%% Interception Chance  <!-- TXT_KEY_PROMOTIONHELP_INTERCEPT -->
- *%d1_percent%% Detection Evasion Bonus  <!-- TXT_KEY_PROMOTIONHELP_EVASION_SPY -->
- *%D1_Change%% Evasion Chance  <!-- TXT_KEY_PROMOTIONHELP_EVASION -->
- *%d1_percent%% Bonus Escape Chance  <!-- TXT_KEY_PROMOTIONHELP_ESCAPE_SPY -->
- *%D1_Change%% Withdrawal Chance  <!-- TXT_KEY_PROMOTIONHELP_WITHDRAWAL -->
- *%D1_Change Cargo Space  <!-- TXT_KEY_PROMOTIONHELP_CARGO -->
- *%D1_Change Cargo Space (in Unit Volumes)  <!-- TXT_KEY_PROMOTIONHELP_SM_CARGO -->
- *%D1_DmgChange%% Collateral Damage  <!-- TXT_KEY_PROMOTIONHELP_COLLATERAL_DAMAGE -->
- *Ranged Assault Distance: %D1_Value  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_RANGE -->
- *Ranged Assault Accuracy: %D1_Value%%  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_ACCURACY -->
- *Ranged Assault Damage: %D1_Value%%  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_DAMAGE -->
- *Ranged Assault Damage Limit: %D1_Value%%  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_DAMAGE_LIMIT -->
- *Ranged Assault Max Targets: %D1_Value  <!-- TXT_KEY_PROMOTIONHELP_DCM_BOMB_DAMAGE_MAX_UNITS -->
- *Ranged Assault is likely to strike lead defender  <!-- TXT_KEY_PROMOTIONHELP_RANGED_BOMBARD_DIRECT -->
- *%D1_DmgChange%% City Bombard Damage  <!-- TXT_KEY_PROMOTIONHELP_BOMBARD -->
- *%d1_Num Extra First Strike  <!-- TXT_KEY_PROMOTIONHELP_FIRST_STRIKE -->
- *%d1_Num Extra First Strike Chance  <!-- TXT_KEY_PROMOTIONHELP_FIRST_STRIKE_CHANCE -->
- *This Unit Cannot Heal Without Assistance  <!-- TXT_KEY_UNITHELP_SELF_HEAL_NONE -->
- *Self heal: %d1%%  <!-- TXT_KEY_UNITHELP_SELF_HEAL -->
- *%d1_percent%% Bonus Unrest From Missions  <!-- TXT_KEY_PROMOTIONHELP_INSTIGATE_SPY -->
- *Heals Extra %d1_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_EXTRA -->
- Damage/Turn in Enemy Lands  <!-- TXT_KEY_PROMOTIONHELP_ENEMY_LANDS -->
- *%d1_percent%% Bonus City Revolt From Missions  <!-- TXT_KEY_PROMOTIONHELP_INSTIGATE2_SPY -->
- Damage/Turn in Neutral Lands  <!-- TXT_KEY_PROMOTIONHELP_NEUTRAL_LANDS -->
- *%d1_percent%% Bonus Unhealthiness From Missions  <!-- TXT_KEY_PROMOTIONHELP_POISON_SPY -->
- Damage/Turn in Friendly Lands  <!-- TXT_KEY_PROMOTIONHELP_FRIENDLY_LANDS -->
- *Can heal %D1_Amount unit(s)/turn  <!-- TXT_KEY_PROMOTIONHELP_HEAL_SUPPORT -->
- *Heals Units in Same Tile Extra %d1_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_SAME -->
- Damage/Turn  <!-- TXT_KEY_PROMOTIONHELP_DAMAGE_TURN -->
- *Heals Units in Adjacent Tiles Extra %d1_Heals%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_ADJACENT -->
- *%D1_Bonus%% Strength  <!-- TXT_KEY_PROMOTIONHELP_STRENGTH -->
- *%d1_Amount%% VS Each Size Rank Larger  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_MOD_PER_SIZE_MORE -->
- *%d1_Amount%% VS Each Size Rank Smaller  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_MOD_PER_SIZE_LESS -->
- *%d1_Amount%% VS Each Group Rank Larger  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_MOD_PER_VOLUME_MORE -->
- *%d1_Amount%% VS Each Group Rank Smaller  <!-- TXT_KEY_PROMOTIONHELP_UNIT_COMBAT_MOD_PER_VOLUME_LESS -->
- *%D1_Bonus%% City Attack  <!-- TXT_KEY_PROMOTIONHELP_CITY_ATTACK -->
- *%D1_Bonus%% City Defense  <!-- TXT_KEY_PROMOTIONHELP_CITY_DEFENSE -->
- *%D1_Change%% Hills Attack  <!-- TXT_KEY_UNITHELP_HILLS_ATTACK -->
- *%D1_Bonus%% Hills Defense  <!-- TXT_KEY_PROMOTIONHELP_HILLS_DEFENSE -->
- *%D1_Change%% Work Speed on Hills  <!-- TXT_KEY_PROMOTIONHELP_HILLS_WORK -->
- *%D1_Change%% Work Speed  <!-- TXT_KEY_PROMOTIONHELP_WORK_RATE -->
- *%d1_Change Cultural Revolt Protection  <!-- TXT_KEY_TEMP_REVOLT_PROTECTION -->
- *Suffers %d1_percent%% less Collateral Damage  <!-- TXT_KEY_PROMOTIONHELP_COLLATERAL_PROTECTION -->
- *Yields %D1_percent%% <gold> from Pillaging  <!-- TXT_KEY_PROMOTIONHELP_PILLAGE_CHANGE -->
- *%d1_percent%% Free Preparation Bonus  <!-- TXT_KEY_PROMOTIONHELP_UPGRADE_DISCOUNT_SPY -->
- *Free upgrades  <!-- TXT_KEY_PROMOTIONHELP_UPGRADE_DISCOUNT_FREE -->
- *%d1_percent%% less <gold> to Upgrade  <!-- TXT_KEY_PROMOTIONHELP_UPGRADE_DISCOUNT -->
- *Gains %D1_percent%% Experience from Combat  <!-- TXT_KEY_PROMOTIONHELP_FASTER_EXPERIENCE -->
- *%D1_percent%% for Self-Destruction  <!-- TXT_KEY_COMBAT_KAMIKAZE_MOD -->
- *%d1%% Change to Air Combat Limit  <!-- TXT_KEY_PROMOTIONHELP_AIR_LIMIT_CHANGE -->
- *%D1_Happy <happy> to any city the unit is in.  <!-- TXT_KEY_PROMOTIONHELP_CELEBRITY -->
- *%d1%% Change to Collateral Damage Limit  <!-- TXT_KEY_PROMOTIONHELP_COLLATERAL_LIMIT_CHANGE -->
- *+%d1 Increase to Maximum Number of Units affected by Collateral Damage.  <!-- TXT_KEY_PROMOTIONHELP_MAX_UNITS_CHANGE -->
- *%d1%% change to Maximum Damage Limit  <!-- TXT_KEY_PROMOTIONHELP_COMBAT_LIMIT -->
- *+%d1 Paradrop Range  <!-- TXT_KEY_PROMOTIONHELP_EXTRA_DROP_RANGE -->
- *+%d1%% Chance to Survive a Combat Loss  <!-- TXT_KEY_PROMOTIONHELP_SURVIVOR -->
- *%d1%% chance to heal self, %d2%% chance to heal all friendly units on the tile and %d3%% chance to heal all friendly units on an adjacent tile on a combat victory  <!-- TXT_KEY_UNITHELP_VICTORY_ADJACENT -->
- *+%d1%% Chance to Heal on Combat Victory  <!-- TXT_KEY_PROMOTIONHELP_VICTORY_HEAL -->
- *%d1%% chance to heal self and %d2%% chance to heal all friendly units on the tile on a combat victory  <!-- TXT_KEY_UNITHELP_VICTORY_STACK -->
- *%D1_Change%% modifier to Attack  <!-- TXT_KEY_PROMOTIONHELP_ATTACK_MODIFIER -->
- *%D1_Change%% modifier to Defense  <!-- TXT_KEY_PROMOTIONHELP_DEFENSE_MODIFIER -->
- *Pursuit Chance: %D1_Change%%  <!-- TXT_KEY_PROMOTIONHELP_PURSUIT -->
- *Starts Withdrawal at %D1_Change%% HP  <!-- TXT_KEY_PROMOTIONHELP_EARLY_WITHDRAW -->
- *+%d1_Change%% vs Non-Animal Barbarians  <!-- TXT_KEY_PROMOTIONHELP_VSBARBS -->
- *%D1_Amount%% Religious Combat Modifier  <!-- TXT_KEY_UNITHELP_RELIGIOUS_COMBAT_MODIFIER_SHORT -->
- *%D1_Change%% Armor Value  <!-- TXT_KEY_PROMOTIONHELP_ARMOR -->
- *%D1_Change Puncture Value  <!-- TXT_KEY_PROMOTIONHELP_PUNCTURE -->
- *Damage Modifier: %D1_Change%  <!-- TXT_KEY_PROMOTIONHELP_DAMAGE_MODIFIER -->
- *Base Unit Upkeep change: %d1%%  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_MODIFIER_BASE -->
- *Extra Unit Upkeep: %s1 <gold>  <!-- TXT_KEY_HELPTEXT_UNIT_UPKEEP_EXTRA -->
- *%D1_Change%% Overrun vs fortify bonuses.  <!-- TXT_KEY_PROMOTIONHELP_OVERRUN -->
- *%D1_Change%% Repel vs Attackers.  <!-- TXT_KEY_PROMOTIONHELP_REPEL -->
- *%D1_Change%% Repel value per turn fortified.  <!-- TXT_KEY_PROMOTIONHELP_FORT_REPEL -->
- *%D1_Change Repel attempts  <!-- TXT_KEY_PROMOTIONHELP_REPEL_RETRIES -->
- *%D1_Change%% Unyielding value vs Knockback/Repel.  <!-- TXT_KEY_PROMOTIONHELP_UNYIELDING -->
- *%D1_Change%% chance per round to Knockback defenders.  <!-- TXT_KEY_PROMOTIONHELP_KNOCKBACK -->
- *%D1_Change Knockback attempts  <!-- TXT_KEY_PROMOTIONHELP_KNOCKBACK_RETRIES -->
- (key not in current GameText)  <!-- TXT_KEY_PROMOTIONHELP_RAMPAGE -->
- *%D1_Change%% Reflexes (additional withdrawal likelihood per Attack in a given turn.)  <!-- TXT_KEY_PROMOTIONHELP_REFLEXES -->
- *%D1_Change%% Frays (less withdrawal likelihood per Attack in a given turn.)  <!-- TXT_KEY_PROMOTIONHELP_FRAYS -->
- *%D1_Change%% Unnerve Value  <!-- TXT_KEY_PROMOTIONHELP_UNNERVE -->
- *%D1_Change%% Enclose Value  <!-- TXT_KEY_PROMOTIONHELP_ENCLOSE -->
- *%D1_Change%% Lunge Value  <!-- TXT_KEY_PROMOTIONHELP_LUNGE -->
- *%D1_Change%% Dynamic Defense Value  <!-- TXT_KEY_PROMOTIONHELP_DYNAMIC_DEFENSE -->
- *Strengthen: %D1_Change Strength  <!-- TXT_KEY_PROMOTIONHELP_STRENGTHEN -->
- *Weaken: %D1_Change Strength  <!-- TXT_KEY_PROMOTIONHELP_WEAKEN -->
- *%D1_Change%% Fortitude  <!-- TXT_KEY_PROMOTIONHELP_FORTITUDE_CHANGE -->
- *+%d1_Amount%% to Front Support Value  <!-- TXT_KEY_PROMOTIONHELP_FRONT_SUPPORT_PERCENT_CHANGE -->
- *+%d1_Amount%% to Short Range Support Value  <!-- TXT_KEY_PROMOTIONHELP_SHORT_RANGE_SUPPORT_PERCENT_CHANGE -->
- *+%d1_Amount%% to Medium Range Support Value  <!-- TXT_KEY_PROMOTIONHELP_MEDIUM_RANGE_SUPPORT_PERCENT_CHANGE -->
- *+%d1_Amount%% to Long Range Support Value  <!-- TXT_KEY_PROMOTIONHELP_LONG_RANGE_SUPPORT_PERCENT_CHANGE -->
- *+%d1_Amount%% to Flank Support Value  <!-- TXT_KEY_PROMOTIONHELP_FLANK_SUPPORT_PERCENT_CHANGE -->
- *Additional %D1_Change%% Dodge  <!-- TXT_KEY_PROMOTIONHELP_DODGE_MODIFIER -->
- *Additional %D1_Change%% Precision  <!-- TXT_KEY_PROMOTIONHELP_PRECISION_MODIFIER -->
- *Additional %D1_Change Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOTS -->
- *Additional %D1_Change%% Combat during Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOT_COMBAT_MODIFIER -->
- *Additional %D1_Change Puncture during Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOT_PUNCTURE_MODIFIER -->
- *Additional %D1_Change Precision during Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOT_PRECISION_MODIFIER -->
- *Additional %D1_Change%% Critical Hit Chance per round of battle during Power Shots  <!-- TXT_KEY_PROMOTIONHELP_POWER_SHOT_CRITICAL_MODIFIER -->
- *Additional %D1_Change%% Critical Hit Chance per round of battle  <!-- TXT_KEY_PROMOTIONHELP_CRITICAL_MODIFIER -->
- *%D1_Change Endurance  <!-- TXT_KEY_PROMOTIONHELP_ENDURANCE -->
- *%D1_Change%% Round Stun Probability  <!-- TXT_KEY_PROMOTIONHELP_ROUND_STUN_PROB -->
- *%D1_Change%% Poison Probability Modifier  <!-- TXT_KEY_PROMOTIONHELP_POISON_PROB -->
- *%D1%% chance to Capture.  <!-- TXT_KEY_UNITHELP_CAPTURE_PROBABILITY_MODIFIER -->
- *%D1%% chance to avoid Capture.  <!-- TXT_KEY_UNITHELP_CAPTURE_RESISTANCE_MODIFIER -->
- *%D1_Change%% Work Speed on Peaks  <!-- TXT_KEY_PROMOTIONHELP_PEAKS_WORK -->
- *Breakdown Chance: %D1%%  <!-- TXT_KEY_UNITHELP_BREAKDOWN_CHANCE -->
- *Breakdown Amount: %D1%%  <!-- TXT_KEY_UNITHELP_BREAKDOWN_DAMAGE -->
- *Taunt Chance: %D1%%  <!-- TXT_KEY_UNITHELP_TAUNT -->
- *Max HP: %D1%% (usually 100.)  <!-- TXT_KEY_PROMOTIONHELP_MAX_HP -->
- *Strength Modifier: %D1%% (Subject to adjustment by Diminishing Return)  <!-- TXT_KEY_PROMOTIONHELP_STRENGTH_MODIFIER -->
- *%D1_Change Stealth Strikes  <!-- TXT_KEY_PROMOTIONHELP_STEALTH_STRIKES -->
- *%D1_Change%% Stealth Combat Modifier  <!-- TXT_KEY_PROMOTIONHELP_STEALTH_COMBAT_MODIFIER -->
- *Trait - Stealth Defend: %D1_Change  <!-- TXT_KEY_PROMOTIONHELP_STEALTH_DEFENSE_CHANGE -->
- *Trait - Defense only: %D1_Change  <!-- TXT_KEY_PROMOTIONHELP_DEFENSE_ONLY_CHANGE -->
- *Trait - Never Invisible: %D1_Change  <!-- TXT_KEY_PROMOTIONHELP_NO_INVISIBILITY_CHANGE -->
- *Gives cause for unit to be unable to capture enemy Cities or Units.  <!-- TXT_KEY_PROMOTIONHELP_ADDS_CANNOT_CAPTURE -->
- *Removes a cause for unit to be unable to capture enemy Cities or Units.  <!-- TXT_KEY_PROMOTIONHELP_REMOVES_CANNOT_CAPTURE -->
- *%D1_Mod%% Withdraw when battle is on %s3_TypeName  <!-- TXT_KEY_PROMOTIONHELP_WITHDRAW_ON -->
- *+1 Move on Defensive Combat Victories  <!-- TXT_KEY_PROMOTIONHELP_DV_MOVE -->
- *No Movement Cost to Paradrop and Can Attack After Paradrop  <!-- TXT_KEY_PROMOTIONHELP_FREE_DROP -->
- *Can Paradrop on FoW Tiles  <!-- TXT_KEY_PROMOTIONHELP_DROP_SIGHT_UNSEEN -->
- *+1 Move on Offensive Combat Victories  <!-- TXT_KEY_PROMOTIONHELP_OV_MOVE -->
- *Number of Extra Lives: %d1  <!-- TXT_KEY_UNITHELP_ONEUP -->
- *Pillages <spy> in addition to <gold>  <!-- TXT_KEY_PROMOTIONHELP_ESPIONAGE_PILLAGE -->
- *Pillages twice the <gold> and potentially twice the Improvement.  <!-- TXT_KEY_PROMOTIONHELP_MARAUDER_PILLAGE -->
- *Automatically pillages Improvements on move.  <!-- TXT_KEY_PROMOTIONHELP_MOVING_PILLAGE -->
- *Reaps the Profits of Pillaging on a Combat Victory (no loss of Improvement though).  <!-- TXT_KEY_PROMOTIONHELP_VICTORY_PILLAGE -->
- *Pillages <beaker> in addition to <gold>  <!-- TXT_KEY_PROMOTIONHELP_RESEARCH_PILLAGE -->
- *Can Attack Multiple Times per Turn  <!-- TXT_KEY_PROMOTIONHELP_BLITZ -->
- *Creates fallout on sabotaged Improvements.  <!-- TXT_KEY_PROMOTIONHELP_RADIATION_SPY -->
- *No Combat Penalty for Attacking from Sea  <!-- TXT_KEY_PROMOTIONHELP_AMPHIB -->
- *No Combat Penalty for Crossing River  <!-- TXT_KEY_PROMOTIONHELP_RIVER_ATTACK -->
- *Can Use Enemy Roads  <!-- TXT_KEY_PROMOTIONHELP_ENEMY_ROADS -->
- *Never Reveals Nationality  <!-- TXT_KEY_PROMOTIONHELP_LOYALTY_SPY -->
- *Can Heal while Moving  <!-- TXT_KEY_PROMOTIONHELP_ALWAYS_HEAL -->
- *Double Movement in Hills  <!-- TXT_KEY_PROMOTIONHELP_HILLS_MOVE -->
- *Immune to First Strikes  <!-- TXT_KEY_PROMOTIONHELP_IMMUNE_FIRST_STRIKES -->
- *Causes unit to fight until it or all defenders are dead.  <!-- TXT_KEY_PROMOTIONHELP_STAMPEDE -->
- *Eliminates one source of Stampede Ability.  <!-- TXT_KEY_PROMOTIONHELP_REMOVE_STAMPEDE -->
- *Gives this Animal %D1_Reasons Causes to Ignore Border Restrictions.  <!-- TXT_KEY_PROMOTIONHELP_ANIMAL_IGNORES_BORDERS -->
- *Gives %D1_Reasons Cause(s) to be unable to benefit from Defense Combat bonuses.  <!-- TXT_KEY_PROMOTIONHELP_NO_DEFENSIVE_BONUS_CHANGE_POSITIVE -->
- *Removes %D1_Reasons Cause(s) to be unable to benefit from Defense Combat bonuses.  <!-- TXT_KEY_PROMOTIONHELP_NO_DEFENSIVE_BONUS_CHANGE_NEGATIVE -->
- *Continues attack while at full <strength>  <!-- TXT_KEY_PROMOTIONHELP_ONSLAUGHT -->
- *Makes the Damage this Unit deals Cold Damage.  <!-- TXT_KEY_PROMOTIONHELP_MAKES_DAMAGE_COLD -->
- *Removes a cause this unit would have to be dealing Cold Damage.  <!-- TXT_KEY_PROMOTIONHELP_MAKES_DAMAGE_NOT_COLD -->
- *Causes unit to be immune to Cold Damage penalties.  <!-- TXT_KEY_PROMOTIONHELP_ADDS_COLD_IMMUNITY -->
- *Removes a cause this unit would have to be immune to Cold Damage penalties.  <!-- TXT_KEY_PROMOTIONHELP_REMOVES_COLD_IMMUNITY -->
- *Adds one cause for the unit to only be able to attack cities.  <!-- TXT_KEY_PROMOTIONHELP_ATTACK_ONLY_CITIES_ADD -->
- *Removes one cause for the unit to only be able to attack cities.  <!-- TXT_KEY_PROMOTIONHELP_ATTACK_ONLY_CITIES_SUBTRACT -->
- *Adds one cause for the unit to ignore Minimum Defense to Attack City requirements.  <!-- TXT_KEY_PROMOTIONHELP_IGNORE_NO_ENTRY_LEVEL_ADD -->
- *Removes one cause for the unit to ignore Minimum Defense to Attack City requirements.  <!-- TXT_KEY_PROMOTIONHELP_IGNORE_NO_ENTRY_LEVEL_SUBTRACT -->
- *Adds one cause for the unit to ignore Zones of Control.  <!-- TXT_KEY_PROMOTIONHELP_IGNORE_ZONE_OF_CONTROL_ADD -->
- *Removes one cause for the unit to ignore Zones of Control.  <!-- TXT_KEY_PROMOTIONHELP_IGNORE_ZONE_OF_CONTROL_SUBTRACT -->
- *Adds one cause for the unit to fly when moving.  <!-- TXT_KEY_PROMOTIONHELP_FLIES_TO_MOVE_ADD -->
- *Removes one cause for the unit to fly when moving.  <!-- TXT_KEY_PROMOTIONHELP_FLIES_TO_MOVE_SUBTRACT -->
- *Makes the unit unable to merge or split.  <!-- TXT_KEY_PROMOTIONHELP_CANNOT_MERGE_SPLIT -->
- *Can Pass through Peaks.  <!-- TXT_KEY_PROMOTIONHELP_CAN_MOVE_PEAKS -->
- *Can lead units through Peaks.  <!-- TXT_KEY_PROMOTIONHELP_CAN_LEAD_THROUGH_PEAKS -->
- *Exerts a Zone Of Control on all adjacent tiles  <!-- TXT_KEY_BUILDINGHELP_ZONE_OF_CONTROL -->
- *%D1_Mod%% vs. %s3_Against  <!-- TXT_KEY_PROMOTIONHELP_VERSUS -->
- *Empowers the ability to remove %s1_PromotionName from those afflicted.  <!-- TXT_KEY_PROMOTIONHELP_CURE_AFFLICTION -->
- *Double Movement in %s1_TerrFeatType  <!-- TXT_KEY_PROMOTIONHELP_DOUBLE_MOVE -->
- *%D1_Change%% resistance and overcome bonus against the %s2_PromotionName affliction.  <!-- TXT_KEY_PROMOTIONHELP_AFFLICTION_FORTITUDE_CHANGE_MODIFIER -->
- *Gives %D1_chance% chance to immediately inflict %s2_PromotionName to struck enemy.  <!-- TXT_KEY_PROMOTIONHELP_AFFLICT_ON_ATTACK_IMMEDIATE -->
- *Gives %D1_chance% chance to inflict %s2_PromotionName to struck enemy, delayed until AFTER battle.  <!-- TXT_KEY_PROMOTIONHELP_AFFLICT_ON_ATTACK_NOT_IMMEDIATE -->
- *Gives %D1_chance% chance to inflict %s2_PromotionName to struck enemy.  <!-- TXT_KEY_PROMOTIONHELP_AFFLICT_ON_ATTACK -->
- *Immune to %s1_promo traps.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_IMMUNITY -->
- *%D1_Value%% chance to Avoid Triggering %s2_trap traps.  <!-- TXT_KEY_PROMOTIONHELP_TRAP_AVOID -->
- *%D1_Change%% %s2_TerrOrFeat Attack  <!-- TXT_KEY_UNITHELP_ATTACK -->
- *%D1_Change%% %s2_TerrOrFeat Defense  <!-- TXT_KEY_UNITHELP_DEFENSE -->
- *%D1_Change%% Work Speed on %s2_Gameobject  <!-- TXT_KEY_PROMOTIONHELP_WORK -->
- *%D1_Change%% vs. %s3_Type  <!-- TXT_KEY_UNITHELP_MOD_VS_TYPE -->
- *%d1_Amount Strength Flank attack against %s2_unitlist  <!-- TXT_KEY_UNITHELP_COMBAT_FLANKING_STRIKES -->
- *Withdraw vs. %s3_AgainstName: %D1_Mod%%  <!-- TXT_KEY_PROMOTIONHELP_WITHDRAW_VERSUS -->
- *Pursuit vs. %s3_AgainstName: %D1_Mod%%  <!-- TXT_KEY_PROMOTIONHELP_PURSUIT_VERSUS -->
- *%D1_Mod%% Repel vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_REPEL_VERSUS -->
- *%D1_Mod%% Knockback vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_KNOCKBACK_VERSUS -->
- *%D1_Mod Puncture vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_PUNCTURE_VERSUS -->
- *%D1_Mod Armor vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_ARMOR_VERSUS -->
- *%D1_Mod Dodge vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_DODGE_VERSUS -->
- *%D1_Mod Precision vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_PRECISION_VERSUS -->
- *%D1_Mod%% Critical Chance vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_CRITICAL_VERSUS -->
- *%D1_Mod%% Round Stun Chance vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_ROUND_STUN_VERSUS -->
- *%s1_Change%% Insidiousness  <!-- TXT_KEY_PROMOTIONHELP_INSIDIOUSNESS -->
- *%s1_Change%% Investigation  <!-- TXT_KEY_PROMOTIONHELP_INVESTIGATION -->
- → `buildDisplayString`

## `setUnitExperienceHelp`

- %d1 XP (Drafted Units get half normal XP)  <!-- TXT_KEY_MISC_EXPERIENCE_DRAFT -->
- %d1 XP  <!-- TXT_KEY_MISC_EXPERIENCE -->
- %d1<star>  <!-- TXT_KEY_MISC_PROMOTIONS -->

## `setUnitHelp`

- <name of the thing>
- *World Unit (%d1_Num Allowed)  <!-- TXT_KEY_UNITHELP_WORLD_UNIT_ALLOWED -->
- World Unit: %d1_Num Left  <!-- TXT_KEY_UNITHELP_WORLD_UNIT_LEFT -->
- *National Unit (%d1_Num Allowed)  <!-- TXT_KEY_UNITHELP_NATIONAL_UNIT_ALLOWED -->
- National Unit: %d1_Num Left  <!-- TXT_KEY_UNITHELP_NATIONAL_UNIT_LEFT -->
- Each Unit in Play Increases Base Cost by %d1%%  <!-- TXT_KEY_UNITHELP_INSTANCE_COST_MOD -->
- Requires  <!-- TXT_KEY_REQUIRES -->
- → `buildDisplayString`
- → `setListHelp`
- or  <!-- TXT_KEY_OR -->
- Nuclear Weapons are Banned  <!-- TXT_KEY_UNITHELP_NO_NUKES -->
- Requires <religion icon> Holy City  <!-- TXT_KEY_UNITHELP_REQUIRES_HOLY_CITY -->
- Requires %s1_Text in city and in city vicinity, or manufactured locally.  <!-- TXT_KEY_REQUIRES_BONUS_VICINITY -->
- Bonus required in city and in city vicinity, or manufactured locally:  <!-- TXT_KEY_REQUIRES_BONUS_VICINITY_ONEOF -->
- %s2  <!-- TXT_KEY_LINK -->
- Requires %s1_Text  <!-- TXT_KEY_UNITHELP_REQUIRES_STRING -->
-   <!-- TXT_KEY_SET_WARNING_COLOR -->
- and  <!-- TXT_KEY_AND -->
- You must have a State <religion> and it must be in the city.  <!-- TXT_KEY_REQUIRES_STATE_RELIGION -->
- Inquisitional conditions required to train, check civics  <!-- TXT_KEY_NOT_INQUISITION_CONDITIONS -->
- %d1_Num Turns  <!-- TXT_KEY_UNITHELP_TURNS -->
- Will lose %d1<hammer> if not produced this turn  <!-- TXT_KEY_PRODUCTION_DECAY -->
- Will lose %d1<hammer> if delayed for %d2 [NUM2:turn:turns]  <!-- TXT_KEY_PRODUCTION_DECAY_TURNS -->
- Double production speed with %s2_Bonus  <!-- TXT_KEY_UNITHELP_DOUBLE_SPEED -->
- Builds %d1_Change%% Faster with %s3_Bonus  <!-- TXT_KEY_UNITHELP_BUILDS_FASTER -->
- Sid's Tips:  <!-- TXT_KEY_SIDS_TIPS -->

## `setUnitHelp`

- , %d1 Range  <!-- TXT_KEY_UNITHELP_AIR_RANGE -->
- <name of the thing>
- Cannot move (%d1)  <!-- TXT_KEY_UNITHELP_IMMOBILE -->
- , XP: (%s1/%d2)  <!-- TXT_KEY_UNITHELP_XP -->
- , CR: %d1  <!-- TXT_KEY_UNITHELP_COMMAND_RANGE -->
- , CP: %d1/%d2  <!-- TXT_KEY_UNITHELP_COMMAND_POINTS -->
- → `setUnitCombatHelp`
- *This Unit will Stealth Defend  <!-- TXT_KEY_UNITHELP_STEALTH_DEFEND -->
- *Doesn't Receive Defensive Bonuses  <!-- TXT_KEY_UNITHELP_NO_DEFENSE_BONUSES -->
- *Repel/turn fortified: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_FORT_REPEL_TOTAL_SHORT -->
- *+%d1_Amount Repel value/turn Fortified.  <!-- TXT_KEY_UNITHELP_FORT_REPEL_TOTAL_MODIFIER -->
- *Overrun: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_OVERRUN_TOTAL_SHORT -->
- *Overrun: Ignores %d1_Amount%% fortification bonuses.  <!-- TXT_KEY_UNITHELP_OVERRUN_TOTAL_MODIFIER -->
- *%D1_Bonus%% Strength  <!-- TXT_KEY_PROMOTIONHELP_STRENGTH -->
- *%D1_Change%% Stealth Combat Modifier  <!-- TXT_KEY_PROMOTIONHELP_STEALTH_COMBAT_MODIFIER -->
- *%d1_Amount%% Attack Strength  <!-- TXT_KEY_UNITHELP_ATTACK_MODIFIER_SHORT -->
- *%D1_Amount%% Attack Strength  <!-- TXT_KEY_UNITHELP_ATTACK_MODIFIER -->
- *%d1_Amount%% Defence Strength  <!-- TXT_KEY_UNITHELP_DEFENSE_MODIFIER_SHORT -->
- *%D1_Amount%% Defence Strength  <!-- TXT_KEY_UNITHELP_DEFENSE_MODIFIER -->
- *%D1_Change%% City Strength  <!-- TXT_KEY_UNITHELP_CITY_STRENGTH_MOD -->
- *%D1_Bonus%% City Attack  <!-- TXT_KEY_PROMOTIONHELP_CITY_ATTACK -->
- *%D1_Bonus%% City Defense  <!-- TXT_KEY_PROMOTIONHELP_CITY_DEFENSE -->
- *%D1_Change%% vs. Wild Animals  <!-- TXT_KEY_UNITHELP_ANIMAL_COMBAT_MOD -->
- *%D1_Amount%% VS Each Size Rank Larger  <!-- TXT_KEY_UNITHELP_COMBAT_MOD_PER_SIZE_MORE -->
- *%D1_Amount%% VS Each Size Rank Smaller  <!-- TXT_KEY_UNITHELP_COMBAT_MOD_PER_SIZE_LESS -->
- *%D1_Amount%% VS Each Group Rank Larger  <!-- TXT_KEY_UNITHELP_COMBAT_MOD_PER_VOLUME_MORE -->
- *%D1_Amount%% VS Each Group Rank Smaller  <!-- TXT_KEY_UNITHELP_COMBAT_MOD_PER_VOLUME_LESS -->
- *%d1_Amount%% VS Barbarians  <!-- TXT_KEY_UNITHELP_VSBARBS_MODIFIER_SHORT -->
- *%d1_Amount%% VS Non-Animal Barbarians  <!-- TXT_KEY_UNITHELP_VSBARBS_MODIFIER -->
- *%D1_Amount%% Religious Combat Modifier  <!-- TXT_KEY_UNITHELP_RELIGIOUS_COMBAT_MODIFIER_SHORT -->
- *%D1_Amount%% Religious Combat Modifier - applies against units that differ in religion to your unit - inverses to a penalty when foe is of same religion.  <!-- TXT_KEY_UNITHELP_RELIGIOUS_COMBAT_MODIFIER -->
- *%D1_Change%% Hills Strength  <!-- TXT_KEY_UNITHELP_HILLS_STRENGTH -->
- *%D1_Change%% Hills Attack  <!-- TXT_KEY_UNITHELP_HILLS_ATTACK -->
- *%D1_Change%% Hills Defense  <!-- TXT_KEY_UNITHELP_HILLS_DEFENSE -->
- ,  <!-- TXT_KEY_COMMA -->
- *%D1_Change%% %s2_TerrOrFeat Strength  <!-- TXT_KEY_UNITHELP_STRENGTH -->
- *%D1_Change%% %s2_TerrOrFeat Attack  <!-- TXT_KEY_UNITHELP_ATTACK -->
- *%D1_Change%% %s2_TerrOrFeat Defense  <!-- TXT_KEY_UNITHELP_DEFENSE -->
- *%D1_Change%% vs. %s3_Type  <!-- TXT_KEY_UNITHELP_MOD_VS_TYPE -->
- *%D1_Change%% Attack vs. %s3_Class.  <!-- TXT_KEY_UNITHELP_ATTACK_MOD_VS_CLASS -->
- *%D1_Change%% Defense vs. %s3_Class  <!-- TXT_KEY_UNITHELP_DEFENSE_MOD_VS_CLASS -->
- *Armor: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_ARMOR_TOTAL_SHORT -->
- *Armor: Reduces damage per hit: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_ARMOR_TOTAL_MODIFIER -->
- *%D1_Mod Armor vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_ARMOR_VERSUS -->
- *Puncture: %d1_Amount  <!-- TXT_KEY_UNITHELP_PUNCTURE_TOTAL_SHORT -->
- *Puncture: Reduces opponent Armor: %d1_Amount  <!-- TXT_KEY_UNITHELP_PUNCTURE_TOTAL_MODIFIER -->
- *%D1_Mod Puncture vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_PUNCTURE_VERSUS -->
- *Damage: %D1_Amount%  <!-- TXT_KEY_UNITHELP_DAMAGE_MODIFIER_TOTAL_SHORT -->
- *Dodge Chance: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_DODGE_TOTAL_SHORT -->
- *%D1_Mod Dodge vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_DODGE_VERSUS -->
- *Precision Chance: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_PRECISION_TOTAL_SHORT -->
- *%D1_Mod Precision vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_PRECISION_VERSUS -->
- *Maximum %d1%% damage to enemy on attack  <!-- TXT_KEY_UNITHELP_COMBAT_LIMIT -->
- *Can Escape Capture (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_ESCAPE_SPY -->
- *Withdrawal Chance: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_WITHDRAWL_PROBABILITY_SHORT -->
- *Can Withdraw from Combat (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_WITHDRAWL_PROBABILITY -->
- *Starts Withdrawal at %d1_Amount%% HP  <!-- TXT_KEY_UNITHELP_EARLY_WITHDRAW_TOTAL_SHORT -->
- *Reflexes: +%d1_Amount Withdrawal Chance/attack  <!-- TXT_KEY_UNITHELP_REFLEXES_TOTAL_SHORT -->
- *Reflexes: Gains +%d1_Amount Withdraw Chance/attack.  <!-- TXT_KEY_UNITHELP_REFLEXES_TOTAL -->
- *Frays: -%d1_Amount Withdrawal Chance/attack  <!-- TXT_KEY_UNITHELP_FRAYS_TOTAL_SHORT -->
- *Frays: Loses +%d1_Amount Withdraw Chance/attack.  <!-- TXT_KEY_UNITHELP_FRAYS_TOTAL -->
- *Withdraw vs. %s3_AgainstName: %D1_Mod%%  <!-- TXT_KEY_PROMOTIONHELP_WITHDRAW_VERSUS -->
- *%D1_Mod%% Withdraw when battle is on %s3_TypeName  <!-- TXT_KEY_PROMOTIONHELP_WITHDRAW_ON -->
- *Escape route planning is contributing %d1%% to Withdrawal Probability  <!-- TXT_KEY_UNITHELP_ESCAPE_PLAN_MOD -->
- *Pursuit Chance: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_PURSUIT_PROBABILITY_SHORT -->
- *Pursuit vs. %s3_AgainstName: %D1_Mod%%  <!-- TXT_KEY_PROMOTIONHELP_PURSUIT_VERSUS -->
- *Repel: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_REPEL_TOTAL_SHORT -->
- *Repel: Repels attackers %d1_Amount%% chance/round.  <!-- TXT_KEY_UNITHELP_REPEL_TOTAL_MODIFIER -->
- *Repel Retries: %d1_Amount  <!-- TXT_KEY_UNITHELP_REPEL_RETRIES_TOTAL -->
- *Knockback: %d1_Amount%% chance/rnd  <!-- TXT_KEY_UNITHELP_KNOCKBACK_TOTAL_SHORT -->
- *Knockback: Force defender to withdraw, %d1_Amount%% chance/round.  <!-- TXT_KEY_UNITHELP_KNOCKBACK_TOTAL_MODIFIER -->
- *%D1_Mod%% Knockback vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_KNOCKBACK_VERSUS -->
- *Knockback Retries: %d1_Amount  <!-- TXT_KEY_UNITHELP_KNOCKBACK_RETRIES_TOTAL -->
- *Unyielding: %d1_Amount%% vs Knockback/Repel  <!-- TXT_KEY_UNITHELP_UNYIELDING_TOTAL_SHORT -->
- *Unyielding: Ignores %d1_Amount%% Knockback and Repel.  <!-- TXT_KEY_UNITHELP_UNYIELDING_TOTAL_MODIFIER -->
- *Can Attack Multiple Times per Turn  <!-- TXT_KEY_PROMOTIONHELP_BLITZ -->
- *Stampede: Fights to the death  <!-- TXT_KEY_UNITHELP_STAMPEDE_SHORT -->
- *Stampede: Continues attacking until victory or death.  <!-- TXT_KEY_UNITHELP_STAMPEDE_LONG -->
- *Attacks until damaged  <!-- TXT_KEY_UNITHELP_ONSLAUGHT_SHORT -->
- *Attacks repeatedly until at less than full HP.  <!-- TXT_KEY_UNITHELP_ONSLAUGHT_LONG -->
- *Creates fallout on sabotaged Improvements.  <!-- TXT_KEY_PROMOTIONHELP_RADIATION_SPY -->
- *No Combat Penalty for Attacking from Sea  <!-- TXT_KEY_PROMOTIONHELP_AMPHIB -->
- *No Combat Penalty for Crossing River  <!-- TXT_KEY_PROMOTIONHELP_RIVER_ATTACK -->
- *First Strikes: %d1  <!-- TXT_KEY_UNITHELP_FIRST_STRIKES -->
- *First Strikes: %d1-%d2  <!-- TXT_KEY_UNITHELP_FIRST_STRIKE_CHANCES -->
- *%d1_Change Stealth Strikes  <!-- TXT_KEY_UNITHELP_STEALTH_STRIKES -->
- *Immune to First Strikes  <!-- TXT_KEY_UNITHELP_FIRST_STRIKES_IMMUNE -->
- *Breakdown Chance: %D1%%  <!-- TXT_KEY_UNITHELP_BREAKDOWN_CHANCE -->
- *Breakdown Amount: %D1%%  <!-- TXT_KEY_UNITHELP_BREAKDOWN_DAMAGE -->
- *Can Only Attack Cities  <!-- TXT_KEY_UNITHELP_CAN_ONLY_ATTACK_CITIES -->
- *Can Always Attack Cities  <!-- TXT_KEY_UNITHELP_CAN_ALWAYS_ATTACK_CITIES -->
- *Can perform Ranged Attacks  <!-- TXT_KEY_IS_DCM_BOMBARD -->
- *Range: %d1_BombRange  <!-- TXT_KEY_IS_RANGE_BOMBARD -->
- *Accuracy: %d1_BombAccuracy  <!-- TXT_KEY_IS_ACCURACY_BOMBARD -->
- *Ranged Assault Damage: %d1_Value%%  <!-- TXT_KEY_RANGED_BOMBARD_DAMAGE -->
- *Ranged Assault Damage Limit: %d1_Value%%  <!-- TXT_KEY_RANGED_BOMBARD_DAMAGE_LIMIT -->
- *Ranged Assault Max Targets: %d1_Value  <!-- TXT_KEY_RANGED_BOMBARD_DAMAGE_MAX_UNITS -->
- *Bombard City: -%d1_Mod%%/Turn  <!-- TXT_KEY_UNITHELP_BOMBARD_RATE_SHORT -->
- *Can Bombard City Defenses (-%d1_Mod%%/Turn).  <!-- TXT_KEY_UNITHELP_BOMBARD_RATE -->
- *Up to %d1% Collateral to %d2 Unit(s)  <!-- TXT_KEY_UNITHELP_COLLATERAL_DAMAGE_REVDCM -->
- *Causes Collateral Damage (%D1_Amount%%)  <!-- TXT_KEY_UNITHELP_COLLATERAL_DAMAGE_EXTRA -->
- *%d1_Amount Strength Flank attack against %s2_unitlist  <!-- TXT_KEY_UNITHELP_COMBAT_FLANKING_STRIKES -->
- *Can Intercept Enemy Spies (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_INTERCEPT_AIRCRAFT_SPY -->
- *Improved Counter Espionage Missions (%d1_Amount%%)  <!-- TXT_KEY_UNITHELP_INTERCEPT_AIRCRAFT_SPY_COUNTER -->
- *Can Intercept Aircraft (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_INTERCEPT_AIRCRAFT -->
- *Can Evade Enemy Detection (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_EVADE_INTERCEPTION_SPY -->
- *Can Evade Interception (%d1_Amount%% Chance)  <!-- TXT_KEY_UNITHELP_EVADE_INTERCEPTION -->
- Can perform Fighter Engagement mission.  <!-- TXT_KEY_IS_FIGHTER_ENGAGE -->
- *Unnerve: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_UNNERVE_TOTAL_SHORT -->
- *Unnerve: Counts as +%d1_Amount%% strength when surrounding.  <!-- TXT_KEY_UNITHELP_UNNERVE_TOTAL -->
- *Enclose: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_ENCLOSE_TOTAL_SHORT -->
- *Enclose: Adds +%d1_Amount%% to the maximum Surround and Destroy bonus when surrounding.  <!-- TXT_KEY_UNITHELP_ENCLOSE_TOTAL -->
- *Lunge: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_LUNGE_TOTAL_SHORT -->
- *Lunge: Gets +%d1_Amount%% bonus from Surround and Destroy.  <!-- TXT_KEY_UNITHELP_LUNGE_TOTAL -->
- *Dynamic Defense: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_DYNAMIC_DEFENSE_TOTAL_SHORT -->
- *Dynamic Defense: Resists %d1_Amount%% of Surround and Destroy penalties.  <!-- TXT_KEY_UNITHELP_DYNAMIC_DEFENSE_TOTAL -->
- *Power Shots: %d1_Amount  <!-- TXT_KEY_UNITHELP_POWER_SHOTS_TOTAL_SHORT -->
- *Power Shots: Your initial %d1_Amount attacks in each battle are enhanced.  <!-- TXT_KEY_UNITHELP_POWER_SHOTS_TOTAL_MODIFIER -->
- *Power Shot Combat Modifier: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_POWER_SHOT_COMBAT_MODIFIER_TOTAL_SHORT -->
- *Power Shot Combat Modifier: Your Power Shots are made with a %d1_Amount%% modifier to Combat Strength.  <!-- TXT_KEY_UNITHELP_POWER_SHOT_COMBAT_MODIFIER_TOTAL_MODIFIER -->
- *Power Shot Puncture Modifier: %d1_Amount  <!-- TXT_KEY_UNITHELP_POWER_SHOT_PUNCTURE_MODIFIER_TOTAL_SHORT -->
- *Power Shot Puncture Modifier: Your Power Shots are made with a %d1_Amount modifier to Puncture.  <!-- TXT_KEY_UNITHELP_POWER_SHOT_PUNCTURE_MODIFIER_TOTAL_MODIFIER -->
- *Power Shot Precision Modifier: Your Power Shots are made with a %d1_Amount modifier to Precision.  <!-- TXT_KEY_UNITHELP_POWER_SHOT_PRECISION_MODIFIER_TOTAL_SHORT -->
- TBD  <!-- TXT_KEY_UNITHELP_POWER_SHOT_PRECISION_MODIFIER_TOTAL_MODIFIER -->
- *Power Shot Critical Modifier: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_POWER_SHOT_CRITICAL_MODIFIER_TOTAL_SHORT -->
- *Power Shot Critical Modifier: Your Power Shots are made with a %d1_Amount%% modifier to the chance to inflict a Critical Injury.  <!-- TXT_KEY_UNITHELP_POWER_SHOT_CRITICAL_MODIFIER_TOTAL_MODIFIER -->
- *Critical Modifier: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_CRITICAL_MODIFIER_TOTAL_SHORT -->
- *Critical Modifier: Your attacks have a %d1_Amount%% modifier to the chance to inflict a Critical Injury.  <!-- TXT_KEY_UNITHELP_CRITICAL_MODIFIER_TOTAL_MODIFIER -->
- *%D1_Mod%% Critical Chance vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_CRITICAL_VERSUS -->
- *Round Stun: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_ROUND_STUN_TOTAL_MODIFIER_SHORT -->
- *Round Stun: Chance to Stun Enemy equal to %d1_Amount%% of damage dealt each round  <!-- TXT_KEY_UNITHELP_ROUND_STUN_TOTAL_MODIFIER -->
- *%D1_Mod%% Round Stun Chance vs. %s3_AgainstName  <!-- TXT_KEY_PROMOTIONHELP_ROUND_STUN_VERSUS -->
- *%d1_Change%% Disable Traps vs %s3_TypeName  <!-- TXT_KEY_UNITHELP_TRAP_DISABLE_TYPE -->
- *%d1_Change%% chance to Avoid Traps of %s3_TypeName  <!-- TXT_KEY_UNITHELP_TRAP_AVOID_TYPE -->
- *%d1_Change%% chance of Triggering when %s3_TypeName moves into armed tile.  <!-- TXT_KEY_UNITHELP_TRAP_TRIGGER_TYPE -->
- *Gives a free %s1_Promotion promotion to any traps this unit sets. (As long as promotion is valid for that trap)  <!-- TXT_KEY_UNITHELP_TRAP_PROMOTION_TYPE -->
- *Immune to %s1_UnitCombat traps.  <!-- TXT_KEY_UNITHELP_TRAP_IMMUNE_TYPE -->
- *May be triggered %d1_value times before final destruction.(%d2_rem remaining)  <!-- TXT_KEY_UNITHELP_TRAP_NUM_ACTIVE_TRIGGERS -->
- *If this trap can trigger against an attacking foe, it will do so before combat begins, weakening the attacker before combat.  <!-- TXT_KEY_UNITHELP_TRAP_TRIGGER_BEFORE_ATTACK -->
- *Disabling Complexity: %d1_value  <!-- TXT_KEY_UNITHELP_TRAP_COMPLEXITY -->
- *Afflict (Immediate): Instant %d1_Probability chance of inflicting %s2_PromotionName on striking your enemy.  <!-- TXT_KEY_AFFLICT_ON_ATTACK_IMMEDIATE -->
- *Afflict: At end of battle, %d1_Probability chance of inflicting %s2_PromotionName to an enemy you injured.  <!-- TXT_KEY_AFFLICT_ON_ATTACK -->
- *Poison Mastery: %D1_Amount%%  <!-- TXT_KEY_UNITHELP_POISON_PROB_MOD_TOTAL_MODIFIER_SHORT -->
- *Poison Mastery: %D1_Amount%% Modifier to all Afflict probabilities from this unit.  <!-- TXT_KEY_UNITHELP_POISON_PROB_MOD_TOTAL_MODIFIER -->
- *Watered Down Poison: %D1_Amount%%  <!-- TXT_KEY_UNITHELP_POISON_PROB_NEG_TOTAL_MODIFIER_SHORT -->
- *Watered Down Poison: %D1_Amount%% Modifier to all Afflict probabilities from this unit.  <!-- TXT_KEY_UNITHELP_POISON_PROB_NEG_TOTAL_MODIFIER -->
- *Deals Cold Damage on Attacks  <!-- TXT_KEY_UNITHELP_DEALS_COLD_DAMAGE_SHORT -->
- *Damage this unit deals is Cold Damage.  <!-- TXT_KEY_UNITHELP_DEALS_COLD_DAMAGE_TOTAL -->
- (key not in current GameText)  <!-- TXT_KEY_UNITHELP_RAGE_TOTAL_SHORT -->
- *%d1_Change Work Rate  <!-- TXT_KEY_TEMP_WORK_RATE -->
- *%d1_Change Cultural Revolt Protection  <!-- TXT_KEY_TEMP_REVOLT_PROTECTION -->
- Cargo Space: %d1/%d2  <!-- TXT_KEY_UNITHELP_CARGO_SPACE -->
- *Cargo Space: %d1  <!-- TXT_KEY_UNITHELP_CARGO_SPACE_FOREIGN -->
- (Carries %s1)  <!-- TXT_KEY_UNITHELP_CARRIES -->
- *Cargo Size: %d1  <!-- TXT_KEY_UNITHELP_CARGO_SIZE_MATTERS -->
- *This Unit Cannot Heal Without Assistance  <!-- TXT_KEY_UNITHELP_SELF_HEAL_NONE -->
- *Never Reveals Nationality  <!-- TXT_KEY_PROMOTIONHELP_LOYALTY_SPY -->
- *Can Heal while Moving  <!-- TXT_KEY_PROMOTIONHELP_ALWAYS_HEAL -->
- *Self heal: %d1%%  <!-- TXT_KEY_UNITHELP_SELF_HEAL -->
- *%d1_percent%% Bonus Unrest From Missions  <!-- TXT_KEY_PROMOTIONHELP_INSTIGATE_SPY -->
- *Heals Extra %d1_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_EXTRA -->
- Damage/Turn in Enemy Lands  <!-- TXT_KEY_PROMOTIONHELP_ENEMY_LANDS -->
- *%d1_percent%% Bonus City Revolt From Missions  <!-- TXT_KEY_PROMOTIONHELP_INSTIGATE2_SPY -->
- Damage/Turn in Neutral Lands  <!-- TXT_KEY_PROMOTIONHELP_NEUTRAL_LANDS -->
- *%d1_percent%% Bonus Unhealthiness From Missions  <!-- TXT_KEY_PROMOTIONHELP_POISON_SPY -->
- Damage/Turn in Friendly Lands  <!-- TXT_KEY_PROMOTIONHELP_FRIENDLY_LANDS -->
- *Heals %d1 [NUM1:Unit:Units]/turn. (%d2 Remaining)  <!-- TXT_KEY_UNITHELP_NUM_HEAL_SUPPORT -->
- *Heals Units in Same Tile Extra %d1_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_SAME -->
- Damage/Turn  <!-- TXT_KEY_PROMOTIONHELP_DAMAGE_TURN -->
- *Heals Units in Adjacent Tiles Extra %d1_Heals%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_ADJACENT -->
- *Assists in Healing %s1_UNITCOMBAT Units in Same Tile %D2_Amount%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_UNITCOMBAT_SAME -->
- *Assists in Healing %s1_UNITCOMBAT Units in Adjacent Tiles %D2_Heals%%  <!-- TXT_KEY_PROMOTIONHELP_HEALS_UNITCOMBAT_ADJACENT -->
- *%d1%% to healing support for any unit types it can normally heal from Established Base of Operations.  <!-- TXT_KEY_UNITHELP_HEALING_HQ_MOD -->
- *%d1%% chance to heal self, %d2%% chance to heal all friendly units on the tile and %d3%% chance to heal all friendly units on an adjacent tile on a combat victory  <!-- TXT_KEY_UNITHELP_VICTORY_ADJACENT -->
- *%d1%% chance to heal self and %d2%% chance to heal all friendly units on the tile on a combat victory  <!-- TXT_KEY_UNITHELP_VICTORY_STACK -->
- *+%d1%% Chance to Heal on Combat Victory  <!-- TXT_KEY_PROMOTIONHELP_VICTORY_HEAL -->
- *Tolerance: Currently %D1_Change%% less likely to contract %s2_Affliction.  <!-- TXT_KEY_UNITHELP_AFFLICTION_TOLERANCE_POSITIVE -->
- *Allergic: Currently %D1_Change%% more likely to contract %s2_Affliction.  <!-- TXT_KEY_UNITHELP_AFFLICTION_TOLERANCE_NEGATIVE -->
- Afflicted with %s1_Affliction. Chance of recovery: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_CHANCE_TO_OVERCOME -->
- Afflicted with %s1_Affliction. Chance of worsening: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_CHANCE_OF_WORSENING -->
- *Aid (%s1_Type): %d2_Amount  <!-- TXT_KEY_UNITHELP_AID_TOTAL_SHORT -->
- *Aid: Assists the chances for other units in stack to Overcome %s1_Type Afflictions by %s2_Amount  <!-- TXT_KEY_UNITHELP_AID_TOTAL_MODIFIER -->
- *Cure: May remove or improve %s1_PromotionName on those afflicted.  <!-- TXT_KEY_CURE_AFFLICTION -->
- *Number of Extra Lives: %d1  <!-- TXT_KEY_UNITHELP_ONEUP -->
- *+%d1%% Chance to Survive a Combat Loss  <!-- TXT_KEY_PROMOTIONHELP_SURVIVOR -->
- *Fortitude: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_FORTITUDE_TOTAL_SHORT -->
- *Fortitude: Resists and improves chance to overcome Afflictions by %d1_Amount%%  <!-- TXT_KEY_UNITHELP_FORTITUDE_TOTAL_MODIFIER -->
- *Endurance: %d1_Amount  <!-- TXT_KEY_UNITHELP_ENDURANCE_TOTAL_SHORT -->
- *Endurance: %d1_Amount - Resists Elemental Damage Effects and counteracts penalties from Fatigue, Tires, and Frays (by round reductions in combat).  <!-- TXT_KEY_UNITHELP_ENDURANCE_TOTAL_MODIFIER -->
- *Immune to Cold Damage Penalties.  <!-- TXT_KEY_UNITHELP_IMMUNITY_TO_COLD_DAMAGE -->
- *Immune to collateral damage from %s2_units  <!-- TXT_KEY_UNITHELP_COLLATERAL_IMMUNE -->
- *Suffers %d1_percent%% less Collateral Damage  <!-- TXT_KEY_PROMOTIONHELP_COLLATERAL_PROTECTION -->
- *Can upgrade almost anywhere.  <!-- TXT_KEY_UPGRADE_ANYWHERE -->
- *Exiled from its own cultural border.  <!-- TXT_KEY_EXCILE -->
- *This unit can enter territories you have a Right of Passage or Open Borders agreement with.  <!-- TXT_KEY_PASSAGE -->
- *This unit cannot enter a city that is not your own without attacking it.  <!-- TXT_KEY_NO_NON_OWNED_CITY -->
- *At peace with humanoid NPC's.  <!-- TXT_KEY_UNITHELP_BARB_COEXIST -->
- *Enters all cities peacefully.  <!-- TXT_KEY_UNITHELP_BLEND_INTO_CITY -->
- *Cannot Capture Enemy Cities or Units  <!-- TXT_KEY_UNITHELP_CANNOT_CAPTURE -->
- *%D1%% chance to Capture.  <!-- TXT_KEY_UNITHELP_CAPTURE_PROBABILITY_MODIFIER -->
- *%D1%% chance to avoid Capture.  <!-- TXT_KEY_UNITHELP_CAPTURE_RESISTANCE_MODIFIER -->
- *This unit can initiate Assassinations against units on the same tile.  <!-- TXT_KEY_UNITHELP_ASSASSIN -->
- *Taunt Chance: %D1%%  <!-- TXT_KEY_UNITHELP_TAUNT -->
- *Insidiousness (Ability to Evade Investigation): %s1_Amount%%  <!-- TXT_KEY_INSIDIOUSNESS -->
- *Investigation (Find Local Criminals): %s1_Amount%%  <!-- TXT_KEY_INVESTIGATION -->
- *Targets any %s1_unit_list first in combat. If Assassination is possible, these units are also the list of units that may be targeted for assassination.  <!-- TXT_KEY_UNITHELP_TARGETS_UNIT_FIRST -->
- *Defends first against %s1_unit_list  <!-- TXT_KEY_UNITHELP_DEFENDS_UNIT_FIRST -->
- *Pillages <beaker> in addition to <gold>  <!-- TXT_KEY_PROMOTIONHELP_RESEARCH_PILLAGE -->
- *Pillages <spy> in addition to <gold>  <!-- TXT_KEY_PROMOTIONHELP_ESPIONAGE_PILLAGE -->
- *Pillages twice the <gold> and potentially twice the Improvement.  <!-- TXT_KEY_PROMOTIONHELP_MARAUDER_PILLAGE -->
- *Automatically pillages Improvements on move.  <!-- TXT_KEY_PROMOTIONHELP_MOVING_PILLAGE -->
- *Reaps the Profits of Pillaging on a Combat Victory (no loss of Improvement though).  <!-- TXT_KEY_PROMOTIONHELP_VICTORY_PILLAGE -->
- *%D1_Happy <happy> to any city the unit is in.  <!-- TXT_KEY_PROMOTIONHELP_CELEBRITY -->
- *%D1_Change%% Work Speed on %s2_Gameobject  <!-- TXT_KEY_PROMOTIONHELP_WORK -->
- *%D1_Change%% Work Speed on Hills  <!-- TXT_KEY_PROMOTIONHELP_HILLS_WORK -->
- *%D1_Change%% Work Speed on Peaks  <!-- TXT_KEY_PROMOTIONHELP_PEAKS_WORK -->
- *Current Front Support: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_FRONT_SUPPORT_PERCENT_TOTAL_SHORT -->
- *Current Front Support: %d1_Amount%% of this unit's Strength  <!-- TXT_KEY_UNITHELP_FRONT_SUPPORT_PERCENT_TOTAL -->
- *Current Short Range Support: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_SHORT_RANGE_SUPPORT_PERCENT_TOTAL_SHORT -->
- *Current Short Range Support: %d1_Amount%% of this unit's Strength  <!-- TXT_KEY_UNITHELP_SHORT_RANGE_SUPPORT_PERCENT_TOTAL -->
- *Current Medium Range Support: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_MEDIUM_RANGE_SUPPORT_PERCENT_TOTAL_SHORT -->
- *Current Medium Range Support: %d1_Amount%% of this unit's Strength  <!-- TXT_KEY_UNITHELP_MEDIUM_RANGE_SUPPORT_PERCENT_TOTAL -->
- *Current Long Range Support: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_LONG_RANGE_SUPPORT_PERCENT_TOTAL_SHORT -->
- *Current Long Range Support: %d1_Amount%% of this unit's Strength  <!-- TXT_KEY_UNITHELP_LONG_RANGE_SUPPORT_PERCENT_TOTAL -->
- *Current Flank Support: %d1_Amount%%  <!-- TXT_KEY_UNITHELP_FLANK_SUPPORT_PERCENT_TOTAL_SHORT -->
- *Current Flank Support: %d1_Amount%% of this unit's Strength  <!-- TXT_KEY_UNITHELP_FLANK_SUPPORT_PERCENT_TOTAL -->
- → `buildDisplayString`
- *From %s1_MOD:  <!-- TXT_KEY_UNITHELP_MODIFIER_PROPERTY_MANIP -->
- *Unit Support Cost: %d1_Amount%  <!-- TXT_KEY_UNITHELP_COST_MODIFIER_TOTAL_SHORT -->
- *Unit Support Cost: %D1_Amount%  <!-- TXT_KEY_UNITHELP_COST_MODIFIER_TOTAL_MODIFIER -->
- *Max HP: %d1 (usually 100.)  <!-- TXT_KEY_UNITHELP_MAX_HP -->
- *Damage that %s1_UNITCOMBAT: %d2_Mod%%. Cannot heal here.  <!-- TXT_KEY_UNITHELP_DAMAGE_BY_UNITCOMBAT_NOHEAL -->
- *Damage that %s1_UNITCOMBAT: %d2_Mod%%. Heals in %d3 Turn(s).  <!-- TXT_KEY_UNITHELP_DAMAGE_BY_UNITCOMBAT -->
- *Fortified: %d1%%  <!-- TXT_KEY_UNITHELP_FORTIFIED -->
- *Military Branch  <!-- TXT_KEY_UNITHELP_BRANCH_MILITARY -->
- *Civilian Branch  <!-- TXT_KEY_UNITHELP_BRANCH_CIVILIAN -->
- *Upkeep: %s1 <gold>  <!-- TXT_KEY_UNITHELP_UPKEEP -->
- → `setEspionageMissionHelp`
- *Cold Damage: %d1_Amount  <!-- TXT_KEY_UNITHELP_COLD_DAMAGE_SHORT -->
- *Cold Damage: Reduces Dodge and Precision by %d1_Amount%%  <!-- TXT_KEY_UNITHELP_COLD_DAMAGE_TOTAL -->
- *Can perform Espionage Missions in an opponent's territory. The longer the unit is stationary the lower the cost to perform a Mission  <!-- TXT_KEY_UNITHELP_IS_SPY -->
- *Can Nuke Enemy Lands  <!-- TXT_KEY_UNITHELP_CAN_NUKE -->
- *Invisible to All Units  <!-- TXT_KEY_UNITHELP_INVISIBLE_ALL -->
- *Invisible to Most Units  <!-- TXT_KEY_UNITHELP_INVISIBLE_MOST -->
- *Can See %F1_Name  <!-- TXT_KEY_UNITHELP_SEE_INVISIBLE -->
- *%D1_Change %F2_Type Spot Range  <!-- TXT_KEY_UNITHELP_INVISIBILITY_SPOT_RANGE_VALUE -->
- *%D1_Change %F2_Type Spot on Same Tile  <!-- TXT_KEY_UNITHELP_INVISIBILITY_SPOT_SAME_TILE_VALUE -->
- *%d1_Change %F2_Type Veil  <!-- TXT_KEY_UNITHELP_INVISIBILITY_VEIL_VALUE -->
- *All %F2_TypeName Veil is Negated  <!-- TXT_KEY_UNITHELP_INVISIBILITY_VEIL_NEGATED -->
- *Always Visible  <!-- TXT_KEY_UNITHELP_ALWAYS_VISIBLE -->
- *%D1_Change Vision Range  <!-- TXT_KEY_PROMOTIONHELP_VISIBILITY -->
- *Can Move through Impassable Terrain  <!-- TXT_KEY_UNITHELP_CAN_MOVE_IMPASSABLE -->
- *Flat Movement Costs  <!-- TXT_KEY_UNITHELP_FLAT_MOVEMENT -->
- *Ignores Terrain Movement Costs  <!-- TXT_KEY_UNITHELP_IGNORE_TERRAIN -->
- *Can Use Enemy Roads  <!-- TXT_KEY_PROMOTIONHELP_ENEMY_ROADS -->
- *Double Movement in Hills  <!-- TXT_KEY_PROMOTIONHELP_HILLS_MOVE -->
- *Can Pass through Peaks.  <!-- TXT_KEY_PROMOTIONHELP_CAN_MOVE_PEAKS -->
- *Can lead units through Peaks.  <!-- TXT_KEY_PROMOTIONHELP_CAN_LEAD_THROUGH_PEAKS -->
- *Double Movement in %s1_TerrFeatType  <!-- TXT_KEY_PROMOTIONHELP_DOUBLE_MOVE -->
- *%D1_Discount Terrain Movement Cost  <!-- TXT_KEY_PROMOTIONHELP_MOVE_DISCOUNT -->
- Can Not Traverse  <!-- TXT_KEY_UNITHELP_CAN_ONLY_TRAVERSE -->
- → `setListHelp`
- *+1 Move on Defensive Combat Victories  <!-- TXT_KEY_PROMOTIONHELP_DV_MOVE -->
- *+1 Move on Offensive Combat Victories  <!-- TXT_KEY_PROMOTIONHELP_OV_MOVE -->
- *Ignores Zones of Control  <!-- TXT_KEY_UNITHELP_IGNORE_ZONE_OF_CONTROL_SHORT -->
- *Unit may ignore Zone of Control effects.  <!-- TXT_KEY_UNITHELP_IGNORE_ZONE_OF_CONTROL_LONG -->
- *Flies to Move  <!-- TXT_KEY_UNITHELP_FLIES_TO_MOVE_SHORT -->
- *Flies to Move(May ignore non-flying pursuit, river and amphibious attack modifiers, tile movement modifiers, and may utilize coastal and unpassable tiles.)  <!-- TXT_KEY_UNITHELP_FLIES_TO_MOVE_LONG -->
- *Ignores Border Restriction  <!-- TXT_KEY_UNITHELP_ANIMAL_IGNORES_BORDERS_SHORT -->
- *This Animal Ignores Border Restriction.  <!-- TXT_KEY_UNITHELP_ANIMAL_IGNORES_BORDERS_LONG -->
- *Unit may not reveal undiscovered terrain except inside a player's territory  <!-- TXT_KEY_UNITHELP_VISIBILITY_MOVE_RANGE -->
- *Can perform paradrops (Range=%d1_range)  <!-- TXT_KEY_UNITHELP_PARADROP_RANGE -->
- *No Movement Cost to Paradrop and Can Attack After Paradrop  <!-- TXT_KEY_PROMOTIONHELP_FREE_DROP -->
- *Can Paradrop on FoW Tiles  <!-- TXT_KEY_PROMOTIONHELP_DROP_SIGHT_UNSEEN -->
- *%D1_Change%% Experience Earned in Battle  <!-- TXT_KEY_EXPERIENCE_PERCENT -->
- *May only exist on a %s1_MapCat.  <!-- TXT_KEY_MAP_CATEGORY_PREREQUISITE -->
- On kill  <!-- TXT_KEY_UNITHELP_ANIMAL_ON_KILL -->

## `setVassalRevoltHelp`

- Land: %d1%% of Master (free at %d2%%)  <!-- TXT_KEY_MISC_VASSAL_LAND_STATS -->
- Population: %d1%% of Master (free at %d2%%)  <!-- TXT_KEY_MISC_VASSAL_POPULATION_STATS -->
- Land: %d1%% of Original (free at %d2%%)  <!-- TXT_KEY_MISC_VASSAL_AREA_LOSS -->
- Master Land: %d1%% of Original (Vassal breaks free at %d2%%)  <!-- TXT_KEY_MISC_MASTER_AREA_LOSS -->

## `setYearStr`

- BC-%s1_Date  <!-- TXT_KEY_TIME_BC_SAVE -->
- %d1_Date BC  <!-- TXT_KEY_TIME_BC -->
- AD-%s1_Date  <!-- TXT_KEY_TIME_AD_SAVE -->
- %d1_Date AD  <!-- TXT_KEY_TIME_AD -->

## `setYieldChangeHelp`

- → `setResumableYieldChangeHelp`

## `setYieldHelp`

- *%D1%F2 from Worked Tiles  <!-- TXT_KEY_MISC_HELP_WORKED_TILES_YIELD -->
- <name of the thing>
- Trade Routes  <!-- TXT_KEY_HEADING_TRADEROUTE_LIST -->
- *%D1 %F2 from other  <!-- TXT_KEY_MISC_HELP_YIELD_OTHER -->
- Base %s1: %d2 %F3  <!-- TXT_KEY_MISC_HELP_BASE_YIELD -->
- *%D1%% %F2 from Traits  <!-- TXT_KEY_MISC_HELP_YIELD_TRAITS -->
- *%D1%%%F2 from Civics  <!-- TXT_KEY_MISC_HELP_YIELD_CIVICS -->
- *%D1%%%F2 from Buildings  <!-- TXT_KEY_MISC_HELP_YIELD_BUILDINGS -->
- *%D1%%%F2 from Resources  <!-- TXT_KEY_MISC_HELP_YIELD_BONUS -->
- *%D1%%%F2 from Power  <!-- TXT_KEY_MISC_HELP_YIELD_POWER -->
- *%D1%%%F2 for Capital  <!-- TXT_KEY_MISC_HELP_YIELD_CAPITAL -->
- *%d1%% %F2 from Events  <!-- TXT_KEY_MISC_HELP_YIELD_EVENTS -->
- Sum: %d1%% = %s2%F3  <!-- TXT_KEY_MISC_HELP_TOTAL_YIELD_MOD -->
- Specialists  <!-- TXT_KEY_CONCEPT_SPECIALISTS -->
- Free Specialists  <!-- TXT_KEY_WB_FREE_SPECIALISTS -->
- *%D1%F2 from Corporations  <!-- TXT_KEY_MISC_HELP_CORPORATION_COMMERCE -->
- Buildings  <!-- TXT_KEY_WB_BUILDINGS -->
- *%D1<hammer> from %D2<food> surplus  <!-- TXT_KEY_MISC_HELP_PROD_FOOD -->
- Overflow from previous build: %d1<hammer>  <!-- TXT_KEY_MISC_HELP_PROD_OVERFLOW -->
- From chopping: %d1<hammer>  <!-- TXT_KEY_MISC_HELP_PROD_CHOPS -->
- Total: %d1 %F2  <!-- TXT_KEY_MISC_HELP_FINAL_YIELD -->

## `setYieldPerPopChangeHelp`

- → `setResumableYieldChangeHelp`