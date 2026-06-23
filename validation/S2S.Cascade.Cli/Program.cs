using S2S.Model;
using S2S.Parity;
using S2S.Enabler;
using S2S.Application;
using S2S.Tally;

// Modes: <City>/compare (techs), civics/compare-civics, builds/compare-builds, resources.
// The cascade is computed purely from internal + raw-observed data: the snapshot's held techs are replayed
// through the researchCompleted event spine into the tech tally, which DERIVES per-team held sets and the world
// tech count. The enabler reads those. The engine verdict is consulted ONLY in the compare modes.

string assetsData = FindAssetsData();
string snapPath = LatestSnapshot();
IReadOnlyDictionary<string, TechInfo> techs = InfoLoader.LoadTechs(assetsData);
Snapshot snap = SnapshotLoader.Load(snapPath);

// Dry-model the tech tally: map each player to its team, then replay every held tech through researchCompleted.
var playerTeam = new Dictionary<int, int>();
foreach (Team t in snap.World.Teams)
    foreach (Empire e in t.Empires)
        playerTeam[e.Id] = t.Id;
var techTally = new TechTally(playerTeam);
foreach (Team t in snap.World.Teams)
    foreach (Empire e in t.Empires)
        foreach (string tech in t.Techs)
            techTally.ResearchCompleted(e.Id, tech);

string mode = args.Length > 0 ? args[0] : "London";
if (string.Equals(mode, "compare", StringComparison.OrdinalIgnoreCase))
    return Compare(techs, snap, techTally);
if (string.Equals(mode, "civics", StringComparison.OrdinalIgnoreCase))
    return ListCivics(techs, InfoLoader.LoadCivicTypes(assetsData), snap, techTally, args.Length > 1 ? args[1] : "London");
if (string.Equals(mode, "compare-civics", StringComparison.OrdinalIgnoreCase))
    return CompareCivics(techs, InfoLoader.LoadCivicTypes(assetsData), snap, techTally);
if (string.Equals(mode, "builds", StringComparison.OrdinalIgnoreCase))
    return ListBuilds(InfoLoader.LoadBuilds(assetsData), snap, techTally, args.Length > 1 ? args[1] : "London");
if (string.Equals(mode, "compare-builds", StringComparison.OrdinalIgnoreCase))
    return CompareBuilds(InfoLoader.LoadBuilds(assetsData), snap, techTally);
if (string.Equals(mode, "resources", StringComparison.OrdinalIgnoreCase))
    return ListResources(techs, InfoLoader.LoadBonusTypes(assetsData), snap, techTally, args.Length > 1 ? args[1] : "London");
if (string.Equals(mode, "buildable", StringComparison.OrdinalIgnoreCase))
    return ListBuildable(assetsData, snap, techTally, args.Length > 1 ? args[1] : "London");
if (string.Equals(mode, "compare-buildable", StringComparison.OrdinalIgnoreCase))
    return CompareBuildable(assetsData, snap, techTally);
if (string.Equals(mode, "trace", StringComparison.OrdinalIgnoreCase))
    return TraceBuild(assetsData, snap, techTally, args.Length > 1 ? args[1] : "London", args.Length > 2 ? args[2] : "");
return ListCiv(techs, snap, techTally, mode);

static int ListCiv(IReadOnlyDictionary<string, TechInfo> techs, Snapshot snap, TechTally tally, string cityName)
{
    Team? team = FindTeam(snap, cityName);
    if (team is null) { Console.Error.WriteLine($"no team owns a city named '{cityName}'"); return 2; }

    IReadOnlySet<string> held = tally.TeamHeld(team.Id);
    var available = TechCascade.Available(techs, held, tally.World).OrderBy(x => x, StringComparer.Ordinal).ToList();

    Console.WriteLine($"{cityName}: team={team.Id}  held={held.Count}  available={available.Count}");
    foreach (string t in available) Console.WriteLine($"  {t}");
    return 0;
}

static int Compare(IReadOnlyDictionary<string, TechInfo> techs, Snapshot snap, TechTally tally)
{
    var outOfScope = techs.Values.Where(t => DependsOnNonTech(t.Requires?.Build))
        .Select(t => t.Type).ToHashSet(StringComparer.Ordinal);
    int civs = 0, exact = 0, totalMismatch = 0, missingOracle = 0;
    var samples = new List<string>();
    var deferred = new HashSet<string>(StringComparer.Ordinal);

    foreach (Team team in snap.World.Teams)
    {
        IReadOnlySet<string> held = tally.TeamHeld(team.Id);
        HashSet<string> cascade = TechCascade.Available(techs, held, tally.World);

        foreach (Empire emp in team.Empires)
        {
            civs++;
            var engine = new HashSet<string>(emp.AvailableTechs, StringComparer.Ordinal);
            if (engine.Count == 0) missingOracle++;

            foreach (string t in cascade.Where(x => !engine.Contains(x)).Concat(engine.Where(x => !cascade.Contains(x))))
                if (outOfScope.Contains(t)) deferred.Add(t);

            var cascadeOnly = cascade.Where(t => !engine.Contains(t) && !outOfScope.Contains(t)).OrderBy(x => x, StringComparer.Ordinal).ToList();
            var engineOnly = engine.Where(t => !cascade.Contains(t) && !outOfScope.Contains(t)).OrderBy(x => x, StringComparer.Ordinal).ToList();

            if (cascadeOnly.Count == 0 && engineOnly.Count == 0) { exact++; continue; }
            totalMismatch += cascadeOnly.Count + engineOnly.Count;
            if (samples.Count < 20)
            {
                string label = emp.Areas.SelectMany(a => a.Cities).FirstOrDefault(c => c.Name is not null)?.Name ?? $"player {emp.Id}";
                samples.Add($"  {label} (team {team.Id}): cascade-only={string.Join(",", cascadeOnly.Take(8))} | engine-only={string.Join(",", engineOnly.Take(8))}");
            }
        }
    }

    Console.WriteLine("TECH PARITY  cascade (isolated) vs engine availableTechs (canResearch)");
    Console.WriteLine($"  civs={civs}  EXACT={exact}/{civs}  in-scope mismatches={totalMismatch}");
    Console.WriteLine($"  out-of-scope techs (requires.build needs a lower layer): {outOfScope.Count} total; "
                    + $"diverged & deferred to that layer: {(deferred.Count == 0 ? "none" : string.Join(", ", deferred.OrderBy(x => x, StringComparer.Ordinal)))}");
    if (missingOracle > 0)
        Console.WriteLine($"  WARNING: {missingOracle} civ(s) have EMPTY availableTechs -- snapshot predates the extractor change; re-extract.");
    foreach (string s in samples) Console.WriteLine(s);
    return totalMismatch == 0 && missingOracle == 0 ? 0 : 1;
}

