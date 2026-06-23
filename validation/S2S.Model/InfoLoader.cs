using System.Text.Json;

namespace S2S.Model;

/// <summary>Loads curated entity JSON from an <c>Assets/Data</c> directory into the typed model.</summary>
public static class InfoLoader
{
    public static readonly JsonSerializerOptions Options = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };

    /// <summary>Deserialize one building JSON file.</summary>
    public static BuildingInfo LoadBuilding(string path)
    {
        string json = File.ReadAllText(path);
        BuildingInfo b = JsonSerializer.Deserialize<BuildingInfo>(json, Options)
            ?? throw new InvalidOperationException($"deserialized to null: {path}");
        return b with { Modifiers = ModifierFamilyParser.ParseAll(b.Families) };
    }

    /// <summary>Load every building under <c>{assetsDataDir}/buildings/**</c> into a repository.
    /// Throws with the offending file path if any single file fails to parse (fail loud, never silent).</summary>
    public static InfoRepository LoadBuildings(string assetsDataDir)
    {
        string dir = Path.Combine(assetsDataDir, "buildings");
        if (!Directory.Exists(dir))
            throw new DirectoryNotFoundException($"buildings dir not found: {dir}");

        var buildings = new Dictionary<string, BuildingInfo>(StringComparer.Ordinal);
        foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
        {
            BuildingInfo b;
            try
            {
                b = LoadBuilding(file);
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException($"failed loading {file}: {ex.Message}", ex);
            }
            buildings[b.Type] = b;
        }
        return new InfoRepository(buildings);
    }

    /// <summary>Load every tech under <c>{assetsDataDir}/techs/**</c>, keyed by type id.</summary>
    public static IReadOnlyDictionary<string, TechInfo> LoadTechs(string assetsDataDir)
    {
        string dir = Path.Combine(assetsDataDir, "techs");
        if (!Directory.Exists(dir))
            throw new DirectoryNotFoundException($"techs dir not found: {dir}");

        var techs = new Dictionary<string, TechInfo>(StringComparer.Ordinal);
        foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
        {
            TechInfo t;
            try
            {
                t = JsonSerializer.Deserialize<TechInfo>(File.ReadAllText(file), Options)
                    ?? throw new InvalidOperationException($"deserialized to null: {file}");
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException($"failed loading {file}: {ex.Message}", ex);
            }
            techs[t.Type] = t;
        }
        return techs;
    }

    /// <summary>Load every BUILD under <c>{assetsDataDir}/builds/**</c>, keyed by type id.</summary>
    public static IReadOnlyDictionary<string, BuildInfo> LoadBuilds(string assetsDataDir)
    {
        string dir = Path.Combine(assetsDataDir, "builds");
        if (!Directory.Exists(dir)) throw new DirectoryNotFoundException($"builds dir not found: {dir}");
        var builds = new Dictionary<string, BuildInfo>(StringComparer.Ordinal);
        foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
        {
            BuildInfo b = JsonSerializer.Deserialize<BuildInfo>(File.ReadAllText(file), Options)
                ?? throw new InvalidOperationException($"deserialized to null: {file}");
            builds[b.Type] = b;
        }
        return builds;
    }

    /// <summary>Load the civic type ids under <c>{assetsDataDir}/civics/**</c>.</summary>
    public static IReadOnlyList<string> LoadCivicTypes(string assetsDataDir)
    {
        string dir = Path.Combine(assetsDataDir, "civics");
        if (!Directory.Exists(dir)) throw new DirectoryNotFoundException($"civics dir not found: {dir}");
        var types = new List<string>();
        foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
        {
            using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(file));
            if (doc.RootElement.TryGetProperty("type", out JsonElement t) && t.ValueKind == JsonValueKind.String)
                types.Add(t.GetString()!);
        }
        return types;
    }

    /// <summary>Load <c>type → requires</c> for every entity under a subdir (e.g. <c>buildings</c>, <c>units</c>),
    /// for the requires-gate (the <c>requires.build</c> condition each candidate is gated by).</summary>
    public static IReadOnlyDictionary<string, Requires?> LoadRequiresMap(string assetsDataDir, string subdir)
    {
        string dir = Path.Combine(assetsDataDir, subdir);
        if (!Directory.Exists(dir)) throw new DirectoryNotFoundException($"requires dir not found: {dir}");
        var map = new Dictionary<string, Requires?>(StringComparer.Ordinal);
        foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
        {
            RequiresEntry e = JsonSerializer.Deserialize<RequiresEntry>(File.ReadAllText(file), Options)
                ?? throw new InvalidOperationException($"deserialized to null: {file}");
            map[e.Type] = e.Requires;
        }
        return map;
    }

    /// <summary>Load the set of entity types flagged <c>identity.notConstructible</c> (cost=-1: auto-built/spawned,
    /// never player-built — the engine's <c>canConstruct getProductionCost()==-1</c> gate). Excluded from buildable.</summary>
    public static IReadOnlySet<string> LoadNotConstructible(string assetsDataDir, string subdir)
    {
        string dir = Path.Combine(assetsDataDir, subdir);
        if (!Directory.Exists(dir)) throw new DirectoryNotFoundException($"notConstructible dir not found: {dir}");
        var set = new HashSet<string>(StringComparer.Ordinal);
        foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
        {
            using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(file));
            JsonElement root = doc.RootElement;
            if (root.TryGetProperty("type", out JsonElement t) && t.ValueKind == JsonValueKind.String
                && root.TryGetProperty("identity", out JsonElement id) && id.ValueKind == JsonValueKind.Object
                && id.TryGetProperty("notConstructible", out JsonElement nc) && nc.ValueKind == JsonValueKind.True)
                set.Add(t.GetString()!);
        }
        return set;
    }

    /// <summary>Load <c>type → allowed</c> (the declarative instance cap: <c>{world|team|empire: N}</c>) for every
    /// entity under a subdir. A build is permitted while the count at the capped scope is below N.</summary>
    public static IReadOnlyDictionary<string, Dictionary<string, int>?> LoadAllowedMap(string assetsDataDir, string subdir)
    {
        string dir = Path.Combine(assetsDataDir, subdir);
        if (!Directory.Exists(dir)) throw new DirectoryNotFoundException($"allowed dir not found: {dir}");
        var map = new Dictionary<string, Dictionary<string, int>?>(StringComparer.Ordinal);
        foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
        {
            using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(file));
            JsonElement root = doc.RootElement;
            if (!root.TryGetProperty("type", out JsonElement t) || t.ValueKind != JsonValueKind.String) continue;
            Dictionary<string, int>? allowed = null;
            if (root.TryGetProperty("allowed", out JsonElement a) && a.ValueKind == JsonValueKind.Object)
            {
                allowed = new Dictionary<string, int>(StringComparer.Ordinal);
                foreach (JsonProperty p in a.EnumerateObject())
                    if (p.Value.ValueKind == JsonValueKind.Number) allowed[p.Name] = p.Value.GetInt32();
            }
            map[t.GetString()!] = allowed;
        }
        return map;
    }

    /// <summary>Load <c>buildingType → identity.specialBuildingType</c> (the member→group link) for the
    /// SpecialBuilding group cap (§3.4): a member is gated by how many of its GROUP the empire already holds.</summary>
    public static IReadOnlyDictionary<string, string> LoadSpecialBuildingType(string assetsDataDir, string subdir)
    {
        string dir = Path.Combine(assetsDataDir, subdir);
        if (!Directory.Exists(dir)) throw new DirectoryNotFoundException($"specialBuildingType dir not found: {dir}");
        var map = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
        {
            using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(file));
            JsonElement root = doc.RootElement;
            if (root.TryGetProperty("type", out JsonElement t) && t.ValueKind == JsonValueKind.String
                && root.TryGetProperty("identity", out JsonElement id) && id.ValueKind == JsonValueKind.Object
                && id.TryGetProperty("specialBuildingType", out JsonElement sb) && sb.ValueKind == JsonValueKind.String)
                map[t.GetString()!] = sb.GetString()!;
        }
        return map;
    }

    /// <summary>Load the forward enabler edges (<c>enables</c>/<c>obsoletes</c>/<c>replaces</c>) of every source
    /// under the given subdirs (e.g. <c>techs</c>, <c>civics</c>, <c>bonuses</c>, <c>buildings</c>). One entry per
    /// source entity; the caller builds the source→targets index.</summary>
    public static IReadOnlyList<EnablerEdges> LoadEnablerEdges(string assetsDataDir, params string[] subdirs)
    {
        var edges = new List<EnablerEdges>();
        foreach (string sub in subdirs)
        {
            string dir = Path.Combine(assetsDataDir, sub);
            if (!Directory.Exists(dir)) throw new DirectoryNotFoundException($"enabler-edges dir not found: {dir}");
            foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
            {
                EnablerEdges e = JsonSerializer.Deserialize<EnablerEdges>(File.ReadAllText(file), Options)
                    ?? throw new InvalidOperationException($"deserialized to null: {file}");
                edges.Add(e);
            }
        }
        return edges;
    }

    /// <summary>Load the bonus (resource) type ids under <c>{assetsDataDir}/bonuses/**</c>.</summary>
    public static IReadOnlyList<string> LoadBonusTypes(string assetsDataDir)
    {
        string dir = Path.Combine(assetsDataDir, "bonuses");
        if (!Directory.Exists(dir)) throw new DirectoryNotFoundException($"bonuses dir not found: {dir}");
        var types = new List<string>();
        foreach (string file in Directory.EnumerateFiles(dir, "*.json", SearchOption.AllDirectories))
        {
            using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(file));
            if (doc.RootElement.TryGetProperty("type", out JsonElement t) && t.ValueKind == JsonValueKind.String)
                types.Add(t.GetString()!);
        }
        return types;
    }
}

/// <summary>In-memory <see cref="IInfoRepository"/>.</summary>
public sealed class InfoRepository(IReadOnlyDictionary<string, BuildingInfo> buildings) : IInfoRepository
{
    public IReadOnlyDictionary<string, BuildingInfo> Buildings { get; } = buildings;
    public int Count => Buildings.Count;
}
