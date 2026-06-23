using System.Text.Json;
using System.Text.Json.Serialization;

namespace S2S.Model;

/// <summary>
/// One curated building definition (<c>Assets/Data/buildings/**.json</c>) — the "definitions" plane.
/// The cascade-critical sections (<c>enables</c>/<c>obsoletes</c>/<c>replaces</c>/<c>disables</c>/
/// <c>requires</c>/<c>allowed</c> + the condition vocabulary) are strongly typed; the remainder (the
/// open-ended modifier families + <c>identity</c>/<c>cost</c>/<c>ai</c>/<c>world</c>/<c>sound</c>) is
/// captured raw via <see cref="Families"/> and typed incrementally — so loading is complete and lossless.
///
/// NOTE: per the real curated data, the text fields (<c>description</c>/<c>civilopedia</c>) sit at TOP LEVEL,
/// not under <c>identity</c> — a known doc-vs-data discrepancy against data-model.md §1/§6.
/// </summary>
public sealed record BuildingInfo
{
    [JsonPropertyName("type")]        public required string Type { get; init; }
    [JsonPropertyName("description")] public string? Description { get; init; }
    [JsonPropertyName("civilopedia")] public string? Civilopedia { get; init; }

    // Availability (the enabler): target-kind (buildings/units/techs/…) -> list of type ids.
    [JsonPropertyName("enables")]   public Dictionary<string, List<string>>? Enables { get; init; }
    [JsonPropertyName("obsoletes")] public Dictionary<string, List<string>>? Obsoletes { get; init; }
    [JsonPropertyName("replaces")]  public Dictionary<string, List<string>>? Replaces { get; init; }
    [JsonPropertyName("disables")]  public Dictionary<string, List<string>>? Disables { get; init; }

    [JsonPropertyName("requires")] public Requires? Requires { get; init; }

    /// <summary>The declarative instance cap — scope key (<c>world</c>/<c>team</c>/<c>empire</c>) -> count.</summary>
    [JsonPropertyName("allowed")] public Dictionary<string, int>? Allowed { get; init; }

    // Typed incrementally — captured raw for now so nothing is lost on load.
    [JsonPropertyName("grants")]   public JsonElement? Grants { get; init; }
    [JsonPropertyName("identity")] public JsonElement? Identity { get; init; }
    [JsonPropertyName("cost")]     public JsonElement? Cost { get; init; }
    [JsonPropertyName("ai")]       public JsonElement? Ai { get; init; }
    [JsonPropertyName("world")]    public JsonElement? World { get; init; }
    [JsonPropertyName("sound")]    public JsonElement? Sound { get; init; }

    /// <summary>Every remaining top-level key — the open-ended modifier families (<c>food</c>, <c>commerce</c>,
    /// <c>culture</c>, <c>PROPERTY_*</c>, <c>hurryCost</c>, <c>allowedSpecialists</c>, …) plus anything not yet
    /// typed. Raw JSON, kept for lossless fallback; the magnitude families are also exposed typed via
    /// <see cref="Modifiers"/>.</summary>
    [JsonExtensionData] public Dictionary<string, JsonElement>? Families { get; init; }

    /// <summary>The typed modifier families parsed from <see cref="Families"/> (populated by the loader via
    /// <see cref="ModifierFamilyParser.ParseAll"/>). Empty until loaded through <see cref="InfoLoader"/>.</summary>
    [JsonIgnore] public IReadOnlyDictionary<string, ModifierFamily> Modifiers { get; init; }
        = new Dictionary<string, ModifierFamily>();
}

/// <summary>The two-phase means gate (data-model.md §3.2): <see cref="Build"/> is the one-time construction
/// gate (greying); <see cref="Operate"/> is the continuous gate (dormancy when lost).</summary>
public sealed record Requires(Condition? Build, Condition? Operate);