// Resources the player can SEE (unlocked): revealed via tech.enables.bonuses (or default-visible), minus those
// obsoleted via tech.obsoletes.bonuses. Pure tech-driven; "available/connected" is separate observed state.
static int ListResources(IReadOnlyDictionary<string, TechInfo> techs, IReadOnlyList<string> bonusTypes,
                         Snapshot snap, TechTally tally, string cityName)
{
    Dictionary<string, List<string>> revealedBy = ResourceCascade.RevealedBy(techs);
    Dictionary<string, List<string>> obsoletedBy = ResourceCascade.ObsoletedBy(techs);

    Team? team = FindTeam(snap, cityName);
    if (team is null) { Console.Error.WriteLine($"no team owns a city named '{cityName}'"); return 2; }
    IReadOnlySet<string> held = tally.TeamHeld(team.Id);

    var seen = ResourceCascade.Seen(bonusTypes, revealedBy, obsoletedBy, held)
        .OrderBy(x => x, StringComparer.Ordinal).ToList();
    int defaultSeen = bonusTypes.Count(b => !revealedBy.ContainsKey(b));
    int obsoletedNow = bonusTypes.Count(b => obsoletedBy.TryGetValue(b, out List<string>? ot) && ot.Any(held.Contains));

    Console.WriteLine($"{cityName}: team={team.Id}  held={held.Count}  bonuses total={bonusTypes.Count}  SEEN={seen.Count}  (default/no-reveal-tech={defaultSeen}, obsoleted-now={obsoletedNow})");
    foreach (string b in seen) Console.WriteLine($"  {b}{(revealedBy.ContainsKey(b) ? "" : "  [default]")}");
    return 0;
}

// Available builds (blind): a build is unlocked iff its requires.build (the tech gate) is satisfied by the held
// techs. A build whose requires.build references a non-tech prereq is out of scope (shown, not dropped).
static int ListBuilds(IReadOnlyDictionary<string, BuildInfo> builds, Snapshot snap, TechTally tally, string cityName)
{
    Team? team = FindTeam(snap, cityName);
    if (team is null) { Console.Error.WriteLine($"no team owns a city named '{cityName}'"); return 2; }

    IReadOnlySet<string> held = tally.TeamHeld(team.Id);
    var state = new EvalState { Techs = held };
    var evaluator = new ConditionEvaluator { StrictStateReligionForBuild = true };

    var available = builds.Values
        .Where(b => !DependsOnNonTech(b.Requires?.Build) && evaluator.Evaluate(b.Requires?.Build, state))
        .Select(b => b.Type).OrderBy(x => x, StringComparer.Ordinal).ToList();
    int outOfScope = builds.Values.Count(b => DependsOnNonTech(b.Requires?.Build));

    Console.WriteLine($"{cityName}: team={team.Id}  held={held.Count}  builds total={builds.Count}  available={available.Count}  out-of-scope(non-tech requires)={outOfScope}");
    foreach (string b in available) Console.WriteLine($"  {b}");
    return 0;
}

