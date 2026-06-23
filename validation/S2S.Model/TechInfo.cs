using System.Text.Json;
using System.Text.Json.Serialization;

namespace S2S.Model;

/// <summary>
/// One curated tech definition (<c>Assets/Data/techs/**.json</c>). For the enabler the load-bearing parts are
/// <see cref="Requires"/> (its <c>build</c> = the prereq tree of tech atoms) and <see cref="Enables"/> (what it
/// unlocks). Everything else rides raw via <see cref="Extra"/>, typed incrementally.
/// </summary>
public sealed record TechInfo
{
    [JsonPropertyName("type")]     public required string Type { get; init; }
    [JsonPropertyName("requires")] public Requires? Requires { get; init; }
    [JsonPropertyName("enables")]  public Dictionary<string, List<string>>? Enables { get; init; }
    [JsonPropertyName("obsoletes")] public Dictionary<string, List<string>>? Obsoletes { get; init; }

    /// <summary>Declarative instance cap (data-model.md §3.4) — e.g. <c>{world:1}</c> for a found-once
    /// religion tech. A world-capped tech is researchable only while the global count is below the cap.</summary>
    [JsonPropertyName("allowed")]  public Dictionary<string, int>? Allowed { get; init; }

    [JsonPropertyName("identity")] public JsonElement? Identity { get; init; }

    [JsonExtensionData] public Dictionary<string, JsonElement>? Extra { get; init; }

    /// <summary>A disabled placeholder tech (<c>identity.disable</c>) — not researchable by anyone.</summary>
    [JsonIgnore]
    public bool IsDisabled =>
        Identity is { ValueKind: JsonValueKind.Object } id
        && id.TryGetProperty("disable", out JsonElement d)
        && d.ValueKind == JsonValueKind.True;
}
