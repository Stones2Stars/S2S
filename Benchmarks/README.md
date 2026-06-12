# Benchmarks/ — live-game observation data

Per-game benchmark runs for the AI-vs-human benchmarking initiative
(`Sources/docs/plans/ai-vs-human-benchmarking.md`). Everything in this folder **except this
README is gitignored** — the data is per-developer observation material (CSV time series,
log captures, snapshot copies), often hundreds of MB per game. The *conventions* live here;
the *data* stays local.

## Folder naming scheme

One folder per observed game:

```
Benchmarks/<start-date>[<seq>]_<scenario>_<human-civ>-p<player-id>/
```

- `<start-date>` — game start date, `yyyy-MM-dd` (sorts chronologically).
- `<seq>` — optional `b`, `c`, … when a second/third game starts the same day.
- `<scenario>` — short kebab-case label: era/size/civ-count or anything that identifies the
  setup, e.g. `prehistoric-15civs`.
- `<human-civ>-p<player-id>` — who the human plays and their player number, e.g.
  `england-p0`. The player id is what analysis scripts use to exclude the human from
  AI-health conclusions.

Example: `Benchmarks/2026-06-11_prehistoric-15civs_england-p0/`

## Contents convention (per game folder)

| File / dir | What |
|---|---|
| `census_timeseries.csv` | Collector output — one row per observed game turn (see `Tools/BenchmarkCensusCollector.ps1`) |
| `players_timeseries.csv` | Collector output from `/players` — one row per player per turn: score, era, techs, research, cities, population, units, gold(+rate), science rate, production, handicap. The progression curves that answer "AI ahead or behind" |
| `cities_timeseries.csv` | Collector output from `/cities` — one row per city per turn: yields, production head, buildings, culture level, and the live crime/education/disease property values |
| `PlotSnapshot_start_t0.csv` | Copy of the game-start plot snapshot (the rotating `turn_tN` files vanish; the start file is the t0 baseline) |
| `snapshots/` | Hand-kept copies of `PlotSnapshot_turn_tN.csv` for turns worth preserving (copy them out before rotation) |
| `logs/` | Captures of the gated AI logs when Autolog was enabled, named `<LogName>_t<from>-<to>.log` (e.g. `BuildEvaluation_t200-260.log`) |
| `notes.md` | Running observations: notable events with turn numbers (civ deaths, razed cities, wars), settings (difficulty, speed, options), and analysis conclusions |

## Collector

`Tools/BenchmarkCensusCollector.ps1` polls the live census (`http://127.0.0.1:7227/units`,
requires the `Autolog__HttpServer` BUG option) and the newest plot snapshot every ~60s,
appending one CSV row per new game turn. Start it when a benchmark game begins:

```powershell
pwsh -NoProfile -File Tools/BenchmarkCensusCollector.ps1 `
     -OutCsv "Benchmarks/<game-folder>/census_timeseries.csv"
```

It stops on its own when the game process exits; create the stop-sentinel file
(`-StopSentinel`, default `%TEMP%\s2s_census_stop`) to stop it early.