// Build parity: every civ's isolated available-build set vs the engine's availableBuilds (tech-prereq unlock).
static int CompareBuilds(IReadOnlyDictionary<string, BuildInfo> builds, Snapshot snap, TechTally tally)
{
    var evaluator = new ConditionEvaluator { StrictStateReligionForBuild = true };
    var outOfScope = builds.Values.Where(b => DependsOnNonTech(b.Requires?.Build))
        .Select(b => b.Type).ToHashSet(StringComparer.Ordinal);
    int civs = 0, exact = 0, totalMismatch = 0, missingOracle = 0;
    var samples = new List<string>();
    var deferred = new HashSet<string>(StringComparer.Ordinal);

    foreach (Team team in snap.World.Teams)
    {
        IReadOnlySet<string> held = tally.TeamHeld(team.Id);
        var state = new EvalState { Techs = held };
        HashSet<string> cascade = builds.Values
            .Where(b => !outOfScope.Contains(b.Type) && evaluator.Evaluate(b.Requires?.Build, state))
            .Select(b => b.Type).ToHashSet(StringComparer.Ordinal);

        foreach (Empire emp in team.Empires)
        {
            civs++;
            var engine = new HashSet<string>(emp.AvailableBuilds, StringComparer.Ordinal);
            if (engine.Count == 0) missingOracle++;

            foreach (string b in cascade.Where(x => !engine.Contains(x)).Concat(engine.Where(x => !cascade.Contains(x))))
                if (outOfScope.Contains(b)) deferred.Add(b);

            var cascadeOnly = cascade.Where(b => !engine.Contains(b) && !outOfScope.Contains(b)).OrderBy(x => x, StringComparer.Ordinal).ToList();
            var engineOnly = engine.Where(b => !cascade.Contains(b) && !outOfScope.Contains(b)).OrderBy(x => x, StringComparer.Ordinal).ToList();

            if (cascadeOnly.Count == 0 && engineOnly.Count == 0) { exact++; continue; }
            totalMismatch += cascadeOnly.Count + engineOnly.Count;
            if (samples.Count < 20)
            {
                string label = emp.Areas.SelectMany(a => a.Cities).FirstOrDefault(c => c.Name is not null)?.Name ?? $"player {emp.Id}";
                samples.Add($"  {label} (team {team.Id}): cascade-only={string.Join(",", cascadeOnly.Take(6))} | engine-only={string.Join(",", engineOnly.Take(6))}");
            }
        }
    }

    Console.WriteLine("BUILD PARITY  cascade (isolated) vs engine availableBuilds (tech-prereq unlock)");
    Console.WriteLine($"  civs={civs}  EXACT={exact}/{civs}  in-scope mismatches={totalMismatch}");
    Console.WriteLine($"  out-of-scope builds (non-tech requires): {outOfScope.Count}; diverged & deferred: {(deferred.Count == 0 ? "none" : string.Join(", ", deferred.OrderBy(x => x, StringComparer.Ordinal)))}");
    if (missingOracle > 0) Console.WriteLine($"  WARNING: {missingOracle} civ(s) have EMPTY availableBuilds -- re-extract.");
    foreach (string s in samples) Console.WriteLine(s);
    return totalMismatch == 0 && missingOracle == 0 ? 0 : 1;
}

// Civic parity: every civ's isolated available-civic set vs the engine's availableCivics (canDoCivics).
static int CompareCivics(IReadOnlyDictionary<string, TechInfo> techs, IReadOnlyList<string> civicTypes,
                         Snapshot snap, TechTally tally)
{
    Dictionary<string, string> enablingTech = CivicCascade.EnablingTech(techs);
    int civs = 0, exact = 0, totalMismatch = 0, missingOracle = 0;
    var samples = new List<string>();

    foreach (Team team in snap.World.Teams)
    {
        IReadOnlySet<string> held = tally.TeamHeld(team.Id);
        HashSet<string> cascade = CivicCascade.Available(civicTypes, enablingTech, held);

        foreach (Empire emp in team.Empires)
        {
            civs++;
            var engine = new HashSet<string>(emp.AvailableCivics, StringComparer.Ordinal);
            if (engine.Count == 0) missingOracle++;

            var cascadeOnly = cascade.Where(c => !engine.Contains(c)).OrderBy(x => x, StringComparer.Ordinal).ToList();
            var engineOnly = engine.Where(c => !cascade.Contains(c)).OrderBy(x => x, StringComparer.Ordinal).ToList();

            if (cascadeOnly.Count == 0 && engineOnly.Count == 0) { exact++; continue; }
            totalMismatch += cascadeOnly.Count + engineOnly.Count;
            if (samples.Count < 20)
            {
                string label = emp.Areas.SelectMany(a => a.Cities).FirstOrDefault(c => c.Name is not null)?.Name ?? $"player {emp.Id}";
                samples.Add($"  {label} (team {team.Id}): cascade-only={string.Join(",", cascadeOnly.Take(8))} | engine-only={string.Join(",", engineOnly.Take(8))}");
            }
        }
    }

    Console.WriteLine("CIVIC PARITY  cascade (isolated) vs engine availableCivics (canDoCivics)");
    Console.WriteLine($"  civs={civs}  EXACT={exact}/{civs}  mismatches={totalMismatch}");
    if (missingOracle > 0)
        Console.WriteLine($"  WARNING: {missingOracle} civ(s) have EMPTY availableCivics -- snapshot predates the extractor change; re-extract.");
    foreach (string s in samples) Console.WriteLine(s);
    return totalMismatch == 0 && missingOracle == 0 ? 0 : 1;
}

// Available civics = enabled by a single tech that's held (tech.enables.civics). Active civics (raw-observed in
// empire.civics) are shown for context -- they feed layers below.
static int ListCivics(IReadOnlyDictionary<string, TechInfo> techs, IReadOnlyList<string> civicTypes,
                      Snapshot snap, TechTally tally, string cityName)
{
    Dictionary<string, string> civicToTech = CivicCascade.EnablingTech(techs);

    Team? team = FindTeam(snap, cityName);
    if (team is null) { Console.Error.WriteLine($"no team owns a city named '{cityName}'"); return 2; }
    IReadOnlySet<string> held = tally.TeamHeld(team.Id);

    var available = CivicCascade.Available(civicTypes, civicToTech, held).OrderBy(x => x, StringComparer.Ordinal).ToList();
    var active = team.Empires.SelectMany(e => e.Civics).Distinct().OrderBy(x => x, StringComparer.Ordinal).ToList();

    Console.WriteLine($"{cityName}: team={team.Id}  civics total={civicTypes.Count}  available={available.Count}  active={active.Count}");
    Console.WriteLine($"  ACTIVE (adopted, feeds downward): {string.Join(", ", active)}");
    Console.WriteLine("  AVAILABLE (enabling tech held; [default] = no enabling tech):");
    foreach (string c in available)
        Console.WriteLine($"    {c}{(civicToTech.ContainsKey(c) ? "" : "  [default]")}");
    return 0;
}

