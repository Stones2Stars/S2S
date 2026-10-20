# What a player sees of its rivals — the score list, the civ hover, the Info screen

The score list (`CvMainInterface.updateScoreStrings`, drawn by `BUG/Scoreboard.py`), the civ hover
(`CvDLLWidgetData::parseContactCivHelp`) and the Info screen (`CvInfoScreen`) show what one player knows of the
others. Every number on the score list, the civ hover's power lines and the Info screen's demographics table is
the engine's: Python places the text and picks colours from the player's options, and works nothing out
([THE ONE HOVER](tooltip-look.md#the-one-hover) derives nothing, and neither does a screen).

## One rule decides what a player may see

`CvPlayer::isDemographicsVisibleTo(eObserver)` and `CvPlayer::isResearchVisibleTo(eObserver)` share one rule,
`CvPlayer::isIntelVisibleTo`: the observer itself, its team, a player whose team is the observer's team's vassal,
debug mode, a game past `GAMESTATE_ON`, or enough espionage points against the player for the mission
(`CvPlayer::canDoEspionageMission`). The missions are the first passive ones carrying `bSeeDemographics` and
`bSeeResearch`, found once at load (`cvInternalGlobals::buildLoadTimeIndexes`), beside the investigate-city mission
behind `CvCity::isOpenTo` — whether a player may see inside a city, which the Info screen's wonder list asks of
each rival city.

⛔ **A screen asks these reads and never rebuilds the rule.** The score list's power and research columns, the civ
hover's power lines and the Info screen's list of civilizations answer the same question, so they agree.

## Power

- Power is the empire's military might (`CvPlayer::getPower`; what it sums:
  [cascade/11 § THE DEMOGRAPHICS LEDGERS](../cascade/11-context-stores-vs-forwards.md)). It is an amount, so it is
  `int64_t` from the ledgers (`WorthLedger`, `CvArea`) through the player, team and game sums to every AI reader
  ([fixed-point-and-scales.md §1b](../specs/curators/fixed-point-and-scales.md)).
- **The ratio** is `CyPlayer::getPowerRatioText(iOther, iDecimals)`: `ratioText` (`UI/CvEntryText`) divides the two
  full values in whole-number math, rounded to the decimals the score list's option asks, 1 to 4
  ([a ratio's decimals are a display choice](../architecture/patterns/06-the-python-read-boundary-one.md#-the-python-read-boundary--one-complete-data-fetching-library)).
  The formula is which player asks: *Rival vs. You* asks the
  rival against the observer, *You vs. Rival* the observer against the rival. Against a player with no power the
  text is empty and the cell stays blank.
- **The colour** is `CvPlayer::getPowerStanding(eObserver, iStrongerPercent, iWeakerPercent)`: the rival's power
  against the observer's, whichever formula is displayed — stronger at or above the High cutoff, weaker at or below
  the Low cutoff, even between. The cutoffs arrive from the options as ratios and become whole percents at the
  `Cy*` boundary.
- **The civ hover** adds the rival's power, the observer's, and the ratio in the score list's formula and decimals
  (the `Scores__PowerFormula` and `Scores__PowerDecimals` options).

## The score change

`CvPlayer::getScoreChange(bIncludeCurrentTurn)`. The score history holds each player's score per game turn
(`CvGame::updateScore`): the score as a finished game turn ended, and the running score for the game turn in play.
Turns run in player order — team order with simultaneous team turns, all at once
with simultaneous turns — and `CvPlayer::hasBegunTurn` answers whether a player's turn of the current game turn has
begun. With the current turn included, the change runs from the end of the game turn before the player's latest
turn to now; without it, it is the player's last finished turn. Both cover one turn of that player's.

## Cities, espionage, worst enemy, war

- **Cities:** `CvPlayer::getCitiesKnownTo(eObserver)` and `CvPlayer::isCityCountExactFor(eObserver)`. The count is
  exact for the player's own team, under open borders from the player's team, with an embassy and a player
  willing to talk, and in debug mode; otherwise it is the cities revealed to the observer's team and the capital
  once any city is known.
- **Espionage:** `CvTeam::isEspionageAheadOf(eTeam)` — more points against that team than it has against this one.
- **Worst enemy:** `CyTeam::getWorstEnemy`, a team id (`CvTeamAI::AI_getWorstEnemy`).
- **War:** `CvPlayer::isTooBusyForWarRequestFrom(eAsker)` — the engine's own war-trade refusal
  (`DENIAL_TOO_MANY_WARS`) for some third team the asker could name.

## The demographics table

`CvPlayer::getDemographicStanding(DemographicKind, DemographicStanding&)`, read by the Info screen as
`CyPlayer::getDemographicsRows`. Each measure is the engine's own number, through the functions the history
graphs record: commerce (`calculateTotalCommerce`), production and food (`calculateTotalYield`), power
(`getPower`), land plots, the real population, happiness and health as the share of the cities' happiness (health)
against their anger (unhealth), and net trade (`calculateTotalExports` less `calculateTotalImports`, of commerce).
Each carries its unit (`demographicUnitOf`): the amounts, the counts (land plots, population) and the two shares
(whole percents), and the row shows each in its own form. The rivals are every other living civilization, met or
not: the best, the average and the worst of them, and the player's rank — one more than the number of rivals above
it.
