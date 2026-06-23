namespace S2S.Model;

/// <summary>
/// The live-state facts a <see cref="Condition"/> evaluates against — the per-city "live-state" plane
/// (techs / civics / bonuses / buildings / property values / …) that the extractor's gamestate snapshot
/// carries. A data record (not a service): the evaluator is the service behind <see cref="IConditionEvaluator"/>.
///
/// NOTE: the simple/complex trait split (GAMEOPTION_LEADER_COMPLEX_TRAITS rid↔base remap) is NOT yet modelled —
/// <see cref="Traits"/> is matched directly. A refinement for once the trait-replacement map is typed.
/// </summary>
public sealed record EvalState
{
    private static readonly IReadOnlySet<string> NoStrings = new HashSet<string>();
    private static readonly IReadOnlyDictionary<string, int> NoCounts = new Dictionary<string, int>();

    public IReadOnlySet<string> Techs { get; init; } = NoStrings;
    public IReadOnlySet<string> Civics { get; init; } = NoStrings;
    public IReadOnlySet<string> Traits { get; init; } = NoStrings;
    public IReadOnlySet<string> Religions { get; init; } = NoStrings;
    public IReadOnlySet<string> Heritages { get; init; } = NoStrings;
    public IReadOnlySet<string> Projects { get; init; } = NoStrings;
    public IReadOnlySet<string> ConstructedBuildings { get; init; } = NoStrings;
    public IReadOnlySet<string> Bonuses { get; init; } = NoStrings;             // trade-connected (hasBonus)
    public IReadOnlySet<string> VicinityBonuses { get; init; } = NoStrings;     // hasVicinityBonus
    public IReadOnlySet<string> PresentCorporations { get; init; } = NoStrings; // isHasCorporation (prereq gate)
    public IReadOnlySet<string> Corporations { get; init; } = NoStrings;        // active corporations
    public IReadOnlySet<string> HolyCityReligions { get; init; } = NoStrings;
    public IReadOnlySet<string> Options { get; init; } = NoStrings;             // active GAMEOPTION_*

    public string? StateReligion { get; init; }
    public bool IsCapital { get; init; }
    public bool IsPowered { get; init; }
    public bool IsGoldenAge { get; init; }
    public bool NonStateReligionCommerce { get; init; }

    public int Population { get; init; }
    public int CityCount { get; init; }
    public int TeamCount { get; init; }
    public int Latitude { get; init; }   // city plot absolute latitude — {latitude:{min,max}} gate (data-model §2.5)

    public IReadOnlyDictionary<string, int> Counts { get; init; } = NoCounts;          // PROPERTY_* values, gate flags, AREA_SIZE
    public IReadOnlyDictionary<string, int> ReligionLevels { get; init; } = NoCounts;
    public IReadOnlyDictionary<string, int> BuildingCounts { get; init; } = NoCounts;  // empire/team tallies
    public IReadOnlyDictionary<string, int> UnitCounts { get; init; } = NoCounts;
}

/// <summary>Optional plot context for plot-relative atoms/predicates (terrain / feature / improvement / route /
/// bonus + hills / peak / water / coast / river / freshwater / irrigation / map-category).</summary>
public sealed record Plot
{
    private static readonly IReadOnlySet<string> NoStrings = new HashSet<string>();

    public string? Terrain { get; init; }
    public string? Feature { get; init; }
    public string? Improvement { get; init; }
    public string? Route { get; init; }
    public string? Bonus { get; init; }
    public bool Hills { get; init; }
    public bool Peak { get; init; }
    public bool Water { get; init; }
    public bool Coast { get; init; }
    public bool River { get; init; }
    public bool Freshwater { get; init; }
    public bool Irrigation { get; init; }

    /// <summary>The plot's map categories. EMPTY = uncategorized, which is VALID for any <c>MAPCATEGORY_*</c>
    /// atom (data-model.md §2.5 — an uncategorized plot passes).</summary>
    public IReadOnlySet<string> MapCategories { get; init; } = NoStrings;
}