// The buildable enabler's GENERATE pass: per city, CAN GET = union(enables.{buildings,units} over HAS) − removal,
// where HAS = team techs + empire civics + the city's present buildings (the city tally). Bonuses + requires come
// next; this is "a valid canBuild WITHOUT the required side" (enabler.md §2/§4).
static int ListBuildable(string assetsData, Snapshot snap, TechTally techTally, string cityName)
{
    IReadOnlyList<EnablerEdges> edges = InfoLoader.LoadEnablerEdges(assetsData, "techs", "civics", "buildings", "bonuses", "religions");
    Dictionary<string, List<string>> enB = BuildableEnabler.Index(edges, "buildings", BuildableEnabler.Edge.Enables);
    Dictionary<string, List<string>> obB = BuildableEnabler.Reverse(edges, BuildableEnabler.Edge.Obsoletes);   // target-side -> reverse map
    Dictionary<string, List<string>> rpB = BuildableEnabler.Reverse(edges, BuildableEnabler.Edge.Replaces);
    Dictionary<string, List<string>> dsB = BuildableEnabler.Index(edges, "buildings", BuildableEnabler.Edge.Disables);
    Dictionary<string, List<string>> enU = BuildableEnabler.Index(edges, "units", BuildableEnabler.Edge.Enables);
    Dictionary<string, List<string>> obU = obB;   // combined reverse map (ids are unique; cross-kind entries are no-ops)
    Dictionary<string, List<string>> rpU = rpB;
    Dictionary<string, List<string>> dsU = BuildableEnabler.Index(edges, "units", BuildableEnabler.Edge.Disables);

    // Replay the snapshot's per-city buildings through buildingCompleted into the city tally (the leaf HAVE).
    var cityTally = new CityTally();
    foreach (Team t in snap.World.Teams)
        foreach (Empire e in t.Empires)
            foreach (Area a in e.Areas)
                foreach (City c in a.Cities)
                    foreach (string b in c.Buildings)
                        ((IBuildingCompleted)cityTally).BuildingCompleted(e.Id, Gid(e.Id, c.Id), b);

    // Replay the snapshot's trade-connected city bonuses through bonusAdded (the empire-wide tradeable HAVE).
    var bonusTally = new BonusTally();
    foreach (Team t in snap.World.Teams)
        foreach (Empire e in t.Empires)
            foreach (Area a in e.Areas)
                foreach (City c in a.Cities)
                    foreach (string bo in c.Bonuses)
                        ((IBonusAdded)bonusTally).BonusAdded(e.Id, bo);

    Team? team = null; Empire? emp = null; Area? area = null; City? city = null;
    foreach (Team t in snap.World.Teams)
        foreach (Empire e in t.Empires)
            foreach (Area a in e.Areas)
                foreach (City c in a.Cities)
                    if (string.Equals(c.Name, cityName, StringComparison.OrdinalIgnoreCase)) { team = t; emp = e; area = a; city = c; }
    if (city is null) { Console.Error.WriteLine($"no city named '{cityName}'"); return 2; }

    var has = new HashSet<string>(StringComparer.Ordinal);
    has.UnionWith(techTally.TeamHeld(team!.Id));   // team techs
    has.UnionWith(emp!.Civics);                    // empire civics
    has.UnionWith(cityTally.Buildings(Gid(emp!.Id, city!.Id)));  // the city's present buildings (which ENABLE units)

    // GENERATE (per city): the enabled frontier. The city's buildings enable units here — "the correct building in
    // the city does the rest" is GENERATION, not a gate.
    HashSet<string> canB = BuildableEnabler.CanGet(has, enB, obB, rpB, dsB);
    HashSet<string> canU = BuildableEnabler.CanGet(has, enU, obU, rpU, dsU);

    // GATE on requires.build. The required gate fires on RESOURCE — a unit like a helicopter (fuel) or ammunition
    // needs a tradeable bonus; Bonuses = VicinityBonuses = the tradeable set, so a resource requirement passes iff
    // the bonus is tradeable.
    IReadOnlyDictionary<string, Requires?> bReq = InfoLoader.LoadRequiresMap(assetsData, "buildings");
    IReadOnlyDictionary<string, Requires?> uReq = InfoLoader.LoadRequiresMap(assetsData, "units");
    IReadOnlySet<string> trade = bonusTally.Bonuses(emp.Id);
    EvalState state = EvalStateBuilder.Build(snap.World, team, emp, area!, city) with { Bonuses = trade, VicinityBonuses = trade };
    var evaluator = new ConditionEvaluator { StrictStateReligionForBuild = true };

    var buildableB = canB.Where(b => evaluator.Evaluate(bReq.GetValueOrDefault(b)?.Build, state)).OrderBy(x => x, StringComparer.Ordinal).ToList();
    var buildableU = canU.Where(u => evaluator.Evaluate(uReq.GetValueOrDefault(u)?.Build, state)).OrderBy(x => x, StringComparer.Ordinal).ToList();

    Console.WriteLine($"{cityName}: team={team.Id} empire={emp.Id}  HAS={has.Count} (cityBuildings={cityTally.Buildings(Gid(emp.Id, city.Id)).Count}, tradeBonuses={trade.Count})");
    Console.WriteLine($"  ENABLED (generation, per city, no requires):  buildings={canB.Count}  units={canU.Count}");
    Console.WriteLine($"  BUILDABLE (after the resource/requires gate):  buildings={buildableB.Count}  units={buildableU.Count}");
    Console.WriteLine("  --- buildable units (sample) ---");
    foreach (string u in buildableU.Take(25)) Console.WriteLine($"    {u}");
    return 0;
}

