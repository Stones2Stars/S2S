using System.Text.Json;

namespace S2S.Parity;

// DTOs mirroring the live gamestate snapshot (field names VERIFIED against a real extract).
// PropertyNameCaseInsensitive maps these PascalCase members onto the camelCase JSON keys; unmapped JSON
// members are ignored. Only the fields the enabler parity needs are declared.

public sealed class Snapshot
{
    public int Turn { get; set; }
    public World World { get; set; } = new();
}

public sealed class World
{
    public List<string> Options { get; set; } = [];
    public Dictionary<string, int> ReligionLevels { get; set; } = [];
    public Dictionary<string, int> UnitCreatedCounts { get; set; } = [];   // getUnitCreatedCount (lifetime, world cap)
    public List<Team> Teams { get; set; } = [];
}

public sealed class Team
{
    public int Id { get; set; }
    public List<string> Techs { get; set; } = [];
    public List<string> Projects { get; set; } = [];
    public List<Empire> Empires { get; set; } = [];
}

public sealed class Empire
{
    public int Id { get; set; }
    public List<string> Civics { get; set; } = [];
    public List<string> Traits { get; set; } = [];
    public List<string> Heritages { get; set; } = [];
    public string? StateReligion { get; set; }
    public int CityCount { get; set; }
    public bool NonStateReligionCommerce { get; set; }
    public Dictionary<string, int> BuildingCounts { get; set; } = [];
    public Dictionary<string, int> UnitCounts { get; set; } = [];

    /// <summary>The engine's <c>canResearch</c> verdict for this player — the COMPARISON oracle only,
    /// never an input to the cascade computation.</summary>
    public List<string> AvailableTechs { get; set; } = [];

    /// <summary>The engine's <c>canDoCivics</c> verdict for this player — COMPARISON oracle only.</summary>
    public List<string> AvailableCivics { get; set; } = [];

    /// <summary>The engine's BUILD-unlock verdict (build's tech prereq held) — COMPARISON oracle only.</summary>
    public List<string> AvailableBuilds { get; set; } = [];

    public List<Area> Areas { get; set; } = [];
}

public sealed class Area
{
    public int AreaSize { get; set; }
    public List<City> Cities { get; set; } = [];
}

public sealed class City
{
    public string? Name { get; set; }
    public long Id { get; set; }
    public List<string> Buildings { get; set; } = [];            // hasBuilding (engine already excludes obsolete)
    public List<string> DormantBuildings { get; set; } = [];     // the ENGINE's verdict — the oracle
    public List<string> Bonuses { get; set; } = [];              // trade-connected
    public List<string> VicinityBonuses { get; set; } = [];
    public List<string> Religions { get; set; } = [];
    public List<string> HolyCity { get; set; } = [];
    public List<string> PresentCorporations { get; set; } = [];
    public List<string> Corporations { get; set; } = [];
    public Dictionary<string, int> Properties { get; set; } = []; // PROPERTY_* current values -> band atoms
    public bool IsCapital { get; set; }
    public bool IsPowered { get; set; }
    public bool IsGoldenAge { get; set; }
    public int Population { get; set; }

    /// <summary>The engine's per-city <c>canConstruct</c> / <c>canTrain</c> verdict — the COMPARISON oracle for
    /// the cascade's isolated per-city buildable frontier; never an input.</summary>
    public List<string> CanConstruct { get; set; } = [];
    public List<string> CanTrain { get; set; } = [];

    /// <summary>The city's workable plots (incl. the city-center plot, <c>IsCity</c>). Feeds plot-relative
    /// <c>requires</c> (HAS_COAST, terrain, vicinity bonus) — previously evaluated against a null plot.</summary>
    public List<CityPlot> Plots { get; set; } = [];

    /// <summary>The city's culture level TYPE (e.g. <c>CULTURELEVEL_TREMENDOUS</c>) — grants the per-city
    /// wonder-category allowance (worldWonders/teamWonders/nationalWonders) for the category cap (§3.4).</summary>
    public string? CultureLevel { get; set; }

    /// <summary>The city's current production order queue — the engine excludes queued items from
    /// <c>canConstruct</c>/<c>canTrain</c>, so the cascade subtracts them from the buildable frontier.</summary>
    public List<string> QueuedBuildings { get; set; } = [];
    public List<string> QueuedUnits { get; set; } = [];

    /// <summary>The city plot's absolute latitude — the <c>{latitude:{min,max}}</c> requires gate
    /// (CvCascadeCondition <c>PRED_LATITUDE</c>: <c>min &lt;= getLatitude() &lt;= max</c>, data-model.md §2.5).</summary>
    public int Latitude { get; set; }
}

public sealed class CityPlot
{
    public string? Terrain { get; set; }
    public string? Bonus { get; set; }
    public string? Route { get; set; }
    public bool Coast { get; set; }
    public bool River { get; set; }
    public bool Irrig { get; set; }
    public bool IsCity { get; set; }
    public bool Worked { get; set; }
}

/// <summary>Loads a gamestate snapshot file into the typed DTOs.</summary>
public static class SnapshotLoader
{
    private static readonly JsonSerializerOptions Opts = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };

    public static Snapshot Load(string path)
        => JsonSerializer.Deserialize<Snapshot>(File.ReadAllText(path), Opts)
           ?? throw new InvalidOperationException($"snapshot deserialized to null: {path}");
}
