using S2S.Model;
using S2S.Parity;

// Composition root: load the definitions (Assets/Data) + the live snapshot, run the enabler active-set parity
// against the engine's own dormantBuildings, and print the report. Exit 0 iff zero bar violations.
//
// Args: [0] snapshot path (default: latest validation/output/gamestate_*.json)
//       [1] Assets/Data dir (default: walk up from the binary)

string snapPath = args.Length > 0 ? Path.GetFullPath(args[0]) : LatestSnapshot();
string assetsData = args.Length > 1 ? Path.GetFullPath(args[1]) : FindAssetsData();

Console.WriteLine($"[parity] definitions: {assetsData}");
Console.WriteLine($"[parity] snapshot:    {snapPath}");

InfoRepository repo = InfoLoader.LoadBuildings(assetsData);
Snapshot snap = SnapshotLoader.Load(snapPath);

IEnablerParity parity = new EnablerParity(new ConditionEvaluator());
ParityReport r = parity.Check(repo, snap);

Console.WriteLine();
Console.WriteLine("PARITY  enabler active-set (requires.operate + replacement)  vs engine dormantBuildings");
Console.WriteLine($"  buildings loaded={repo.Count:N0}  cities={r.Cities}  constructed={r.Constructed:N0}");
Console.WriteLine($"  EXACT cities (dry inactive == engine dormant): {r.ExactCities}/{r.Cities}");
Console.WriteLine($"  MISSED dormant (engine dormant, dry ACTIVE = BAR VIOLATION, target 0): {r.Missed}  by-cause={Fmt(r.MissedByCause)}");
foreach (string s in r.MissedSamples) Console.WriteLine($"     {s}");
Console.WriteLine($"  FALSE dormant (dry dormant, engine ACTIVE): {r.FalseDormant}  by-cause={Fmt(r.FalseByCause)}");
foreach (string s in r.FalseSamples) Console.WriteLine($"     {s}");

return r.Missed == 0 ? 0 : 1;

static string Fmt(Dictionary<string, int> d)
    => "{" + string.Join(", ", d.OrderByDescending(kv => kv.Value).Select(kv => $"{kv.Key}:{kv.Value}")) + "}";

static string FindAssetsData()
{
    var dir = new DirectoryInfo(AppContext.BaseDirectory);
    while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "Assets", "Data")))
        dir = dir.Parent;
    return dir is null
        ? throw new DirectoryNotFoundException("could not locate Assets/Data")
        : Path.Combine(dir.FullName, "Assets", "Data");
}

static string LatestSnapshot()
{
    var dir = new DirectoryInfo(AppContext.BaseDirectory);
    while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "validation", "output")))
        dir = dir.Parent;
    if (dir is null) throw new DirectoryNotFoundException("could not locate validation/output");

    string outDir = Path.Combine(dir.FullName, "validation", "output");
    FileInfo f = new DirectoryInfo(outDir).GetFiles("gamestate_*.json")
        .OrderByDescending(x => x.LastWriteTime).FirstOrDefault()
        ?? throw new FileNotFoundException($"no gamestate_*.json in {outDir}");
    return f.FullName;
}