// Buildable parity: every city's isolated buildable frontier (generate → requires gate) vs the engine's
// canConstruct (buildings) / canTrain (units). Indexes + tallies loaded once, then per-city diff, both directions.
static int CompareBuildable(string assetsData, Snapshot snap, TechTally techTally)
{
    IReadOnlyList<EnablerEdges> edges = InfoLoader.LoadEnablerEdges(assetsData, "techs", "civics", "buildings", "bonuses", "religions");
    Dictionary<string, List<string>> enB = BuildableEnabler.Index(edges, "buildings", BuildableEnabler.Edge.Enables);
    Dictionary<string, List<string>> obB = BuildableEnabler.Reverse(edges, BuildableEnabler.Edge.Obsoletes);   // target-side -> reverse map
    Dictionary<string, List<string>> rpB = BuildableEnabler.Reverse(edges, BuildableEnabler.Edge.Replaces);
    Dictionary<string, List<string>> dsB = BuildableEnabler.Index(edges, "buildings", BuildableEnabler.Edge.Disables);
    Dictionary<string, List<string>> enU = BuildableEnabler.Index(edges, "units", BuildableEnabler.Edge.Enables);
    Dictionary<string, List<string>> obU = obB;   // combined reverse map (ids are unique; cross-kind entries are no-ops)
    Dictionary<string, List<string>> rpU = rpB;
    Dictionary<string, List<string>> dsU = BuildableEnabler.Index(edges, "units", BuildableEnabler.Edge.Disables);
    IReadOnlyDictionary<string, Requires?> bReq = InfoLoader.LoadRequiresMap(assetsData, "buildings");
    IReadOnlyDictionary<string, Requires?> uReq = InfoLoader.LoadRequiresMap(assetsData, "units");
    IReadOnlySet<string> notConB = InfoLoader.LoadNotConstructible(assetsData, "buildings");   // cost=-1 -> not player-built
    // `allowed` instance cap (wonders / national wonders / group caps): buildable only while the count at the capped
    // scope is below the cap. world = sum over ALL empires (the "global tally is an easy wire"); team = sum over the
    // team's empires; empire = the empire's own count. city/plot caps are already covered by the per-city built set.
    IReadOnlyDictionary<string, Dictionary<string, int>?> allowedB = InfoLoader.LoadAllowedMap(assetsData, "buildings");
    var worldCountB = new Dictionary<string, int>(StringComparer.Ordinal);
    var teamCountB = new Dictionary<int, Dictionary<string, int>>();
    foreach (Team t0 in snap.World.Teams)
    {
        Dictionary<string, int> tc = teamCountB[t0.Id] = new Dictionary<string, int>(StringComparer.Ordinal);
        foreach (Empire e0 in t0.Empires)
            foreach (KeyValuePair<string, int> kv in e0.BuildingCounts)
            {
                worldCountB[kv.Key] = worldCountB.GetValueOrDefault(kv.Key) + kv.Value;
                tc[kv.Key] = tc.GetValueOrDefault(kv.Key) + kv.Value;
            }
    }
    bool CapOk(string b, Empire e, int teamId)
    {
        if (!allowedB.TryGetValue(b, out Dictionary<string, int>? cap) || cap is null) return true;
        foreach (KeyValuePair<string, int> kv in cap)
        {
            int count = kv.Key switch
            {
                "world" => worldCountB.GetValueOrDefault(b),
                "team" => teamCountB[teamId].GetValueOrDefault(b),
                "empire" => e.BuildingCounts.GetValueOrDefault(b),
                _ => 0,
            };
            if (count >= kv.Value) return false;
        }
        return true;
    }

    // SpecialBuilding GROUP cap (§3.4): a member is gated by how many of its group the EMPIRE already holds
    // (the group entity carries allowed:{empire:N}). (The per-city wonder CATEGORY cap is engine-dynamic, §132,
    // deferred to the engine — see the note at the buildable filter below.)
    IReadOnlyDictionary<string, Dictionary<string, int>?> groupCaps = InfoLoader.LoadAllowedMap(assetsData, "specialbuildings");
    IReadOnlyDictionary<string, string> memberGroup = InfoLoader.LoadSpecialBuildingType(assetsData, "buildings");
    var groupCountByEmpire = new Dictionary<int, Dictionary<string, int>>();
    foreach (Team gt in snap.World.Teams)
        foreach (Empire ge in gt.Empires)
        {
            Dictionary<string, int> gc = groupCountByEmpire[ge.Id] = new Dictionary<string, int>(StringComparer.Ordinal);
            foreach (KeyValuePair<string, int> kv in ge.BuildingCounts)
                if (memberGroup.TryGetValue(kv.Key, out string? g))
                    gc[g] = gc.GetValueOrDefault(g) + kv.Value;
        }
    bool GroupOk(string b, int empireId)
    {
        if (!memberGroup.TryGetValue(b, out string? g)) return true;
        if (!groupCaps.TryGetValue(g, out Dictionary<string, int>? cap) || cap is null || !cap.TryGetValue("empire", out int n)) return true;
        return groupCountByEmpire[empireId].GetValueOrDefault(g) < n;
    }

    var cityTally = new CityTally();
    var bonusTally = new BonusTally();
    foreach (Team t in snap.World.Teams)
        foreach (Empire e in t.Empires)
            foreach (Area a in e.Areas)
                foreach (City c in a.Cities)
                {
                    foreach (string b in c.Buildings) ((IBuildingCompleted)cityTally).BuildingCompleted(e.Id, Gid(e.Id, c.Id), b);
                    foreach (string bo in c.Bonuses) ((IBonusAdded)bonusTally).BonusAdded(e.Id, bo);
                }

    var evaluator = new ConditionEvaluator { StrictStateReligionForBuild = true };
    int cities = 0, exactB = 0, exactU = 0, misB = 0, misU = 0, missingOracle = 0;
    int totBMe = 0, totBEng = 0, totUMe = 0, totUEng = 0;
    var perEmp = new Dictionary<int, (string City, int BMe, int BEng, int UMe, int UEng)>();
    var sampledEmpires = new HashSet<int>();
    var samples = new List<string>();

    foreach (Team t in snap.World.Teams)
        foreach (Empire e in t.Empires)
            foreach (Area a in e.Areas)
                foreach (City c in a.Cities)
                {
                    cities++;
                    var engB = new HashSet<string>(c.CanConstruct, StringComparer.Ordinal);
                    var engU = new HashSet<string>(c.CanTrain, StringComparer.Ordinal);
                    if (engB.Count == 0 && engU.Count == 0) missingOracle++;

                    IReadOnlySet<string> trade = bonusTally.Bonuses(e.Id);
                    IReadOnlySet<string> built = cityTally.Buildings(Gid(e.Id, c.Id));
                    var has = new HashSet<string>(StringComparer.Ordinal);
                    has.UnionWith(techTally.TeamHeld(t.Id));   // team techs
                    has.UnionWith(e.Civics);                   // empire civics
                    has.UnionWith(built);                      // the city's present buildings (enable units)
                    has.UnionWith(trade);                      // tradeable bonuses (enable bonus-buildings)
                    has.UnionWith(c.Religions);                // present religions (enable missionaries)
                    HashSet<string> canB = BuildableEnabler.CanGet(has, enB, obB, rpB, dsB);
                    HashSet<string> canU = BuildableEnabler.CanGet(has, enU, obU, rpU, dsU);
                    EvalState state = EvalStateBuilder.Build(snap.World, t, e, a, c);   // actual c.Bonuses / c.VicinityBonuses
                    // the city-center (isCity) plot drives the city-level plot predicates (HAS_COAST etc.).
                    CityPlot? cityPlot = c.Plots.FirstOrDefault(p => p.IsCity);
                    Plot? plot = cityPlot is null ? null : new Plot
                    {
                        Terrain = cityPlot.Terrain, Bonus = cityPlot.Bonus, Route = cityPlot.Route,
                        Coast = cityPlot.Coast, River = cityPlot.River, Freshwater = cityPlot.River, Irrigation = cityPlot.Irrig,
                    };
                    // NOTE: the per-city wonder-category cap is ENGINE-DYNAMIC (era-scaled + `+extra` bumps,
                    // data-model.md §3.4/§132). A static count<allowance over-excludes — London holds 26 real
                    // worldWonders vs a 12 static `PHENOMENAL` allowance, i.e. it *built* past the static number.
                    // So it's deferred to the engine; only the static GROUP cap (above) is modeled here.
                    // a building already in the city is not buildable again (engine canConstruct excludes it).
                    // canConstruct = build AND operate ("operate is required to build, but build is not required to
                    // operate" — operate-side gates, e.g. PrereqReligion/PrereqCorporation, also gate the build).
                    // ...and a building/unit already in the production QUEUE is dropped from canConstruct/canTrain.
                    var myB = canB.Where(b => !built.Contains(b) && !notConB.Contains(b) && !c.QueuedBuildings.Contains(b) && CapOk(b, e, t.Id) && GroupOk(b, e.Id)
                        && evaluator.Evaluate(bReq.GetValueOrDefault(b)?.Build, state, plot)
                        && evaluator.Evaluate(bReq.GetValueOrDefault(b)?.Operate, state, plot)).ToHashSet(StringComparer.Ordinal);
                    var myU = canU.Where(u => !c.QueuedUnits.Contains(u)
                        && evaluator.Evaluate(uReq.GetValueOrDefault(u)?.Build, state, plot)
                        && evaluator.Evaluate(uReq.GetValueOrDefault(u)?.Operate, state, plot)).ToHashSet(StringComparer.Ordinal);

                    var bMe = myB.Where(x => !engB.Contains(x)).OrderBy(x => x, StringComparer.Ordinal).ToList();
                    var bEng = engB.Where(x => !myB.Contains(x)).OrderBy(x => x, StringComparer.Ordinal).ToList();
                    var uMe = myU.Where(x => !engU.Contains(x)).OrderBy(x => x, StringComparer.Ordinal).ToList();
                    var uEng = engU.Where(x => !myU.Contains(x)).OrderBy(x => x, StringComparer.Ordinal).ToList();

                    if (bMe.Count == 0 && bEng.Count == 0) exactB++;
                    if (uMe.Count == 0 && uEng.Count == 0) exactU++;
                    misB += bMe.Count + bEng.Count;
                    misU += uMe.Count + uEng.Count;
                    totBMe += bMe.Count; totBEng += bEng.Count; totUMe += uMe.Count; totUEng += uEng.Count;
                    perEmp.TryGetValue(e.Id, out (string City, int BMe, int BEng, int UMe, int UEng) pe);
                    perEmp[e.Id] = (pe.City ?? c.Name ?? $"emp{e.Id}", pe.BMe + bMe.Count, pe.BEng + bEng.Count, pe.UMe + uMe.Count, pe.UEng + uEng.Count);
                    // sample one city per EMPIRE (not just England) that has a mismatch
                    if (!sampledEmpires.Contains(e.Id) && (bMe.Count + bEng.Count + uMe.Count + uEng.Count) > 0
                        && sampledEmpires.Add(e.Id))
                        samples.Add($"  [emp {e.Id}] {c.Name}: BUILDINGS mine={myB.Count} eng={engB.Count}  (me-only={bMe.Count}, eng-only={bEng.Count})  ||  UNITS mine={myU.Count} eng={engU.Count}  (me-only={uMe.Count}, eng-only={uEng.Count})\n"
                            + $"      B me-only:  {string.Join(", ", bMe.Take(6))}\n"
                            + $"      B eng-only: {string.Join(", ", bEng.Take(6))}\n"
                            + $"      U me-only:  {string.Join(", ", uMe.Take(6))}\n"
                            + $"      U eng-only: {string.Join(", ", uEng.Take(6))}");
                }

    Console.WriteLine("BUILDABLE PARITY  cascade (generate -> requires) vs engine canConstruct / canTrain (per city)");
    Console.WriteLine($"  cities={cities}  buildings EXACT={exactB}/{cities} (mismatches={misB})  units EXACT={exactU}/{cities} (mismatches={misU})");
    Console.WriteLine($"  FINAL DIFFS -- buildings: me-only={totBMe} eng-only={totBEng}  ||  units: me-only={totUMe} eng-only={totUEng}");
    if (missingOracle > 0)
        Console.WriteLine($"  WARNING: {missingOracle} city/cities have EMPTY canConstruct+canTrain -- snapshot predates the oracle; re-extract.");
    Console.WriteLine("  per-EMPIRE totals (B me-only/eng-only, U me-only/eng-only):");
    foreach (KeyValuePair<int, (string City, int BMe, int BEng, int UMe, int UEng)> kv in perEmp.OrderBy(x => x.Key))
        Console.WriteLine($"    emp {kv.Key,2} ({kv.Value.City,-16}): B {kv.Value.BMe,5}/{kv.Value.BEng,-5}  U {kv.Value.UMe,5}/{kv.Value.UEng,-5}");
    foreach (string s in samples) Console.WriteLine(s);
    return misB == 0 && misU == 0 && missingOracle == 0 ? 0 : 1;
}

