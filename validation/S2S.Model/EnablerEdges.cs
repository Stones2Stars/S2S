using System.Text.Json;
using System.Text.Json.Serialization;

namespace S2S.Model;

/// <summary>
/// The enabler edges authored on an entity. Two DIRECTIONS, by design (owner ruling 2026-06-22):
/// <list type="bullet">
///   <item><b><c>enables</c></b> — SOURCE-side: what THIS entity unlocks (a tech enables buildings/units).
///         Generation scans HAS forward over this.</item>
///   <item><b><c>obsoletedBy</c> / <c>replacedBy</c></b> — TARGET-side: what supersedes THIS entity (the
///         tech/building that obsoletes it, the building that replaces it). Authored the natural way the XML
///         and engine express it (<c>ObsoleteTech</c>/<c>ObsoletesToBuilding</c>/<c>ReplacementBuildings</c> read
///         off the entity itself), so the curator never inverts. Keyed by the SUPERSEDER's kind.</item>
/// </list>
/// The source-side obsoletes/replaces index the cascade needs is reconstructed in-memory at load by
/// <see cref="S2S.Enabler.BuildableEnabler.Reverse"/> (the "reverse mapping" — never in the JSON).
/// </summary>
public sealed record EnablerEdges
{
    [JsonPropertyName("type")]        public required string Type { get; init; }
    [JsonPropertyName("enables")]     public Dictionary<string, List<string>>? Enables { get; init; }
    [JsonPropertyName("obsoletedBy")] public Dictionary<string, List<string>>? ObsoletedBy { get; init; }
    [JsonPropertyName("replacedBy")]  public Dictionary<string, List<string>>? ReplacedBy { get; init; }
    [JsonPropertyName("disables")]    public Dictionary<string, List<string>>? Disables { get; init; }

    [JsonExtensionData] public Dictionary<string, JsonElement>? Extra { get; init; }
}

/// <summary>A target entity's <c>type</c> + its <c>requires</c> — the gate side. Loaded per subdir for the
/// requires pass (enabler.md §3); only the build gate (<c>requires.build</c>) is consulted for buildability.</summary>
public sealed record RequiresEntry
{
    [JsonPropertyName("type")]     public required string Type { get; init; }
    [JsonPropertyName("requires")] public Requires? Requires { get; init; }

    [JsonExtensionData] public Dictionary<string, JsonElement>? Extra { get; init; }
}
