# BenchmarkCensusCollector.ps1 — live-game census time-series collector.
#
# Part of the AI-vs-human benchmarking initiative
# (Sources/docs/plans/ai-vs-human-benchmarking.md). Samples the game's HTTP census
# (http://127.0.0.1:7227/units, requires the Autolog__HttpServer BUG option) plus the newest
# PlotSnapshot_turn_t*.csv every $IntervalSec seconds and appends one CSV row per *new game
# turn* observed. Point -OutCsv into the game's folder under Benchmarks/ (see
# Benchmarks/README.md for the folder naming scheme).
#
# Stops when: the game process exits, the stop-sentinel file appears, or 8h elapse.
param(
    [int]$IntervalSec = 60,
    [string]$OutCsv = "$env:TEMP\s2s_census_timeseries.csv",
    [string]$StopSentinel = "$env:TEMP\s2s_census_stop"
)

$logsDir = "$env:USERPROFILE\Documents\My Games\Beyond the Sword\Logs"
$deadline = (Get-Date).AddHours(8)

# Long-format per-player progression series (from /players), one row per player per turn.
$PlayersCsv = Join-Path (Split-Path $OutCsv -Parent) 'players_timeseries.csv'
# Long-format per-city series (from /cities), one row per city per turn.
$CitiesCsv = Join-Path (Split-Path $OutCsv -Parent) 'cities_timeseries.csv'

if (-not (Test-Path $OutCsv)) {
    "timestamp,gameId,turn,civUnits,hunters,hunterEscorts,huntersOnHunt,civCities,npcCities,ruins,animalsLand,animalsWater,avgLandStr,aggressivePct,barbNeanUnits,huntersPerOwner,civUnitsPerOwner" |
        Set-Content $OutCsv
}
if (-not (Test-Path $PlayersCsv)) {
    "timestamp,gameId,turn,id,civ,name,human,npc,score,era,techs,research,cities,population,units,gold,goldRate,scienceRate,production,handicap" |
        Set-Content $PlayersCsv
}
if (-not (Test-Path $CitiesCsv)) {
    "timestamp,gameId,turn,id,owner,name,population,food,production,commerce,producing,producingTurns,buildings,cultureLevel,capital,crime,education,disease" |
        Set-Content $CitiesCsv
}

$lastTurn = -1
while ((Get-Date) -lt $deadline -and -not (Test-Path $StopSentinel)) {
    try { Get-Process Civ4BeyondSword -ErrorAction Stop | Out-Null } catch { break }
    try {
        $resp = Invoke-RestMethod "http://127.0.0.1:7227/units" -TimeoutSec 10
        if ($resp.turn -ne $lastTurn) {
            $lastTurn = $resp.turn
            $units = $resp.units
            # PC-player slots are 0..39; NPC slots (animals 40-42, neanderthal/barb 49-50) are fixed above them
            $civ  = @($units | Where-Object { [int]$_.owner -le 39 })
            $hun  = @($civ | Where-Object { $_.ai -eq 'UNITAI_HUNTER' })
            $hesc = @($civ | Where-Object { $_.ai -eq 'UNITAI_HUNTER_ESCORT' })
            $hOnHunt = @($hun + $hesc | Where-Object { [int]$_.missionAI -eq 40 }).Count
            $barb = @($units | Where-Object { [int]$_.owner -in 49,50 }).Count
            $hpo = ($hun | Group-Object owner | Sort-Object {[int]$_.Name} | ForEach-Object { "$($_.Name):$($_.Count)" }) -join ';'
            $cpo = ($civ | Group-Object owner | Sort-Object {[int]$_.Name} | ForEach-Object { "$($_.Name):$($_.Count)" }) -join ';'

            # Snapshot-derived fields (best effort -- the file rotates and may race)
            $civCities = ''; $npcCities = ''; $ruins = ''; $aLand = ''; $aWater = ''; $avgStr = ''; $aggPct = ''
            try {
                $snapFile = Get-ChildItem "$logsDir\PlotSnapshot_turn_t*.csv" -ErrorAction Stop |
                    Sort-Object {[int]($_.BaseName -replace '.*_t','')} -Descending | Select-Object -First 1
                if ($snapFile) {
                    $snap = Get-Content $snapFile.FullName -ErrorAction Stop | Select-Object -Skip 1 | ConvertFrom-Csv
                    $civCities = @($snap | Where-Object { $_.isCity -eq '1' -and [int]$_.owner -le 39 }).Count
                    $npcCities = @($snap | Where-Object { $_.isCity -eq '1' -and [int]$_.owner -gt 39 }).Count
                    $ruins = @($snap | Where-Object { $_.improvement -eq 'IMPROVEMENT_CITY_RUINS' }).Count
                    $l = 0; $w = 0; $strSum = 0; $aggN = 0
                    foreach ($r in $snap) {
                        if ($r.animals) {
                            $toks = $r.animals -split '\|'
                            if ($r.isWater -eq '1') { $w += $toks.Count }
                            else {
                                $l += $toks.Count
                                foreach ($t in $toks) {
                                    if ($t -match 'c(\d+)a(-?\d+)') {
                                        $strSum += [int]$Matches[1]
                                        if ([int]$Matches[2] -gt 0) { $aggN++ }
                                    }
                                }
                            }
                        }
                    }
                    $aLand = $l; $aWater = $w
                    if ($l -gt 0) {
                        $avgStr = [math]::Round($strSum / $l, 2)
                        $aggPct = [math]::Round(100 * $aggN / $l)
                    }
                }
            } catch { }

            $ts = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
            "$ts,$($resp.gameId),$($resp.turn),$($civ.Count),$($hun.Count),$($hesc.Count),$hOnHunt,$civCities,$npcCities,$ruins,$aLand,$aWater,$avgStr,$aggPct,$barb,""$hpo"",""$cpo""" |
                Add-Content $OutCsv

            # Per-player progression rows (best effort -- endpoint absent on pre-/players DLLs)
            try {
                $pl = Invoke-RestMethod "http://127.0.0.1:7227/players" -TimeoutSec 10
                $rows = foreach ($p in $pl.players) {
                    "$ts,$($pl.gameId),$($pl.turn),$($p.id),$($p.civ),""$($p.name -replace '"','_')"",$([int]$p.human),$([int]$p.npc),$($p.score),$($p.era),$($p.techs),$($p.research),$($p.cities),$($p.population),$($p.units),$($p.gold),$($p.goldRate),$($p.scienceRate),$($p.production),$($p.handicap)"
                }
                if ($rows) { $rows | Add-Content $PlayersCsv }
            } catch { }

            # Per-city rows (best effort -- endpoint absent on pre-/cities DLLs)
            try {
                $ci = Invoke-RestMethod "http://127.0.0.1:7227/cities" -TimeoutSec 10
                $rows = foreach ($c in $ci.cities) {
                    "$ts,$($ci.gameId),$($ci.turn),$($c.id),$($c.owner),""$($c.name -replace '"','_')"",$($c.population),$($c.food),$($c.production),$($c.commerce),$($c.producing),$($c.producingTurns),$($c.buildings),$($c.cultureLevel),$([int]$c.capital),$($c.crime),$($c.education),$($c.disease)"
                }
                if ($rows) { $rows | Add-Content $CitiesCsv }
            } catch { }
        }
    } catch { }
    Start-Sleep -Seconds $IntervalSec
}