// Trace one target's fate in one city: who enables it, what removes it (replace/obsolete/disable from HAS),
// whether it's in HAS / already built. Maps the exact cause of a divergence instead of guessing.
static int TraceBuild(string assetsData, Snapshot snap, TechTally techTally, string cityName, string target)
{
    IReadOnlyList<EnablerEdges> edges = InfoLoader.LoadEnablerEdges(assetsData, "techs", "civics", "buildings", "bonuses", "religions");
    string kind = target.StartsWith("UNIT_", StringComparison.Ordinal) ? "units" : "buildings";
    // enables/disables are SOURCE-side (scan all sources); obsoletedBy/replacedBy are TARGET-side (read off the
    // target entity itself).
    List<string> By(BuildableEnabler.Edge ed) => edges
        .Where(s => (ed == BuildableEnabler.Edge.Enables ? s.Enables : s.Disables)
            ?.GetValueOrDefault(kind)?.Contains(target) == true)
        .Select(s => s.Type).ToList();
    EnablerEdges? tgt = edges.FirstOrDefault(e => e.Type == target);
    List<string> obsoleters = tgt?.ObsoletedBy?.Values.SelectMany(v => v).ToList() ?? [];
    List<string> replacers = tgt?.ReplacedBy?.Values.SelectMany(v => v).ToList() ?? [];

    var cityTally = new CityTally();
    var bonusTally = new BonusTally();
    foreach (Team t in snap.World.Teams)
        foreach (Empire e in t.Empires)
            foreach (Area a in e.Areas)
                foreach (City c in a.Cities)
                {
                    foreach (string b in c.Buildings) ((IBuildingCompleted)cityTally).BuildingCompleted(e.Id, Gid(e.Id, c.Id), b);
                    foreach (string bo in c.Bonuses) ((IBonusAdded)bonusTally).BonusAdded(e.Id, bo);
                }

    Team? team = null; Empire? emp = null; Area? area = null; City? city = null;
    foreach (Team t in snap.World.Teams)
        foreach (Empire e in t.Empires)
            foreach (Area a in e.Areas)
                foreach (City c in a.Cities)
                    if (string.Equals(c.Name, cityName, StringComparison.OrdinalIgnoreCase)) { team = t; emp = e; area = a; city = c; }
    if (city is null) { Console.Error.WriteLine($"no city named '{cityName}'"); return 2; }

    var has = new HashSet<string>(StringComparer.Ordinal);
    has.UnionWith(techTally.TeamHeld(team!.Id));
    has.UnionWith(emp!.Civics);
    has.UnionWith(cityTally.Buildings(Gid(emp.Id, city.Id)));
    has.UnionWith(bonusTally.Bonuses(emp.Id));
    has.UnionWith(city.Religions);

    List<string> InHas(List<string> xs) => xs.Where(has.Contains).ToList();
    Console.WriteLine($"TRACE {target} in {cityName} (team {team.Id}, empire {emp.Id}):");
    Console.WriteLine($"  enablers (all):    {string.Join(", ", By(BuildableEnabler.Edge.Enables))}");
    Console.WriteLine($"  enablers IN HAS:   {string.Join(", ", InHas(By(BuildableEnabler.Edge.Enables)))}");
    Console.WriteLine($"  replacedBy (all):  {string.Join(", ", replacers)}");
    Console.WriteLine($"  replacedBy IN HAS: {string.Join(", ", InHas(replacers))}");
    Console.WriteLine($"  obsoletedBy (all): {string.Join(", ", obsoleters)}");
    Console.WriteLine($"  obsoletedBy IN HAS:{string.Join(", ", InHas(obsoleters))}");
    Console.WriteLine($"  disablers IN HAS:  {string.Join(", ", InHas(By(BuildableEnabler.Edge.Disables)))}");

    // --- full pipeline (hard facts) ---
    Dictionary<string, List<string>> enX = BuildableEnabler.Index(edges, kind, BuildableEnabler.Edge.Enables);
    Dictionary<string, List<string>> obX = BuildableEnabler.Reverse(edges, BuildableEnabler.Edge.Obsoletes);
    Dictionary<string, List<string>> rpX = BuildableEnabler.Reverse(edges, BuildableEnabler.Edge.Replaces);
    Dictionary<string, List<string>> dsX = BuildableEnabler.Index(edges, kind, BuildableEnabler.Edge.Disables);
    HashSet<string> frontier = BuildableEnabler.CanGet(has, enX, obX, rpX, dsX);
    IReadOnlySet<string> built = cityTally.Buildings(Gid(emp.Id, city.Id));
    IReadOnlySet<string> notCon = InfoLoader.LoadNotConstructible(assetsData, "buildings");
    bool isQueued = (kind == "units" ? city.QueuedUnits : city.QueuedBuildings).Contains(target);
    EvalState state = EvalStateBuilder.Build(snap.World, team, emp, area!, city);
    CityPlot? cp = city.Plots.FirstOrDefault(p => p.IsCity);
    Plot? plot = cp is null ? null : new Plot { Terrain = cp.Terrain, Bonus = cp.Bonus, Route = cp.Route, Coast = cp.Coast, River = cp.River, Freshwater = cp.River, Irrigation = cp.Irrig };
    var evaluator = new ConditionEvaluator { StrictStateReligionForBuild = true };
    Requires? req = InfoLoader.LoadRequiresMap(assetsData, kind).GetValueOrDefault(target);
    bool reqBuild = evaluator.Evaluate(req?.Build, state, plot);
    bool reqOper = evaluator.Evaluate(req?.Operate, state, plot);
    bool engVerdict = (kind == "units" ? city.CanTrain : city.CanConstruct).Contains(target);

    Console.WriteLine($"  -- pipeline --");
    Console.WriteLine($"  in CAN GET frontier: {frontier.Contains(target)}");
    Console.WriteLine($"  already built: {built.Contains(target)}   notConstructible: {notCon.Contains(target)}   queued: {isQueued}");
    Console.WriteLine($"  requires.build eval: {reqBuild}   requires.operate eval: {reqOper}");
    Console.WriteLine($"  >>> MY verdict (buildable): {frontier.Contains(target) && !built.Contains(target) && !notCon.Contains(target) && !isQueued && reqBuild && reqOper}");
    Console.WriteLine($"  >>> ENGINE {(kind == "units" ? "canTrain" : "canConstruct")}: {engVerdict}");
    return 0;
}

