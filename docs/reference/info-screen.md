# The Info screen — where each tab's numbers come from

The Info screen (`CvInfoScreen`) has four tabs: Demographics, Graphs, Top Cities and Statistics. Its counts,
rankings and lists, and every rule for what a player may see, are the engine's
([what a player sees of its rivals](rival-intel.md)); Python places the text, picks colours, formats years and the
time played, and draws the graphs.

## The civilizations listed

The screen lists every civilization the viewer knows of, alive or dead: `CvPlayer::isKnownTo(eObserver)` — the
viewer's team has met it, debug mode, or the game is past `GAMESTATE_ON`. Those whose statistics the viewer may see
([one rule](rival-intel.md#one-rule-decides-what-a-player-may-see)) are drawn on the graphs; the others are named
under them.

## Demographics

[The demographics table](rival-intel.md#the-demographics-table).

## Graphs

Each graph is one measure of each civilization's history (`PlayerHistoryKind`): score, economy, industry,
agriculture, power, culture, espionage, and with Revolutions on, stability. `CyPlayer::getHistory(kind, first,
last)` answers a civilization's whole run of turns in one read (`CvPlayer::getHistory`), 0 for a turn with no
record. The stability graph draws 1000 less the recorded stability.

The economy, industry, agriculture, power and culture histories are amounts (`CvPlayer::isAmountHistory`): recorded
in the engine's form and reduced by their readers — `CyPlayer::getHistory`, and the replay (`CvReplayInfo`). A save
that holds them under their earlier count tags (`numEconHistEntries` and its kin) stored them reduced, and
`CvPlayer::readAmountHistory` scales those turns as they load.

## Top Cities

The five most valuable cities of the civilizations, the most valuable first (`CvGame::getTopCities`): 6 per
citizen, 2 per food, 3 per production and per commerce, and 1 per 30 of the owner's culture in the city. Every city
is ranked; between equal values the earlier player and the earlier city come first.

Each shows its size, its founding year (`CvCity::getYearFounded`, in the game's calendar; the Domestic advisor's
Founded column shows the same year) and its world wonders (`CyCity::getWonders`). A city revealed to the viewer's team also shows its name, its civilization and a view of it;
any other reads "Unknown".

## The wonders

The wonders table lists one of three things (`WonderListKind`, `getWonderRows` in `UI/CvWonderList`):

- **the world wonders** — buildings capped across the world;
- **the national wonders** — buildings capped per team or per empire;
- **the projects**.

A building's category is the widest scope of its own cap (`wonderScope`, [json.md §4.4](../specs/json/04-availability.md)).
What the viewer sees:

- **built world wonders:** all of them; the builder is named for the viewer's team and the teams it has met;
- **built national wonders:** the viewer's team's, and those of teams it has met in cities revealed to it;
- **projects:** every living team's, the builder (the team's leader) named on the same terms as the world wonders;
- **under construction:** the viewer's team's, and a rival city's when that city is revealed to the viewer and open
  to it (`CvCity::isOpenTo`).

What is under construction comes first; then what is built, the most recent first, the projects (which carry no
year) last. A row names its city when that city is revealed to the viewer's team, and can zoom to it when the
builder is named as well (`WonderRow::bZoomable`). A city's own wonders are listed in building order.

## Statistics

- **Time played** — `CyGame.getMinutesPlayed`.
- **Cities built and cities razed** — the player's statistics record (`CyStatistics`, published by the executable
  over `CvPlayerRecord`).
- **Religions founded** — `CvPlayerRecord::getNumReligionsFounded`.
- **Units** — each unit type the player has, built, killed or lost (`getUnitStatistics` in `UI/CvPlayerStatistics`):
  the count it has now (`CvPlayer::getUnitCount`) and the record's built, killed and lost.
- **Buildings** — each building type the player has built, and how many times (`getBuildingStatistics`).
- **Improvements** — each improvement type on the player's land and how many (`getImprovementCounts`, over
  `CvPlayer::getImprovementCount`), shown when the advisor option asks for it.