// City ids are per-PLAYER in Civ4 (CvCity::getID), NOT globally unique — a documented gotcha (185 cities here
// share only 110 ids). Compose with the owning empire id for a global key, else cross-player cities with the same
// id pollute each other's built-set (e.g. London + 3 others all id 8192).
static long Gid(int empireId, long cityId) => ((long)empireId << 32) | (uint)cityId;

// A building's wonder category derives from its self-cap scope (data-model.md §3.4): world->worldWonders, etc.
static string? WonderCategory(Dictionary<string, int>? allowed) =>
    allowed is null ? null
    : allowed.ContainsKey("world") ? "worldWonders"
    : allowed.ContainsKey("team") ? "teamWonders"
    : allowed.ContainsKey("empire") ? "nationalWonders"
    : null;

static Team? FindTeam(Snapshot snap, string cityName) =>
    snap.World.Teams.FirstOrDefault(t =>
        t.Empires.Any(e => e.Areas.Any(a => a.Cities.Any(c =>
            string.Equals(c.Name, cityName, StringComparison.OrdinalIgnoreCase)))));

// A tech is out of scope for the tech-only cascade iff its requires.build references a non-tech prereq
// (a BUILDING/BONUS atom or a runtime predicate) -- something a lower, not-yet-validated layer owns.
static bool DependsOnNonTech(Condition? c) => c switch
{
    ConditionGroup g => (g.All?.Any(DependsOnNonTech) ?? false)
                     || (g.Any?.Any(grp => grp.Any(DependsOnNonTech)) ?? false)
                     || (g.NoneOf?.Any(DependsOnNonTech) ?? false)
                     || DependsOnNonTech(g.Enabled) || DependsOnNonTech(g.Disabled),
    Atom a => !a.Type.StartsWith("TECH_", StringComparison.Ordinal),
    BarePredicate => true,
    RawCondition => true,
    _ => false,
};

static string FindAssetsData()
{
    var dir = new DirectoryInfo(AppContext.BaseDirectory);
    while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "Assets", "Data")))
        dir = dir.Parent;
    return dir is null ? throw new DirectoryNotFoundException("Assets/Data not found")
                       : Path.Combine(dir.FullName, "Assets", "Data");
}

static string LatestSnapshot()
{
    var dir = new DirectoryInfo(AppContext.BaseDirectory);
    while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "validation", "output")))
        dir = dir.Parent;
    if (dir is null) throw new DirectoryNotFoundException("validation/output not found");
    string outDir = Path.Combine(dir.FullName, "validation", "output");
    FileInfo f = new DirectoryInfo(outDir).GetFiles("gamestate_*.json")
        .OrderByDescending(x => x.LastWriteTime).FirstOrDefault()
        ?? throw new FileNotFoundException($"no gamestate_*.json in {outDir}");
    return f.FullName;
}
